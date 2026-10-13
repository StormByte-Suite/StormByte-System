include(GNUInstallDirs)

file(GLOB_RECURSE STORMBYTE_SYSTEM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cxx")

if(BUILD_SHARED_LIBS)
	set(_mode shared)
else()
	set(_mode static)
endif()

set(_opt "BACKEND=host;ALIAS=StormByte::System")
if(WIN32)
	set(_opt "${_opt};LINK={advapi32;iphlpapi;ws2_32}")
elseif(APPLE)
	set(_opt "${_opt};LINK={-framework CoreFoundation;-framework IOKit}")
endif()

buildmaster_component(
	StormByte-System
	"StormByte Suite - System"
	"${STORMBYTE_SYSTEM_SOURCES}"
	""
	${_mode}
	"StormByte-System"
	"${_opt}"
)

if(BUILD_SHARED_LIBS)
	target_compile_definitions(StormByte-System PUBLIC STORMBYTE_SYSTEM_SHARED)
endif()

set_target_properties(StormByte-System PROPERTIES
	LINKER_LANGUAGE CXX
	SOVERSION ${CMAKE_PROJECT_VERSION}
	VERSION ${CMAKE_PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE ON
	C_VISIBILITY_PRESET hidden
	CXX_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON
)

if(MSVC)
	target_compile_options(StormByte-System PRIVATE
		/EHsc
		$<$<CONFIG:Release>:/O2>
		$<$<CONFIG:Debug>:/Od>
	)
else()
	target_compile_options(StormByte-System PRIVATE
		-fvisibility=hidden
		$<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>
		$<$<CONFIG:Debug>:-pipe -g -ggdb -Wall -Wextra -Wnon-virtual-dtor -pedantic -pedantic-errors -O0>
	)
endif()

target_include_directories(StormByte-System
	SYSTEM BEFORE PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/public"
		"${CMAKE_CURRENT_LIST_DIR}/private"
)
