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

#include <StormByte/system/host.hxx>

#include <StormByte/error.txx>
#include <StormByte/safe/wstring.hxx>

#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef WINDOWS
#include <windows.h>
#elifdef MACOS
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#else
#include <fstream>
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

StormByte::Safe::String Host::Architecture() {
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

StormByte::Safe::String Host::CPU() {
#ifdef WINDOWS
	wchar_t value[512];
	DWORD size = sizeof(value);
	if (RegGetValueW(HKEY_LOCAL_MACHINE,
			L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
			L"ProcessorNameString", RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS)
		return StoreText({}, false);
	return StoreText(std::string(StormByte::Safe::String(StormByte::Safe::WString(value))), true);
#elifdef MACOS
	char brand[256] = {};
	std::size_t len = sizeof(brand);
	if (sysctlbyname("machdep.cpu.brand_string", brand, &len, nullptr, 0) != 0)
		return StoreText({}, false);
	return StoreText(brand, true);
#else
	std::ifstream in("/proc/cpuinfo");
	if (!in)
		return StoreText({}, false);
	std::string line;
	while (std::getline(in, line)) {
		if (line.rfind("model name", 0) != 0)
			continue;
		const auto pos = line.find(':');
		if (pos == std::string::npos)
			return StoreText({}, false);
		std::string model = line.substr(pos + 1);
		while (!model.empty() && model.front() == ' ')
			model.erase(model.begin());
		return StoreText(model, !model.empty());
	}
	return StoreText({}, false);
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

unsigned Host::LogicalProcessors() noexcept {
	return std::thread::hardware_concurrency();
}

unsigned Host::Bitness() noexcept {
	return static_cast<unsigned>(sizeof(void*) * 8u);
}

namespace StormByte::System {
	const StormByte::Error::Category<enum Host::Error>& host_category() noexcept {
		static StormByte::Error::Category<enum Host::Error> instance;
		return instance;
	}
}
