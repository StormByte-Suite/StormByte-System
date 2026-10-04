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

#include <StormByte/system/pipe.hxx>

#include <cerrno>
#ifdef UNIX
#include <fcntl.h>
#include <limits.h>
#include <mutex>
#include <signal.h>
#include <unistd.h>
#endif

using namespace StormByte::System;

#ifdef WINDOWS
SECURITY_ATTRIBUTES Pipe::m_sAttr = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
#endif

Pipe::Pipe() noexcept:
#ifdef WINDOWS
	m_fd{ INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE } {
#else
	m_fd{ -1, -1 } {
#endif
	#ifdef UNIX
	static std::once_flag sigpipe_once;
	std::call_once(sigpipe_once, [] {
		signal(SIGPIPE, SIG_IGN);
	});
	int result;
	#ifdef LINUX
	result = pipe2(m_fd, O_CLOEXEC);
	#else
	result = pipe(m_fd);
	if (result == 0 && (fcntl(m_fd[0], F_SETFD, FD_CLOEXEC) == -1 || fcntl(m_fd[1], F_SETFD, FD_CLOEXEC) == -1))
		result = -1;
	#endif
	if (result == 0) {
		for (int& fd: m_fd) {
			if (fd >= STDIN_FILENO + 3)
				continue;
			const int duplicate = fcntl(fd, F_DUPFD, STDIN_FILENO + 3);
			if (duplicate == -1 || fcntl(duplicate, F_SETFD, FD_CLOEXEC) == -1) {
				if (duplicate != -1)
					close(duplicate);
				result = -1;
				break;
			}

			close(fd);
			fd = duplicate;
		}
	}

	if (result == -1) {
		CloseRead();
		CloseWrite();
	}
	#else
	if (!CreatePipe(&m_fd[0], &m_fd[1], &m_sAttr, 0)) {
		m_fd[0] = INVALID_HANDLE_VALUE;
		m_fd[1] = INVALID_HANDLE_VALUE;
	}
	#endif
}

Pipe::Pipe(Pipe&& pipe) noexcept:
#ifdef WINDOWS
	m_fd{ pipe.m_fd[0], pipe.m_fd[1] } {
	pipe.m_fd[0] = INVALID_HANDLE_VALUE;
	pipe.m_fd[1] = INVALID_HANDLE_VALUE;
#else
	m_fd{ pipe.m_fd[0], pipe.m_fd[1] } {
	pipe.m_fd[0] = -1;
	pipe.m_fd[1] = -1;
#endif
}

Pipe& Pipe::operator=(Pipe&& pipe) noexcept {
	if (this == &pipe)
		return *this;
	CloseRead();
	CloseWrite();
	m_fd[0] = pipe.m_fd[0];
	m_fd[1] = pipe.m_fd[1];
#ifdef WINDOWS
	pipe.m_fd[0] = INVALID_HANDLE_VALUE;
	pipe.m_fd[1] = INVALID_HANDLE_VALUE;
#else
	pipe.m_fd[0] = -1;
	pipe.m_fd[1] = -1;
#endif
	return *this;
}

Pipe::~Pipe() noexcept {
	CloseRead();
	CloseWrite();
}

Pipe::operator bool() const noexcept {
#ifdef WINDOWS
	return m_fd[0] != INVALID_HANDLE_VALUE && m_fd[1] != INVALID_HANDLE_VALUE;
#else
	return m_fd[0] != -1 && m_fd[1] != -1;
#endif
}

#ifdef UNIX
bool Pipe::BindRead(int dest) noexcept {
	return Bind(m_fd[0], dest);
}

bool Pipe::BindWrite(int dest) noexcept {
	return Bind(m_fd[1], dest);
}

ssize_t Pipe::Write(std::string_view data) {
	return write(m_fd[1], data.data(), data.size());
}

bool Pipe::WriteEOF() const {
	pollfd poll_data;
	poll_data.fd = m_fd[1];
	poll_data.events = POLLOUT;
	int result;
	do {
		result = poll(&poll_data, 1, -1);
	} while (result == -1 && errno == EINTR);
	if (result == -1 || result == 0)
		return false;
	return !((poll_data.revents & POLLOUT) == POLLOUT) || ((poll_data.revents & POLLERR) == POLLERR);
}

ssize_t Pipe::Read(StormByte::BinaryData& buffer, ssize_t bytes) const {
	return read(m_fd[0], buffer.data(), static_cast<size_t>(bytes));
}

bool Pipe::ReadEOF() const {
	pollfd poll_data;
	poll_data.fd = m_fd[0];
	poll_data.events = POLLIN;
	int result;
	do {
		result = poll(&poll_data, 1, -1);
	} while (result == -1 && errno == EINTR);
	if (result == -1 || result == 0)
		return false;
	return ((poll_data.revents & POLLHUP) == POLLHUP) || ((poll_data.revents & POLLERR) == POLLERR);
}

#else
void Pipe::ReadHandleInformation(DWORD mask, DWORD flags) {
	HandleInformation(m_fd[0], mask, flags);
}

void Pipe::WriteHandleInformation(DWORD mask, DWORD flags) {
	HandleInformation(m_fd[1], mask, flags);
}

HANDLE Pipe::ReadHandle() const {
	return m_fd[0];
}

HANDLE Pipe::WriteHandle() const {
	return m_fd[1];
}

DWORD Pipe::Write(std::string_view data) {
	DWORD dwWritten = 0;
	SetLastError(ERROR_SUCCESS);
	WriteFile(m_fd[1], data.data(), static_cast<DWORD>(data.size()), &dwWritten, NULL);
	return dwWritten;
}

DWORD Pipe::Read(StormByte::BinaryData& buffer, DWORD size) const {
	DWORD dwRead = 0;
	SetLastError(ERROR_SUCCESS);
	ReadFile(m_fd[0], buffer.data(), size, &dwRead, NULL);
	return dwRead;
}

#endif
#ifdef UNIX
bool Pipe::WaitReadable(const std::shared_ptr<std::atomic_bool>& cancelled) const {
	pollfd poll_data{ m_fd[0], POLLIN, 0 };
	while (!cancelled || !cancelled->load()) {
		int result;
		do {
			result = poll(&poll_data, 1, 10);
		} while (result == -1 && errno == EINTR);
		if (result > 0)
			return (poll_data.revents & (POLLIN | POLLHUP)) != 0;
		if (result == -1 && errno != EINTR)
			return false;
	}

	return false;
}

bool Pipe::WriteAtomic(std::string_view data, const std::shared_ptr<std::atomic_bool>& cancelled) {
	std::string_view out(data);
	if (out.empty())
		return true;
	do {
		if (cancelled && cancelled->load())
			return false;
		const size_t chunk_size = (out.length() > static_cast<size_t>(PIPE_BUF)) ? static_cast<size_t>(PIPE_BUF) : out.length();
		pollfd poll_data{ m_fd[1], POLLOUT, 0 };
		int poll_result;
		do {
			poll_result = poll(&poll_data, 1, 10);
		} while (poll_result == -1 && errno == EINTR);
		if (poll_result == -1 || (poll_result > 0 && (poll_data.revents & (POLLERR | POLLHUP | POLLNVAL))))
			return false;
		if (poll_result == 0)
			continue;
		const ssize_t bytes_written = ::write(m_fd[1], out.data(), chunk_size);
		if (bytes_written < 0 && errno == EINTR)
			continue;
		if (bytes_written < 0 || static_cast<size_t>(bytes_written) != chunk_size)
			return false;

		out.remove_prefix(chunk_size);
	} while (!out.empty());
	return out.empty();
}

#else
bool Pipe::WaitReadable(const std::shared_ptr<std::atomic_bool>& cancelled) const {
	DWORD available = 0;
	while (!cancelled || !cancelled->load()) {
		if (!PeekNamedPipe(m_fd[0], nullptr, 0, nullptr, &available, nullptr))
			return GetLastError() == ERROR_BROKEN_PIPE;
		if (available > 0)
			return true;
		Sleep(10);
	}

	return false;
}

bool Pipe::WriteAtomic(std::string_view data, const std::shared_ptr<std::atomic_bool>& cancelled) {
	std::string_view out(data);
	if (out.empty())
		return true;
	do {
		if (cancelled && cancelled->load())
			return false;
		const size_t chunk_size = (out.length() > 4096) ? 4096 : out.length();
		DWORD dwWritten = 0;
		SetLastError(ERROR_SUCCESS);
		if (!WriteFile(m_fd[1], out.data(), static_cast<DWORD>(chunk_size), &dwWritten, NULL) ||
			dwWritten != static_cast<DWORD>(chunk_size)) {
			return false;
		}

		out.remove_prefix(chunk_size);
	} while (!out.empty());
	return out.empty();
}

#endif
void Pipe::CloseRead() noexcept {
	Close(m_fd[0]);
}

void Pipe::CloseWrite() noexcept {
	Close(m_fd[1]);
}

bool Pipe::operator<<(std::string_view data) {
	return WriteAtomic(data);
}

bool Pipe::WriteAtomic(StormByte::Safe::String&& data, const std::shared_ptr<std::atomic_bool>& cancelled) {
	return WriteAtomic(static_cast<std::string_view>(data), cancelled);
}

std::unique_ptr<std::thread> Pipe::Connect(std::shared_ptr<Pipe> source, std::shared_ptr<Pipe> destination, const std::shared_ptr<std::atomic_bool>& cancelled, std::function<void()> on_failure) {
	return std::make_unique<std::thread>([source = std::move(source), destination = std::move(destination), cancelled, on_failure = std::move(on_failure)] {
		bool forwarding = true;
		try {
#ifdef UNIX
			StormByte::BinaryData buffer;
			buffer.resize(StormByte::ByteSize{ MAX_READ_BYTES });
			ssize_t bytes_read;
			while (forwarding) {
				if (!source->WaitReadable(cancelled))
					break;
				bytes_read = source->Read(buffer, static_cast<ssize_t>(MAX_READ_BYTES));
				if (bytes_read > 0)
					forwarding = destination->WriteAtomic(std::string_view(reinterpret_cast<const char*>(buffer.data()), static_cast<size_t>(bytes_read)), cancelled);
				else if (bytes_read == 0)
					break;
				else if (errno != EINTR)
					forwarding = false;
			}
#else
			StormByte::BinaryData buffer;
			buffer.resize(StormByte::ByteSize{ MAX_READ_BYTES });
			DWORD bytes_read;
			while (forwarding) {
				if (!source->WaitReadable(cancelled))
					break;
				bytes_read = source->Read(buffer, static_cast<DWORD>(MAX_READ_BYTES));
				if (bytes_read > 0)
					forwarding = destination->WriteAtomic(std::string_view(reinterpret_cast<const char*>(buffer.data()), static_cast<size_t>(bytes_read)), cancelled);
				else if (GetLastError() != ERROR_SUCCESS && GetLastError() != ERROR_BROKEN_PIPE)
					forwarding = false;
				else
					break;
			}
#endif
		} catch (...) {
			forwarding = false;
		}

		if (!forwarding && (!cancelled || !cancelled->load()) && on_failure) {
			try {
				on_failure();
			} catch (...) {}
		}
		destination->CloseWrite();
	});
}

StormByte::Safe::String& Pipe::operator>>(StormByte::Safe::String& out) const {
	#ifdef UNIX
	ssize_t bytes;
	#else
	DWORD bytes;
	#endif
	StormByte::BinaryData buffer;
	std::string raw;
	buffer.resize(StormByte::ByteSize{ MAX_READ_BYTES });
	while (true) {
		#ifdef UNIX
		bytes = Read(buffer, static_cast<ssize_t>(MAX_READ_BYTES));
		#else
		bytes = Read(buffer, static_cast<DWORD>(MAX_READ_BYTES));
		#endif
		if (bytes > 0)
			raw.append(reinterpret_cast<const char*>(buffer.data()), static_cast<size_t>(bytes));
		else
			break;
	}

	if (!raw.empty()) {
		if (!out.empty()) {
			std::string merged(static_cast<std::string_view>(out));
			merged.append(raw);
			raw = std::move(merged);
		}
		out = StormByte::Safe::String(std::string_view(raw));
	}
	return out;
}

#ifdef UNIX
bool Pipe::Bind(int& src, int dest) noexcept {
	if (dup2(src, dest) == -1)
		return false;
	if (src == dest) {
		src = -1;
		return true;
	}

	close(src);
	src = -1;
	return true;
}

void Pipe::Close(int& fd) noexcept {
	if (fd == -1)
		return;
	close(fd);
	fd = -1;
}

#else
void Pipe::Close(HANDLE& fd) noexcept {
	if (fd == INVALID_HANDLE_VALUE)
		return;
	CloseHandle(fd);
	fd = INVALID_HANDLE_VALUE;
}

void Pipe::HandleInformation(HANDLE handle, DWORD mask, DWORD flags) {
	SetHandleInformation(handle, mask, flags);
}
#endif
