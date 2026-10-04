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

#include <StormByte/safe/string.hxx>
#include <StormByte/system/process.hxx>
#include <StormByte/system/variable.hxx>
#include <StormByte/test_handlers.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <StormByte/type_traits/safe.hxx>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef UNIX
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

using StormByte::Safe::String;
using StormByte::System::Process;

static_assert(StormByte::Type::IsSafe<StormByte::Safe::Vector<String>>::value);

namespace {
	std::string Trim(std::string s) {
		auto not_space = [](unsigned char c) { return !std::isspace(c); };
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
		s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
		return s;
	}

	StormByte::Safe::Vector<String> Args(std::initializer_list<const char*> items) {
		StormByte::Safe::Vector<String> out;
		out.reserve(items.size());
		for (const char* item : items)
			out.emplace_back(item);
		return out;
	}

	bool IsProcessError(const Process& proc, const StormByte::System::Process::Error expected) {
		return proc.Fault().code() == make_error_code(expected);
	}
}

#ifdef UNIX
// -------------------
// Basic
// -------------------
int test_basic_execution() {
	const std::string fn = "test_basic_execution";
	Process proc(String("echo"), Args({"Hello, World!"}));
	ASSERT_TRUE(fn, static_cast<bool>(proc));
	ASSERT_FALSE(fn, static_cast<bool>(proc.Fault()));
	String output;
	proc >> output;
	ASSERT_EQUAL(fn, "Hello, World!\n", output);
	ASSERT_EQUAL(fn, 0, proc.Wait());
	ASSERT_FALSE(fn, static_cast<bool>(proc));
	RETURN_TEST(fn, 0);
}

int test_process_pid() {
	const std::string fn = "test_process_pid";
	Process proc("sleep", Args({"1"}));
	ASSERT_TRUE(fn, proc.Pid() > 0);
	ASSERT_EQUAL(fn, 0, proc.Wait());
	ASSERT_EQUAL(fn, -1, proc.Pid());
	RETURN_TEST(fn, 0);
}

int test_process_to_ostream() {
	const std::string fn = "test_process_to_ostream";
	Process proc("echo", Args({"Hello, World!"}));
	std::ostringstream oss;
	oss << proc;
	ASSERT_EQUAL(fn, "Hello, World!\n", oss.str());
	proc.Wait();
	RETURN_TEST(fn, 0);
}

// -------------------
// Errors
// -------------------
int test_missing_executable() {
	const std::string fn = "test_missing_executable";
	Process proc("/no/such/stormbyte-executable");
	ASSERT_FALSE(fn, static_cast<bool>(proc));
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::ExecutableNotFound));
	RETURN_TEST(fn, 0);
}

int test_missing_executable_on_path() {
	const std::string fn = "test_missing_executable_on_path";
	Process proc("stormbyte-no-such-command-xyz");
	ASSERT_FALSE(fn, static_cast<bool>(proc));
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::ExecutableNotFound));
	RETURN_TEST(fn, 0);
}

int test_wait_already_exited() {
	const std::string fn = "test_wait_already_exited";
	Process proc("true");
	ASSERT_EQUAL(fn, 0, proc.Wait());
	ASSERT_FALSE(fn, static_cast<bool>(proc));
	ASSERT_EQUAL(fn, -1, proc.Wait());
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::AlreadyExited));
	RETURN_TEST(fn, 0);
}

int test_wait_after_external_reap() {
	const std::string fn = "test_wait_after_external_reap";
	Process proc("true");
	const pid_t child = proc.Pid();
	ASSERT_TRUE(fn, child > 0);
	ASSERT_EQUAL(fn, child, waitpid(child, nullptr, 0));
	ASSERT_EQUAL(fn, -1, proc.Wait());
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::AlreadyExited));
	RETURN_TEST(fn, 0);
}

int test_wait_timeout_sets_timed_out() {
	const std::string fn = "test_wait_timeout_sets_timed_out";
	Process proc("sleep", Args({"1"}));
	ASSERT_TRUE(fn, static_cast<bool>(proc));
	ASSERT_EQUAL(fn, -1, proc.Wait(std::chrono::milliseconds(10)));
	ASSERT_TRUE(fn, static_cast<bool>(proc));
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::TimedOut));
	ASSERT_EQUAL(fn, 0, proc.Wait());
	ASSERT_FALSE(fn, static_cast<bool>(proc));
	RETURN_TEST(fn, 0);
}

int test_write_after_consumer_exit() {
	const std::string fn = "test_write_after_consumer_exit";
	Process proc("true");
	ASSERT_EQUAL(fn, 0, proc.Wait());
	proc << std::string_view(std::string(4096, 'x'));
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::BrokenPipe));
	RETURN_TEST(fn, 0);
}

// -------------------
// Exit
// -------------------
int test_exit_code_false() {
	const std::string fn = "test_exit_code_false";
	Process proc("false");
	ASSERT_TRUE(fn, proc.Wait() != 0);
	RETURN_TEST(fn, 0);
}

int test_exit_code_true() {
	const std::string fn = "test_exit_code_true";
	Process proc("true");
	ASSERT_EQUAL(fn, 0, proc.Wait());
	RETURN_TEST(fn, 0);
}

int test_signaled_process() {
	const std::string fn = "test_signaled_process";
	Process proc("sh", Args({"-c", "kill -TERM $$"}));
	ASSERT_EQUAL(fn, -1, proc.Wait());
	RETURN_TEST(fn, 0);
}

int test_suspend_resume() {
	const std::string fn = "test_suspend_resume";
	Process proc("sleep", Args({"1"}));
	proc.Suspend();
	ASSERT_TRUE(fn, static_cast<bool>(proc));
	proc.Resume();
	ASSERT_EQUAL(fn, 0, proc.Wait());
	RETURN_TEST(fn, 0);
}

// -------------------
// Move
// -------------------
int test_move_assignment() {
	const std::string fn = "test_move_assignment";
	Process source("echo", Args({"assigned"}));
	Process destination("echo", Args({"discarded"}));
	destination = std::move(source);
	(void)source.Wait();
	String output;
	destination >> output;
	ASSERT_EQUAL(fn, "assigned\n", output);
	ASSERT_EQUAL(fn, 0, destination.Wait());
	RETURN_TEST(fn, 0);
}

int test_move_process() {
	const std::string fn = "test_move_process";
	Process original("echo", Args({"moved"}));
	Process moved(std::move(original));
	ASSERT_FALSE(fn, static_cast<bool>(original));
	ASSERT_TRUE(fn, IsProcessError(original, StormByte::System::Process::Error::NotRunning));
	ASSERT_TRUE(fn, static_cast<bool>(moved));
	(void)original.Wait();
	String output;
	moved >> output;
	ASSERT_EQUAL(fn, "moved\n", output);
	ASSERT_EQUAL(fn, 0, moved.Wait());
	RETURN_TEST(fn, 0);
}

// -------------------
// Pipeline
// -------------------
int test_pipeline_accumulated_and_future_output() {
	const std::string fn = "test_pipeline_accumulated_and_future_output";
	Process producer("cat");
	Process consumer("cat");
	producer << "before\n";
	producer >> consumer;
	producer << "after\n";
	producer << StormByte::System::EoF;
	String output;
	consumer >> output;
	ASSERT_EQUAL(fn, "before\nafter\n", output);
	producer.Wait();
	consumer.Wait();
	RETURN_TEST(fn, 0);
}

int test_pipeline_destination_exits_first() {
	const std::string fn = "test_pipeline_destination_exits_first";
	Process producer("yes");
	{
		Process consumer("head", Args({"-c", "1"}));
		producer >> consumer;
		String output;
		consumer >> output;
		ASSERT_EQUAL(fn, 1u, output.size());
	}

	producer.Wait();
	RETURN_TEST(fn, 0);
}

int test_pipeline_echo_sort_wc() {
	const std::string fn = "test_pipeline_echo_sort_wc";
	Process proc1("printf", Args({"%s", "orange\nbanana\napple\ncherry\nbanana\napple\n"}));
	Process proc2("sort");
	Process proc3("uniq");
	Process proc4("wc", Args({"-l"}));
	proc1 >> proc2 >> proc3 >> proc4;
	String output;
	proc4 >> output;
	ASSERT_EQUAL(fn, "4", Trim(std::string(output)));
	proc1.Wait();
	proc2.Wait();
	proc3.Wait();
	proc4.Wait();
	RETURN_TEST(fn, 0);
}

int test_pipeline_execution() {
	const std::string fn = "test_pipeline_execution";
	Process proc1("printf", Args({"%s", "Hello\n"}));
	Process proc2("wc", Args({"-c"}));
	proc1 >> proc2;
	String output;
	proc2 >> output;
	ASSERT_EQUAL(fn, "6", Trim(std::string(output)));
	proc1.Wait();
	proc2.Wait();
	RETURN_TEST(fn, 0);
}

int test_pipeline_find_sort_wc() {
	const std::string fn = "test_pipeline_find_sort_wc";
	Process proc1("printf", Args({"%s", "apple\nbanana\ncherry\napple\nbanana\ncherry\n"}));
	Process proc2("grep", Args({"apple"}));
	Process proc3("sort");
	Process proc4("wc", Args({"-l"}));
	proc1 >> proc2 >> proc3 >> proc4;
	String output;
	proc4 >> output;
	ASSERT_EQUAL(fn, "2", Trim(std::string(output)));
	proc1.Wait();
	proc2.Wait();
	proc3.Wait();
	proc4.Wait();
	RETURN_TEST(fn, 0);
}

int test_pipeline_reconnect() {
	const std::string fn = "test_pipeline_reconnect";
	Process producer("cat");
	Process first_consumer("cat");
	Process second_consumer("cat");
	producer >> first_consumer;
	producer << "before\n";
	producer >> second_consumer;
	producer << "after\n";
	producer << StormByte::System::EoF;
	String output;
	second_consumer >> output;
	ASSERT_TRUE(fn, output.ends_with("after\n"));
	producer.Wait();
	first_consumer.Wait();
	second_consumer.Wait();
	RETURN_TEST(fn, 0);
}

int test_pipeline_reconnect_long_lived() {
	const std::string fn = "test_pipeline_reconnect_long_lived";
	Process producer("sleep", Args({"1"}));
	Process first_consumer("cat");
	Process second_consumer("cat");
	producer >> first_consumer;
	producer >> second_consumer;
	ASSERT_EQUAL(fn, 0, producer.Wait());
	ASSERT_EQUAL(fn, 0, first_consumer.Wait());
	ASSERT_EQUAL(fn, 0, second_consumer.Wait());
	RETURN_TEST(fn, 0);
}

int test_pipeline_sort() {
	const std::string fn = "test_pipeline_sort";
	Process proc1("printf", Args({"%s", "banana\napple\ncherry\n"}));
	Process proc2("sort");
	proc1 >> proc2;
	String output;
	proc2 >> output;
	ASSERT_EQUAL(fn, "apple\nbanana\ncherry\n", output);
	proc1.Wait();
	proc2.Wait();
	RETURN_TEST(fn, 0);
}

int test_tr_pipeline() {
	const std::string fn = "test_tr_pipeline";
	Process proc1("printf", Args({"%s", "abc"}));
	Process proc2("tr", Args({"a-z", "A-Z"}));
	proc1 >> proc2;
	String output;
	proc2 >> output;
	ASSERT_EQUAL(fn, "ABC", output);
	proc1.Wait();
	proc2.Wait();
	RETURN_TEST(fn, 0);
}

// -------------------
// Stderr
// -------------------
int test_stderr_capture() {
	const std::string fn = "test_stderr_capture";
	Process proc("sh", Args({"-c", "printf '%s' 'err-msg' 1>&2"}));
	String err;
	proc.Stderr(err);
	ASSERT_EQUAL(fn, "err-msg", err);
	ASSERT_EQUAL(fn, 0, proc.Wait());
	RETURN_TEST(fn, 0);
}

// -------------------
// Stdin
// -------------------
int test_stdin_roundtrip() {
	const std::string fn = "test_stdin_roundtrip";
	Process proc("cat");
	proc << "line-one\n";
	proc << "line-two\n";
	proc << StormByte::System::EoF;
	String output;
	proc >> output;
	ASSERT_EQUAL(fn, "line-one\nline-two\n", output);
	ASSERT_EQUAL(fn, 0, proc.Wait());
	RETURN_TEST(fn, 0);
}

// -------------------
// Variable
// -------------------
int test_variable_expansion() {
	const std::string fn = "test_variable_expansion";
	ASSERT_EQUAL(fn, "foo~bar", std::string(StormByte::System::Variable::Expand("foo~bar")));
	const char* home = std::getenv("HOME");
	if (home != nullptr && *home != '\0') {
		ASSERT_EQUAL(fn, std::string(home), std::string(StormByte::System::Variable::Expand("~")));
		ASSERT_EQUAL(fn, std::string(home) + "/a", std::string(StormByte::System::Variable::Expand("~/a")));
	}

	RETURN_TEST(fn, 0);
}

// -------------------
// Wait
// -------------------
volatile sig_atomic_t wait_interrupt_signal = 0;

void wait_interrupt_handler(int) {
	wait_interrupt_signal = 1;
}

int test_standard_descriptor_reuse() {
	const std::string fn = "test_standard_descriptor_reuse";
	const int saved_stdin = dup(STDIN_FILENO);
	const int saved_stdout = dup(STDOUT_FILENO);
	const int saved_stderr = dup(STDERR_FILENO);
	if (saved_stdin == -1 || saved_stdout == -1 || saved_stderr == -1)
		return 1;
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	String output;
	int result = 0;
	{
		Process proc("echo", Args({"descriptor-safe"}));
		if (!proc)
			result = -1;
		else {
			proc >> output;
			result = proc.Wait();
		}
	}

	dup2(saved_stdin, STDIN_FILENO);
	dup2(saved_stdout, STDOUT_FILENO);
	dup2(saved_stderr, STDERR_FILENO);
	close(saved_stdin);
	close(saved_stdout);
	close(saved_stderr);
	ASSERT_EQUAL(fn, 0, result);
	ASSERT_EQUAL(fn, "descriptor-safe\n", output);
	RETURN_TEST(fn, 0);
}

int test_wait_interrupted_by_signal() {
	const std::string fn = "test_wait_interrupted_by_signal";
	struct sigaction action{};
	action.sa_handler = wait_interrupt_handler;
	sigemptyset(&action.sa_mask);
	struct sigaction previous{};
	sigaction(SIGUSR1, &action, &previous);
	const pthread_t main_thread = pthread_self();
	Process proc("sleep", Args({"1"}));
	std::thread interrupter([main_thread] {
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
		pthread_kill(main_thread, SIGUSR1);
	});
	const int exit_code = proc.Wait(std::chrono::milliseconds(1500));
	interrupter.join();
	sigaction(SIGUSR1, &previous, nullptr);
	ASSERT_EQUAL(fn, 1, wait_interrupt_signal);
	ASSERT_EQUAL(fn, 0, exit_code);
	RETURN_TEST(fn, 0);
}

int test_wait_timeout() {
	const std::string fn = "test_wait_timeout";
	Process proc("sleep", Args({"1"}));
	ASSERT_EQUAL(fn, -1, proc.Wait(std::chrono::milliseconds(10)));
	ASSERT_EQUAL(fn, 0, proc.Wait());
	RETURN_TEST(fn, 0);
}

int test_wait_with_undrained_pipeline() {
	const std::string fn = "test_wait_with_undrained_pipeline";
	Process producer("dd", Args({"if=/dev/zero", "bs=1048576", "count=16"}));
	Process consumer("cat");
	producer >> consumer;
	(void)producer.Wait();
	(void)consumer.Wait();
	RETURN_TEST(fn, 0);
}

#elifdef WINDOWS
// -------------------
// Basic
// -------------------
int test_basic_execution_windows() {
	const std::string fn = "test_basic_execution_windows";
	Process proc("cmd.exe", Args({"/d", "/c", "echo Hello, World!"}));
	ASSERT_TRUE(fn, static_cast<bool>(proc));
	String output;
	proc >> output;
	ASSERT_EQUAL(fn, "Hello, World!", Trim(std::string(output)));
	ASSERT_EQUAL(fn, 0u, proc.Wait());
	ASSERT_FALSE(fn, static_cast<bool>(proc));
	RETURN_TEST(fn, 0);
}

int test_process_pid_windows() {
	const std::string fn = "test_process_pid_windows";
	Process proc("cmd.exe", Args({"/d", "/c", "exit /b 0"}));
	ASSERT_TRUE(fn, proc.Pid() != 0);
	ASSERT_EQUAL(fn, 0u, proc.Wait());
	ASSERT_EQUAL(fn, 0u, proc.Pid());
	RETURN_TEST(fn, 0);
}

int test_dir_lists_something() {
	const std::string fn = "test_dir_lists_something";
	Process proc("cmd.exe", Args({"/d", "/c", "dir /b"}));
	String output;
	proc >> output;
	ASSERT_FALSE(fn, Trim(std::string(output)).empty());
	ASSERT_EQUAL(fn, 0u, proc.Wait());
	RETURN_TEST(fn, 0);
}

int test_windows_argument_with_quotes() {
	const std::string fn = "test_windows_argument_with_quotes";
	Process proc("cmd.exe", Args({"/d", "/c", "echo hello \"world\""}));
	String output;
	proc >> output;
	ASSERT_EQUAL(fn, "hello \"world\"", Trim(std::string(output)));
	ASSERT_EQUAL(fn, 0u, proc.Wait());
	RETURN_TEST(fn, 0);
}

int test_windows_argument_with_space() {
	const std::string fn = "test_windows_argument_with_space";
	Process proc("cmd.exe", Args({"/d", "/c", "echo hello world"}));
	String output;
	proc >> output;
	ASSERT_EQUAL(fn, "hello world", Trim(std::string(output)));
	ASSERT_EQUAL(fn, 0u, proc.Wait());
	RETURN_TEST(fn, 0);
}

// -------------------
// Errors
// -------------------
int test_missing_executable_windows() {
	const std::string fn = "test_missing_executable_windows";
	Process proc("C:\\no\\such\\stormbyte-executable.exe");
	ASSERT_FALSE(fn, static_cast<bool>(proc));
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::ExecutableNotFound));
	RETURN_TEST(fn, 0);
}

int test_wait_already_exited_windows() {
	const std::string fn = "test_wait_already_exited_windows";
	Process proc("cmd.exe", Args({"/d", "/c", "exit /b 0"}));
	ASSERT_EQUAL(fn, 0u, proc.Wait());
	ASSERT_EQUAL(fn, static_cast<DWORD>(-1), proc.Wait());
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::AlreadyExited));
	RETURN_TEST(fn, 0);
}

int test_wait_timeout_windows() {
	const std::string fn = "test_wait_timeout_windows";
	Process proc("cmd.exe", Args({"/d", "/c", "timeout /t 2 /nobreak >NUL"}));
	ASSERT_EQUAL(fn, static_cast<DWORD>(-1), proc.Wait(std::chrono::milliseconds(10)));
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::TimedOut));
	(void)proc.Wait();
	RETURN_TEST(fn, 0);
}

int test_write_after_consumer_exit_windows() {
	const std::string fn = "test_write_after_consumer_exit_windows";
	Process proc("cmd.exe", Args({"/d", "/c", "exit /b 0"}));
	ASSERT_EQUAL(fn, 0u, proc.Wait());
	proc << std::string_view(std::string(4096, 'x'));
	ASSERT_TRUE(fn, IsProcessError(proc, StormByte::System::Process::Error::BrokenPipe));
	RETURN_TEST(fn, 0);
}

// -------------------
// Exit
// -------------------
int test_exit_code_windows() {
	const std::string fn = "test_exit_code_windows";
	Process proc("cmd.exe", Args({"/d", "/c", "exit /b 7"}));
	ASSERT_EQUAL(fn, 7u, proc.Wait());
	RETURN_TEST(fn, 0);
}

int test_suspend_resume_windows() {
	const std::string fn = "test_suspend_resume_windows";
	Process proc("cmd.exe", Args({"/d", "/c", "timeout /t 2 /nobreak >NUL"}));
	proc.Suspend();
	ASSERT_TRUE(fn, static_cast<bool>(proc));
	proc.Resume();
	ASSERT_EQUAL(fn, 0u, proc.Wait());
	RETURN_TEST(fn, 0);
}

// -------------------
// Move
// -------------------
int test_move_process_windows() {
	const std::string fn = "test_move_process_windows";
	Process original("cmd.exe", Args({"/d", "/c", "echo moved"}));
	Process moved(std::move(original));
	(void)original.Wait();
	ASSERT_TRUE(fn, IsProcessError(original, StormByte::System::Process::Error::NotRunning));
	String output;
	moved >> output;
	ASSERT_EQUAL(fn, "moved", Trim(std::string(output)));
	ASSERT_EQUAL(fn, 0u, moved.Wait());
	RETURN_TEST(fn, 0);
}

// -------------------
// Stdin
// -------------------
int test_stdin_roundtrip_windows() {
	const std::string fn = "test_stdin_roundtrip_windows";
	Process proc("sort.exe");
	proc << "b\r\n";
	proc << "a\r\n";
	proc << StormByte::System::EoF;
	String output;
	proc >> output;
	std::string normalized;
	normalized.reserve(output.size());
	for (size_t i = 0; i < output.size(); ++i) {
		if (output[i] == '\r')
			continue;
		normalized.push_back(output[i]);
	}

	ASSERT_EQUAL(fn, "a\nb\n", normalized);
	ASSERT_EQUAL(fn, 0u, proc.Wait());
	RETURN_TEST(fn, 0);
}

// -------------------
// Variable
// -------------------
int test_windows_long_environment_expansion() {
	const std::string fn = "test_windows_long_environment_expansion";
	const std::string value = "0123456789";
	_putenv_s("STORMBYTE_LONG_ENV", value.c_str());
	std::wstring input;
	for (size_t i = 0; i < 4000; ++i)
		input += L"%STORMBYTE_LONG_ENV%";
	const String expanded = StormByte::System::Variable::Expand(std::wstring_view(input));
	_putenv_s("STORMBYTE_LONG_ENV", "");
	ASSERT_EQUAL(fn, 40000u, expanded.size());
	RETURN_TEST(fn, 0);
}

#endif

int main() {
	int result = 0;
#ifdef UNIX
	// -------------------
	// Basic
	// -------------------
	result += test_basic_execution();
	result += test_process_pid();
	result += test_process_to_ostream();

	// -------------------
	// Errors
	// -------------------
	result += test_missing_executable();
	result += test_missing_executable_on_path();
	result += test_wait_already_exited();
	result += test_wait_after_external_reap();
	result += test_wait_timeout_sets_timed_out();
	result += test_write_after_consumer_exit();

	// -------------------
	// Exit
	// -------------------
	result += test_exit_code_false();
	result += test_exit_code_true();
	result += test_signaled_process();
	result += test_suspend_resume();

	// -------------------
	// Move
	// -------------------
	result += test_move_assignment();
	result += test_move_process();

	// -------------------
	// Pipeline
	// -------------------
	result += test_pipeline_accumulated_and_future_output();
	result += test_pipeline_destination_exits_first();
	result += test_pipeline_echo_sort_wc();
	result += test_pipeline_execution();
	result += test_pipeline_find_sort_wc();
	result += test_pipeline_reconnect();
	result += test_pipeline_reconnect_long_lived();
	result += test_pipeline_sort();
	result += test_tr_pipeline();

	// -------------------
	// Stderr
	// -------------------
	result += test_stderr_capture();

	// -------------------
	// Stdin
	// -------------------
	result += test_stdin_roundtrip();

	// -------------------
	// Variable
	// -------------------
	result += test_variable_expansion();

	// -------------------
	// Wait
	// -------------------
	result += test_standard_descriptor_reuse();
	result += test_wait_interrupted_by_signal();
	result += test_wait_timeout();
	result += test_wait_with_undrained_pipeline();
#elifdef WINDOWS
	// -------------------
	// Basic
	// -------------------
	result += test_basic_execution_windows();
	result += test_process_pid_windows();
	result += test_dir_lists_something();
	result += test_windows_argument_with_quotes();
	result += test_windows_argument_with_space();

	// -------------------
	// Errors
	// -------------------
	result += test_missing_executable_windows();
	result += test_wait_already_exited_windows();
	result += test_wait_timeout_windows();
	result += test_write_after_consumer_exit_windows();

	// -------------------
	// Exit
	// -------------------
	result += test_exit_code_windows();
	result += test_suspend_resume_windows();

	// -------------------
	// Move
	// -------------------
	result += test_move_process_windows();

	// -------------------
	// Stdin
	// -------------------
	result += test_stdin_roundtrip_windows();

	// -------------------
	// Variable
	// -------------------
	result += test_windows_long_environment_expansion();
#endif
	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
