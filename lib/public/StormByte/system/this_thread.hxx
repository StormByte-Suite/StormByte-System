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
#include <StormByte/safe/string.hxx>
#include <StormByte/system/visibility.h>

#include <chrono>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

/**
 * @brief System module of the StormByte suite.
 */
namespace StormByte::System {
	/**
	 * @brief Calling-thread helpers (the current thread, not a child process).
	 *
	 * @c Sleep does not touch @ref LastError. Every other call returns
	 * @c true on success and updates @ref LastError for this thread.
	 * An output @c String is valid only when the call returned @c true.
	 */
	namespace ThisThread {
		/**
		 * @enum Error
		 * @brief ThisThread enumerators.
		 *
		 * Domain tag `StormByte.System.ThisThread`. Zero is success.
		 */
		enum class Error {
			Success = 0,	///< No error
			TooLong,		///< The name exceeds the platform limit
			Failed			///< The platform call failed
		};

		/**
		 * @brief Last ThisThread error on this thread.
		 * @return Success or an Error code. Stored in the System module TLS.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::Error::Fault LastError() noexcept;

		/**
		 * @brief Suspend the calling thread.
		 * @tparam Rep Duration representation.
		 * @tparam Period Duration period.
		 * @param duration Sleep interval.
		 */
		template<typename Rep, typename Period>
		inline void Sleep(const std::chrono::duration<Rep, Period>& duration) {
			std::this_thread::sleep_for(duration);
		}

		/**
		 * @brief Set the calling thread name.
		 * @param name Proposed name. Not truncated.
		 * @return true if the name was set.
		 */
		STORMBYTE_SYSTEM_PUBLIC bool Name(std::string_view name);

		/**
		 * @brief Read the calling thread name.
		 * @param[out] name Owned UTF-8 text. Written only on success.
		 * @return true if a name was read.
		 */
		STORMBYTE_SYSTEM_PUBLIC bool Name(StormByte::Safe::String& name);
	}
}

/**
 * @brief Domain for @ref StormByte::System::ThisThread::Error.
 */
template<>
struct StormByte::Error::Domain<StormByte::System::ThisThread::Error> {
	static constexpr const char* Name = "StormByte.System.ThisThread";	///< Stable category tag

	/**
	 * @brief Text for one ThisThread enumerator.
	 * @param e Enumerator.
	 * @return Human-readable message.
	 */
	static STORMBYTE_FORCE_INLINE std::string Message(StormByte::System::ThisThread::Error e) {
		switch (e) {
			case StormByte::System::ThisThread::Error::Success:
				return "Success";
			case StormByte::System::ThisThread::Error::TooLong:
				return "Thread name is too long";
			case StormByte::System::ThisThread::Error::Failed:
				return "Thread name operation failed";
		}
		return "Unknown ThisThread error";
	}
};

namespace StormByte::System {
	/**
	 * @brief Category singleton for @ref ThisThread::Error.
	 * @return Process-wide ThisThread category.
	 */
	STORMBYTE_SYSTEM_PUBLIC const StormByte::Error::Category<ThisThread::Error>& this_thread_category() noexcept;

	/**
	 * @brief Builds an `std::error_code` from @ref ThisThread::Error.
	 * Header-only: a `std::` object is not a DLL return.
	 * @param e Enumerator.
	 * @return Code in @ref this_thread_category().
	 */
	STORMBYTE_FORCE_INLINE std::error_code make_error_code(ThisThread::Error e) noexcept {
		return std::error_code(static_cast<int>(e), this_thread_category());
	}
}

namespace std {
	/**
	 * @brief Marks @ref StormByte::System::ThisThread::Error as an `std::error_code` enum.
	 */
	template<>
	struct is_error_code_enum<StormByte::System::ThisThread::Error>: true_type {};
}
