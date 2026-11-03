option(ENABLE_TEST "Enable Unit Tests" OFF)
if(ENABLE_TEST)
	enable_testing()

	add_test(NAME OptimizationTests
		COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/optimizations_test.cmake")
	set_tests_properties(OptimizationTests PROPERTIES TIMEOUT 15)

	add_executable(DeviceTests device_test.cxx)
	target_link_libraries(DeviceTests StormByte::System)
	add_test(NAME DeviceTests COMMAND DeviceTests)
	set_tests_properties(DeviceTests PROPERTIES TIMEOUT 15)

	add_executable(DirectoryTests directory_test.cxx)
	target_link_libraries(DirectoryTests StormByte::System)
	add_test(NAME DirectoryTests COMMAND DirectoryTests)
	set_tests_properties(DirectoryTests PROPERTIES TIMEOUT 15)

	add_executable(FileTests file_test.cxx)
	target_link_libraries(FileTests StormByte::System)
	add_test(NAME FileTests COMMAND FileTests)
	set_tests_properties(FileTests PROPERTIES TIMEOUT 15)

	add_executable(HostTests host_test.cxx)
	target_link_libraries(HostTests StormByte::System)
	add_test(NAME HostTests COMMAND HostTests)
	set_tests_properties(HostTests PROPERTIES TIMEOUT 15)

	add_executable(ProcessTests process_test.cxx)
	target_link_libraries(ProcessTests StormByte::System)
	add_test(NAME ProcessTests COMMAND ProcessTests)
	set_tests_properties(ProcessTests PROPERTIES TIMEOUT 15)

	add_executable(ThisThreadTests this_thread_test.cxx)
	target_link_libraries(ThisThreadTests StormByte::System)
	add_test(NAME ThisThreadTests COMMAND ThisThreadTests)
	set_tests_properties(ThisThreadTests PROPERTIES TIMEOUT 15)

	if(WITH_STORMBYTE STREQUAL "BUNDLED" AND NOT CMAKE_CROSSCOMPILING)
		set(_package_cxx_flags "${CMAKE_CXX_FLAGS}")
		set(_package_link_flags "${CMAKE_EXE_LINKER_FLAGS}")
		if(COMMAND _pgo_strip_profile_flags)
			_pgo_strip_profile_flags("${_package_cxx_flags}" _package_cxx_flags)
			_pgo_strip_profile_flags("${_package_link_flags}" _package_link_flags)
		endif()

		add_test(NAME PackageDiscoveryTests
			COMMAND "${CMAKE_COMMAND}"
				"-DSYSTEM_BUILD_DIR=${PROJECT_BINARY_DIR}"
				"-DCONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package_consumer"
				"-DBASE_PREFIX=${BUILDMASTER_INSTALL_DIR}"
				"-DBUILD_CONFIG=$<CONFIG>"
				"-DGENERATOR=${CMAKE_GENERATOR}"
				"-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
				"-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
				"-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
				"-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
				"-DCXX_COMPILER_TARGET=${CMAKE_CXX_COMPILER_TARGET}"
				"-DCXX_FLAGS=${_package_cxx_flags}"
				"-DEXE_LINKER_FLAGS=${_package_link_flags}"
				"-DOSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
				"-DOSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
				"-DEXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
				-P "${CMAKE_CURRENT_SOURCE_DIR}/package_discovery_test.cmake")
		set_tests_properties(PackageDiscoveryTests PROPERTIES TIMEOUT 120)
	endif()
endif()
