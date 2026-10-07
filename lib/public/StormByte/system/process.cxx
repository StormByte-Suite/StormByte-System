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

#include <StormByte/error.txx>
#include <StormByte/safe/wstring.hxx>
#include <StormByte/system/pipe.hxx>
#include <StormByte/system/process.hxx>
#include <StormByte/system/process/implementation.hxx>

#include <filesystem>
#include <string>
#include <vector>

#ifdef UNIX
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#else
#include <cctype>
#include <sstream>
#include <tlhelp32.h>
#endif

using namespace StormByte::System;

namespace {
	std::filesystem::path NativePath(const StormByte::Safe::String& text) {
#ifdef WINDOWS
		const StormByte::Safe::WString wide(text);
		return std::filesystem::path(static_cast<std::wstring_view>(wide));
#else
		return std::filesystem::path(static_cast<std::string_view>(text));
#endif
	}

	std::vector<std::string> NarrowArgs(const StormByte::Safe::Vector<StormByte::Safe::String>& args) {
		std::vector<std::string> out;
		out.reserve(args.size());
		for (const StormByte::Safe::String& arg : args)
			out.emplace_back(std::string(std::string_view(arg)));
		return out;
	}

	void Fail(ProcessImplementation& impl, const enum Process::Error code) {
		impl.m_fault = StormByte::Error::Fault{make_error_code(code)};
		impl.m_status = Process::Status::TERMINATED;
#ifdef UNIX
		impl.m_pid = -1;
#endif
	}

#ifdef UNIX
	Process::Error ProcessErrorFromNative(const int error) noexcept {
		if (error == ECHILD || error == ESRCH)
			return Process::Error::AlreadyExited;
		if (error == EACCES || error == EPERM)
			return Process::Error::Permission;
		return Process::Error::OperationFailed;
	}
#else
	Process::Error ProcessErrorFromNative(const DWORD error) noexcept {
		if (error == ERROR_ACCESS_DENIED)
			return Process::Error::Permission;
		if (error == ERROR_INVALID_HANDLE || error == ERROR_INVALID_PARAMETER)
			return Process::Error::AlreadyExited;
		return Process::Error::OperationFailed;
	}
#endif
}

Process::Process(const std::string_view prog, const StormByte::Safe::Vector<StormByte::Safe::String>& args) noexcept {
	try {
		m_implementation = StormByte::Safe::MakeUnique<ProcessImplementation>();
		const StormByte::Safe::String owned_program(prog);
		Initialize(owned_program, args);
	} catch (...) {
		if (m_implementation)
			Fail(*m_implementation, Process::Error::CreationFailed);
	}
}

void Process::Initialize(const StormByte::Safe::String& prog, const StormByte::Safe::Vector<StormByte::Safe::String>& args) {
	m_implementation->m_status = Status::RUNNING;
#ifdef UNIX
	m_implementation->m_pid = -1;
#endif
	m_implementation->m_pstdout = std::make_shared<Pipe>();
	m_implementation->m_pstdin = std::make_shared<Pipe>();
	m_implementation->m_pstderr = std::make_shared<Pipe>();
	m_implementation->m_program = NativePath(prog);
	m_implementation->m_arguments = NarrowArgs(args);
#ifdef WINDOWS
	ZeroMemory(&m_implementation->m_siStartInfo, sizeof(STARTUPINFOW));
	ZeroMemory(&m_implementation->m_piProcInfo, sizeof(PROCESS_INFORMATION));
#endif
	if (!*m_implementation->m_pstdout || !*m_implementation->m_pstdin || !*m_implementation->m_pstderr) {
		Fail(*m_implementation, Process::Error::CreationFailed);
		return;
	}
	Run();
}

Process::operator bool() const noexcept {
	if (!m_implementation)
		return false;
	return m_implementation->m_status == Status::RUNNING ||
		m_implementation->m_status == Status::SUSPENDED;
}

StormByte::Error::Fault Process::Fault() const noexcept {
	if (!m_implementation)
		return StormByte::Error::Fault{make_error_code(Process::Error::NotRunning)};
	return m_implementation->m_fault;
}

void Process::ReleaseOwnership() noexcept {
	if (!m_implementation)
		return;
#ifdef UNIX
	m_implementation->m_pid = -1;
#else
	m_implementation->m_suspended_threads.clear();
	ZeroMemory(&m_implementation->m_piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&m_implementation->m_siStartInfo, sizeof(STARTUPINFOW));
#endif
	m_implementation->m_status = Status::TERMINATED;
	m_implementation->m_pstdout.reset();
	m_implementation->m_pstdin.reset();
	m_implementation->m_pstderr.reset();
	m_implementation->m_forwarder.reset();
}

void Process::JoinForwarder() noexcept {
	if (!m_implementation || !m_implementation->m_forwarder)
		return;
	try {
		if (m_implementation->m_forwarder->joinable())
			m_implementation->m_forwarder->join();
	} catch (...) {
		try {
			if (m_implementation->m_forwarder->joinable())
				m_implementation->m_forwarder->detach();
		} catch (...) {}
	}

	m_implementation->m_forwarder.reset();
}

void Process::StopForwarder(bool close_source_read) noexcept {
	if (!m_implementation || !m_implementation->m_forwarder)
		return;
	m_implementation->m_forwarder_cancel->store(true);
	JoinForwarder();
	if (close_source_read)
		m_implementation->m_pstdout->CloseRead();
}

Process::Process(Process&& proc) noexcept:
	m_implementation(std::move(proc.m_implementation)) {}

Process& Process::operator=(Process&& proc) noexcept {
	if (this != &proc) {
		Wait();
		m_implementation = std::move(proc.m_implementation);
	}

	return *this;
}

Process::~Process() noexcept {
	Wait();
	if (!m_implementation)
		return;
	m_implementation->m_pstdout.reset();
	m_implementation->m_pstdin.reset();
	m_implementation->m_pstderr.reset();
#ifdef WINDOWS
	ZeroMemory(&m_implementation->m_siStartInfo, sizeof(STARTUPINFOW));
	ZeroMemory(&m_implementation->m_piProcInfo, sizeof(PROCESS_INFORMATION));
#endif
}

Process& Process::operator>>(Process& exe) {
	if (!m_implementation || !exe.m_implementation)
		return exe;
	if (m_implementation->m_forwarder && m_implementation->m_forwarder->joinable())
		StopForwarder(false);

	try {
		m_implementation->m_forwarder_cancel = std::make_shared<std::atomic_bool>(false);
#ifdef UNIX
	const pid_t source_pid = m_implementation->m_pid;
	m_implementation->m_forwarder = Pipe::Connect(m_implementation->m_pstdout, exe.m_implementation->m_pstdin, m_implementation->m_forwarder_cancel, [source_pid] {
		if (source_pid > 0)
			kill(source_pid, SIGTERM);
	});
#else
	const HANDLE source_process = m_implementation->m_piProcInfo.hProcess;
	m_implementation->m_forwarder = Pipe::Connect(m_implementation->m_pstdout, exe.m_implementation->m_pstdin, m_implementation->m_forwarder_cancel, [source_process] {
		if (source_process != nullptr)
			TerminateProcess(source_process, 0);
	});
#endif
	} catch (...) {
		m_implementation->m_forwarder_cancel.reset();
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::CreationFailed)};
	}
	return exe;
}

StormByte::Safe::String& Process::operator>>(StormByte::Safe::String& data) const {
	StormByte::Safe::String raw;
	if (m_implementation && m_implementation->m_pstdout)
		*m_implementation->m_pstdout >> raw;
	data = std::move(raw);
	return data;
}

StormByte::Safe::String& Process::Stderr(StormByte::Safe::String& str) const {
	StormByte::Safe::String raw;
	if (m_implementation && m_implementation->m_pstderr)
		*m_implementation->m_pstderr >> raw;
	str = std::move(raw);
	return str;
}

Process& Process::operator<<(std::string_view data) {
	Send(data);
	return *this;
}

void Process::operator<<(const System::_EoF&) {
	if (m_implementation && m_implementation->m_pstdin)
		m_implementation->m_pstdin->CloseWrite();
}

void Process::Run() {
#ifdef UNIX
	std::vector<char*> argv;
	argv.reserve(m_implementation->m_arguments.size() + 2);
	argv.push_back(const_cast<char*>(m_implementation->m_program.c_str()));
	for (std::string& argument : m_implementation->m_arguments)
		argv.push_back(argument.data());
	argv.push_back(nullptr);

	int exec_status[2] = { -1, -1 };
#ifdef LINUX
	if (pipe2(exec_status, O_CLOEXEC) == -1) {
		Fail(*m_implementation, errno == EACCES || errno == EPERM
			? Process::Error::Permission
			: Process::Error::CreationFailed);
		return;
	}
#else
	if (pipe(exec_status) == -1 || fcntl(exec_status[0], F_SETFD, FD_CLOEXEC) == -1 || fcntl(exec_status[1], F_SETFD, FD_CLOEXEC) == -1) {
		const int error = errno;
		if (exec_status[0] != -1)
			close(exec_status[0]);
		if (exec_status[1] != -1)
			close(exec_status[1]);
		Fail(*m_implementation, error == EACCES || error == EPERM
			? Process::Error::Permission
			: Process::Error::CreationFailed);
		return;
	}
#endif
	m_implementation->m_pid = fork();
	if (m_implementation->m_pid == 0) {
		close(exec_status[0]);
		auto report_exec_error = [&exec_status] {
			const int error = errno;
			const char* data = reinterpret_cast<const char*>(&error);
			size_t remaining = sizeof(error);
			while (remaining > 0) {
				const ssize_t written = write(exec_status[1], data, remaining);
				if (written > 0) {
					data += written;
					remaining -= static_cast<size_t>(written);
				} else if (written < 0 && errno == EINTR) {
					continue;
				} else {
					break;
				}
			}
		};
		m_implementation->m_pstdin->CloseWrite();
		if (!m_implementation->m_pstdin->BindRead(STDIN_FILENO)) {
			report_exec_error();
			_exit(127);
		}

		m_implementation->m_pstdout->CloseRead();
		if (!m_implementation->m_pstdout->BindWrite(STDOUT_FILENO)) {
			report_exec_error();
			_exit(127);
		}

		m_implementation->m_pstderr->CloseRead();
		if (!m_implementation->m_pstderr->BindWrite(STDERR_FILENO)) {
			report_exec_error();
			_exit(127);
		}

		execvp(m_implementation->m_program.c_str(), argv.data());
		report_exec_error();
		_exit(127);
	} else if (m_implementation->m_pid > 0) {
		close(exec_status[1]);
		m_implementation->m_pstdin->CloseRead();
		m_implementation->m_pstdout->CloseWrite();
		m_implementation->m_pstderr->CloseWrite();
		int child_error = 0;
		char* data = reinterpret_cast<char*>(&child_error);
		size_t remaining = sizeof(child_error);
		while (remaining > 0) {
			const ssize_t bytes_read = read(exec_status[0], data, remaining);
			if (bytes_read > 0) {
				data += bytes_read;
				remaining -= static_cast<size_t>(bytes_read);
			} else if (bytes_read < 0 && errno == EINTR) {
				continue;
			} else {
				break;
			}
		}

		close(exec_status[0]);
		if (remaining == sizeof(child_error))
			return;
		if (remaining == 0) {
			waitpid(m_implementation->m_pid, nullptr, 0);
			if (child_error == ENOENT)
				Fail(*m_implementation, Process::Error::ExecutableNotFound);
			else if (child_error == EACCES || child_error == EPERM)
				Fail(*m_implementation, Process::Error::Permission);
			else
				Fail(*m_implementation, Process::Error::CreationFailed);
			return;
		}

		waitpid(m_implementation->m_pid, nullptr, 0);
		Fail(*m_implementation, Process::Error::CreationFailed);
	} else {
		close(exec_status[0]);
		close(exec_status[1]);
		Fail(*m_implementation, errno == EACCES || errno == EPERM
			? Process::Error::Permission
			: Process::Error::CreationFailed);
	}
#else
	ZeroMemory(&m_implementation->m_piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&m_implementation->m_siStartInfo, sizeof(STARTUPINFOW));
	m_implementation->m_siStartInfo.cb = sizeof(STARTUPINFOW);
	m_implementation->m_siStartInfo.hStdError = m_implementation->m_pstderr->WriteHandle();
	m_implementation->m_siStartInfo.hStdOutput = m_implementation->m_pstdout->WriteHandle();
	m_implementation->m_siStartInfo.hStdInput = m_implementation->m_pstdin->ReadHandle();
	m_implementation->m_siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
	m_implementation->m_pstdout->ReadHandleInformation(HANDLE_FLAG_INHERIT, FALSE);
	m_implementation->m_pstderr->ReadHandleInformation(HANDLE_FLAG_INHERIT, FALSE);
	m_implementation->m_pstdin->WriteHandleInformation(HANDLE_FLAG_INHERIT, FALSE);
	std::wstring command = FullCommand();
	std::vector<wchar_t> cmdline(command.begin(), command.end());
	cmdline.push_back(L'\0');
	LPWSTR szCmdline = cmdline.data();
	if (CreateProcessW(NULL,
			szCmdline,
			NULL,
			NULL,
			TRUE,
			CREATE_NO_WINDOW,
			NULL,
			NULL,
			&m_implementation->m_siStartInfo,
			&m_implementation->m_piProcInfo)) {
		m_implementation->m_pstdout->WriteHandleInformation(HANDLE_FLAG_INHERIT, 0);
		m_implementation->m_pstderr->WriteHandleInformation(HANDLE_FLAG_INHERIT, 0);
		m_implementation->m_pstdin->ReadHandleInformation(HANDLE_FLAG_INHERIT, 0);
		m_implementation->m_pstdout->CloseWrite();
		m_implementation->m_pstderr->CloseWrite();
		m_implementation->m_pstdin->CloseRead();
	} else {
		const DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			Fail(*m_implementation, Process::Error::ExecutableNotFound);
		else if (error == ERROR_ACCESS_DENIED)
			Fail(*m_implementation, Process::Error::Permission);
		else
			Fail(*m_implementation, Process::Error::CreationFailed);
	}
#endif
}

void Process::Send(std::string_view str) {
	if (!m_implementation || !m_implementation->m_pstdin) {
		if (m_implementation)
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::BrokenPipe)};
		return;
	}
	if (!(*m_implementation->m_pstdin << str))
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::BrokenPipe)};
}

#ifdef UNIX
int Process::Wait() noexcept {
	if (!m_implementation || m_implementation->m_status == Status::TERMINATED || m_implementation->m_pid <= 0) {
		if (m_implementation && m_implementation->m_status == Status::TERMINATED && !m_implementation->m_fault)
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::AlreadyExited)};
		return -1;
	}
	if (m_implementation->m_forwarder)
		StopForwarder(true);

	int status = 0;
	pid_t result;
	do {
		result = waitpid(m_implementation->m_pid, &status, 0);
	} while (result == -1 && errno == EINTR);
	if (result == -1) {
		m_implementation->m_status = Status::TERMINATED;
		m_implementation->m_pid = -1;
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(errno))};
		if (m_implementation->m_forwarder)
			JoinForwarder();
		return -1;
	}

	m_implementation->m_status = Status::TERMINATED;
	m_implementation->m_pid = -1;
	if (m_implementation->m_forwarder)
		JoinForwarder();

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
}

int Process::Wait(std::chrono::milliseconds timeout) noexcept {
	if (!m_implementation || m_implementation->m_status == Status::TERMINATED || m_implementation->m_pid <= 0) {
		if (m_implementation && m_implementation->m_status == Status::TERMINATED && !m_implementation->m_fault)
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::AlreadyExited)};
		return -1;
	}
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	int status = 0;
	while (true) {
		const pid_t result = waitpid(m_implementation->m_pid, &status, WNOHANG);
		if (result == m_implementation->m_pid) {
			m_implementation->m_status = Status::TERMINATED;
			m_implementation->m_pid = -1;
			if (m_implementation->m_forwarder)
				StopForwarder(true);
			return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		}

		if (result == -1 && errno != EINTR) {
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(errno))};
			m_implementation->m_status = Status::TERMINATED;
			m_implementation->m_pid = -1;
			if (m_implementation->m_forwarder)
				StopForwarder(true);
			return -1;
		}

		if (std::chrono::steady_clock::now() >= deadline) {
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::TimedOut)};
			if (m_implementation->m_forwarder)
				StopForwarder(true);
			return -1;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

pid_t Process::Pid() noexcept {
	if (!m_implementation)
		return -1;
	return m_implementation->m_pid;
}

#else
DWORD Process::Wait() noexcept {
	if (!m_implementation || m_implementation->m_status == Status::TERMINATED || m_implementation->m_piProcInfo.hProcess == nullptr) {
		if (m_implementation && m_implementation->m_status == Status::TERMINATED && !m_implementation->m_fault)
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::AlreadyExited)};
		return static_cast<DWORD>(-1);
	}
	if (m_implementation->m_forwarder)
		StopForwarder(true);

	DWORD exitCode = 0;
	if (WaitForSingleObject(m_implementation->m_piProcInfo.hProcess, INFINITE) == WAIT_FAILED) {
		const DWORD error = GetLastError();
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(error))};
		return static_cast<DWORD>(-1);
	}

	if (!GetExitCodeProcess(m_implementation->m_piProcInfo.hProcess, &exitCode)) {
		const DWORD error = GetLastError();
		CloseHandle(m_implementation->m_piProcInfo.hProcess);
		CloseHandle(m_implementation->m_piProcInfo.hThread);
		m_implementation->m_suspended_threads.clear();
		m_implementation->m_suspension_incomplete = false;
		ZeroMemory(&m_implementation->m_piProcInfo, sizeof(PROCESS_INFORMATION));
		m_implementation->m_status = Status::TERMINATED;
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(error))};
		if (m_implementation->m_forwarder)
			JoinForwarder();
		return static_cast<DWORD>(-1);
	}

	CloseHandle(m_implementation->m_piProcInfo.hProcess);
	CloseHandle(m_implementation->m_piProcInfo.hThread);
	m_implementation->m_suspended_threads.clear();
	m_implementation->m_suspension_incomplete = false;
	ZeroMemory(&m_implementation->m_piProcInfo, sizeof(PROCESS_INFORMATION));
	m_implementation->m_status = Status::TERMINATED;
	if (m_implementation->m_forwarder)
		JoinForwarder();

	return exitCode;
}

DWORD Process::Wait(std::chrono::milliseconds timeout) noexcept {
	if (!m_implementation || m_implementation->m_status == Status::TERMINATED || m_implementation->m_piProcInfo.hProcess == nullptr) {
		if (m_implementation && m_implementation->m_status == Status::TERMINATED && !m_implementation->m_fault)
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::AlreadyExited)};
		return static_cast<DWORD>(-1);
	}
	const auto count = timeout.count() < 0 ? 0 : timeout.count();
	const DWORD wait_result = WaitForSingleObject(m_implementation->m_piProcInfo.hProcess, static_cast<DWORD>(count));
	if (wait_result != WAIT_OBJECT_0) {
		if (wait_result == WAIT_TIMEOUT)
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::TimedOut)};
		else
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(GetLastError()))};
		if (m_implementation->m_forwarder)
			StopForwarder(true);
		return static_cast<DWORD>(-1);
	}

	if (m_implementation->m_forwarder)
		StopForwarder(true);

	DWORD exitCode = 0;
	if (!GetExitCodeProcess(m_implementation->m_piProcInfo.hProcess, &exitCode)) {
		const DWORD error = GetLastError();
		CloseHandle(m_implementation->m_piProcInfo.hProcess);
		CloseHandle(m_implementation->m_piProcInfo.hThread);
		m_implementation->m_suspended_threads.clear();
		m_implementation->m_suspension_incomplete = false;
		ZeroMemory(&m_implementation->m_piProcInfo, sizeof(PROCESS_INFORMATION));
		m_implementation->m_status = Status::TERMINATED;
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(error))};
		return static_cast<DWORD>(-1);
	}

	CloseHandle(m_implementation->m_piProcInfo.hProcess);
	CloseHandle(m_implementation->m_piProcInfo.hThread);
	m_implementation->m_suspended_threads.clear();
	m_implementation->m_suspension_incomplete = false;
	ZeroMemory(&m_implementation->m_piProcInfo, sizeof(PROCESS_INFORMATION));
	m_implementation->m_status = Status::TERMINATED;
	return exitCode;
}

DWORD Process::Pid() noexcept {
	if (!m_implementation)
		return 0;
	return m_implementation->m_piProcInfo.dwProcessId;
}

#endif
void Process::Suspend() {
	if (!m_implementation || !*this) {
		if (m_implementation)
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::NotRunning)};
		return;
	}
	if (m_implementation->m_status == Status::SUSPENDED) {
#ifdef WINDOWS
		if (!m_implementation->m_suspension_incomplete)
			return;
#else
		return;
#endif
	}
#ifdef UNIX
	if (m_implementation->m_pid <= 0 || ::kill(m_implementation->m_pid, SIGSTOP) != 0) {
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(
			m_implementation->m_pid <= 0 ? Process::Error::NotRunning : ProcessErrorFromNative(errno))};
		return;
	}
#else
	const DWORD process_state = WaitForSingleObject(m_implementation->m_piProcInfo.hProcess, 0);
	if (process_state == WAIT_OBJECT_0) {
		(void)Wait();
		return;
	}
	if (process_state == WAIT_FAILED) {
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(GetLastError()))};
		return;
	}
	for (auto thread = m_implementation->m_suspended_threads.begin(); thread != m_implementation->m_suspended_threads.end();) {
		if (WaitForSingleObject(thread->handle, 0) == WAIT_OBJECT_0)
			thread = m_implementation->m_suspended_threads.erase(thread);
		else
			++thread;
	}
	if (m_implementation->m_suspended_threads.empty()) {
		m_implementation->m_status = Status::RUNNING;
		m_implementation->m_suspension_incomplete = false;
	}
	if (m_implementation->m_piProcInfo.dwProcessId == 0) {
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::NotRunning)};
		return;
	}
	HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE) {
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(GetLastError()))};
		return;
	}
	bool snapshot_open = true;
	try {
		std::vector<DWORD> thread_ids;
		THREADENTRY32 te32;
		te32.dwSize = sizeof(THREADENTRY32);
		if (!Thread32First(hThreadSnap, &te32)) {
			const DWORD error = GetLastError();
			CloseHandle(hThreadSnap);
			snapshot_open = false;
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(error))};
			return;
		}
		do {
			if (te32.th32OwnerProcessID == m_implementation->m_piProcInfo.dwProcessId)
				thread_ids.push_back(te32.th32ThreadID);
		} while (Thread32Next(hThreadSnap, &te32));
		const DWORD enumeration_error = GetLastError();
		CloseHandle(hThreadSnap);
		snapshot_open = false;
		if (enumeration_error != ERROR_NO_MORE_FILES) {
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(enumeration_error))};
			return;
		}
		if (thread_ids.empty()) {
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::AlreadyExited)};
			return;
		}

		m_implementation->m_suspended_threads.reserve(
			m_implementation->m_suspended_threads.size() + thread_ids.size());
		bool failed = false;
		for (const DWORD thread_id : thread_ids) {
			bool already_suspended = false;
			for (const auto& thread : m_implementation->m_suspended_threads) {
				if (thread.id == thread_id) {
					already_suspended = true;
					break;
				}
			}
			if (already_suspended)
				continue;
			HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | SYNCHRONIZE, FALSE, thread_id);
			if (hThread == nullptr) {
				const DWORD error = GetLastError();
				if (error == ERROR_INVALID_PARAMETER || error == ERROR_INVALID_HANDLE)
					continue;
				m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(error))};
				failed = true;
				continue;
			}
			if (SuspendThread(hThread) == static_cast<DWORD>(-1)) {
				m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(GetLastError()))};
				failed = true;
			} else {
				m_implementation->m_suspended_threads.emplace_back(thread_id, hThread);
				hThread = nullptr;
			}
			if (hThread != nullptr)
				CloseHandle(hThread);
		}
		m_implementation->m_suspension_incomplete = failed;
		if (!m_implementation->m_suspended_threads.empty())
			m_implementation->m_status = Status::SUSPENDED;
		if (failed)
			return;
		if (m_implementation->m_suspended_threads.empty()) {
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::AlreadyExited)};
			return;
		}
	} catch (...) {
		if (snapshot_open)
			CloseHandle(hThreadSnap);
		m_implementation->m_suspension_incomplete = true;
		if (!m_implementation->m_suspended_threads.empty())
			m_implementation->m_status = Status::SUSPENDED;
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::OperationFailed)};
		return;
	}
	if (m_implementation->m_status == Status::SUSPENDED)
		return;
#endif
	m_implementation->m_status = Status::SUSPENDED;
}

void Process::Resume() {
	if (!m_implementation)
		return;
	if (m_implementation->m_status != Status::SUSPENDED) {
		if (m_implementation->m_status == Status::TERMINATED)
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(Process::Error::NotRunning)};
		return;
	}
#ifdef UNIX
	if (m_implementation->m_pid <= 0 || ::kill(m_implementation->m_pid, SIGCONT) != 0) {
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(
			m_implementation->m_pid <= 0 ? Process::Error::NotRunning : ProcessErrorFromNative(errno))};
		return;
	}
#else
	const DWORD process_state = WaitForSingleObject(m_implementation->m_piProcInfo.hProcess, 0);
	if (process_state == WAIT_OBJECT_0) {
		(void)Wait();
		return;
	}
	if (process_state == WAIT_FAILED) {
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(GetLastError()))};
		return;
	}
	bool failed = false;
	for (auto thread = m_implementation->m_suspended_threads.begin(); thread != m_implementation->m_suspended_threads.end();) {
		if (WaitForSingleObject(thread->handle, 0) == WAIT_OBJECT_0) {
			thread = m_implementation->m_suspended_threads.erase(thread);
			continue;
		}
		if (ResumeThread(thread->handle) == static_cast<DWORD>(-1)) {
			m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(GetLastError()))};
			failed = true;
			++thread;
		} else {
			thread = m_implementation->m_suspended_threads.erase(thread);
		}
	}
		if (failed || !m_implementation->m_suspended_threads.empty()) {
			if (failed)
				m_implementation->m_suspension_incomplete = true;
		return;
		}
	const DWORD resumed_state = WaitForSingleObject(m_implementation->m_piProcInfo.hProcess, 0);
	if (resumed_state == WAIT_OBJECT_0) {
		(void)Wait();
		return;
	}
	if (resumed_state == WAIT_FAILED) {
		m_implementation->m_fault = StormByte::Error::Fault{make_error_code(ProcessErrorFromNative(GetLastError()))};
		m_implementation->m_suspension_incomplete = false;
		m_implementation->m_status = Status::RUNNING;
		return;
	}
	m_implementation->m_suspension_incomplete = false;
#endif
	m_implementation->m_status = Status::RUNNING;
}

#ifdef WINDOWS
std::string Process::QuoteWindowsArgument(std::string_view argument) {
	std::string quoted = "\"";
	size_t backslashes = 0;
	for (const char character: argument) {
		if (character == '\\') {
			++backslashes;
			continue;
		}

		if (character == '"')
			quoted.append(backslashes * 2 + 1, '\\');
		else
			quoted.append(backslashes, '\\');
		quoted += character;
		backslashes = 0;
	}

	quoted.append(backslashes * 2, '\\');
	quoted += '"';
	return quoted;
}

std::wstring Process::FullCommand() const {
	std::stringstream ss;
	std::vector<std::string> full = { m_implementation->m_program.string() };
	full.insert(full.end(), m_implementation->m_arguments.begin(), m_implementation->m_arguments.end());
	std::string executable = m_implementation->m_program.filename().string();
	for (char& character: executable)
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	const bool command_processor = executable == "cmd.exe" || executable == "cmd";
	bool command_text = false;
	for (size_t i = 0; i < full.size(); ++i) {
		if (i)
			ss << ' ';
		if (command_processor && i > 0) {
			if (command_text) {
				ss << full[i];
				continue;
			}

			if (full[i] == "/c" || full[i] == "/k") {
				ss << full[i];
				command_text = true;
				continue;
			}

			if (full[i] == "/d" || full[i] == "/s") {
				ss << full[i];
				continue;
			}
		}

		ss << QuoteWindowsArgument(full[i]);
	}

	const std::string narrow = ss.str();
	int wchars_num = MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, NULL, 0);
	std::unique_ptr<wchar_t[]> wstr_buff = std::make_unique<wchar_t[]>(static_cast<size_t>(wchars_num));
	MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, wstr_buff.get(), wchars_num);
	return std::wstring(wstr_buff.get());
}
#endif

namespace StormByte::System {
	const StormByte::Error::Category<enum Process::Error>& process_category() noexcept {
		static StormByte::Error::Category<enum Process::Error> instance;
		return instance;
	}

	std::error_code make_error_code(const enum Process::Error e) noexcept {
		return std::error_code(static_cast<int>(e), process_category());
	}
}
