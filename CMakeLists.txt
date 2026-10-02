cmake_minimum_required(VERSION 3.28)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _sb_ver)
string(STRIP "${_sb_ver}" _sb_ver)

project("StormByte C++ Library: System module"
	VERSION ${_sb_ver}
	DESCRIPTION "StormByte-System is the C++26 process and environment module of the StormByte suite."
	HOMEPAGE_URL "https://suite.stormbyte.org/StormByte-System"
)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

include(cmake/optimizations.cmake)
include(cmake/asan.cmake)
cmake_policy(SET CMP0079 NEW)

include(cmake/load_buildmaster.cmake)
add_subdirectory(doc)
add_subdirectory(thirdparty)
add_subdirectory(lib)
add_subdirectory(test)

include(cmake/install.cmake)
