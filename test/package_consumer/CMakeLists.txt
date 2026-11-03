cmake_minimum_required(VERSION 3.28)
project(StormByteSystemPackageConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(PREPEND CMAKE_MODULE_PATH "${BASE_PREFIX}/share/cmake/Modules")
list(PREPEND CMAKE_PREFIX_PATH "${SYSTEM_PREFIX}" "${BASE_PREFIX}")
set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH FALSE)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)

find_package(StormByte REQUIRED COMPONENTS System)
find_package(StormByte REQUIRED COMPONENTS Base System)

foreach(path IN ITEMS "${STORMBYTE_System_INCLUDE_DIR}" "${STORMBYTE_System_LIBRARY}")
	cmake_path(IS_PREFIX SYSTEM_PREFIX "${path}" NORMALIZE _in_system_prefix)
	if(NOT _in_system_prefix)
		message(FATAL_ERROR "System was not found in ${SYSTEM_PREFIX}: ${path}")
	endif()
endforeach()
cmake_path(IS_PREFIX BASE_PREFIX "${STORMBYTE_LIBRARY}" NORMALIZE _in_base_prefix)
if(NOT _in_base_prefix)
	message(FATAL_ERROR "Base was not found in ${BASE_PREFIX}: ${STORMBYTE_LIBRARY}")
endif()

add_executable(system_package_consumer consumer.cxx)
target_link_libraries(system_package_consumer PRIVATE StormByte::System)
set_target_properties(system_package_consumer PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/bin>"
)
