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

#include <StormByte/safe/string.hxx>
#ifdef WINDOWS
#include <StormByte/safe/wstring.hxx>
#endif
#include <StormByte/system/visibility.h>

#include <filesystem>
#include <string_view>

namespace StormByte::System {
	/**
	 * @class Variable
	 * @brief Environment variable expansion helpers.
	 *
	 * Windows: ExpandEnvironmentStrings. UNIX: expand `~` to the home path.
	 */
	class STORMBYTE_SYSTEM_PUBLIC Variable {
		public:
			/**
			 * @brief Expand environment variables in UTF-8 text.
			 * @param str Input.
			 * @return Expanded owned text.
			 */
			static StormByte::Safe::String Expand(std::string_view str);

			/**
			 * @brief Expand environment variables in owned UTF-8 text.
			 * @param str Input.
			 * @return Expanded owned text.
			 */
			static StormByte::Safe::String Expand(const StormByte::Safe::String& str);

			#ifdef WINDOWS
			/**
			 * @brief Expand environment variables in wide text.
			 * @param str Input.
			 * @return Expanded owned UTF-8 text.
			 */
			static StormByte::Safe::String Expand(std::wstring_view str);

			/**
			 * @brief Expand environment variables in owned wide text.
			 * @param str Input.
			 * @return Expanded owned UTF-8 text.
			 */
			static StormByte::Safe::String Expand(const StormByte::Safe::WString& str);

			#endif

		private:
			/**
			 * @brief Platform implementation for UTF-8 text.
			 * @param str Input.
			 * @return Expanded owned text.
			 */
			static StormByte::Safe::String ExpandEnvironmentVariable(std::string_view str);

			#ifdef WINDOWS
			/**
			 * @brief Platform implementation for wide text.
			 * @param str Input.
			 * @return Expanded owned UTF-8 text.
			 */
			static StormByte::Safe::String ExpandEnvironmentVariable(std::wstring_view str);
			#else
			/**
			 * @brief Current user home directory.
			 * @return Home path.
			 */
			static std::filesystem::path HomePath();
			#endif
	};
}
