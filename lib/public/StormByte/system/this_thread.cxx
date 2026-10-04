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

#include <StormByte/system/this_thread.hxx>

#include <StormByte/error.txx>
#include <StormByte/safe/wstring.hxx>

#ifdef WINDOWS
#include <windows.h>
#include <memory>
#else
#include <pthread.h>
#endif

using namespace StormByte::System;

namespace {
	thread_local StormByte::Error::Fault g_last{make_error_code(ThisThread::Error::Success)};

	constexpr std::size_t UnixNameLimit = 15;

	bool Store(const enum ThisThread::Error code) {
		g_last = StormByte::Error::Fault{make_error_code(code)};
		return code == ThisThread::Error::Success;
	}
}

StormByte::Error::Fault ThisThread::LastError() noexcept {
	return g_last;
}

bool ThisThread::Name(std::string_view name) {
	try {
	if (name.empty() || name.find('\0') != std::string_view::npos)
		return Store(ThisThread::Error::Failed);
#ifdef WINDOWS
	const StormByte::Safe::String utf8(name);
	const StormByte::Safe::WString wide(utf8);
	if (FAILED(SetThreadDescription(GetCurrentThread(), static_cast<const wchar_t*>(wide))))
		return Store(ThisThread::Error::Failed);
	return Store(ThisThread::Error::Success);
#else
	if (name.size() > UnixNameLimit)
		return Store(ThisThread::Error::TooLong);
	std::string buffer(name);
#ifdef MACOS
	if (pthread_setname_np(buffer.c_str()) != 0)
		return Store(ThisThread::Error::Failed);
#else
	if (pthread_setname_np(pthread_self(), buffer.c_str()) != 0)
		return Store(ThisThread::Error::Failed);
#endif
	return Store(ThisThread::Error::Success);
#endif
	} catch (...) {
		return Store(ThisThread::Error::Failed);
	}
}

bool ThisThread::Name(StormByte::Safe::String& name) {
	try {
#ifdef WINDOWS
	PWSTR wide = nullptr;
	const HRESULT result = GetThreadDescription(GetCurrentThread(), &wide);
	auto free_local = [](wchar_t* value) noexcept {
		if (value != nullptr)
			LocalFree(value);
	};
	std::unique_ptr<wchar_t, decltype(free_local)> owner(wide, free_local);
	if (FAILED(result) || wide == nullptr)
		return Store(ThisThread::Error::Failed);
	name = StormByte::Safe::String(StormByte::Safe::WString(wide));
	return Store(ThisThread::Error::Success);
#else
	char buffer[UnixNameLimit + 1] = {};
	if (pthread_getname_np(pthread_self(), buffer, sizeof(buffer)) != 0)
		return Store(ThisThread::Error::Failed);
	name = StormByte::Safe::String(std::string_view(buffer));
	return Store(ThisThread::Error::Success);
#endif
	} catch (...) {
		return Store(ThisThread::Error::Failed);
	}
}

namespace StormByte::System {
	const StormByte::Error::Category<enum ThisThread::Error>& this_thread_category() noexcept {
		static StormByte::Error::Category<enum ThisThread::Error> instance;
		return instance;
	}

	std::error_code make_error_code(const enum ThisThread::Error e) noexcept {
		return std::error_code(static_cast<int>(e), this_thread_category());
	}
}
