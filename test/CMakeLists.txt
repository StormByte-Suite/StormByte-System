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
endif()
