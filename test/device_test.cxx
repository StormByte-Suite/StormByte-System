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

#include <StormByte/safe/string.hxx>
#include <StormByte/system/device.hxx>
#include <StormByte/test_handlers.h>
#include <StormByte/type_traits/safe.hxx>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#ifdef UNIX
#include <chrono>
#endif

using StormByte::System::Device;

static_assert(StormByte::Type::MaybeSafe<class Device::Access>);
static_assert(StormByte::Type::MaybeSafe<struct Device::Throughput>);
static_assert(StormByte::Type::MaybeSafe<struct Device::Window>);
static_assert(StormByte::Type::SafeValue<class Device::Access>);
static_assert(StormByte::Type::SafeValue<struct Device::Throughput>);
static_assert(StormByte::Type::SafeValue<struct Device::Window>);

namespace {
	const char* KindName(const enum Device::Kind kind) {
		switch (kind) {
			case Device::Kind::HDD:			return "HDD";
			case Device::Kind::SSD:			return "SSD";
			case Device::Kind::NVMeGen3:	return "NVMeGen3";
			case Device::Kind::NVMeGen4:	return "NVMeGen4";
			case Device::Kind::NVMeGen5:	return "NVMeGen5";
			case Device::Kind::USBHDD:		return "USBHDD";
			case Device::Kind::USBStick:	return "USBStick";
			case Device::Kind::Network:		return "Network";
		}
		return "Unknown";
	}

	void Dump(const char* label, const Device& device) {
		std::cout << label << " path=" << std::string(device.Path())
			<< " ok=" << (static_cast<bool>(device) ? "true" : "false")
			<< " fault=" << device.Fault().what() << '\n';
		if (!device)
			return;
		const auto access = device.Access();
		const auto rate = device.Throughput();
		const auto window = device.Window();
		std::cout << label << " kind=" << KindName(device.Kind())
			<< " readable=" << (access.Has(Device::AccessFlag::Readable) ? "true" : "false")
			<< " writable=" << (access.Has(Device::AccessFlag::Writable) ? "true" : "false")
			<< " read_bps=" << static_cast<std::uint64_t>(rate.read_bps)
			<< " write_bps=" << static_cast<std::uint64_t>(rate.write_bps)
			<< " window_read=" << static_cast<std::uint64_t>(window.read)
			<< " window_write=" << static_cast<std::uint64_t>(window.write)
			<< '\n';
	}

	std::filesystem::path RootPath() {
#ifdef WINDOWS
		return std::filesystem::path("C:\\");
#else
		return std::filesystem::path("/");
#endif
	}
}

// -------------------
// Errors
// -------------------
int test_broken_symlink() {
#ifndef UNIX
	RETURN_TEST(0);
#else
	const auto name = "StormByte-System-broken-link-" + std::to_string(
		std::chrono::steady_clock::now().time_since_epoch().count());
	const auto link = std::filesystem::temp_directory_path() / name;
	const auto target = link.string() + "-missing-target";
	std::error_code error;
	std::filesystem::create_symlink(target, link, error);
	ASSERT_FALSE(static_cast<bool>(error));
	Device device(link);
	const bool correctly_classified = device.Fault().code() ==
		StormByte::System::make_error_code(Device::Error::BrokenSymlink);
	ASSERT_FALSE(static_cast<bool>(device));
	std::filesystem::remove(link, error);
	ASSERT_FALSE(static_cast<bool>(error));
	ASSERT_TRUE(correctly_classified);
	RETURN_TEST(0);
#endif
}

int test_not_found() {
	const auto path = std::filesystem::current_path().root_path() / "StormByte-System-no-such-device-2.0.0";
	Device device(path);
	ASSERT_FALSE(static_cast<bool>(device));
	ASSERT_EQUAL(StormByte::System::make_error_code(Device::Error::DeviceNotFound), device.Fault().code());
	ASSERT_EQUAL(StormByte::System::device_category(), device.Fault().code().category());
	RETURN_TEST(0);
}

// -------------------
// Lifecycle
// -------------------
int test_copy_move_and_path() {
	const StormByte::Safe::String owned{std::string_view{RootPath().string()}};
	Device from_owned(owned);
	Device from_view(std::string_view{RootPath().string()});
	Device from_path(RootPath());
	ASSERT_EQUAL(std::string(from_owned.Path()), std::string(from_view.Path()));
	ASSERT_EQUAL(std::string(from_owned.Path()), std::string(from_path.Path()));
	Device copied(from_path);
	ASSERT_EQUAL(std::string(from_path.Path()), std::string(copied.Path()));
	Device moved(std::move(copied));
	ASSERT_EQUAL(std::string(from_path.Path()), std::string(moved.Path()));
	Device assigned(std::string_view{"unused"});
	assigned = from_path;
	ASSERT_EQUAL(std::string(from_path.Path()), std::string(assigned.Path()));
	Device move_assigned(std::string_view{"unused"});
	move_assigned = std::move(moved);
	ASSERT_EQUAL(std::string(from_path.Path()), std::string(move_assigned.Path()));
	RETURN_TEST(0);
}

// -------------------
// Probe
// -------------------
int test_root() {
	Device device(RootPath());
	Dump("root", device);
	ASSERT_TRUE(static_cast<bool>(device));
	ASSERT_FALSE(static_cast<bool>(device.Fault()));
	ASSERT_TRUE(device.Access().Has(Device::AccessFlag::Readable));
	const auto rate = device.Throughput();
	const auto window = device.Window();
	const StormByte::ByteSize floor{16ull * 1024ull};
	const StormByte::ByteSize ceiling{1024ull * 1024ull};
	ASSERT_TRUE(window.read >= floor);
	ASSERT_TRUE(window.read <= ceiling);
	ASSERT_TRUE(window.write >= floor);
	ASSERT_TRUE(window.write <= ceiling);
	ASSERT_TRUE(rate.read_bps > StormByte::ByteSize{0});
	ASSERT_TRUE(rate.write_bps > StormByte::ByteSize{0});
	RETURN_TEST(0);
}

int test_temp() {
	Device device(std::filesystem::temp_directory_path());
	Dump("temp", device);
	ASSERT_TRUE(static_cast<bool>(device));
	ASSERT_TRUE(device.Access().Has(Device::AccessFlag::Readable));
	const auto rate = device.Throughput();
	const auto window = device.Window();
	const StormByte::ByteSize floor{16ull * 1024ull};
	const StormByte::ByteSize ceiling{1024ull * 1024ull};
	ASSERT_TRUE(window.read >= floor);
	ASSERT_TRUE(window.read <= ceiling);
	ASSERT_TRUE(window.write >= floor);
	ASSERT_TRUE(window.write <= ceiling);
	ASSERT_TRUE(rate.read_bps > StormByte::ByteSize{0});
	ASSERT_TRUE(rate.write_bps > StormByte::ByteSize{0});
	RETURN_TEST(0);
}

int main() {
	int result = 0;

	// -------------------
	// Errors
	// -------------------
	result += test_broken_symlink();
	result += test_not_found();

	// -------------------
	// Lifecycle
	// -------------------
	result += test_copy_move_and_path();

	// -------------------
	// Probe
	// -------------------
	result += test_root();
	result += test_temp();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
