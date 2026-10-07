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

#include <StormByte/system/file.hxx>
#include <StormByte/test_handlers.h>

#include <filesystem>
#include <iostream>
#include <string>
#ifdef UNIX
#include <cstdlib>
#endif

using StormByte::Safe::String;
using StormByte::System::File::CurrentExecutable;
using StormByte::System::File::LastError;
using StormByte::System::File::Temporary;

namespace {
	void Dump(const char* label, const bool ok, const String& path) {
		std::cout << label
			<< " ok=" << (ok ? "true" : "false")
			<< " value=" << std::string(path)
			<< " fault=" << LastError().what()
			<< '\n';
	}
}

// -------------------
// CurrentExecutable
// -------------------
int test_current_executable() {
	String path;
	const bool ok = CurrentExecutable(path);
	Dump("current_executable", ok, path);
	ASSERT_TRUE(ok);
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_EQUAL(StormByte::System::make_error_code(StormByte::System::File::Error::Success), LastError().code());
	ASSERT_EQUAL(StormByte::System::file_category(), LastError().code().category());
	ASSERT_NOT_EMPTY(path);
	ASSERT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(std::string(path))));
	RETURN_TEST(0);
}

// -------------------
// Temporary
// -------------------
int test_temporary() {
	String path;
	const bool ok = Temporary(path, "SB", ".tmp");
	Dump("temporary", ok, path);
	ASSERT_TRUE(ok);
	ASSERT_FALSE(static_cast<bool>(LastError()));
	ASSERT_NOT_EMPTY(path);
	const std::string text(path);
	ASSERT_CONTAINS(text, "SB");
	ASSERT_CONTAINS(text, ".tmp");
	const auto native_path = std::filesystem::path(text);
	ASSERT_TRUE(std::filesystem::exists(native_path));
	ASSERT_TRUE(std::filesystem::is_regular_file(native_path));
	ASSERT_EQUAL(static_cast<std::uintmax_t>(0), std::filesystem::file_size(native_path));
	std::filesystem::remove(native_path);
	ASSERT_FALSE(std::filesystem::exists(native_path));
	RETURN_TEST(0);
}

int test_temporary_default_prefix() {
	String path;
	const bool ok = Temporary(path);
	Dump("temporary_default", ok, path);
	ASSERT_TRUE(ok);
	ASSERT_NOT_EMPTY(path);
	const auto native_path = std::filesystem::path(std::string(path));
	ASSERT_TRUE(std::filesystem::exists(native_path));
	std::filesystem::remove(native_path);
	ASSERT_FALSE(std::filesystem::exists(native_path));
	RETURN_TEST(0);
}

#ifdef UNIX
int test_temporary_missing_directory() {
	const char* previous = std::getenv("TMPDIR");
	const bool had_previous = previous != nullptr;
	const std::string previous_value = had_previous ? previous : "";
	setenv("TMPDIR", "/stormbyte-system-no-such-temp-directory-2.0.0", 1);
	String path;
	const bool created = Temporary(path);
	const auto error = LastError().code();
	if (had_previous)
		setenv("TMPDIR", previous_value.c_str(), 1);
	else
		unsetenv("TMPDIR");
	ASSERT_FALSE(created);
	ASSERT_EQUAL(StormByte::System::make_error_code(StormByte::System::File::Error::NotFound), error);
	ASSERT_EQUAL(StormByte::System::file_category(), error.category());
	RETURN_TEST(0);
}
#endif

int main() {
	int result = 0;

	// -------------------
	// CurrentExecutable
	// -------------------
	result += test_current_executable();

	// -------------------
	// Temporary
	// -------------------
	result += test_temporary();
	result += test_temporary_default_prefix();
#ifdef UNIX
	result += test_temporary_missing_directory();
#endif

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
