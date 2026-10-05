# StormByte coding style

This is the flavor used in Base. Other suite modules follow it unless their own file says otherwise. Match the files already in the tree when something here is silent.

## Files

Headers are `.hxx`, sources `.cxx`, template bodies `.txx` included at the bottom of the header. Start every C or C++ file with `#pragma once` in the header and with the license banner used in this repository, unchanged. CMake and Markdown do not take that banner.

Include `StormByte/…` first, then a blank line, then the standard library. Do not `using namespace` in a header. `using namespace StormByte;` in a `.cxx` after the includes is fine.

Indent with tabs. Spaces for indentation are wrong. Do not mix them to line up code; Doxygen `///<` on members may share a column by using tabs.

## Shape

Braces are K&R: the `{` sits on the same line as `class`, `struct`, `enum`, `namespace`, `if`, `for`, `while` or the function signature. `public:` / `private:` are one tab in; members one more.

A single-statement `if` / `else` / `else if` has no braces. Put `else` and `else if` on their own line, not on the same line as a closing `}`.

```
if (unit == 0 || remainder == 0)
	std::snprintf(...);
else {
	...
}
```

Pointers and references bind to the type: `const char* str`, `String& other`, `operator const char*()`. Not `char *str`.

Types, enumerations and functions are PascalCase (`Process`, `Expand`, `Wait`). Macros are `SCREAMING_SNAKE` (`STORMBYTE_SYSTEM_PUBLIC`, `WINDOWS`). One statement per line.

## Language

C++26. RAII: no bare `new` / `delete` in new code.

Public templates use `StormByte::Type` concepts. Do not put `std::enable_if`, `void_t` or a raw `std::is_*` next to those concepts.

`enum class` only. Converting constructors are `explicit` unless the type already documents an implicit conversion. Mark `noexcept` only when it is true. Prefer `constexpr` when there is no heap and no I/O.

Platform tests are `#ifdef WINDOWS`, `#elifdef MACOS`, `#else`. Not `#if defined(WINDOWS)`.

No anonymous namespace in a public header. An anonymous namespace in a `.cxx` is for helpers used in that translation unit only.

## DLL boundary

`STORMBYTE_SYSTEM_PUBLIC` comes **first** on a function declaration. clang-cl rejects `__declspec` after a reference return type.

```
STORMBYTE_SYSTEM_PUBLIC std::ostream& operator<<(std::ostream& ostream, const Process& proc);
```

Do not write `std::ostream& STORMBYTE_SYSTEM_PUBLIC operator<<(...);`.

A class keeps the attribute on the type: `class STORMBYTE_SYSTEM_PUBLIC Process`.

Do not repeat `STORMBYTE_SYSTEM_PUBLIC` on an ordinary `.cxx` definition. Do not put `dllexport` on a member of a class that is already exported.

Values that leave the shared library are `StormByte::Safe::String`, `StormByte::Safe::WString`, `Size`, or a `const char*` owned by this library. Do not return `std::string` by value across a DLL boundary. Do not put `std::string` fields on a class another module can inherit (`Process` keeps state in a private PIMPL). Treat Base text as length-aware ranges, not C strings. Materialize a local `std::string` / `std::wstring` inside System before passing text to a native API that requires NUL termination.

## Doxygen

Document every public declaration except `= delete`. Large classes use `@name` groups. `@ref` uses the qualified name (`StormByte::System::Process`, `StormByte::Safe::String`). Align member `///<` comments to the same column when they fit.

Wrap `extern template` noise in `/// @cond` / `/// @endcond` so it does not show up as a page of instantiations.

## Commits and tests

Conventional Commits in English (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`). One topic per commit.

Test section banners are identical in the test body and in `main`:

```
// -------------------
// Construct
// -------------------
```

Do not invent `=== Construct ===` or print section titles with `cout`.
