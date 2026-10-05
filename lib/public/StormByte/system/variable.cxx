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

#include <StormByte/system/variable.hxx>

#ifdef WINDOWS
#include <windows.h>
#include <vector>
#else
#include <cstdlib>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace StormByte::System;

StormByte::Safe::String Variable::Expand(std::string_view var) {
	return ExpandEnvironmentVariable(var);
}

StormByte::Safe::String Variable::Expand(const StormByte::Safe::String& var) {
	return ExpandEnvironmentVariable(std::string_view(var));
}

#ifdef WINDOWS
StormByte::Safe::String Variable::Expand(std::wstring_view var) {
	return ExpandEnvironmentVariable(var);
}

StormByte::Safe::String Variable::Expand(const StormByte::Safe::WString& var) {
	return ExpandEnvironmentVariable(std::wstring_view(var));
}

#endif

StormByte::Safe::String Variable::ExpandEnvironmentVariable(std::string_view var) {
	#ifdef WINDOWS
	return ExpandEnvironmentVariable(std::wstring_view(static_cast<StormByte::Safe::WString>(StormByte::Safe::String(var))));
	#else
	if (var != "~" && (var.size() < 2 || var[0] != '~' || var[1] != '/'))
		return StormByte::Safe::String(var);
	const std::filesystem::path home = HomePath();
	if (home.empty())
		return StormByte::Safe::String(var);
	if (var.size() == 1)
		return StormByte::Safe::String(home.string());
	return StormByte::Safe::String(home.string() + std::string(var.substr(1)));
	#endif
}

#ifdef WINDOWS
StormByte::Safe::String Variable::ExpandEnvironmentVariable(std::wstring_view var) {
	const std::wstring terminated(var);
	DWORD size = ::ExpandEnvironmentStringsW(terminated.c_str(), nullptr, 0);
	if (size == 0)
		return StormByte::Safe::String(StormByte::Safe::WString(var));
	std::vector<wchar_t> buffer(size);
	while (true) {
		const DWORD result = ::ExpandEnvironmentStringsW(terminated.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
		if (result == 0)
			return StormByte::Safe::String(StormByte::Safe::WString(var));
		if (result <= buffer.size())
			return StormByte::Safe::String(StormByte::Safe::WString(std::wstring_view(buffer.data(), result - 1)));
		buffer.resize(result);
	}
}
#else
std::filesystem::path Variable::HomePath() {
	if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
		return home;
	const struct passwd* pw = getpwuid(getuid());
	return pw == nullptr || pw->pw_dir == nullptr ? std::filesystem::path() : std::filesystem::path(pw->pw_dir);
}
#endif
