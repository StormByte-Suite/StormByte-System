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
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>
#include <StormByte/system/visibility.h>

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#ifdef WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace StormByte::System {
	class Pipe;
	class ProcessImplementation;

	/**
	 * @struct _EoF
	 * @brief Tag type to close process stdin (write end).
	 */
	struct {} typedef _EoF;

	/**
	 * @brief Sentinel used as `process << System::EoF` to close stdin.
	 */
	static constexpr const _EoF EoF = {};

	/**
	 * @class Process
	 * @brief Runs an external program with piped stdin/stdout/stderr.
	 *
	 * Starts immediately on construction. Move-only.
	 * Construction does not throw. @c operator bool is true only while a
	 * child is live (running or suspended). A finished, moved-from or
	 * failed spawn is false. Inspect @ref Fault for the reason.
	 *
	 * Supports chaining (`p1 >> p2`), writing stdin, reading stdout/stderr,
	 * Suspend/Resume.
	 */
	class STORMBYTE_SYSTEM_PUBLIC Process {
		public:
			/**
			 * @enum Error
			 * @brief Child-process enumerators.
			 *
			 * Domain tag `StormByte.System.Process`. Zero is success.
			 */
			enum class Error {
				Success = 0,			///< No error
				ExecutableNotFound,		///< The program path or name could not be resolved
				CreationFailed,			///< The child could not be created (fork, pipe, CreateProcess)
				Permission,				///< The caller may not create or signal this child
				NotRunning,				///< There is no live child for this operation
				AlreadyExited,			///< The child has already exited
				TimedOut,				///< A timed wait expired
				BrokenPipe,				///< stdin/stdout/stderr pipe is closed or unusable
				Canceled,					///< The operation was canceled
				OperationFailed			///< A process operation failed at the platform boundary
			};

			/**
			 * @enum Status
			 * @brief Process lifecycle.
			 */
			enum class Status: unsigned short {
				RUNNING,	///< Running
				SUSPENDED,	///< Suspended
				TERMINATED	///< Finished / cleaned up
			};

			/**
			 * @brief Construct and start.
			 * @param prog Executable path or name. A literal, a view and a Safe::String all bind here.
			 * @param args Argument list (not including argv[0]).
			 */
			Process(std::string_view prog, const StormByte::Safe::Vector<StormByte::Safe::String>& args = {}) noexcept;

			Process(const Process& proc) = delete;

			/**
			 * @brief Move constructor (invalidates the source).
			 */
			Process(Process&& proc) noexcept;

			Process& operator=(const Process& proc) = delete;

			/**
			 * @brief Move assignment (invalidates the source).
			 */
			Process& operator=(Process&& proc) noexcept;

			/**
			 * @brief Destructor (waits if still owning a child, then frees pipes).
			 */
			virtual ~Process() noexcept;

			/**
			 * @brief Whether a child is live.
			 * @return true if the status is running or suspended.
			 */
			explicit operator bool() const noexcept;

			/**
			 * @brief Last Process error.
			 * @return Success, or a @ref Error code.
			 */
			StormByte::Error::Fault Fault() const noexcept;

			#ifdef UNIX
			/**
			 * @brief Block until the process exits (no timeout).
			 * @return Exit code, or -1 on failure, signal termination, or already reaped.
			 */
			int Wait() noexcept;

			/**
			 * @brief Wait for the process to exit up to @p timeout.
			 * @param timeout Maximum wait duration.
			 * @return Exit code, or -1 on timeout, failure, or already reaped.
			 */
			int Wait(std::chrono::milliseconds timeout) noexcept;

			/**
			 * @brief Child PID.
			 * @return PID, or -1 if not owning a process.
			 */
			pid_t Pid() noexcept;
			#else
			/**
			 * @brief Block until the process exits (no timeout).
			 * @return Exit code, or (DWORD)-1 on failure.
			 */
			DWORD Wait() noexcept;

			/**
			 * @brief Wait for the process to exit up to @p timeout.
			 * @param timeout Maximum wait duration.
			 * @return Exit code, or (DWORD)-1 on timeout, failure, or already reaped.
			 */
			DWORD Wait(std::chrono::milliseconds timeout) noexcept;

			/**
			 * @brief Child process identifier.
			 * @return Process identifier, or zero if no child is owned.
			 */
			DWORD Pid() noexcept;
			#endif

			/**
			 * @brief Suspend the child process.
			 */
			void Suspend();

			/**
			 * @brief Resume a suspended child process.
			 */
			void Resume();

			/**
			 * @brief Forward this process stdout to @p proc stdin (background thread).
			 * @param proc Target process.
			 * @return Reference to @p proc.
			 */
			Process& operator>>(Process& proc);

			/**
			 * @brief Read remaining stdout into owned text.
			 * @param str Destination. Replaced with the captured text.
			 * @return Reference to @p str.
			 */
			StormByte::Safe::String& operator>>(StormByte::Safe::String& str) const;

			/**
			 * @brief Read remaining stderr into owned text.
			 * @param str Destination. Replaced with the captured text.
			 * @return Reference to @p str.
			 */
			StormByte::Safe::String& Stderr(StormByte::Safe::String& str) const;

			/**
			 * @brief Stream process stdout to an ostream.
			 * @param ostream Destination. Grown in the caller.
			 * @param proc Process.
			 * @return @p ostream.
			 */
			STORMBYTE_FORCE_INLINE friend std::ostream& operator<<(std::ostream& ostream, const Process& proc) {
				StormByte::Safe::String owned;
				proc >> owned;
				return ostream << static_cast<std::string>(owned);
			}

			/**
			 * @brief Write UTF-8 text to process stdin.
			 * @param str Data. A literal and a Safe::String bind here.
			 * @return *this.
			 */
			Process& operator<<(std::string_view str);

			/**
			 * @brief Close process stdin (write end).
			 * @param eof EoF sentinel.
			 */
			void operator<<(const System::_EoF& eof);

		private:
			#ifdef WINDOWS
			/**
			 * @brief Quote one argument for the Windows command-line parser.
			 * @param argument Argument text.
			 * @return Quoted command-line argument.
			 */
			static std::string QuoteWindowsArgument(std::string_view argument);

			/**
			 * @brief Full command line as wide string.
			 * @return Command line.
			 */
			std::wstring FullCommand() const;
			#endif

			/**
			 * @brief Write to stdin.
			 * @param str Data.
			 */
			void Send(std::string_view str);

			/**
			 * @brief Initialize owned process state and start the child.
			 * @param prog Executable path or name.
			 * @param args Argument list (not including argv[0]).
			 */
			void Initialize(const StormByte::Safe::String& prog, const StormByte::Safe::Vector<StormByte::Safe::String>& args);

			/**
			 * @brief Spawn the child process.
			 */
			void Run();

			/**
			 * @brief Clear ownership so Wait/destructor are no-ops.
			 */
			void ReleaseOwnership() noexcept;

			/**
			 * @brief Join the forwarder without allowing exceptions to escape lifecycle methods.
			 */
			void JoinForwarder() noexcept;

			/**
			 * @brief Stop forwarding and optionally close the producer output read end.
			 * @param close_source_read Whether to close the local stdout read end.
			 */
			void StopForwarder(bool close_source_read) noexcept;

			StormByte::Safe::Unique<ProcessImplementation> m_implementation; ///< Provider-owned opaque process state.
	};

}

/**
 * @brief Domain for @ref StormByte::System::Process::Error.
 */
template<>
struct StormByte::Error::Domain<StormByte::System::Process::Error> {
	static constexpr const char* Name = "StormByte.System.Process";	///< Stable category tag

	/**
	 * @brief Text for one Process enumerator.
	 * @param e Enumerator.
	 * @return Human-readable message.
	 */
	static std::string Message(StormByte::System::Process::Error e) {
		switch (e) {
			case StormByte::System::Process::Error::Success:
				return "Success";
			case StormByte::System::Process::Error::ExecutableNotFound:
				return "Executable not found";
			case StormByte::System::Process::Error::CreationFailed:
				return "Process creation failed";
			case StormByte::System::Process::Error::Permission:
				return "Process permission denied";
			case StormByte::System::Process::Error::NotRunning:
				return "Process is not running";
			case StormByte::System::Process::Error::AlreadyExited:
				return "Process has already exited";
			case StormByte::System::Process::Error::TimedOut:
				return "Process wait timed out";
			case StormByte::System::Process::Error::BrokenPipe:
				return "Process pipe is broken";
			case StormByte::System::Process::Error::Canceled:
				return "Process operation canceled";
			case StormByte::System::Process::Error::OperationFailed:
				return "Process operation failed";
		}
		return "Unknown Process error";
	}
};

namespace StormByte::System {
	/**
	 * @brief Category singleton for @ref Process::Error.
	 * @return Process-wide Process category.
	 */
	STORMBYTE_SYSTEM_PUBLIC const StormByte::Error::Category<Process::Error>& process_category() noexcept;

	/**
	 * @brief Builds an `std::error_code` from @ref Process::Error.
	 * @param e Enumerator.
	 * @return Code in @ref process_category().
	 */
	STORMBYTE_SYSTEM_PUBLIC std::error_code make_error_code(Process::Error e) noexcept;
}

namespace std {
	/**
	 * @brief Marks @ref StormByte::System::Process::Error as an `std::error_code` enum.
	 */
	template<>
	struct is_error_code_enum<StormByte::System::Process::Error>: true_type {};
}
