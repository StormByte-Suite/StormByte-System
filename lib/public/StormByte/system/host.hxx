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

#include <StormByte/error.hxx>
#include <StormByte/byte_size.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/system/visibility.h>

#include <string>
#include <system_error>

/**
 * @brief System module of the StormByte suite.
 */
namespace StormByte::System {
	/**
	 * @brief Identity and capacity of this machine.
	 *
	 * @c Name returns @c true on success. @c Architecture, @c CPU, @c OS,
	 * @c Kernel, @c PageSize, @c PhysicalMemory and @c AvailableMemory
	 * return a value and update @ref LastError. On failure the string is
	 * empty or the @c ByteSize is zero. @c LogicalProcessors and @c Bitness
	 * do not touch @ref LastError.
	 */
	namespace Host {
		/**
		 * @enum Error
		 * @brief Host enumerators.
		 *
		 * Domain tag `StormByte.System.Host`. Zero is success.
		 */
		enum class Error {
			Success = 0,	///< No error
			Failed			///< The platform call failed
		};

		/**
		 * @brief Last Host error on this thread.
		 * @return Success or an Error code. Stored in the System module TLS.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::Error::Fault LastError() noexcept;

		/**
		 * @brief Host name of this machine.
		 * @param[out] name Owned UTF-8 text. Written only on success.
		 * @return true if the name was read.
		 */
		STORMBYTE_SYSTEM_PUBLIC bool Name(StormByte::Safe::String& name);

		/**
		 * @brief Instruction-set name of this machine.
		 * @return `x86_64`, `arm64`, `x86` or another platform token. Empty on failure.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::Safe::String Architecture();

		/**
		 * @brief Processor brand string.
		 * @return Brand text. Empty on failure.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::Safe::String CPU();

		/**
		 * @brief Operating system brand and version.
		 * @return Text such as `Gentoo 2.18`, `Windows 11` or `macOS 15.1`. Empty on failure.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::Safe::String OS();

		/**
		 * @brief Kernel identification.
		 * @return Text such as `Linux 7.2.7 SMP PREEMPT_DYNAMIC`, `NT 10.0.26100` or `Darwin 24.1.0`. Empty on failure.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::Safe::String Kernel();

		/**
		 * @brief System page size.
		 * @return Page size in bytes, or zero on failure.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::ByteSize PageSize();

		/**
		 * @brief Installed physical memory.
		 * @return Size in bytes, or zero on failure.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::ByteSize PhysicalMemory();

		/**
		 * @brief Currently available physical memory.
		 * @return Size in bytes, or zero on failure.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::ByteSize AvailableMemory();

		/**
		 * @brief Logical processor count.
		 * @return Hardware concurrency, or @c 0 if unknown.
		 */
		STORMBYTE_SYSTEM_PUBLIC unsigned LogicalProcessors() noexcept;

		/**
		 * @brief Pointer width of this process.
		 * @return @c 32 or @c 64.
		 */
		STORMBYTE_SYSTEM_PUBLIC unsigned Bitness() noexcept;
	}
}

/**
 * @brief Domain for @ref StormByte::System::Host::Error.
 */
template<>
struct StormByte::Error::Domain<StormByte::System::Host::Error> {
	static constexpr const char* Name = "StormByte.System.Host";	///< Stable category tag

	/**
	 * @brief Text for one Host enumerator.
	 * @param e Enumerator.
	 * @return Human-readable message.
	 */
	static STORMBYTE_FORCE_INLINE std::string Message(StormByte::System::Host::Error e) {
		switch (e) {
			case StormByte::System::Host::Error::Success:
				return "Success";
			case StormByte::System::Host::Error::Failed:
				return "Host operation failed";
		}
		return "Unknown Host error";
	}
};

namespace StormByte::System {
	/**
	 * @brief Category singleton for @ref Host::Error.
	 * @return Process-wide Host category.
	 */
	STORMBYTE_SYSTEM_PUBLIC const StormByte::Error::Category<Host::Error>& host_category() noexcept;

	/**
	 * @brief Builds an `std::error_code` from @ref Host::Error.
	 * Header-only: a `std::` object is not a DLL return.
	 * @param e Enumerator.
	 * @return Code in @ref host_category().
	 */
	STORMBYTE_FORCE_INLINE std::error_code make_error_code(Host::Error e) noexcept {
		return std::error_code(static_cast<int>(e), host_category());
	}
}

namespace std {
	/**
	 * @brief Marks @ref StormByte::System::Host::Error as an `std::error_code` enum.
	 */
	template<>
	struct is_error_code_enum<StormByte::System::Host::Error>: true_type {};
}
