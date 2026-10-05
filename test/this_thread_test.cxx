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
#include <StormByte/test_handlers.h>

#include <chrono>
#include <iostream>
#include <string>

using StormByte::Safe::String;

// -------------------
// Name
// -------------------
int test_this_thread_name() {
	const std::string fn = "test_this_thread_name";
	const bool set_ok = StormByte::System::ThisThread::Name("sb-thread");
	std::cout << "set ok=" << (set_ok ? "true" : "false")
		<< " fault=" << StormByte::System::ThisThread::LastError().what() << '\n';
	String name;
	const bool get_ok = StormByte::System::ThisThread::Name(name);
	std::cout << "get ok=" << (get_ok ? "true" : "false")
		<< " value=" << std::string(name)
		<< " fault=" << StormByte::System::ThisThread::LastError().what() << '\n';
	RETURN_TEST(fn, 0);
}

int test_this_thread_bounded_unicode_name() {
	const std::string fn = "test_this_thread_bounded_unicode_name";
	const std::string backing = "sb-\xC3\xA9-ignored";
	const String owned(std::string_view(backing.data(), 5));
	ASSERT_TRUE(fn, StormByte::System::ThisThread::Name(static_cast<std::string_view>(owned)));
	String name;
	ASSERT_TRUE(fn, StormByte::System::ThisThread::Name(name));
	ASSERT_EQUAL(fn, std::string(owned), std::string(name));
	RETURN_TEST(fn, 0);
}

// -------------------
// Sleep
// -------------------
int test_this_thread_sleep() {
	const std::string fn = "test_this_thread_sleep";
	StormByte::System::ThisThread::Sleep(std::chrono::milliseconds(1));
	std::cout << "sleep done\n";
	RETURN_TEST(fn, 0);
}

// -------------------
// TooLong
// -------------------
int test_this_thread_too_long() {
	const std::string fn = "test_this_thread_too_long";
	const bool ok = StormByte::System::ThisThread::Name("this-name-is-way-too-long-for-pthread");
	std::cout << "too_long ok=" << (ok ? "true" : "false")
		<< " fault=" << StormByte::System::ThisThread::LastError().what() << '\n';
	RETURN_TEST(fn, 0);
}

int main() {
	int result = 0;

	// -------------------
	// Name
	// -------------------
	result += test_this_thread_name();
	result += test_this_thread_bounded_unicode_name();

	// -------------------
	// Sleep
	// -------------------
	result += test_this_thread_sleep();

	// -------------------
	// TooLong
	// -------------------
	result += test_this_thread_too_long();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
