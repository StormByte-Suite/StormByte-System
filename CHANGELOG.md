# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Summary]

StormByte System is the C++26 process, device and host layer of the StormByte suite.

It depends directly on [StormByte Base 2.0.0](https://github.com/StormByte-Suite/StormByte/releases/tag/2.0.0) or newer. This repository is not Base, Buffer, Config, Crypto, Database, Logger, Multimedia or Network.

Spawn children with piped stdin/stdout/stderr, classify the medium behind a path, resolve directories and the current executable, inspect the machine, name the calling thread, and expand environment strings. POSIX and Windows stay behind one API. Failures are `StormByte::Error::Fault` in a per-type domain (`StormByte.System.*`). Nothing in this module throws. Text that crosses a DLL boundary is `StormByte::Safe::String` / `StormByte::Safe::WString`, not `std::string` by value.

From 2.0.0, original System sources are dual-licensed: GNU Lesser General Public License v3.0 or later, or a commercial license from the copyright holder. That change does not cover other StormByte modules or third-party material under `thirdparty/` (including bundled StormByte Base).

If you landed here from a release link and have not read the tree:

- What this module is, how to build it, and short examples: [README.md](https://github.com/StormByte-Suite/StormByte-System/blob/master/README.md)
- License: dual license LGPL-3.0-or-later or commercial, [LICENSE](https://github.com/StormByte-Suite/StormByte-System/blob/master/LICENSE)

## [Unreleased]

[Unreleased]: https://github.com/StormByte-Suite/StormByte-System/compare/2.0.0...HEAD

## [2.0.0] - 2026-10-10

### Added

- **Device**: classify the medium behind a path (`Kind`, `Access` bitmask, nominal `Throughput`, suggested `Window`).
	- Copyable; stores only the caller accessor as `StormByte::Safe::String`.
	- Probe is on-demand. `operator bool` is probe success, not permission.
	- Errors are `StormByte::System::Device::Error` in domain `StormByte.System.Device`, held as `StormByte::Error::Fault`.
	- `Access`, `Throughput` and `Window` are `MaybeSafe` values.
- **Directory**: current, home, temporary and current-executable directories. `bool` + out `String`; `LastError()` is TLS in this module.
- **File**: `Temporary(prefix, suffix)` creates an empty file the caller unlinks; `CurrentExecutable` is the running image.
- **Host**: name, architecture, CPU brand, OS, kernel, page size, physical/available memory, logical processors, process bitness.
- **ThisThread**: `Sleep`; get/set thread name (`TooLong` if the platform limit is exceeded; the name is not truncated).
- Dual license on original System sources: LGPL-3.0-or-later **or** commercial (`LICENSE` + `COPYING.LGPLv3`).
- Shared vs static follows CMake `BUILD_SHARED_LIBS` (declared in `lib/`, default ON). There is no `STORMBYTE_SYSTEM_SHARED` CMake option. When the library is shared, the compile definition `STORMBYTE_SYSTEM_SHARED` is still set so `visibility.h` can distinguish `dllexport` / `dllimport` / static. Vendored StormByte Base follows the same `BUILD_SHARED_LIBS` mode and is configured with `ENABLE_TEST=OFF`.

### Changed

- **Breaking:** Windows `Process::Pid()` returns only the child process identifier (`DWORD`); process and thread handles remain private to the `Process` owner.
- **Breaking:** Process no longer throws. Spawn, wait and stdin failures are `StormByte::System::Process::Error` in domain `StormByte.System.Process`, held as `Fault()`.
	- `operator bool` is true only while a child is live (`RUNNING` or `SUSPENDED`).
	- Timed `Wait` sets `TimedOut` and leaves the child running. A second wait after a successful reap sets `AlreadyExited`.
	- A failed stdin write sets `BrokenPipe`.
- **Breaking:** `Variable::Expand` has one public entry per width: `std::string_view` and `std::wstring_view`. A literal binds. A `Safe::String` / `Safe::WString` binds through its view conversion. The result is `StormByte::Safe::String`. On Windows, a failed `ExpandEnvironmentStringsW` returns the original text (same as a missing UNIX home).
- **Breaking:** `Process` has one public constructor: UTF-8 `std::string_view` plus `StormByte::Safe::Vector<StormByte::Safe::String>` arguments. A literal binds. A `Safe::String` binds through its view conversion. The view is copied into Base-owned text before the DLL boundary. Native filesystem-path conversion stays inside System, so neither `std::filesystem::path` nor `std::vector` crosses that boundary.
- **Breaking:** `Process::operator<<` has one public text entry, `std::string_view`. A literal and a `Safe::String` bind there. `operator<<(EoF)` still closes stdin.
- Pipe construction and I/O no longer throw. Invalid pipes convert to `false`.
- Public text across a DLL boundary uses `StormByte::Safe::String` / `StormByte::Safe::WString`; borrowed views carry explicit lengths. Native C-string copies are allocated and destroyed inside System, without relying on NUL termination in Base text buffers.
- **Breaking:** System vendors [StormByte Base 2.0.0](https://github.com/StormByte-Suite/StormByte/releases/tag/2.0.0) directly instead of StormByte-String and exposes Base's `StormByte::Safe` owned-text types in its public API. Headers include `StormByte/safe/*.hxx` instead of `StormByte/string/*.hxx`, `StormByte/cstring.hxx` and `StormByte/wcstring.hxx`.
- Visibility macros follow Base/Logger (`EXPORTS` / `STORMBYTE_SYSTEM_SHARED` / static empty).
- Windows Device probe links `iphlpapi` and `ws2_32`. Host CPU name links `advapi32`. macOS Device probe links IOKit and CoreFoundation.
- **Breaking:** `Host::PageSize`, `PhysicalMemory` and `AvailableMemory` return `ByteSize`. They are octet lengths.
- **Breaking:** `Device::Window` is `ByteSize`. `Device::Throughput` stores octets per second as `ByteSize`, not `std::size_t`.
- **Breaking:** `Process::operator>>` and `Stderr` take `StormByte::Safe::String`, not `std::string`. The captured text is owned by Base. `operator<<(std::ostream&, const Process&)` is `STORMBYTE_FORCE_INLINE`, so the stream buffer grows in the caller. `operator>>` stays `String` only: a view cannot own the bytes that were read.
- Process owns its private implementation through `StormByte::Safe::Unique`, allocated and freed on Base's heap.

### Removed

- **Breaking:** `Process` stdin and `Variable::Expand` overloads for Base's removed `CString` / `WCString` types. Use `String`, `WString` or the corresponding length-aware views.
- **Breaking:** `StormByte/system/exception.hxx` (`Exception`, `FileIOError`, `ExecutableNotFound`, `ProcessCreationError`).
- **Breaking:** `StormByte::System::Error` and `StormByte/system/error.hxx` (domain `StormByte.System`). Device, Process, Directory, File, Host and ThisThread keep their own domains.

[2.0.0]: https://github.com/StormByte-Suite/StormByte-System/compare/1.1.0...2.0.0

## [1.1.0] - 2026-09-13

### Changed

- **Public process behavior**
	- Ported System exception messages to the `StormByte::Component` format and added `ProcessCreationError` for process creation failures.
	- Added a public timed `Wait(std::chrono::milliseconds)` overload; the existing untimed overload remains unchanged.
- **Process pipeline internals**
	- Moved forwarding into the internal `Pipe` abstraction while preserving buffered and future output.
	- Moved Process state into the private process implementation header, reducing public-header ABI exposure.
- **Dependencies and build configuration**
	- Updated the StormByte/base dependency to 1.1.0.
	- Switched Windows release optimization handling to CMake interprocedural optimization without duplicate manual compiler/linker flags.

### Fixed

- **Pipe and process lifecycle**
	- Pipe construction now checks platform errors, normalizes UNIX descriptors, and moved pipes invalidate their source endpoints.
	- Pipe reads, writes, polling, EOF handling, and descriptor binding now distinguish interruption, EOF, and failure.
	- Process pipeline forwarding retains pipe ownership independently of Process lifetime, supports safe reconnection, and cancels without cross-thread descriptor closure.
	- Process waiting no longer deadlocks on downstream backpressure; lifecycle joins handle unexpected thread errors without escaping `noexcept` cleanup paths.
	- Direct writes, interrupted waits, and Windows wait failures now preserve error and ownership semantics.
- **Process startup and platform handling**
	- UNIX startup reports `execvp` failures to the parent and throws the appropriate exception eagerly.
	- Windows startup quotes command-line arguments and distinguishes missing executables from other creation failures.
	- Environment expansion handles missing home directories safely, expands only leading `~` paths, and grows Windows buffers as needed.
- **Regression coverage**
	- Added coverage for pipelines, process moves, signal termination, interrupted waits, descriptor reuse, consumer exit, direct write failures, and oversized Windows environment expansion.
- **CI portability**
	- Fixed Windows-only Process implementation initialization.
	- Made UNIX process tests resolve utilities through `PATH` for macOS portability.

[1.1.0]: https://github.com/StormByte-Suite/StormByte-System/releases/tag/1.1.0

## [1.0.0] - 2026-09-05

Initial public release of StormByte-System.

### Added

- **Process**: run external programs with piped stdin / stdout / stderr
	- Move-only ownership; starts on construction
	- `Wait()` for exit code (blocking, no timeout)
	- `Suspend()` / `Resume()`
	- Stream operators: write stdin, read stdout, `<< System::EoF` to close stdin
	- Process chaining (`p1 >> p2`) via background forwarder
	- `Stderr()` to read the stderr pipe
	- Cross-platform (POSIX fork/exec and Windows `CreateProcessW`)
- **Pipe** (internal): anonymous pipes for IPC (UNIX `pipe`/`pipe2`, Windows `CreatePipe`)
	- Atomic chunked writes, bind/dup helpers, handle inheritance flags on Windows
- **Variable**: expand environment strings (Windows `ExpandEnvironmentStrings`; UNIX `~` → home)
- **Exceptions**: `Exception`, `FileIOError`, `ExecutableNotFound`
- Unit tests for Linux, macOS and Windows (pipelines, stdin, exit codes, move)

### Fixed

- Process move no longer double-waits the same child (ownership is fully transferred)
- Failed `execvp` in the child uses `_exit(127)` instead of throwing across `fork`
- Removed unimplemented `Pipe::BindRead(Pipe&)` / `BindWrite(Pipe&)` declarations
- Pipes owned with `std::unique_ptr` instead of raw `new`/`delete`
- `SIGPIPE` ignored once per process (not on every Pipe construction)
- `WriteAtomic` treats empty input as success
- Windows command line built without a trailing space

### Notes

- On UNIX, if the executable cannot be started, the child exits with status **127**; the parent does not throw from the child path.
- `Wait()` has no timeout; it blocks until the process ends.

[1.0.0]: https://github.com/StormByte-Suite/StormByte-System/releases/tag/1.0.0
