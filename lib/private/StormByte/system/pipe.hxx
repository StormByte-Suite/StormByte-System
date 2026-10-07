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

#include <StormByte/safe/binary.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/system/visibility.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#ifdef WINDOWS
#include <windows.h>
#else
#include <poll.h>
#endif

namespace StormByte::System {
	/**
	 * @class Pipe
	 * @brief Cross-platform anonymous pipe for process IPC.
	 *
	 * UNIX: pipe(2)/pipe2. Windows: CreatePipe. Move-only.
	 * Construction does not throw. @c operator bool is true when both ends are open.
	 * @note On UNIX, SIGPIPE is ignored process-wide once (first Pipe construction).
	 */
	class STORMBYTE_SYSTEM_PRIVATE Pipe {
		public:
			/**
			 * @brief Maximum bytes per read (4 MiB).
			 */
			static constexpr const size_t MAX_READ_BYTES = 4 * 1024 * 1024;

			/**
			 * @brief Create a new pipe pair.
			 */
			Pipe() noexcept;

			/**
			 * @brief Copying a pipe is not possible.
			 */
			Pipe(const Pipe&) = delete;

			/**
			 * @brief Move constructor.
			 * @param pipe Source. Left closed.
			 */
			Pipe(Pipe&& pipe) noexcept;

			/**
			 * @brief Close both ends.
			 */
			~Pipe() noexcept;

			/**
			 * @brief Copying a pipe is not possible.
			 */
			Pipe& operator=(const Pipe&) = delete;

			/**
			 * @brief Move assignment.
			 * @param pipe Source. Left closed.
			 * @return This pipe.
			 */
			Pipe& operator=(Pipe&& pipe) noexcept;

			/**
			 * @brief Whether both ends are open.
			 * @return true if the pipe is usable.
			 */
			explicit operator bool() const noexcept;

			#ifdef UNIX
			/**
			 * @brief Dup2 read end onto @p fd.
			 * @param fd Destination file descriptor.
			 * @return false if dup2 failed.
			 */
			bool BindRead(int fd) noexcept;

			/**
			 * @brief Dup2 write end onto @p fd.
			 * @param fd Destination file descriptor.
			 * @return false if dup2 failed.
			 */
			bool BindWrite(int fd) noexcept;

			/**
			 * @brief Write bytes to the write end.
			 * @param str Data.
			 * @return Bytes written.
			 */
			ssize_t Write(std::string_view str);

			/**
			 * @brief Whether the write end is no longer writable (HUP/ERR).
			 * @return true if so.
			 */
			bool WriteEOF() const;

			/**
			 * @brief Read into @p buffer up to @p size bytes.
			 * @param buffer Destination. Must already hold at least @p size bytes.
			 * @param size Max bytes.
			 * @return Bytes read.
			 */
			ssize_t Read(StormByte::Safe::Binary& buffer, ssize_t size) const;

			/**
			 * @brief Whether the read end reports HUP/ERR.
			 * @return true if so.
			 */
			bool ReadEOF() const;
			#else
			/**
			 * @brief Set handle information on the read end.
			 * @param mask Mask.
			 * @param flags Flags.
			 */
			void ReadHandleInformation(DWORD mask, DWORD flags);

			/**
			 * @brief Set handle information on the write end.
			 * @param mask Mask.
			 * @param flags Flags.
			 */
			void WriteHandleInformation(DWORD mask, DWORD flags);

			/**
			 * @brief Read HANDLE.
			 * @return Handle.
			 */
			HANDLE ReadHandle() const;

			/**
			 * @brief Write HANDLE.
			 * @return Handle.
			 */
			HANDLE WriteHandle() const;

			/**
			 * @brief Write bytes to the write end.
			 * @param str Data.
			 * @return Bytes written.
			 */
			DWORD Write(std::string_view str);

			/**
			 * @brief Read into @p buffer up to @p size bytes.
			 * @param buffer Destination. Must already hold at least @p size bytes.
			 * @param size Max bytes.
			 * @return Bytes read.
			 */
			DWORD Read(StormByte::Safe::Binary& buffer, DWORD size) const;
			#endif

			/**
			 * @brief Write @p str in chunks until complete or peer closes.
			 * @param str Data. Empty view succeeds immediately.
			 * @param cancelled Optional cancellation flag.
			 * @return true if all data was written.
			 */
			bool WriteAtomic(std::string_view str, const std::shared_ptr<std::atomic_bool>& cancelled = {});

			/**
			 * @brief Write @p str in chunks until complete or peer closes.
			 * @param str Data (moved). Empty text succeeds immediately.
			 * @param cancelled Optional cancellation flag.
			 * @return true if all data was written.
			 */
			bool WriteAtomic(StormByte::Safe::String&& str, const std::shared_ptr<std::atomic_bool>& cancelled = {});

			/**
			 * @brief Close the read end.
			 */
			void CloseRead() noexcept;

			/**
			 * @brief Close the write end.
			 */
			void CloseWrite() noexcept;

			/**
			 * @brief Write @p str via WriteAtomic.
			 * @param str Data.
			 * @return true if every byte was written.
			 */
			bool operator<<(std::string_view str);

			/**
			 * @brief Write @p str via WriteAtomic.
			 * @param str Data.
			 * @return true if every byte was written.
			 */
			bool operator<<(const StormByte::Safe::String& str) {
				return *this << static_cast<std::string_view>(str);
			}

			/**
			 * @brief Read until EOF into @p str.
			 * @param str Destination.
			 * @return Reference to @p str.
			 */
			StormByte::Safe::String& operator>>(StormByte::Safe::String& str) const;

			/**
			 * @brief Forward all current and future data to another pipe.
			 * @param source Source pipe.
			 * @param destination Destination pipe.
			 * @param cancelled Cancellation flag.
			 * @param on_failure Called if the destination closes before forwarding completes.
			 * @return Provider-owned forwarding thread.
			 */
			static std::unique_ptr<std::thread> Connect(std::shared_ptr<Pipe> source, std::shared_ptr<Pipe> destination, const std::shared_ptr<std::atomic_bool>& cancelled, std::function<void()> on_failure = {});

		private:
			/**
			 * @brief Wait until the read end is readable or cancellation is requested.
			 * @param cancelled Optional cancellation state.
			 * @return true when a read should be attempted.
			 */
			bool WaitReadable(const std::shared_ptr<std::atomic_bool>& cancelled) const;

			#ifdef WINDOWS
			HANDLE m_fd[2];						///< Read / write handles
			static SECURITY_ATTRIBUTES m_sAttr;	///< Inherit attributes
			#else
			int m_fd[2];						///< Read / write fds
			#endif

			#ifdef UNIX
			/**
			 * @brief Dup2 and close source.
			 * @param src Source fd (set to -1).
			 * @param dst Destination fd.
			 * @return false if dup2 failed.
			 */
			bool Bind(int& src, int dst) noexcept;

			/**
			 * @brief Close @p fd if open.
			 * @param fd File descriptor.
			 */
			void Close(int& fd) noexcept;
			#else
			/**
			 * @brief Close @p handle if valid.
			 * @param handle Handle.
			 */
			void Close(HANDLE& handle) noexcept;

			/**
			 * @brief SetHandleInformation wrapper.
			 * @param handle Handle.
			 * @param mask Mask.
			 * @param flags Flags.
			 */
			void HandleInformation(HANDLE handle, DWORD mask, DWORD flags);
			#endif
	};
}
