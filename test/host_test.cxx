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
#include <StormByte/test_handlers.h>

#include <cstdint>
#include <iostream>
#include <string>

using StormByte::Safe::String;
using StormByte::System::Host::Architecture;
using StormByte::System::Host::AvailableMemory;
using StormByte::System::Host::Bitness;
using StormByte::System::Host::CPU;
using StormByte::System::Host::Kernel;
using StormByte::System::Host::LastError;
using StormByte::System::Host::LogicalProcessors;
using StormByte::System::Host::Name;
using StormByte::System::Host::OS;
using StormByte::System::Host::PageSize;
using StormByte::System::Host::PhysicalMemory;

namespace {
	void DumpText(const char* label, const String& value) {
		std::cout << label
			<< " value=" << std::string(value)
			<< " fault=" << LastError().what()
			<< '\n';
	}
}

// -------------------
// Architecture
// -------------------
int test_architecture() {
	const String value = Architecture();
	DumpText("architecture", value);
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_EQUAL(StormByte::System::host_category(), LastError().code().category());
	ASSERT_NOT_EMPTY(value);
	RETURN_TEST(0);
}

// -------------------
// Bitness
// -------------------
int test_bitness() {
	const unsigned bits = Bitness();
	std::cout << "bitness=" << bits << '\n';
	ASSERT_TRUE(bits == 32 || bits == 64);
	RETURN_TEST(0);
}

// -------------------
// CPU
// -------------------
int test_cpu() {
	const String value = CPU();
	DumpText("cpu", value);
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_NOT_EMPTY(value);
	RETURN_TEST(0);
}

// -------------------
// Kernel
// -------------------
int test_kernel() {
	const String value = Kernel();
	DumpText("kernel", value);
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_NOT_EMPTY(value);
	RETURN_TEST(0);
}

// -------------------
// Memory
// -------------------
int test_memory() {
	const auto page = PageSize();
	std::cout << "page_size=" << static_cast<std::uint64_t>(page)
		<< " fault=" << LastError().what() << '\n';
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_TRUE(page > StormByte::ByteSize{0});
	const auto physical = PhysicalMemory();
	std::cout << "physical_memory=" << static_cast<std::uint64_t>(physical)
		<< " fault=" << LastError().what() << '\n';
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_TRUE(physical > StormByte::ByteSize{0});
	const auto available = AvailableMemory();
	std::cout << "available_memory=" << static_cast<std::uint64_t>(available)
		<< " fault=" << LastError().what() << '\n';
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_TRUE(available > StormByte::ByteSize{0});
	ASSERT_TRUE(available <= physical);
	RETURN_TEST(0);
}

// -------------------
// Name
// -------------------
int test_name() {
	String name;
	const bool ok = Name(name);
	std::cout << "name ok=" << (ok ? "true" : "false")
		<< " value=" << std::string(name)
		<< " fault=" << LastError().what() << '\n';
	ASSERT_TRUE(ok);
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_EQUAL(StormByte::System::make_error_code(StormByte::System::Host::Error::Success), LastError().code());
	ASSERT_NOT_EMPTY(name);
	RETURN_TEST(0);
}

// -------------------
// OS
// -------------------
int test_os() {
	const String value = OS();
	DumpText("os", value);
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_NOT_EMPTY(value);
	RETURN_TEST(0);
}

// -------------------
// Processors
// -------------------
int test_processors() {
	const unsigned count = LogicalProcessors();
	std::cout << "logical_processors=" << count << '\n';
	ASSERT_TRUE(count > 0);
	RETURN_TEST(0);
}

int main() {
	int result = 0;

	// -------------------
	// Architecture
	// -------------------
	result += test_architecture();

	// -------------------
	// Bitness
	// -------------------
	result += test_bitness();

	// -------------------
	// CPU
	// -------------------
	result += test_cpu();

	// -------------------
	// Kernel
	// -------------------
	result += test_kernel();

	// -------------------
	// Memory
	// -------------------
	result += test_memory();

	// -------------------
	// Name
	// -------------------
	result += test_name();

	// -------------------
	// OS
	// -------------------
	result += test_os();

	// -------------------
	// Processors
	// -------------------
	result += test_processors();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
