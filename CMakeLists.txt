cmake_minimum_required(VERSION 3.28)

project(AudioInterfaceEscapeRoom VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

# Dependencies come through CPM with one shared download cache, so every build
# folder reuses the same sources and works offline after the first configure.
# Set CPM_SOURCE_CACHE (CMake variable or environment) to use another location.
if(NOT DEFINED CPM_SOURCE_CACHE AND NOT DEFINED ENV{CPM_SOURCE_CACHE})
	set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cache/cpm" CACHE PATH "Shared dependency source cache")
endif()
include(cmake/CPM.cmake)

CPMAddPackage(
	NAME nlohmann_json
	VERSION 3.11.3
	URL https://github.com/nlohmann/json/archive/9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03.tar.gz
	URL_HASH SHA256=0dbc5e40a01ff142e7e68c03e85247a4dcede2f592d12d3677dee3664d17975a
)

option(ESCAPE_ROOM_BUILD_APP "Build the JUCE standalone application" OFF)
option(ESCAPE_ROOM_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(ESCAPE_ROOM_USE_SOLUTION "Use correct core implementations with the selected mission setup" OFF)
option(ESCAPE_ROOM_USE_START_CODE "Build the mission's starting code, not exercise/, e.g. for release packages" OFF)
set(ESCAPE_ROOM_MISSION "baseline" CACHE STRING "Mission implementation to build")
set_property(CACHE ESCAPE_ROOM_MISSION PROPERTY STRINGS
	baseline 01-test-tone 02-right-channel 03-mono-playback 04-playback-buzz
	05-saved-setup final-customer-report)

find_package(Threads REQUIRED)

if(ESCAPE_ROOM_ENABLE_SANITIZERS)
	if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
		add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
		add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
	else()
		message(FATAL_ERROR "Sanitizers are not configured for ${CMAKE_CXX_COMPILER_ID}")
	endif()
endif()

add_subdirectory(audio-core)
add_subdirectory(tests)

if(ESCAPE_ROOM_BUILD_APP)
	CPMAddPackage(
		NAME JUCE
		VERSION 9.0.1
		URL https://github.com/juce-framework/JUCE/archive/e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8.tar.gz
		URL_HASH SHA256=1c43b675dcf3c99889fed6f68317873048b15449455974e22859275f17b2e69b
	)

	add_subdirectory(app)
endif()
