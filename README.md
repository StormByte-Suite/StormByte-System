# StormByte-System

![Multiplatform](https://img.shields.io/badge/Linux%20%7C%20Windows%20%7C%20macOS-Supported-1793D1)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.28+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3 or commercial](https://img.shields.io/badge/License-LGPL--3.0--or--later%20OR%20Commercial-blue.svg)
[![CI](https://github.com/StormByte-Suite/StormByte-System/actions/workflows/ci.yml/badge.svg)](https://github.com/StormByte-Suite/StormByte-System/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-GitHub-ea4aaa?logo=github-sponsors&logoColor=white)](https://github.com/sponsors/StormBytePP)

StormByte-System is the C++26 process, device and host module of the [StormByte](https://suite.stormbyte.org/StormByte) suite.

Spawn children with piped stdin/stdout/stderr, classify the medium behind a path, resolve directories and the current executable, inspect the machine, name the calling thread, and expand environment strings. POSIX and Windows stay behind one API. Failures are `StormByte::Error::Fault` in a per-type domain (`StormByte.System.*`). Nothing in this module throws.

## Table of Contents

- [Repository](#repository)
- [Installation](#installation)
- [Why StormByte-System](#why-stormbyte-system)
- [Features](#features)
- [Dependencies](#dependencies)
- [The rest of the suite](#the-rest-of-the-suite)
- [Public API](#public-api)
- [Examples](#examples)
	- [Run a process](#run-a-process)
	- [Pipe two processes](#pipe-two-processes)
	- [Classify a device](#classify-a-device)
	- [Directories and files](#directories-and-files)
	- [Host](#host)
	- [Calling thread](#calling-thread)
	- [Expand variables](#expand-variables)
- [Design notes](#design-notes)
- [Testing](#testing)
- [Contributing](#contributing)
- [License](#license)
- [Support](#support)

## Repository

- [StormByte-System](https://github.com/StormByte-Suite/StormByte-System)

## Installation

```bash
git clone --recursive https://github.com/StormByte-Suite/StormByte-System.git
cd StormByte-System
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build
```

Shared vs static follows CMake `BUILD_SHARED_LIBS` (declared in `lib/`, default ON). A plain configure builds the shared library. `-DBUILD_SHARED_LIBS=OFF` builds a static archive; on Windows the headers then do not use `dllimport`. Vendored StormByte Base follows the same mode.

A shared build keeps this library as its own `.so` / `.dll`. Under the LGPL that is usually the simpler way to ship: the user can replace that file. A static archive is folded into your binary. The LGPL still applies to this code; you must give the recipient a way to relink your product with a different build of this library. If that does not fit how you distribute the final product, a commercial license is available from the copyright holder (see [License](#license)).

## Why StormByte-System

| Goal | How it is achieved |
|------|--------------------|
| **One process API** | `Process` starts on construct; pipes are private. Errors are `Fault()`, not exceptions. |
| **Shell-like chaining** | `p1 >> p2` forwards stdout to stdin on a worker thread. |
| **stdin control** | `<<` writes a `string_view`; `<< System::EoF` closes the write end. |
| **Medium behind a path** | `Device` probes Kind, Access, nominal Throughput and a suggested Window on demand. |
| **This process on disk** | `Directory` and `File` resolve cwd, home, temp and the running image. |
| **This machine** | `Host` reports name, OS, kernel, CPU, ISA, RAM and bitness. |
| **This thread** | `ThisThread::Sleep` and `Name` (reject, do not truncate, if too long). |
| **Environment text** | `Variable::Expand` (`%VAR%` on Windows, `~` on UNIX). |

## Features

- Move-only `Process` (fork/exec or `CreateProcessW`)
- Piped stdin, stdout, stderr; chaining; `Wait` / timed `Wait`; `Suspend` / `Resume`
- `Device` classification (HDD, SSD, NVMe generations, USB, Network)
- `Directory` / `File` locations of this process
- `Host` identity and capacity
- `ThisThread` sleep and name
- `Variable::Expand`
- Private `Pipe` (`pipe2` / `CreatePipe`)
- Dual license on original sources

## Dependencies

| Dependency | Role |
|------------|------|
| [StormByte Base 2.0.0](https://github.com/StormByte-Suite/StormByte/releases/tag/2.0.0) | Error/Fault, visibility, `ByteSize`, safe text across the DLL boundary |

## The rest of the suite

| Module | Role | API |
| --- | --- | --- |
| [Base](https://github.com/StormByte-Suite/StormByte) | Exceptions, Expected, serialization, UUID, concepts, safe text and pointers | [/StormByte](https://suite.stormbyte.org/StormByte) |
| [Buffer](https://github.com/StormByte-Suite/StormByte-Buffer) | FIFO, SharedFIFO, Ring, Producer/Consumer and multi-stage pipelines | [/StormByte-Buffer](https://suite.stormbyte.org/StormByte-Buffer) |
| [Config](https://github.com/StormByte-Suite/StormByte-Config) | Human-readable text and versioned binary documents | [/StormByte-Config](https://suite.stormbyte.org/StormByte-Config) |
| [Crypto](https://github.com/StormByte-Suite/StormByte-Crypto) | Hash, compress, encrypt, sign and key agreement | [/StormByte-Crypto](https://suite.stormbyte.org/StormByte-Crypto) |
| [Database](https://github.com/StormByte-Suite/StormByte-Database) | One API over SQLite, PostgreSQL and MariaDB | [/StormByte-Database](https://suite.stormbyte.org/StormByte-Database) |
| [Logger](https://github.com/StormByte-Suite/StormByte-Logger) | Stream logger with levels, headers and redaction | [/StormByte-Logger](https://suite.stormbyte.org/StormByte-Logger) |
| [Multimedia](https://github.com/StormByte-Suite/StormByte-Multimedia) | Decode, encode and containers without raw FFmpeg types | [/StormByte-Multimedia](https://suite.stormbyte.org/StormByte-Multimedia) |
| [Network](https://github.com/StormByte-Suite/StormByte-Network) | Framed packets, Client/Server, IPv4/IPv6 TCP | [/StormByte-Network](https://suite.stormbyte.org/StormByte-Network) |
| **System** | This repository | [/StormByte-System](https://suite.stormbyte.org/StormByte-System) |

## Public API

| Name | Role |
|------|------|
| `Process` | Spawn and talk to a child. One public constructor: `string_view` plus `Safe::Vector<Safe::String>`. A literal binds; a `Safe::String` binds through its view conversion. `operator bool` is true only while the child is live. `Pid()` exposes only its scalar process ID; native process/thread handles stay private to the owner. `Fault()` is `StormByte.System.Process`. |
| `Device` | Medium behind a path. `operator bool` is probe success, not permission. `Fault()` is `StormByte.System.Device`. `Throughput` is a nominal rate. `Window` is a suggested transfer size. Both are `ByteSize`. |
| `Directory` | `Current`, `Home`, `Temporary`, `CurrentExecutable`. `bool` + out `String`. `LastError()` is TLS in this module. |
| `File` | `Temporary(prefix, suffix)` (caller unlinks) and `CurrentExecutable`. Same `bool` + `LastError` contract. |
| `Host` | `Name`, `Architecture`, `CPU`, `OS`, `Kernel`, `PageSize`, `PhysicalMemory`, `AvailableMemory`, `LogicalProcessors`, `Bitness`. Sizes are `ByteSize`. |
| `ThisThread` | `Sleep`; `Name` get/set. Set returns `false` and `TooLong` if the platform limit is exceeded. |
| `Variable` | Expand environment strings. One public entry per width (`string_view`, `wstring_view`). The result is `StormByte::Safe::String`. |
| `System::EoF` | Close process stdin. |

`Pipe` is private. There is no `StormByte/system/exception.hxx` and no generic `StormByte.System` error domain.

## Examples

### Run a process

```cpp
#include <StormByte/system/process.hxx>

using StormByte::System::Process;

Process missing("/no/such/stormbyte-tool");
if (!missing) {
	if (missing.Fault().code() == make_error_code(Process::Error::ExecutableNotFound))
		/* spawn failed */;
}

StormByte::Safe::Vector<StormByte::Safe::String> args;
args.emplace_back("hello");
Process echo("echo", args);
if (!echo)
	return;
StormByte::Safe::String out;
echo >> out;
if (echo.Wait() != 0)
	/* child status */;
if (echo.Fault().code() == make_error_code(Process::Error::AlreadyExited))
	/* second Wait */;
```

On Windows use a real binary (`cmd.exe`, `where.exe`) instead of `echo` if it is not on `PATH` the way you expect.

### Pipe two processes

```cpp
StormByte::Safe::Vector<StormByte::Safe::String> producer_args;
producer_args.emplace_back("%s");
producer_args.emplace_back("hello\n");
Process producer("printf", producer_args);
StormByte::Safe::Vector<StormByte::Safe::String> consumer_args;
consumer_args.emplace_back("a-z");
consumer_args.emplace_back("A-Z");
Process consumer("tr", consumer_args);
producer >> consumer;
producer << StormByte::System::EoF;
StormByte::Safe::String out;
consumer >> out;
producer.Wait();
consumer.Wait();
```

### Classify a device

```cpp
#include <StormByte/system/device.hxx>

#include <filesystem>

StormByte::System::Device root(std::filesystem::path("/"));
if (!root) {
	/* BrokenSymlink, DeviceNotFound, Permission, ProbeFailed */
	return;
}
auto kind = root.Kind();
auto access = root.Access();
auto rate = root.Throughput();
auto window = root.Window();
```

On Windows pass `std::filesystem::path("C:\\")`. `operator bool` is not “can write”. A special device node is never writable. Symlinks are followed; a dangling link is `BrokenSymlink`.

### Directories and files

```cpp
#include <StormByte/system/directory.hxx>
#include <StormByte/system/file.hxx>

StormByte::Safe::String cwd, home, tmpdir, exe_dir, exe, scratch;
if (!StormByte::System::Directory::Current(cwd))
	/* Directory::LastError() */;
StormByte::System::Directory::Home(home);
StormByte::System::Directory::Temporary(tmpdir);
StormByte::System::Directory::CurrentExecutable(exe_dir);
StormByte::System::File::CurrentExecutable(exe);
if (StormByte::System::File::Temporary(scratch, "SB", ".tmp")) {
	/* use scratch; caller unlinks */
}
```

Windows `File::Temporary` only uses the first three characters of the prefix (`GetTempFileNameW`). The suffix is appended after the generated name.

### Host

```cpp
#include <StormByte/system/host.hxx>

StormByte::Safe::String hostname;
StormByte::System::Host::Name(hostname);
auto os = StormByte::System::Host::OS();       // "Gentoo 2.18", "Windows 11", "macOS 15.1"
auto kernel = StormByte::System::Host::Kernel(); // "Linux … SMP PREEMPT_DYNAMIC", "NT 10.0.26100", "Darwin …"
auto cpu = StormByte::System::Host::CPU();
auto arch = StormByte::System::Host::Architecture();
auto page = StormByte::System::Host::PageSize();
auto ram = StormByte::System::Host::PhysicalMemory();
unsigned bits = StormByte::System::Host::Bitness();
```

`LogicalProcessors` and `Bitness` do not update `LastError`. The others do. A failed `ByteSize` is zero; a failed `String` is empty.

### Calling thread

```cpp
#include <StormByte/system/this_thread.hxx>

StormByte::System::ThisThread::Sleep(std::chrono::milliseconds(10));
if (!StormByte::System::ThisThread::Name("worker-1")) {
	/* TooLong or Failed; LastError() */
}
StormByte::Safe::String name;
StormByte::System::ThisThread::Name(name);
```

On Linux/macOS the pthread name limit is 15 characters. Windows `SetThreadDescription` does not use that limit.

### Expand variables

```cpp
#include <StormByte/system/variable.hxx>

auto home = StormByte::System::Variable::Expand("~");
auto tmp = StormByte::System::Variable::Expand("%TEMP%");
```

A literal binds. A `Safe::String` binds through its view conversion. A failed Windows expand returns the original text (same idea as a missing UNIX `$HOME`).

## Design notes

- `Process` construction starts the child immediately and does not throw. `operator bool` is live status only.
- The public `Process` constructor and `operator<<` take `std::string_view`. A literal binds. A `Safe::String` binds through its view conversion. The view is copied into Base-owned text before it crosses the DLL boundary.
- Windows process and thread handles remain inside `Process` and are closed by `Wait()` or destruction; `Pid()` returns only the child ID.
- The private `Process` implementation uses `StormByte::Safe::Unique` and is destroyed by the System module.
- Timed `Wait` sets `TimedOut` and leaves the child running.
- On UNIX, System ignores `SIGPIPE` once process-wide so a closed pipe peer reports write failure instead of killing the host.
- Windows `Suspend()` / `Resume()` snapshot the child threads; a thread created during enumeration may be missed.
- Windows stdio handles are made non-inheritable after `CreateProcessW`; a short inheritance window exists during creation.
- `Device` stores only the caller accessor. Kind/Access/Throughput/Window are valid only when the Device converts to `true`. Pass a `std::filesystem::path`: a string literal is ambiguous between `Safe::String` and `string_view`.
- `Directory` / `File` / `Host` / `ThisThread` `LastError()` is `thread_local` inside this module, exposed by an exported getter. Do not put `thread_local` in a public header.
- Public owned text across a DLL boundary is `StormByte::Safe::String` / `StormByte::Safe::WString`. Borrowed views carry explicit lengths; System copies text into module-local NUL-terminated storage when a native API needs a C string. `Process` stdin writes preserve embedded NUL bytes.
- Destructor of `Process` waits if the child is still owned. Move invalidates the source.

## Testing

Enable tests in CMake (`ENABLE_TEST`) and run CTest from the build tree. Process, Device and File tests assert lifecycle and error behavior; Directory, Host and ThisThread also print machine-dependent probe data. Use `ctest -V` to see stdout.

## Contributing

Issues and pull requests belong on this repository. Fork and open a PR against `master`.

Read [CONTRIBUTING.md](CONTRIBUTING.md) before you send a patch (copyright assignment and review rules). Coding rules are in [CODING_STYLE.md](CODING_STYLE.md).

## License

Original StormByte-System sources are dual-licensed: GNU Lesser General Public License v3 or later **or** a commercial license from the copyright holder (David C. Manuelda <StormByte@gmail.com>).

The grant applies only to original StormByte-System source in this repository. It does not cover other StormByte modules or third-party material shipped here (including everything under `thirdparty/`), which remains under its own license. Neither license grants patent rights.

See [LICENSE](LICENSE) for the dual-license notice and [COPYING.LGPLv3](COPYING.LGPLv3) for the full GNU LGPL version 3 text. Also <https://www.gnu.org/licenses/lgpl-3.0.html>.

Static linking under the LGPL is described under [Installation](#installation).

## Support

StormByte is developed in spare time. Sponsorship is optional and does not buy features, priority or support.

- [GitHub Sponsors](https://github.com/sponsors/StormBytePP)
- [PayPal](https://paypal.me/StormBytePP)
