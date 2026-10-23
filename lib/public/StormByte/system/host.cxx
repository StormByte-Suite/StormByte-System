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

#include <StormByte/error.txx>
#include <StormByte/safe/wstring.hxx>
#include <StormByte/system/host.hxx>

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef WINDOWS
#include <windows.h>
#elifdef MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

using namespace StormByte::System;

namespace {
	thread_local StormByte::Error::Fault g_last{make_error_code(Host::Error::Success)};

	bool Store(const enum Host::Error code) {
		g_last = StormByte::Error::Fault{make_error_code(code)};
		return code == Host::Error::Success;
	}

	StormByte::ByteSize StoreSize(const std::uint64_t bytes, const bool ok) {
		if (!ok || bytes == 0) {
			Store(Host::Error::Failed);
			return StormByte::ByteSize{0ull};
		}
		Store(Host::Error::Success);
		return StormByte::ByteSize{bytes};
	}

	StormByte::Safe::String StoreText(std::string text, const bool ok) {
		if (!ok || text.empty()) {
			Store(Host::Error::Failed);
			return StormByte::Safe::String();
		}
		Store(Host::Error::Success);
		return StormByte::Safe::String(text);
	}

	StormByte::Safe::String TextError(const Host::Error code) {
		Store(code);
		return {};
	}

	unsigned StoreCount(const unsigned count) {
		Store(count == 0 ? Host::Error::Unavailable : Host::Error::Success);
		return count;
	}

	std::string Trim(std::string value) {
		const auto begin = value.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return {};
		const auto end = value.find_last_not_of(" \t\r\n");
		return value.substr(begin, end - begin + 1);
	}

#ifdef WINDOWS
	StormByte::Safe::String RegistryText(const wchar_t* key, const wchar_t* field) {
		DWORD size = 0;
		auto status = RegGetValueW(HKEY_LOCAL_MACHINE, key, field, RRF_RT_REG_SZ, nullptr, nullptr, &size);
		auto fail = [](const LSTATUS error) {
			if (error == ERROR_ACCESS_DENIED)
				return TextError(Host::Error::Permission);
			if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
				return TextError(Host::Error::Unavailable);
			return TextError(Host::Error::Failed);
		};
		if (status != ERROR_SUCCESS)
			return fail(status);
		if (size < sizeof(wchar_t))
			return TextError(Host::Error::Unavailable);
		std::vector<wchar_t> buffer((size + sizeof(wchar_t) - 1) / sizeof(wchar_t), L'\0');
		status = RegGetValueW(HKEY_LOCAL_MACHINE, key, field, RRF_RT_REG_SZ, nullptr, buffer.data(), &size);
		if (status != ERROR_SUCCESS)
			return fail(status);
		const auto text = Trim(std::string(StormByte::Safe::String(StormByte::Safe::WString(buffer.data()))));
		if (text.empty())
			return TextError(Host::Error::Unavailable);
		return StoreText(text, true);
	}
#else
	Host::Error NativeError(const int code) {
		if (code == EACCES || code == EPERM)
			return Host::Error::Permission;
		if (code == ENOENT || code == ENODEV)
			return Host::Error::Unavailable;
		return Host::Error::Failed;
	}

	StormByte::Safe::String NormalizeArch(std::string_view raw) {
		if (raw == "x86_64" || raw == "amd64" || raw == "AMD64")
			return StormByte::Safe::String("x86_64");
		if (raw == "aarch64" || raw == "arm64" || raw == "ARM64")
			return StormByte::Safe::String("arm64");
		if (raw == "x86" || raw == "i386" || raw == "i686" || raw == "i586")
			return StormByte::Safe::String("x86");
		if (raw == "arm" || raw.rfind("armv", 0) == 0)
			return StormByte::Safe::String("arm");
		return StormByte::Safe::String(raw);
	}

#ifdef MACOS
	StormByte::Safe::String SysctlText(const char* key) {
		std::size_t size = 0;
		if (sysctlbyname(key, nullptr, &size, nullptr, 0) != 0)
			return TextError(NativeError(errno));
		if (size == 0)
			return TextError(Host::Error::Unavailable);
		std::vector<char> buffer(size + 1, '\0');
		if (sysctlbyname(key, buffer.data(), &size, nullptr, 0) != 0)
			return TextError(NativeError(errno));
		const auto text = Trim(buffer.data());
		if (text.empty())
			return TextError(Host::Error::Unavailable);
		return StoreText(text, true);
	}

	StormByte::Safe::String PlatformText(const CFStringRef key) {
		const io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
		if (service == IO_OBJECT_NULL)
			return TextError(Host::Error::Unavailable);
		const CFTypeRef property = IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0);
		IOObjectRelease(service);
		if (property == nullptr)
			return TextError(Host::Error::Unavailable);
		std::string text;
		bool valid = true;
		if (CFGetTypeID(property) == CFDataGetTypeID()) {
			const auto data = static_cast<CFDataRef>(property);
			const auto length = CFDataGetLength(data);
			if (length > 0) {
				text.assign(reinterpret_cast<const char*>(CFDataGetBytePtr(data)), static_cast<std::size_t>(length));
				const auto nul = text.find('\0');
				if (nul != std::string::npos)
					text.resize(nul);
			}
		}
		else if (CFGetTypeID(property) == CFStringGetTypeID()) {
			const auto string = static_cast<CFStringRef>(property);
			const auto size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string), kCFStringEncodingUTF8) + 1;
			std::vector<char> buffer(static_cast<std::size_t>(size), '\0');
			valid = CFStringGetCString(string, buffer.data(), size, kCFStringEncodingUTF8);
			if (valid)
				text = buffer.data();
		}
		else
			valid = false;
		CFRelease(property);
		if (!valid)
			return TextError(Host::Error::Failed);
		text = Trim(std::move(text));
		if (text.empty())
			return TextError(Host::Error::Unavailable);
		return StoreText(std::move(text), true);
	}
#else
	StormByte::Safe::String FileText(const char* path) {
		errno = 0;
		std::ifstream in(path);
		if (!in)
			return TextError(NativeError(errno));
		std::string text;
		std::getline(in, text);
		if (in.bad())
			return TextError(Host::Error::Failed);
		text = Trim(std::move(text));
		if (text.empty())
			return TextError(Host::Error::Unavailable);
		return StoreText(std::move(text), true);
	}

	StormByte::Safe::String CpuField(const std::initializer_list<std::string_view> keys) {
		errno = 0;
		std::ifstream in("/proc/cpuinfo");
		if (!in)
			return TextError(NativeError(errno));
		std::string line;
		while (std::getline(in, line)) {
			const auto colon = line.find(':');
			if (colon == std::string::npos)
				continue;
			const auto key = Trim(line.substr(0, colon));
			for (const auto candidate: keys) {
				if (key != candidate)
					continue;
				auto value = Trim(line.substr(colon + 1));
				if (!value.empty())
					return StoreText(std::move(value), true);
			}
		}
		return TextError(in.bad() ? Host::Error::Failed : Host::Error::Unavailable);
	}
#endif
#endif

#ifdef LINUX
	std::string Unquote(std::string value) {
		if (value.size() >= 2 && ((value.front() == '\'' && value.back() == '\'') || (value.front() == '"' && value.back() == '"')))
			return value.substr(1, value.size() - 2);
		return value;
	}

	std::string OsReleaseField(const std::string& key) {
		std::ifstream in("/etc/os-release");
		if (!in)
			return {};
		std::string line;
		const std::string prefix = key + "=";
		while (std::getline(in, line)) {
			if (line.rfind(prefix, 0) != 0)
				continue;
			return Unquote(line.substr(prefix.size()));
		}
		return {};
	}
#endif
}

StormByte::Error::Fault Host::LastError() noexcept {
	return g_last;
}

bool Host::Name(StormByte::Safe::String& name) {
#ifdef WINDOWS
	DWORD size = 0;
	GetComputerNameExW(ComputerNamePhysicalDnsHostname, nullptr, &size);
	if (size == 0)
		return Store(Host::Error::Failed);
	std::vector<wchar_t> buffer(size);
	if (!GetComputerNameExW(ComputerNamePhysicalDnsHostname, buffer.data(), &size))
		return Store(Host::Error::Failed);
	name = StormByte::Safe::String(StormByte::Safe::WString(std::wstring_view(buffer.data())));
	return Store(Host::Error::Success);
#else
	char buffer[256] = {};
	if (gethostname(buffer, sizeof(buffer)) != 0)
		return Store(Host::Error::Failed);
	buffer[sizeof(buffer) - 1] = '\0';
	name = StormByte::Safe::String(std::string_view(buffer));
	return Store(Host::Error::Success);
#endif
}

StormByte::Safe::String Host::CPU::Architecture() const {
#ifdef WINDOWS
	SYSTEM_INFO info {};
	GetNativeSystemInfo(&info);
	switch (info.wProcessorArchitecture) {
		case PROCESSOR_ARCHITECTURE_AMD64:
			return StoreText("x86_64", true);
		case PROCESSOR_ARCHITECTURE_ARM64:
			return StoreText("arm64", true);
		case PROCESSOR_ARCHITECTURE_INTEL:
			return StoreText("x86", true);
		case PROCESSOR_ARCHITECTURE_ARM:
			return StoreText("arm", true);
		default:
			return StoreText({}, false);
	}
#else
	struct utsname info {};
	if (uname(&info) != 0)
		return StoreText({}, false);
	return StoreText(std::string(NormalizeArch(info.machine)), true);
#endif
}

StormByte::Safe::String Host::CPU::Model() const {
#ifdef WINDOWS
	return RegistryText(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
#elifdef MACOS
	return SysctlText("machdep.cpu.brand_string");
#else
	return CpuField({"model name", "Processor"});
#endif
}

StormByte::Safe::String Host::CPU::Manufacturer() const {
#ifdef WINDOWS
	return RegistryText(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"VendorIdentifier");
#elifdef MACOS
	return SysctlText("machdep.cpu.vendor");
#else
	return CpuField({"vendor_id", "CPU implementer"});
#endif
}

unsigned Host::CPU::PhysicalCores() const {
#ifdef WINDOWS
	DWORD size = 0;
	if (GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &size) || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		Store(Host::Error::Failed);
		return 0;
	}
	std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX> buffer(
		(size + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) - 1) / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX));
	if (!GetLogicalProcessorInformationEx(RelationProcessorCore, buffer.data(), &size)) {
		Store(Host::Error::Failed);
		return 0;
	}
	unsigned count = 0;
	std::size_t offset = 0;
	while (offset < size) {
		const auto remaining = size - offset;
		constexpr auto header_size = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor);
		if (remaining < header_size) {
			Store(Host::Error::Failed);
			return 0;
		}
		const auto entry = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
			reinterpret_cast<const unsigned char*>(buffer.data()) + offset);
		if (entry->Size < header_size || entry->Size > remaining) {
			Store(Host::Error::Failed);
			return 0;
		}
		if (entry->Relationship == RelationProcessorCore)
			++count;
		offset += entry->Size;
	}
	return StoreCount(count);
#elifdef MACOS
	unsigned count = 0;
	std::size_t size = sizeof(count);
	if (sysctlbyname("hw.physicalcpu", &count, &size, nullptr, 0) != 0) {
		Store(NativeError(errno));
		return 0;
	}
	return StoreCount(count);
#else
	std::error_code error;
	std::filesystem::directory_iterator entries("/sys/devices/system/cpu", error);
	if (error) {
		Store(NativeError(error.value()));
		return 0;
	}
	std::set<std::pair<int, int>> cores;
	const std::filesystem::directory_iterator end;
	for (; entries != end; entries.increment(error)) {
		if (error)
			break;
		const auto name = entries->path().filename().string();
		if (!name.starts_with("cpu") || name.size() == 3 || name.find_first_not_of("0123456789", 3) != std::string::npos)
			continue;
		const auto topology = entries->path() / "topology";
		errno = 0;
		std::ifstream package_file(topology / "physical_package_id");
		if (!package_file) {
			Store(NativeError(errno));
			return 0;
		}
		int package = -1;
		int core = -1;
		if (!(package_file >> package)) {
			Store(Host::Error::Failed);
			return 0;
		}
		errno = 0;
		std::ifstream core_file(topology / "core_id");
		if (!core_file) {
			Store(NativeError(errno));
			return 0;
		}
		if (!(core_file >> core)) {
			Store(Host::Error::Failed);
			return 0;
		}
		if (package < 0 || core < 0) {
			Store(Host::Error::Unavailable);
			return 0;
		}
		cores.emplace(package, core);
	}
	if (error || cores.size() > std::numeric_limits<unsigned>::max()) {
		Store(Host::Error::Failed);
		return 0;
	}
	return StoreCount(static_cast<unsigned>(cores.size()));
#endif
}

StormByte::Safe::String Host::Motherboard::Manufacturer() const {
#ifdef WINDOWS
	return RegistryText(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardManufacturer");
#elifdef MACOS
	return PlatformText(CFSTR("manufacturer"));
#else
	return FileText("/sys/class/dmi/id/board_vendor");
#endif
}

StormByte::Safe::String Host::Motherboard::Model() const {
#ifdef WINDOWS
	return RegistryText(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardProduct");
#elifdef MACOS
	return PlatformText(CFSTR("board-id"));
#else
	return FileText("/sys/class/dmi/id/board_name");
#endif
}

StormByte::Safe::String Host::Motherboard::Revision() const {
#ifdef WINDOWS
	return RegistryText(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardVersion");
#elifdef MACOS
	return TextError(Host::Error::NotSupported);
#else
	return FileText("/sys/class/dmi/id/board_version");
#endif
}

StormByte::Safe::String Host::OS() {
#ifdef WINDOWS
	using RtlGetVersionFn = LONG (WINAPI*)(OSVERSIONINFOW*);
	const auto rtl = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
	OSVERSIONINFOW info {};
	info.dwOSVersionInfoSize = sizeof(info);
	if (rtl == nullptr || rtl(&info) != 0)
		return StoreText({}, false);
	std::string text = "Windows ";
	if (info.dwMajorVersion >= 10 && info.dwBuildNumber >= 22000)
		text += "11";
	else if (info.dwMajorVersion >= 10)
		text += "10";
	else
		text += std::to_string(info.dwMajorVersion);
	return StoreText(text, true);
#elifdef MACOS
	char version[64] = {};
	std::size_t len = sizeof(version);
	if (sysctlbyname("kern.osproductversion", version, &len, nullptr, 0) != 0)
		return StoreText({}, false);
	return StoreText(std::string("macOS ") + version, true);
#else
	std::string name = OsReleaseField("NAME");
	const std::string version = OsReleaseField("VERSION_ID");
	if (name.empty())
		name = OsReleaseField("PRETTY_NAME");
	if (name.empty())
		return StoreText({}, false);
	if (!version.empty())
		name += " " + version;
	return StoreText(name, true);
#endif
}

StormByte::Safe::String Host::Kernel() {
#ifdef WINDOWS
	using RtlGetVersionFn = LONG (WINAPI*)(OSVERSIONINFOW*);
	const auto rtl = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
	OSVERSIONINFOW info {};
	info.dwOSVersionInfoSize = sizeof(info);
	if (rtl == nullptr || rtl(&info) != 0)
		return StoreText({}, false);
	std::ostringstream out;
	out << "NT " << info.dwMajorVersion << '.' << info.dwMinorVersion << '.' << info.dwBuildNumber;
	return StoreText(out.str(), true);
#else
	struct utsname info {};
	if (uname(&info) != 0)
		return StoreText({}, false);
	std::string flags;
	auto append = [&](std::string_view token) {
		if (std::string_view(info.version).find(token) == std::string_view::npos)
			return;
		if (!flags.empty())
			flags.push_back(' ');
		flags.append(token);
	};
	append("SMP");
	append("PREEMPT_DYNAMIC");
	if (flags.find("PREEMPT") == std::string::npos)
		append("PREEMPT");
	std::string text = std::string(info.sysname) + " " + info.release;
	if (!flags.empty())
		text += " " + flags;
	return StoreText(text, true);
#endif
}

StormByte::ByteSize Host::PageSize() {
#ifdef WINDOWS
	SYSTEM_INFO info {};
	GetSystemInfo(&info);
	return StoreSize(info.dwPageSize, info.dwPageSize != 0);
#else
	const long page = sysconf(_SC_PAGESIZE);
	return StoreSize(page > 0 ? static_cast<std::uint64_t>(page) : 0, page > 0);
#endif
}

StormByte::ByteSize Host::PhysicalMemory() {
#ifdef WINDOWS
	MEMORYSTATUSEX status {};
	status.dwLength = sizeof(status);
	if (!GlobalMemoryStatusEx(&status))
		return StoreSize(0, false);
	return StoreSize(status.ullTotalPhys, true);
#elifdef MACOS
	std::uint64_t bytes = 0;
	std::size_t len = sizeof(bytes);
	int mib[] = { CTL_HW, HW_MEMSIZE };
	if (sysctl(mib, 2, &bytes, &len, nullptr, 0) != 0)
		return StoreSize(0, false);
	return StoreSize(bytes, true);
#else
	const long pages = sysconf(_SC_PHYS_PAGES);
	const long page = sysconf(_SC_PAGESIZE);
	if (pages <= 0 || page <= 0)
		return StoreSize(0, false);
	return StoreSize(static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(page), true);
#endif
}

StormByte::ByteSize Host::AvailableMemory() {
#ifdef WINDOWS
	MEMORYSTATUSEX status {};
	status.dwLength = sizeof(status);
	if (!GlobalMemoryStatusEx(&status))
		return StoreSize(0, false);
	return StoreSize(status.ullAvailPhys, true);
#elifdef MACOS
	vm_statistics64_data_t stats {};
	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
	if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&stats), &count) != KERN_SUCCESS)
		return StoreSize(0, false);
	const long page = sysconf(_SC_PAGESIZE);
	if (page <= 0)
		return StoreSize(0, false);
	const std::uint64_t pages = static_cast<std::uint64_t>(stats.free_count) + static_cast<std::uint64_t>(stats.inactive_count);
	return StoreSize(pages * static_cast<std::uint64_t>(page), true);
#else
	const long pages = sysconf(_SC_AVPHYS_PAGES);
	const long page = sysconf(_SC_PAGESIZE);
	if (pages <= 0 || page <= 0)
		return StoreSize(0, false);
	return StoreSize(static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(page), true);
#endif
}

unsigned Host::CPU::LogicalProcessors() const noexcept {
#ifdef WINDOWS
	const auto count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
	if (count == 0) {
		Store(Host::Error::Failed);
		return 0;
	}
	return StoreCount(count);
#elifdef MACOS
	unsigned count = 0;
	std::size_t size = sizeof(count);
	if (sysctlbyname("hw.logicalcpu", &count, &size, nullptr, 0) != 0) {
		Store(NativeError(errno));
		return 0;
	}
	return StoreCount(count);
#else
	const long count = sysconf(_SC_NPROCESSORS_ONLN);
	if (count <= 0 || static_cast<unsigned long>(count) > std::numeric_limits<unsigned>::max()) {
		Store(Host::Error::Failed);
		return 0;
	}
	return StoreCount(static_cast<unsigned>(count));
#endif
}

unsigned Host::CPU::Bitness() const noexcept {
	return static_cast<unsigned>(sizeof(void*) * 8u);
}

namespace StormByte::System {
	const StormByte::Error::Category<enum Host::Error>& host_category() noexcept {
		static StormByte::Error::Category<enum Host::Error> instance;
		return instance;
	}
}
