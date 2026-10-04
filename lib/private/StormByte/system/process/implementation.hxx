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

#include <StormByte/system/pipe.hxx>
#include <StormByte/system/process.hxx>

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace StormByte::System {
	/**
	 * @class ProcessImplementation
	 * @brief Private implementation state for Process.
	 */
	class STORMBYTE_SYSTEM_PRIVATE ProcessImplementation {
		public:
			/**
			 * @brief Lifecycle of the child.
			 */
			Process::Status m_status { Process::Status::TERMINATED };

			/**
			 * @brief Last Process error. Success until spawn or a later operation fails.
			 */
			StormByte::Error::Fault m_fault;

#ifdef UNIX
			/**
			 * @brief Child PID. -1 if none is owned.
			 */
			pid_t m_pid { -1 };
#else
			/**
			 * @brief Windows startup information for CreateProcessW.
			 */
			STARTUPINFOW m_siStartInfo {};

			/**
			 * @brief Windows process and thread handles.
			 */
			PROCESS_INFORMATION m_piProcInfo {};

			/**
			 * @struct SuspendedThread
			 * @brief Owns one thread handle and the single suspension added by Process.
			 */
			struct SuspendedThread {
				/** @brief Thread identifier used to avoid suspending it twice. */
				DWORD id{};

				/** @brief Handle retained until this Process resumes the thread. */
				HANDLE handle{nullptr};

				/**
				 * @brief Take ownership of the handle for a successfully suspended thread.
				 * @param thread_id Native thread identifier.
				 * @param thread_handle Open thread handle.
				 */
				SuspendedThread(DWORD thread_id, HANDLE thread_handle) noexcept:
					id(thread_id), handle(thread_handle) {}

				/**
				 * @brief Copy construction is disabled for unique handle ownership.
				 * @param other Owner not copied.
				 */
				SuspendedThread(const SuspendedThread& other) = delete;

				/**
				 * @brief Transfer thread-handle ownership.
				 * @param other Owner being moved from.
				 */
				SuspendedThread(SuspendedThread&& other) noexcept:
					id(other.id), handle(std::exchange(other.handle, nullptr)) {}

				/**
				 * @brief Copy assignment is disabled for unique handle ownership.
				 * @param other Owner not copied.
				 * @return This owner.
				 */
				SuspendedThread& operator=(const SuspendedThread& other) = delete;

				/**
				 * @brief Release this handle and transfer another suspension owner.
				 * @param other Owner being moved from.
				 * @return This owner.
				 */
				SuspendedThread& operator=(SuspendedThread&& other) noexcept {
					if (this != &other) {
						if (handle != nullptr)
							CloseHandle(handle);
						id = other.id;
						handle = std::exchange(other.handle, nullptr);
					}
					return *this;
				}

				/**
				 * @brief Close the retained thread handle.
				 */
				~SuspendedThread() noexcept {
					if (handle != nullptr)
						CloseHandle(handle);
				}
			};

			/**
			 * @brief Handles for threads suspended by this Process and still requiring one resume.
			 */
			std::vector<SuspendedThread> m_suspended_threads;

			/**
			 * @brief Whether a previous suspend/resume pass left threads in a partial state.
			 */
			bool m_suspension_incomplete{false};
#endif

			/**
			 * @brief Child stdout pipe.
			 */
			std::shared_ptr<Pipe> m_pstdout;

			/**
			 * @brief Child stdin pipe.
			 */
			std::shared_ptr<Pipe> m_pstdin;

			/**
			 * @brief Child stderr pipe.
			 */
			std::shared_ptr<Pipe> m_pstderr;

			/**
			 * @brief Executable path or name.
			 */
			std::filesystem::path m_program;

			/**
			 * @brief Narrow argument list used at spawn.
			 */
			std::vector<std::string> m_arguments;

			/**
			 * @brief Background stdout-to-stdin forwarder, if chained.
			 */
			std::unique_ptr<std::thread> m_forwarder;

			/**
			 * @brief Cancellation flag for the forwarder thread.
			 */
			std::shared_ptr<std::atomic_bool> m_forwarder_cancel;
	};
}
