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

#pragma once

#include <StormByte/bitmask.hxx>
#include <StormByte/error.hxx>
#include <StormByte/byte_size.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/system/visibility.h>
#include <StormByte/type_traits/safe.hxx>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

/**
 * @brief System module of the StormByte suite.
 */
namespace StormByte::System {
	/**
	 * @class Device
	 * @brief Query object for the medium behind a filesystem accessor.
	 *
	 * Stores only the caller path as @ref StormByte::Safe::String.
	 * Fault, Kind, Access, Throughput and Window are computed on each call.
	 *
	 * Construction does not throw and does not open a handle.
	 * @c operator bool is true only when the current probe produced
	 * a success code. It does not mean readable or writable.
	 *
	 * @par Validity
	 * Kind, Access, Throughput and Window are defined only when the Device
	 * converts to @c true. Path is always the stored accessor.
	 *
	 * @par Path kinds
	 * The accessor may be a regular file, a directory, a missing leaf on
	 * an existing volume, or a special device node.
	 * Symlinks are followed (`stat`). A dangling symlink is
	 * @ref Error::BrokenSymlink.
	 * Special device nodes are never @ref AccessFlag::Writable.
	 *
	 * @par Copy
	 * Copyable, movable and assignable. Only @c m_path is owned.
	 *
	 * @par Derivation
	 * Throughput and Window are virtual so another module can report its
	 * own rates without System talking to sockets.
	 */
	class STORMBYTE_SYSTEM_PUBLIC Device {
		public:
			/**
			 * @enum Error
			 * @brief Device classification and probe enumerators.
			 *
			 * Domain tag `StormByte.System.Device`. Zero is success.
			 */
			enum class Error {
				Success = 0,		///< No error
				BrokenSymlink,		///< The path is a symlink whose target does not exist
				DeviceNotFound,		///< No volume or node could be resolved
				NotADevice,			///< The path exists but is not a classifiable file, directory or device node
				Permission,			///< The process may not stat or access this path
				ProbeFailed			///< Classification I/O failed
			};

			/**
			 * @enum Kind
			 * @brief Classified storage or network medium.
			 *
			 * Acronyms stay uppercase. Nominal throughput presets follow Kind.
			 */
			enum class Kind {
				HDD,		///< Rotational local disk
				SSD,		///< Non-rotational local disk that is not NVMe
				NVMeGen3,	///< NVMe PCIe 3.0
				NVMeGen4,	///< NVMe PCIe 4.0
				NVMeGen5,	///< NVMe PCIe 5.0
				USBHDD,		///< Removable rotational disk
				USBStick,	///< Removable solid-state key
				Network		///< Network filesystem or remote drive
			};

			/**
			 * @enum AccessFlag
			 * @brief Transfer permission of this path for this process.
			 *
			 * Not SMART and not medium feature flags.
			 */
			enum class AccessFlag: std::uint8_t {
				Readable = 0x01,	///< The process may read this path
				Writable = 0x02		///< The process may write this path
			};

			/**
			 * @class Access
			 * @brief Bitmask of @ref AccessFlag.
			 */
			class STORMBYTE_SYSTEM_PUBLIC Access: public StormByte::Bitmask<Access, AccessFlag> {
				public:
					using Bitmask::Bitmask;
			};

			/**
			 * @struct Throughput
			 * @brief Nominal sequential rates of the resolved medium.
			 *
			 * Octets per second as @ref StormByte::ByteSize. Not a buffer length
			 * and not a benchmark.
			 */
			struct Throughput {
				StormByte::ByteSize read_bps;	///< Nominal sequential read
				StormByte::ByteSize write_bps;	///< Nominal sequential write
			};

			/**
			 * @struct Window
			 * @brief Suggested transfer windows derived from Throughput.
			 *
			 * Each side is `bps / 500` clamped to 16 KiB–1 MiB.
			 */
			struct Window {
				StormByte::ByteSize read;	///< Suggested read window
				StormByte::ByteSize write;	///< Suggested write window
			};

			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Store an owned UTF-8 accessor.
			 * @param path Caller path. Need not exist.
			 */
			explicit Device(const StormByte::Safe::String& path) noexcept;

			/**
			 * @brief Store a UTF-8 accessor view.
			 * @param path Caller path. Need not exist.
			 */
			explicit Device(std::string_view path) noexcept;

			/**
			 * @brief Store a wide accessor as UTF-8 text.
			 * @param path Caller path. Need not exist.
			 */
			explicit Device(std::wstring_view path) noexcept;

			/**
			 * @brief Store a filesystem path using its native path view.
			 * @param path Caller path. Need not exist; its text is copied before return.
			 */
			explicit Device(const std::filesystem::path& path) noexcept:
#ifdef WINDOWS
				Device(std::wstring_view(path.native())) {}
#else
				Device(std::string_view(path.native())) {}
#endif

			/**
			 * @brief Copy constructor.
			 */
			Device(const Device&) = default;

			/**
			 * @brief Move constructor.
			 */
			Device(Device&&) noexcept = default;

			/**
			 * @brief Copy assignment.
			 */
			Device& operator=(const Device&) = default;

			/**
			 * @brief Move assignment.
			 */
			Device& operator=(Device&&) noexcept = default;

			/**
			 * @brief Destructor.
			 */
			virtual ~Device() = default;

			/** @} */

			/**
			 * @name Probe
			 * @{
			 */

			/**
			 * @brief Whether the current probe succeeded.
			 * @return true if Fault is a success code.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Result of the current probe.
			 * @return Success or an Error code.
			 */
			StormByte::Error::Fault Fault() const noexcept;

			/**
			 * @brief Accessor stored at construction.
			 * @return Owned UTF-8 text. Not a canonical target path.
			 */
			const StormByte::Safe::String& Path() const noexcept;

			/** @} */

			/**
			 * @name Classification
			 * @{
			 */

			/**
			 * @brief Classified medium.
			 * @return Kind. Valid only when @c *this is true.
			 */
			enum Kind Kind() const noexcept;

			/**
			 * @brief Transfer permissions for this process on this path.
			 * @return Access bitmask. Valid only when @c *this is true.
			 */
			class Access Access() const noexcept;

			/**
			 * @brief Nominal sequential rates of the medium.
			 * @return Throughput. Valid only when @c *this is true.
			 *
			 * A derived Device may replace this with its own rates.
			 */
			virtual struct Throughput Throughput() const noexcept;

			/**
			 * @brief Suggested read and write windows.
			 * @return Window. Valid only when @c *this is true.
			 *
			 * A derived Device may replace this with its own windows.
			 */
			virtual struct Window Window() const noexcept;

			/** @} */

		private:
			StormByte::Safe::String m_path;	///< Accessor supplied by the caller
			bool m_initialization_failed{false};	///< Whether storing the accessor failed
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(class StormByte::System::Device::Access);
STORMBYTE_DECLARE_MAYBE_SAFE(struct StormByte::System::Device::Throughput);
STORMBYTE_DECLARE_MAYBE_SAFE(struct StormByte::System::Device::Window);

/**
 * @brief Domain for @ref StormByte::System::Device::Error.
 */
template<>
struct StormByte::Error::Domain<StormByte::System::Device::Error> {
	static constexpr const char* Name = "StormByte.System.Device";	///< Stable category tag

	/**
	 * @brief Text for one Device enumerator.
	 * @param e Enumerator.
	 * @return Human-readable message.
	 */
	static std::string Message(StormByte::System::Device::Error e) {
		switch (e) {
			case StormByte::System::Device::Error::Success:
				return "Success";
			case StormByte::System::Device::Error::BrokenSymlink:
				return "Broken symbolic link";
			case StormByte::System::Device::Error::DeviceNotFound:
				return "Device not found";
			case StormByte::System::Device::Error::NotADevice:
				return "Path is not a classifiable device";
			case StormByte::System::Device::Error::Permission:
				return "Device permission denied";
			case StormByte::System::Device::Error::ProbeFailed:
				return "Device probe failed";
		}
		return "Unknown Device error";
	}
};

namespace StormByte::System {
	/**
	 * @brief Category singleton for @ref Device::Error.
	 * @return Process-wide Device category.
	 */
	STORMBYTE_SYSTEM_PUBLIC const StormByte::Error::Category<Device::Error>& device_category() noexcept;

	/**
	 * @brief Builds an `std::error_code` from @ref Device::Error.
	 * @param e Enumerator.
	 * @return Code in @ref device_category().
	 */
	STORMBYTE_SYSTEM_PUBLIC std::error_code make_error_code(Device::Error e) noexcept;
}

namespace std {
	/**
	 * @brief Marks @ref StormByte::System::Device::Error as an `std::error_code` enum.
	 */
	template<>
	struct is_error_code_enum<StormByte::System::Device::Error>: true_type {};
}
