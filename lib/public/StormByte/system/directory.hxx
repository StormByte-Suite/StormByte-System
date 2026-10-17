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

#include <string>
#include <system_error>

/**
 * @brief System module of the StormByte suite.
 */
namespace StormByte::System {
	/**
	 * @brief Directory locations of this process or user.
	 *
	 * Each call returns @c true on success and updates @ref LastError
	 * for this thread. An output @c String is valid only when the call
	 * returned @c true.
	 */
	namespace Directory {
		/**
		 * @enum Error
		 * @brief Directory enumerators.
		 *
		 * Domain tag `StormByte.System.Directory`. Zero is success.
		 */
		enum class Error {
			Success = 0,	///< No error
			Permission,		///< The process may not read this directory
			NotFound,		///< The directory or a required environment value is missing
			Failed			///< The platform call failed
		};

		/**
		 * @brief Last Directory error on this thread.
		 * @return Success or an Error code. Stored in the System module TLS.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::Error::Fault LastError() noexcept;

		/**
		 * @brief Current working directory.
		 * @param[out] path Absolute directory. Written only on success.
		 * @return true if the directory was resolved.
		 */
		STORMBYTE_SYSTEM_PUBLIC bool Current(StormByte::Safe::String& path);

		/**
		 * @brief Current user home directory.
		 * @param[out] path Absolute directory. Written only on success.
		 * @return true if the directory was resolved.
		 */
		STORMBYTE_SYSTEM_PUBLIC bool Home(StormByte::Safe::String& path);

		/**
		 * @brief Directory used for temporary files.
		 * @param[out] path Absolute directory. Written only on success.
		 * @return true if the directory was resolved.
		 */
		STORMBYTE_SYSTEM_PUBLIC bool Temporary(StormByte::Safe::String& path);

		/**
		 * @brief Directory that contains the running executable.
		 * @param[out] path Absolute directory. Written only on success.
		 * @return true if the directory was resolved.
		 */
		STORMBYTE_SYSTEM_PUBLIC bool CurrentExecutable(StormByte::Safe::String& path);
	}
}

/**
 * @brief Domain for @ref StormByte::System::Directory::Error.
 */
template<>
struct StormByte::Error::Domain<StormByte::System::Directory::Error> {
	static constexpr const char* Name = "StormByte.System.Directory";	///< Stable category tag

	/**
	 * @brief Text for one Directory enumerator.
	 * @param e Enumerator.
	 * @return Human-readable message.
	 */
	static STORMBYTE_FORCE_INLINE std::string Message(StormByte::System::Directory::Error e) {
		switch (e) {
			case StormByte::System::Directory::Error::Success:
				return "Success";
			case StormByte::System::Directory::Error::Permission:
				return "Directory permission denied";
			case StormByte::System::Directory::Error::NotFound:
				return "Directory not found";
			case StormByte::System::Directory::Error::Failed:
				return "Directory resolution failed";
		}
		return "Unknown Directory error";
	}
};

namespace StormByte::System {
	/**
	 * @brief Category singleton for @ref Directory::Error.
	 * @return Process-wide Directory category.
	 */
	STORMBYTE_SYSTEM_PUBLIC const StormByte::Error::Category<Directory::Error>& directory_category() noexcept;

	/**
	 * @brief Builds an `std::error_code` from @ref Directory::Error.
	 * Header-only: a `std::` object is not a DLL return.
	 * @param e Enumerator.
	 * @return Code in @ref directory_category().
	 */
	STORMBYTE_FORCE_INLINE std::error_code make_error_code(Directory::Error e) noexcept {
		return std::error_code(static_cast<int>(e), directory_category());
	}
}

namespace std {
	/**
	 * @brief Marks @ref StormByte::System::Directory::Error as an `std::error_code` enum.
	 */
	template<>
	struct is_error_code_enum<StormByte::System::Directory::Error>: true_type {};
}
