/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-System.
 *
 * StormByte-System original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-System source in this
 * repository. They do not cover other StormByte modules or any third-party
 * material shipped with this repository (including everything under
 * thirdparty/, and in particular the bundled StormByte Base tree), which
 * remains under its own license.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-System is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-System. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include <StormByte/system/device.hxx>

#include <StormByte/error.txx>
#include <StormByte/safe/wstring.hxx>

#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#ifdef WINDOWS
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	include <iphlpapi.h>
#	include <windows.h>
#	include <winioctl.h>
#	include <io.h>
#elifdef LINUX
#	include <cstring>
#	include <linux/ethtool.h>
#	include <linux/sockios.h>
#	include <net/if.h>
#	include <sys/ioctl.h>
#	include <sys/socket.h>
#	include <sys/stat.h>
#	include <sys/statfs.h>
#	include <sys/statvfs.h>
#	include <sys/sysmacros.h>
#	include <unistd.h>
#elifdef MACOS
#	include <CoreFoundation/CoreFoundation.h>
#	include <IOKit/IOBSD.h>
#	include <IOKit/IOKitLib.h>
#	include <IOKit/storage/IOMedia.h>
#	include <cstring>
#	include <sys/mount.h>
#	include <sys/stat.h>
#	include <unistd.h>
#endif

using namespace StormByte::System;

using Access = class Device::Access;
using AccessFlag = enum Device::AccessFlag;
using Kind = enum Device::Kind;
using Throughput = struct Device::Throughput;
using Window = struct Device::Window;

namespace {
	constexpr StormByte::ByteSize MiB{1024ull * 1024ull};
	constexpr StormByte::ByteSize GiB{1024ull * 1024ull * 1024ull};
	constexpr StormByte::ByteSize MinWindow{16ull * 1024ull};
	constexpr StormByte::ByteSize MaxWindow{1024ull * 1024ull};

	struct Preset {
		Kind kind;
		Throughput rate;
	};

	constexpr std::array<Preset, 8> kRate {{
		{ Kind::HDD,      { 150 * MiB, 150 * MiB } },
		{ Kind::SSD,      { 500 * MiB, 500 * MiB } },
		{ Kind::NVMeGen3, {   3 * GiB,   3 * GiB } },
		{ Kind::NVMeGen4, {   6 * GiB,   6 * GiB } },
		{ Kind::NVMeGen5, {  10 * GiB,  10 * GiB } },
		{ Kind::USBHDD,   { 100 * MiB, 100 * MiB } },
		{ Kind::USBStick, {  30 * MiB,  12 * MiB } },
		{ Kind::Network,  {  30 * MiB,  30 * MiB } }
	}};

	struct Snapshot {
		StormByte::Error::Fault fault;
		Kind kind;
		Access access;
		Throughput throughput;
	};

	constexpr Throughput RateOf(const Kind kind) noexcept {
		for (const auto& row : kRate) {
			if (row.kind == kind)
				return row.rate;
		}
		return kRate.front().rate;
	}

	Snapshot Ok(const Kind kind, const Access access) {
		return { StormByte::Error::Fault{Device::Error::Success}, kind, access, RateOf(kind) };
	}

	Snapshot Fail(const enum Device::Error code) {
		return { StormByte::Error::Fault{code}, Kind::HDD, Access{}, RateOf(Kind::HDD) };
	}

	constexpr Throughput FromLinkBps(const std::uint64_t link_bps) noexcept {
		if (link_bps == 0)
			return RateOf(Kind::Network);
		const StormByte::ByteSize useful{link_bps * 80ull / 100ull / 8ull};
		if (useful == 0)
			return RateOf(Kind::Network);
		return { useful, useful };
	}

	Window WindowFrom(const Throughput& rate) noexcept {
		auto one = [](const StormByte::ByteSize bps) noexcept {
			const StormByte::ByteSize raw = bps / 500ull;
			if (raw < MinWindow)
				return MinWindow;
			if (raw > MaxWindow)
				return MaxWindow;
			return raw;
		};
		return { one(rate.read_bps), one(rate.write_bps) };
	}

	std::string Lower(std::string s) {
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	bool IsNetworkFsName(const std::string& type) noexcept {
		const auto t = Lower(type);
		return t == "nfs" || t == "nfs4" || t == "cifs" || t == "smb" || t == "smb2" ||
			t == "smb3" || t == "smbfs" || t == "afpfs" || t == "afp" || t == "webdav" ||
			t == "9p" || t == "afs" || t.find("fuse") != std::string::npos;
	}

	std::filesystem::path NativePath(const StormByte::Safe::String& text) {
#ifdef WINDOWS
		const StormByte::Safe::WString wide(text);
		return std::filesystem::path(static_cast<std::wstring_view>(wide));
#else
		return std::filesystem::path(static_cast<std::string_view>(text));
#endif
	}

#ifdef LINUX
	std::string ReadSys(const std::filesystem::path& path) {
		std::ifstream in(path);
		if (!in)
			return {};
		std::string s;
		std::getline(in, s);
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
			s.pop_back();
		return s;
	}

	std::filesystem::path BlockRoot(const std::filesystem::path& sys_dev) {
		std::error_code ec;
		auto cur = std::filesystem::canonical(sys_dev, ec);
		if (ec)
			return {};
		for (int i = 0; i < 8; ++i) {
			if (std::filesystem::exists(cur / "queue" / "rotational", ec))
				return cur;
			const auto parent = cur.parent_path();
			if (parent == cur)
				break;
			cur = parent;
		}
		return {};
	}

	int NvmeGen(const std::filesystem::path& block) {
		const auto name = block.filename().string();
		if (name.rfind("nvme", 0) != 0)
			return 0;
		const auto link = Lower(ReadSys(block / "device" / "device" / "current_link_speed"));
		if (link.find("32.0") != std::string::npos)
			return 5;
		if (link.find("16.0") != std::string::npos)
			return 4;
		if (link.find("8.0") != std::string::npos)
			return 3;
		return 3;
	}

	std::string FsMagic(const std::filesystem::path& path) {
		struct statfs st {};
		if (statfs(path.c_str(), &st) != 0)
			return {};
		switch (st.f_type) {
			case 0x6969: return "nfs";
			case 0xFF534D42: return "cifs";
			default: break;
		}
		return {};
	}

	std::string DefaultIface() {
		std::ifstream in("/proc/net/route");
		if (!in)
			return {};
		std::string line;
		std::getline(in, line);
		while (std::getline(in, line)) {
			std::istringstream ss(line);
			std::string iface, dest;
			if (!(ss >> iface >> dest))
				continue;
			if (dest == "00000000")
				return iface;
		}
		return {};
	}

	std::uint64_t NicBits(const std::string& iface) {
		if (iface.empty())
			return 0;
		const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
		if (fd < 0)
			return 0;
		struct ifreq ifr {};
		struct ethtool_cmd cmd {};
		std::strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
		cmd.cmd = ETHTOOL_GSET;
		ifr.ifr_data = reinterpret_cast<char*>(&cmd);
		std::uint64_t bits = 0;
		if (ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
			const unsigned int mbps = ethtool_cmd_speed(&cmd);
			if (mbps != 0 && mbps != static_cast<unsigned int>(-1))
				bits = static_cast<std::uint64_t>(mbps) * 1000ull * 1000ull;
		}
		::close(fd);
		return bits;
	}

	Kind ClassifyLinuxBlock(const dev_t id) {
		const auto sys = std::filesystem::path("/sys/dev/block") /
			(std::to_string(gnu_dev_major(id)) + ":" + std::to_string(gnu_dev_minor(id)));
		const auto block = BlockRoot(sys);
		if (block.empty())
			return Kind::HDD;

		const bool rotational = ReadSys(block / "queue" / "rotational") == "1";
		const bool removable = ReadSys(block / "removable") == "1";
		const bool nvme = block.filename().string().rfind("nvme", 0) == 0;

		if (removable)
			return rotational ? Kind::USBHDD : Kind::USBStick;
		if (rotational)
			return Kind::HDD;
		if (nvme) {
			const int gen = NvmeGen(block);
			if (gen >= 5)
				return Kind::NVMeGen5;
			if (gen >= 4)
				return Kind::NVMeGen4;
			return Kind::NVMeGen3;
		}
		return Kind::SSD;
	}

	bool MountReadOnly(const std::filesystem::path& path) {
		struct statvfs st {};
		if (statvfs(path.c_str(), &st) != 0)
			return false;
		return (st.f_flag & ST_RDONLY) != 0;
	}

	bool CanReadPath(const std::filesystem::path& path) {
		return ::access(path.c_str(), R_OK) == 0;
	}

	bool CanWritePath(const std::filesystem::path& path) {
		return ::access(path.c_str(), W_OK) == 0;
	}

	bool CanCreateIn(const std::filesystem::path& dir) {
		return ::access(dir.c_str(), W_OK | X_OK) == 0;
	}

	Snapshot ClassifyLinuxKind(const std::filesystem::path& volume, const bool network) {
		if (network) {
			Snapshot snap = Ok(Kind::Network, Access{});
			snap.throughput = FromLinkBps(NicBits(DefaultIface()));
			return snap;
		}
		if (IsNetworkFsName(FsMagic(volume))) {
			Snapshot snap = Ok(Kind::Network, Access{});
			snap.throughput = FromLinkBps(NicBits(DefaultIface()));
			return snap;
		}
		struct stat st {};
		if (stat(volume.c_str(), &st) != 0)
			return Fail(Device::Error::ProbeFailed);
		return Ok(ClassifyLinuxBlock(st.st_dev), Access{});
	}

	Snapshot ProbeLinux(const std::filesystem::path& path) {
		struct stat linkst {};
		const int link_rc = lstat(path.c_str(), &linkst);
		if (link_rc == 0 && S_ISLNK(linkst.st_mode)) {
			struct stat tgt {};
			if (stat(path.c_str(), &tgt) != 0) {
				if (errno == EACCES || errno == EPERM)
					return Fail(Device::Error::Permission);
				if (errno == ENOENT || errno == ENOTDIR || errno == ELOOP)
					return Fail(Device::Error::BrokenSymlink);
				return Fail(Device::Error::ProbeFailed);
			}
		}

		struct stat st {};
		if (stat(path.c_str(), &st) == 0) {
			if (S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode))
				return Fail(Device::Error::NotADevice);
			if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
				Access access;
				if (CanReadPath(path))
					access |= Access{AccessFlag::Readable};
				return Ok(ClassifyLinuxBlock(st.st_rdev), access);
			}
			if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
				return Fail(Device::Error::NotADevice);

			auto snap = ClassifyLinuxKind(path, false);
			if (snap.fault)
				return snap;
			if (CanReadPath(path))
				snap.access |= Access{AccessFlag::Readable};
			if (!MountReadOnly(path) && CanWritePath(path))
				snap.access |= Access{AccessFlag::Writable};
			return snap;
		}

		if (errno == EACCES || errno == EPERM)
			return Fail(Device::Error::Permission);
		if (errno != ENOENT)
			return Fail(Device::Error::ProbeFailed);

		std::filesystem::path cursor = path.parent_path();
		while (!cursor.empty() && cursor != cursor.root_path()) {
			struct stat pst {};
			if (stat(cursor.c_str(), &pst) != 0) {
				if (errno == EACCES || errno == EPERM)
					return Fail(Device::Error::Permission);
				cursor = cursor.parent_path();
				continue;
			}
			if (S_ISBLK(pst.st_mode) || S_ISCHR(pst.st_mode))
				return Fail(Device::Error::DeviceNotFound);
			if (!S_ISDIR(pst.st_mode))
				return Fail(Device::Error::DeviceNotFound);

			auto snap = ClassifyLinuxKind(cursor, false);
			if (snap.fault)
				return snap;
			if (!MountReadOnly(cursor) && CanCreateIn(cursor))
				snap.access |= Access{AccessFlag::Writable};
			return snap;
		}
		return Fail(Device::Error::DeviceNotFound);
	}
#elifdef WINDOWS
	Snapshot WindowsNic(const Access access) {
		DWORD index = 0;
		if (GetBestInterface(htonl(0x08080808), &index) != NO_ERROR)
			return Ok(Kind::Network, access);
		MIB_IF_ROW2 row {};
		row.InterfaceIndex = index;
		if (GetIfEntry2(&row) != NO_ERROR || row.TransmitLinkSpeed == 0)
			return Ok(Kind::Network, access);
		Snapshot snap = Ok(Kind::Network, access);
		snap.throughput = FromLinkBps(row.TransmitLinkSpeed);
		return snap;
	}

	bool IsReparse(const std::wstring& wide) {
		const DWORD attr = GetFileAttributesW(wide.c_str());
		if (attr == INVALID_FILE_ATTRIBUTES)
			return false;
		return (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
	}

	bool ExistsFollow(const std::wstring& wide) {
		const DWORD attr = GetFileAttributesW(wide.c_str());
		return attr != INVALID_FILE_ATTRIBUTES;
	}

	Snapshot ClassifyWindowsVolume(const std::wstring& wide, const Access access) {
		if (wide.size() < 2 || wide[1] != L':')
			return Ok(Kind::HDD, access);

		wchar_t drive[] = { wide[0], L':', L'\\', 0 };
		const UINT type = GetDriveTypeW(drive);
		if (type == DRIVE_REMOTE)
			return WindowsNic(access);
		if (type == DRIVE_CDROM)
			return Ok(Kind::HDD, access);

		wchar_t volume[] = { L'\\', L'\\', L'.', L'\\', wide[0], L':', 0 };
		const HANDLE disk = CreateFileW(volume, 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
		if (disk == INVALID_HANDLE_VALUE) {
			if (type == DRIVE_REMOVABLE)
				return Ok(Kind::USBStick, access);
			return Ok(Kind::HDD, access);
		}

		STORAGE_PROPERTY_QUERY query {};
		query.QueryType = PropertyStandardQuery;
		query.PropertyId = StorageDeviceSeekPenaltyProperty;
		DEVICE_SEEK_PENALTY_DESCRIPTOR penalty {};
		DWORD got = 0;
		const bool has_penalty = DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY,
			&query, sizeof(query), &penalty, sizeof(penalty), &got, nullptr) != 0;

		query.PropertyId = StorageAdapterProperty;
		STORAGE_ADAPTER_DESCRIPTOR adapter {};
		const bool has_adapter = DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY,
			&query, sizeof(query), &adapter, sizeof(adapter), &got, nullptr) != 0;
		CloseHandle(disk);

		const bool seek_penalty = has_penalty && penalty.IncursSeekPenalty;
		const bool nvme = has_adapter && adapter.BusType == BusTypeNvme;
		const bool removable = type == DRIVE_REMOVABLE;

		if (removable)
			return Ok(seek_penalty ? Kind::USBHDD : Kind::USBStick, access);
		if (seek_penalty)
			return Ok(Kind::HDD, access);
		if (nvme)
			return Ok(Kind::NVMeGen3, access);
		return Ok(Kind::SSD, access);
	}

	bool IsRawDevice(const std::wstring& wide) {
		return wide.rfind(L"\\\\.\\", 0) == 0 || wide.rfind(L"\\\\?\\", 0) == 0;
	}

	bool CanReadWide(const std::wstring& wide) {
		return _waccess(wide.c_str(), 4) == 0;
	}

	bool CanWriteWide(const std::wstring& wide) {
		return _waccess(wide.c_str(), 2) == 0;
	}

	Snapshot ProbeWindows(const std::filesystem::path& path) {
		const std::wstring wide = path.wstring();
		if (wide.empty())
			return Fail(Device::Error::DeviceNotFound);

		if (IsReparse(wide) && !ExistsFollow(wide)) {
			const DWORD error = GetLastError();
			if (error == ERROR_ACCESS_DENIED)
				return Fail(Device::Error::Permission);
			if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
				return Fail(Device::Error::BrokenSymlink);
			return Fail(Device::Error::ProbeFailed);
		}

		const DWORD attr = GetFileAttributesW(wide.c_str());
		if (attr != INVALID_FILE_ATTRIBUTES) {
			if (IsRawDevice(wide)) {
				Access access;
				if (CanReadWide(wide))
					access |= Access{AccessFlag::Readable};
				return ClassifyWindowsVolume(wide, access);
			}

			const UINT type = (wide.size() >= 2 && wide[1] == L':')
				? GetDriveTypeW(std::wstring{ wide[0], L':', L'\\', 0 }.c_str())
				: DRIVE_UNKNOWN;
			Access access;
			if (CanReadWide(wide))
				access |= Access{AccessFlag::Readable};
			if (type != DRIVE_CDROM && CanWriteWide(wide) && (attr & FILE_ATTRIBUTE_READONLY) == 0)
				access |= Access{AccessFlag::Writable};
			return ClassifyWindowsVolume(wide, access);
		}

		const DWORD err = GetLastError();
		if (err == ERROR_ACCESS_DENIED)
			return Fail(Device::Error::Permission);
		if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND)
			return Fail(Device::Error::ProbeFailed);

		std::filesystem::path cursor = path.parent_path();
		while (!cursor.empty() && cursor != cursor.root_path()) {
			const std::wstring parent = cursor.wstring();
			const DWORD pattr = GetFileAttributesW(parent.c_str());
			if (pattr == INVALID_FILE_ATTRIBUTES) {
				cursor = cursor.parent_path();
				continue;
			}
			if (IsRawDevice(parent))
				return Fail(Device::Error::DeviceNotFound);
			if ((pattr & FILE_ATTRIBUTE_DIRECTORY) == 0)
				return Fail(Device::Error::DeviceNotFound);

			Access access;
			const UINT type = (parent.size() >= 2 && parent[1] == L':')
				? GetDriveTypeW(std::wstring{ parent[0], L':', L'\\', 0 }.c_str())
				: DRIVE_UNKNOWN;
			if (type != DRIVE_CDROM && CanWriteWide(parent) && (pattr & FILE_ATTRIBUTE_READONLY) == 0)
				access |= Access{AccessFlag::Writable};
			return ClassifyWindowsVolume(parent, access);
		}
		return Fail(Device::Error::DeviceNotFound);
	}
#elifdef MACOS
	std::string CfToString(CFTypeRef ref) {
		if (!ref)
			return {};
		if (CFGetTypeID(ref) == CFStringGetTypeID()) {
			char buf[256];
			if (CFStringGetCString(static_cast<CFStringRef>(ref), buf, sizeof(buf),
					kCFStringEncodingUTF8))
				return buf;
		}
		if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
			double n = 0;
			CFNumberGetValue(static_cast<CFNumberRef>(ref), kCFNumberDoubleType, &n);
			return std::to_string(n);
		}
		return {};
	}

	bool CfBool(CFTypeRef ref, const bool fallback) {
		if (ref && CFGetTypeID(ref) == CFBooleanGetTypeID())
			return CFBooleanGetValue(static_cast<CFBooleanRef>(ref));
		return fallback;
	}

	int PcieGenFromGt(const std::string& raw) {
		const auto s = Lower(raw);
		if (s.find("32") != std::string::npos)
			return 5;
		if (s.find("16") != std::string::npos)
			return 4;
		if (s.find("8") != std::string::npos)
			return 3;
		return 3;
	}

	Kind FromIoRegistry(const std::string& bsd) {
		if (bsd.empty())
			return Kind::HDD;

		CFMutableDictionaryRef match = IOBSDNameMatching(kIOMainPortDefault, 0, bsd.c_str());
		if (!match)
			return Kind::HDD;
		io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, match);
		if (!service)
			return Kind::HDD;

		bool solid = false;
		bool removable = false;
		bool nvme = false;
		bool sata = false;
		int gen = 3;

		io_service_t cursor = service;
		for (int depth = 0; depth < 12 && cursor; ++depth) {
			CFMutableDictionaryRef props = nullptr;
			if (IORegistryEntryCreateCFProperties(cursor, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
				solid = CfBool(CFDictionaryGetValue(props, CFSTR("Solid State")), solid);
				removable = CfBool(CFDictionaryGetValue(props, CFSTR("Removable")), removable) ||
					CfBool(CFDictionaryGetValue(props, CFSTR("Ejectable")), removable);

				if (const auto proto = CFDictionaryGetValue(props, CFSTR("Protocol Characteristics"))) {
					if (CFGetTypeID(proto) == CFDictionaryGetTypeID()) {
						const auto interconnect = CfToString(CFDictionaryGetValue(
							static_cast<CFDictionaryRef>(proto), CFSTR("Physical Interconnect")));
						const auto low = Lower(interconnect);
						if (low.find("pci") != std::string::npos || low.find("nvme") != std::string::npos)
							nvme = true;
						if (low.find("sata") != std::string::npos || low.find("ata") != std::string::npos)
							sata = true;
					}
				}

				const auto cls = Lower(CfToString(CFDictionaryGetValue(props, CFSTR("IOClass"))));
				if (cls.find("nvme") != std::string::npos)
					nvme = true;
				if (cls.find("sata") != std::string::npos)
					sata = true;
				if (cls.find("pci") != std::string::npos) {
					const auto speed = CfToString(CFDictionaryGetValue(props, CFSTR("negotiated-link-speed")));
					if (!speed.empty())
						gen = PcieGenFromGt(speed);
				}
				CFRelease(props);
			}

			io_service_t parent = 0;
			if (IORegistryEntryGetParentEntry(cursor, kIOServicePlane, &parent) != KERN_SUCCESS)
				break;
			if (cursor != service)
				IOObjectRelease(cursor);
			cursor = parent;
		}
		if (cursor && cursor != service)
			IOObjectRelease(cursor);
		IOObjectRelease(service);

		if (removable)
			return solid ? Kind::USBStick : Kind::USBHDD;
		if (nvme) {
			if (gen >= 5)
				return Kind::NVMeGen5;
			if (gen >= 4)
				return Kind::NVMeGen4;
			return Kind::NVMeGen3;
		}
		if (solid || sata)
			return Kind::SSD;
		return Kind::HDD;
	}

	Kind ClassifyMacVolume(const std::filesystem::path& path) {
		struct statfs st {};
		if (statfs(path.c_str(), &st) != 0)
			return Kind::HDD;
		if (IsNetworkFsName(st.f_fstypename) || (st.f_flags & MNT_LOCAL) == 0)
			return Kind::Network;
		std::string bsd = st.f_mntfromname;
		if (bsd.rfind("/dev/", 0) == 0)
			bsd.erase(0, 5);
		return FromIoRegistry(bsd);
	}

	bool MountReadOnly(const std::filesystem::path& path) {
		struct statfs st {};
		if (statfs(path.c_str(), &st) != 0)
			return false;
		return (st.f_flags & MNT_RDONLY) != 0;
	}

	Snapshot ProbeMac(const std::filesystem::path& path) {
		struct stat linkst {};
		if (lstat(path.c_str(), &linkst) == 0 && S_ISLNK(linkst.st_mode)) {
			struct stat tgt {};
			if (stat(path.c_str(), &tgt) != 0) {
				if (errno == EACCES || errno == EPERM)
					return Fail(Device::Error::Permission);
				if (errno == ENOENT || errno == ENOTDIR || errno == ELOOP)
					return Fail(Device::Error::BrokenSymlink);
				return Fail(Device::Error::ProbeFailed);
			}
		}

		struct stat st {};
		if (stat(path.c_str(), &st) == 0) {
			if (S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode))
				return Fail(Device::Error::NotADevice);
			if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
				Access access;
				if (::access(path.c_str(), R_OK) == 0)
					access |= Access{AccessFlag::Readable};
				return Ok(ClassifyMacVolume(path), access);
			}
			if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
				return Fail(Device::Error::NotADevice);

			Access access;
			if (::access(path.c_str(), R_OK) == 0)
				access |= Access{AccessFlag::Readable};
			if (!MountReadOnly(path) && ::access(path.c_str(), W_OK) == 0)
				access |= Access{AccessFlag::Writable};
			return Ok(ClassifyMacVolume(path), access);
		}

		if (errno == EACCES || errno == EPERM)
			return Fail(Device::Error::Permission);
		if (errno != ENOENT)
			return Fail(Device::Error::ProbeFailed);

		std::filesystem::path cursor = path.parent_path();
		while (!cursor.empty() && cursor != cursor.root_path()) {
			struct stat pst {};
			if (stat(cursor.c_str(), &pst) != 0) {
				cursor = cursor.parent_path();
				continue;
			}
			if (S_ISBLK(pst.st_mode) || S_ISCHR(pst.st_mode))
				return Fail(Device::Error::DeviceNotFound);
			if (!S_ISDIR(pst.st_mode))
				return Fail(Device::Error::DeviceNotFound);

			Access access;
			if (!MountReadOnly(cursor) && ::access(cursor.c_str(), W_OK | X_OK) == 0)
				access |= Access{AccessFlag::Writable};
			return Ok(ClassifyMacVolume(cursor), access);
		}
		return Fail(Device::Error::DeviceNotFound);
	}
#endif

	Snapshot Probe(const StormByte::Safe::String& text, const bool initialization_failed) noexcept {
		if (initialization_failed)
			return Fail(Device::Error::ProbeFailed);
		if (static_cast<std::string_view>(text).empty())
			return Fail(Device::Error::DeviceNotFound);
		try {
			const auto path = NativePath(text);
#ifdef WINDOWS
			return ProbeWindows(path);
#elifdef MACOS
			return ProbeMac(path);
#elifdef LINUX
			return ProbeLinux(path);
#else
			static_cast<void>(path);
			return Fail(Device::Error::ProbeFailed);
#endif
		} catch (const std::filesystem::filesystem_error& error) {
			if (error.code() == std::errc::permission_denied)
				return Fail(Device::Error::Permission);
			if (error.code() == std::errc::no_such_file_or_directory)
				return Fail(Device::Error::DeviceNotFound);
			if (error.code() == std::errc::too_many_symbolic_link_levels)
				return Fail(Device::Error::BrokenSymlink);
			return Fail(Device::Error::ProbeFailed);
		} catch (...) {
			return Fail(Device::Error::ProbeFailed);
		}
	}
}

Device::Device(const StormByte::Safe::String& path) noexcept {
	try {
		m_path = path;
	} catch (...) {
		m_initialization_failed = true;
	}
}

Device::Device(const std::string_view path) noexcept {
	try {
		m_path = path;
	} catch (...) {
		m_initialization_failed = true;
	}
}

Device::Device(const std::wstring_view path) noexcept {
	try {
		m_path = StormByte::Safe::String(StormByte::Safe::WString(path));
	} catch (...) {
		m_initialization_failed = true;
	}
}

Device::operator bool() const noexcept {
	return !Fault();
}

StormByte::Error::Fault Device::Fault() const noexcept {
	return Probe(m_path, m_initialization_failed).fault;
}

const StormByte::Safe::String& Device::Path() const noexcept {
	return m_path;
}

Kind Device::Kind() const noexcept {
	return Probe(m_path, m_initialization_failed).kind;
}

Access Device::Access() const noexcept {
	return Probe(m_path, m_initialization_failed).access;
}

Throughput Device::Throughput() const noexcept {
	return Probe(m_path, m_initialization_failed).throughput;
}

Window Device::Window() const noexcept {
	return WindowFrom(Probe(m_path, m_initialization_failed).throughput);
}

namespace StormByte::System {
	const StormByte::Error::Category<enum Device::Error>& device_category() noexcept {
		static StormByte::Error::Category<enum Device::Error> instance;
		return instance;
	}

	std::error_code make_error_code(const enum Device::Error e) noexcept {
		return std::error_code(static_cast<int>(e), device_category());
	}
}
