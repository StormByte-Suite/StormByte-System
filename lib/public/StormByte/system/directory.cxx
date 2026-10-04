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

#include <StormByte/system/directory.hxx>

#include <StormByte/error.txx>
#include <StormByte/safe/wstring.hxx>
#include <StormByte/system/variable.hxx>

#include <filesystem>
#include <vector>

#ifdef WINDOWS
#include <windows.h>
#elifdef MACOS
#include <mach-o/dyld.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif

using namespace StormByte::System;

namespace {
	thread_local StormByte::Error::Fault g_last{make_error_code(Directory::Error::Success)};

	Directory::Error FromFilesystemError(const std::error_code& error) noexcept {
		if (error == std::errc::permission_denied)
			return Directory::Error::Permission;
		if (error == std::errc::no_such_file_or_directory)
			return Directory::Error::NotFound;
		return Directory::Error::Failed;
	}

	bool Store(const enum Directory::Error code) {
		g_last = StormByte::Error::Fault{make_error_code(code)};
		return code == Directory::Error::Success;
	}

	StormByte::Safe::String FromNative(const std::filesystem::path& path) {
#ifdef WINDOWS
		return StormByte::Safe::String(StormByte::Safe::WString(path.wstring()));
#else
		return StormByte::Safe::String(path.string());
#endif
	}

	std::filesystem::path ExecutableFile() {
#ifdef WINDOWS
		std::vector<wchar_t> buf(MAX_PATH);
		for (;;) {
			const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
			if (n == 0)
				return {};
			if (n < buf.size())
				return std::filesystem::path(std::wstring(buf.data(), n));
			buf.resize(buf.size() * 2);
		}
#elifdef MACOS
		uint32_t size = 0;
		_NSGetExecutablePath(nullptr, &size);
		if (size == 0)
			return {};
		std::vector<char> buf(size);
		if (_NSGetExecutablePath(buf.data(), &size) != 0)
			return {};
		return std::filesystem::path(buf.data());
#else
		std::vector<char> buf(256);
		for (;;) {
			const ssize_t count = readlink("/proc/self/exe", buf.data(), buf.size());
			if (count < 0)
				return {};
			if (static_cast<size_t>(count) < buf.size())
				return std::filesystem::path(std::string(buf.data(), static_cast<size_t>(count)));
			buf.resize(buf.size() * 2);
		}
#endif
	}
}

StormByte::Error::Fault Directory::LastError() noexcept {
	return g_last;
}

bool Directory::Current(StormByte::Safe::String& path) {
	try {
		path = FromNative(std::filesystem::current_path());
		return Store(Directory::Error::Success);
	} catch (const std::filesystem::filesystem_error& error) {
		return Store(FromFilesystemError(error.code()));
	} catch (...) {
		return Store(Directory::Error::Failed);
	}
}

bool Directory::Home(StormByte::Safe::String& path) {
	try {
#ifdef WINDOWS
		const StormByte::Safe::String home = Variable::Expand("%USERPROFILE%");
#else
		const StormByte::Safe::String home = Variable::Expand("~");
#endif
		if (home.empty())
			return Store(Directory::Error::NotFound);
		path = home;
		return Store(Directory::Error::Success);
	} catch (...) {
		return Store(Directory::Error::Failed);
	}
}

bool Directory::Temporary(StormByte::Safe::String& path) {
	try {
#ifdef WINDOWS
		wchar_t tempPath[MAX_PATH];
		const DWORD n = GetTempPathW(MAX_PATH, tempPath);
		if (n == 0 || n >= MAX_PATH)
			return Store(Directory::Error::Failed);
		path = FromNative(std::filesystem::path(std::wstring(tempPath, n)));
		return Store(Directory::Error::Success);
#else
		const char* env = std::getenv("TMPDIR");
		if (env == nullptr || *env == '\0')
			env = std::getenv("TMP");
		if (env == nullptr || *env == '\0')
			env = std::getenv("TEMP");
		path = FromNative((env != nullptr && *env != '\0') ? std::filesystem::path(env) : std::filesystem::path("/tmp"));
		return Store(Directory::Error::Success);
#endif
	} catch (const std::filesystem::filesystem_error& error) {
		return Store(FromFilesystemError(error.code()));
	} catch (...) {
		return Store(Directory::Error::Failed);
	}
}

bool Directory::CurrentExecutable(StormByte::Safe::String& path) {
	try {
		const std::filesystem::path file = ExecutableFile();
		if (file.empty())
			return Store(Directory::Error::Failed);
		path = FromNative(file.parent_path());
		return Store(Directory::Error::Success);
	} catch (const std::filesystem::filesystem_error& error) {
		return Store(FromFilesystemError(error.code()));
	} catch (...) {
		return Store(Directory::Error::Failed);
	}
}

namespace StormByte::System {
	const StormByte::Error::Category<enum Directory::Error>& directory_category() noexcept {
		static StormByte::Error::Category<enum Directory::Error> instance;
		return instance;
	}

	std::error_code make_error_code(const enum Directory::Error e) noexcept {
		return std::error_code(static_cast<int>(e), directory_category());
	}
}
