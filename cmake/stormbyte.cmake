# URL of a StormByte Base release archive for the system being configured.
#
# stormbyte_component_url(<outvar> <tag>)
# Writes the download URL for that release tag into <outvar>.
# The archive name matches release.yml: StormByte-<tag>-<os>-<arch>[-stdlib].zip
# Windows has no runtime suffix. Linux and macOS ask the C++ compiler which
# standard library it will use, including a clang config that never appears
# as a -stdlib flag. One preprocessor pass, no compile and no link.

function(_stormbyte_cxx_blob _out)
	set(_blob "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_INIT} $ENV{CXXFLAGS}")
	if(CMAKE_BUILD_TYPE)
		string(TOUPPER "${CMAKE_BUILD_TYPE}" _cfg)
		string(APPEND _blob " ${CMAKE_CXX_FLAGS_${_cfg}}")
	endif()
	set(${_out} "${_blob}" PARENT_SCOPE)
endfunction()

function(_stormbyte_stdlib _out)
	if(NOT CMAKE_CXX_COMPILER)
		message(FATAL_ERROR
			"stormbyte_component_url: the C++ compiler is not known yet")
	endif()
	_stormbyte_cxx_blob(_blob)
	separate_arguments(_flags UNIX_COMMAND "${_blob}")
	set(_src "${CMAKE_BINARY_DIR}/CMakeFiles/stormbyte-stdlib.cpp")
	file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/CMakeFiles")
	file(WRITE "${_src}" "#include <cstddef>\n")
	execute_process(
		COMMAND "${CMAKE_CXX_COMPILER}" -E -dM -x c++ ${_flags} "${_src}"
		OUTPUT_VARIABLE _defs
		ERROR_VARIABLE _err
		RESULT_VARIABLE _rc
	)
	if(NOT _rc EQUAL 0)
		message(FATAL_ERROR
			"stormbyte_component_url: cannot detect the C++ standard library "
			"(${CMAKE_CXX_COMPILER})\n${_err}")
	endif()
	if(_defs MATCHES "(^|\n)[ \t]*#define[ \t]+_LIBCPP_VERSION[ \t]")
		set(${_out} "libc++" PARENT_SCOPE)
	elseif(_defs MATCHES "(^|\n)[ \t]*#define[ \t]+__GLIBCXX__[ \t]")
		set(${_out} "libstdc++" PARENT_SCOPE)
	else()
		message(FATAL_ERROR
			"stormbyte_component_url: ${CMAKE_CXX_COMPILER} uses neither "
			"libc++ nor libstdc++")
	endif()
endfunction()

function(stormbyte_component_url out tag)
	if("${out}" STREQUAL "" OR "${tag}" STREQUAL "")
		message(FATAL_ERROR "stormbyte_component_url: <outvar> and <tag> are required")
	endif()

	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
		set(_arch "arm64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|x64)$")
		set(_arch "x64")
	else()
		message(FATAL_ERROR
			"stormbyte_component_url: unsupported architecture '${CMAKE_SYSTEM_PROCESSOR}'")
	endif()

	if(WIN32)
		set(_tag "windows-${_arch}")
	elseif(APPLE)
		_stormbyte_stdlib(_stdlib)
		set(_tag "macos-${_arch}-${_stdlib}")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		_stormbyte_stdlib(_stdlib)
		set(_tag "linux-${_arch}-${_stdlib}")
	else()
		message(FATAL_ERROR
			"stormbyte_component_url: unsupported system '${CMAKE_SYSTEM_NAME}'")
	endif()

	set(_name "StormByte-${tag}-${_tag}.zip")
	message(STATUS "StormByte Base archive: ${_name}")
	set(${out}
		"https://github.com/StormByte-Suite/StormByte/releases/download/${tag}/${_name}"
		PARENT_SCOPE)
endfunction()