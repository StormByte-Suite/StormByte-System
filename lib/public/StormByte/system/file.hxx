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
#include <string_view>
#include <system_error>

/**
 * @brief System module of the StormByte suite.
 */
namespace StormByte::System {
	/**
	 * @brief File locations of this process.
	 *
	 * Each call returns @c true on success and updates @ref LastError
	 * for this thread. An output @c String is valid only when the call
	 * returned @c true. The caller unlinks a file created by @ref Temporary.
	 */
	namespace File {
		/**
		 * @enum Error
		 * @brief File enumerators.
		 *
		 * Domain tag `StormByte.System.File`. Zero is success.
		 */
		enum class Error {
			Success = 0,	///< No error
			Permission,		///< The process may not create or read this file
			NotFound,		///< The file could not be resolved
			Failed			///< The platform call failed
		};

		/**
		 * @brief Last File error on this thread.
		 * @return Success or an Error code. Stored in the System module TLS.
		 */
		STORMBYTE_SYSTEM_PUBLIC StormByte::Error::Fault LastError() noexcept;

		/**
		 * @brief Create a unique empty temporary file.
		 * @param[out] path Absolute path of the created file. Written only on success.
		 * @param prefix File-name prefix. Default `"TMP"`.
		 * @param suffix File-name suffix (including a leading dot if wanted). Default empty.
		 * @return true if the file was created.
		 *
		 * POSIX uses `mkstemp` / `mkstemps`. Windows uses `GetTempFileNameW`
		 * and only the first three prefix characters; the suffix is appended
		 * after the generated name when it is not empty.
		 */
		STORMBYTE_SYSTEM_PUBLIC bool Temporary(
			StormByte::Safe::String& path,
			std::string_view prefix = "TMP",
			std::string_view suffix = {}
		);

		/**
		 * @brief Full path of the running executable.
		 * @param[out] path Absolute file path. Written only on success.
		 * @return true if the path was resolved.
		 */
		STORMBYTE_SYSTEM_PUBLIC bool CurrentExecutable(StormByte::Safe::String& path);
	}
}

/**
 * @brief Domain for @ref StormByte::System::File::Error.
 */
template<>
struct StormByte::Error::Domain<StormByte::System::File::Error> {
	static constexpr const char* Name = "StormByte.System.File";	///< Stable category tag

	/**
	 * @brief Text for one File enumerator.
	 * @param e Enumerator.
	 * @return Human-readable message.
	 */
	static STORMBYTE_FORCE_INLINE std::string Message(StormByte::System::File::Error e) {
		switch (e) {
			case StormByte::System::File::Error::Success:
				return "Success";
			case StormByte::System::File::Error::Permission:
				return "File permission denied";
			case StormByte::System::File::Error::NotFound:
				return "File not found";
			case StormByte::System::File::Error::Failed:
				return "File operation failed";
		}
		return "Unknown File error";
	}
};

namespace StormByte::System {
	/**
	 * @brief Category singleton for @ref File::Error.
	 * @return Process-wide File category.
	 */
	STORMBYTE_SYSTEM_PUBLIC const StormByte::Error::Category<File::Error>& file_category() noexcept;

	/**
	 * @brief Builds an `std::error_code` from @ref File::Error.
	 * Header-only: a `std::` object is not a DLL return.
	 * @param e Enumerator.
	 * @return Code in @ref file_category().
	 */
	STORMBYTE_FORCE_INLINE std::error_code make_error_code(File::Error e) noexcept {
		return std::error_code(static_cast<int>(e), file_category());
	}
}

namespace std {
	/**
	 * @brief Marks @ref StormByte::System::File::Error as an `std::error_code` enum.
	 */
	template<>
	struct is_error_code_enum<StormByte::System::File::Error>: true_type {};
}
