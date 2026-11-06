# Rebuild the tests and run the focused check for the exercise you last built
# with exercise.cmake. Run it after every edit, from the repository root:
#
#     cmake -P scripts/check.cmake
#
# It rebuilds only the test programs, not the app, so it is quick. It uses the
# most recently configured build (build/workshop/app or build/workshop/core).
# Add -DAPP=OFF to use the core build.
cmake_minimum_required(VERSION 3.28)

get_filename_component(workspace_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/focused-checks.cmake")

set(start_command "cmake -DEX=1 -DMODE=start -P scripts/exercise.cmake")
if(NOT DEFINED BUILD_DIRECTORY)
    set(app_build "${workspace_root}/build/workshop/app")
    set(core_build "${workspace_root}/build/workshop/core")
    if(DEFINED APP AND NOT APP)
        set(BUILD_DIRECTORY "${core_build}")
    elseif(EXISTS "${app_build}/CMakeCache.txt" AND EXISTS "${core_build}/CMakeCache.txt")
        if("${core_build}/CMakeCache.txt" IS_NEWER_THAN "${app_build}/CMakeCache.txt")
            set(BUILD_DIRECTORY "${core_build}")
        else()
            set(BUILD_DIRECTORY "${app_build}")
        endif()
    elseif(EXISTS "${core_build}/CMakeCache.txt")
        set(BUILD_DIRECTORY "${core_build}")
    else()
        set(BUILD_DIRECTORY "${app_build}")
    endif()
endif()
if(NOT EXISTS "${BUILD_DIRECTORY}/CMakeCache.txt")
    message(FATAL_ERROR "No exercise build found in ${BUILD_DIRECTORY}. "
        "Build an exercise first, for example:\n  ${start_command}")
endif()

file(STRINGS "${BUILD_DIRECTORY}/CMakeCache.txt" cache_lines
    REGEX "^ESCAPE_ROOM_(MISSION:STRING|USE_SOLUTION:BOOL)=")
set(mission "")
set(solution OFF)
foreach(line IN LISTS cache_lines)
    if(line MATCHES "^ESCAPE_ROOM_MISSION:STRING=(.*)$")
        set(mission "${CMAKE_MATCH_1}")
    elseif(line MATCHES "^ESCAPE_ROOM_USE_SOLUTION:BOOL=(.*)$")
        set(solution "${CMAKE_MATCH_1}")
    endif()
endforeach()
if(NOT DEFINED focused_check_tests_${mission})
    message(FATAL_ERROR "${BUILD_DIRECTORY} is not built for an exercise (${mission}). "
        "Build one first, for example:\n  ${start_command}")
endif()
set(focused_tests ${focused_check_tests_${mission}})

# "03-mono-playback" -> the runner's -DEX=3.
if(mission MATCHES "^0([0-9])-")
    set(exercise "${CMAKE_MATCH_1}")
else()
    set(exercise final)
endif()
set(title "${focused_check_title_${mission}}")
if(solution)
    string(APPEND title ", solution build")
endif()

if(NOT SKIP_BUILD)
    message(STATUS "Rebuilding the tests in ${BUILD_DIRECTORY}")
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIRECTORY}" --target
            escape_room_core_tests escape_room_mission_tests escape_room_scenario_tests
        RESULT_VARIABLE build_result)
    if(NOT build_result STREQUAL "0")
        message(FATAL_ERROR "The build failed. Fix the compiler errors above, then run "
            "cmake -P scripts/check.cmake again.")
    endif()
endif()

# Ask CTest for each focused test's command, then run it directly. Running each one
# separately means an exercise with two checks reports both, not just the first.
get_filename_component(cmake_directory "${CMAKE_COMMAND}" DIRECTORY)
find_program(ctest_program NAMES ctest HINTS "${cmake_directory}" REQUIRED)
execute_process(COMMAND "${ctest_program}" --test-dir "${BUILD_DIRECTORY}" --show-only=json-v1
    OUTPUT_VARIABLE test_json RESULT_VARIABLE query_result)
if(NOT query_result STREQUAL "0")
    message(FATAL_ERROR "Could not list the tests in ${BUILD_DIRECTORY}.")
endif()
string(JSON test_count LENGTH "${test_json}" tests)
math(EXPR last_test "${test_count} - 1")

set(report "")
set(failed OFF)
set(found "")
foreach(index RANGE ${last_test})
    string(JSON name GET "${test_json}" tests ${index} name)
    if(NOT name IN_LIST focused_tests)
        continue()
    endif()
    list(APPEND found "${name}")
    string(JSON argument_count LENGTH "${test_json}" tests ${index} command)
    math(EXPR last_argument "${argument_count} - 1")
    set(command "")
    foreach(argument_index RANGE ${last_argument})
        string(JSON argument GET "${test_json}" tests ${index} command ${argument_index})
        list(APPEND command "${argument}")
    endforeach()
    execute_process(COMMAND ${command} WORKING_DIRECTORY "${workspace_root}"
        OUTPUT_VARIABLE output ERROR_VARIABLE output RESULT_VARIABLE result)
    if(result STREQUAL "0")
        string(APPEND report "  PASS  ${name}\n")
    else()
        set(failed ON)
        if(NOT result MATCHES "^[0-9]+$")
            set(output "${output}\nThe check crashed: ${result}")
        endif()
        string(STRIP "${output}" output)
        string(REPLACE "\n" "\n          " output "${output}")
        string(APPEND report "  FAIL  ${name}\n          ${output}\n")
    endif()
endforeach()
foreach(name IN LISTS focused_tests)
    if(NOT name IN_LIST found)
        message(FATAL_ERROR "The focused test ${name} is missing from ${BUILD_DIRECTORY}.")
    endif()
endforeach()

set(rule "======================================================================")
if(failed)
    set(status FAIL)
    if(solution)
        set(next "The solution should pass. Ask a helper.")
    else()
        set(next "This is expected until the bug is fixed. Edit the files in exercise/,
then run this again:  cmake -P scripts/check.cmake

Other tests in the full suite may fail too while the bug remains: the same
bug breaks them. The focused check is the one to pass.")
    endif()
else()
    set(status PASS)
    if(solution)
        set(next "This is the reference solution. To build your own code again:
  cmake -DEX=${exercise} -DMODE=start -P scripts/exercise.cmake")
    elseif(RUNNER)
        set(next "The app that just opened includes this code.")
    else()
        set(next "To hear the fix, close the app and run:
  cmake -DEX=${exercise} -DMODE=start -P scripts/exercise.cmake")
    endif()
endif()
message("
${rule}
${title}
Focused check: ${status}

${report}
Read the check: ${focused_check_source_${mission}}

${next}
${rule}
")
