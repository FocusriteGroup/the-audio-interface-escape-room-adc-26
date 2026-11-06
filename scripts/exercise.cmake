cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED EX OR NOT EX MATCHES "^(0?[1-5]|final)$")
    message(FATAL_ERROR "Use -DEX=1 through -DEX=5, or -DEX=final.")
endif()
if(NOT DEFINED MODE OR NOT MODE MATCHES "^(start|solution)$")
    message(FATAL_ERROR "Use -DMODE=start or -DMODE=solution.")
endif()

if(NOT DEFINED RESET)
    set(RESET OFF)
endif()
string(TOUPPER "${RESET}" RESET)
if(NOT RESET MATCHES "^(ON|OFF)$" OR (RESET AND MODE STREQUAL "solution"))
    message(FATAL_ERROR "RESET must be ON or OFF, and only applies to -DMODE=start.")
endif()

if(NOT DEFINED APP)
    set(APP ON)
endif()
string(TOUPPER "${APP}" APP)
if(NOT APP MATCHES "^(ON|OFF)$")
    message(FATAL_ERROR "APP must be ON or OFF.")
endif()
if(NOT DEFINED LAUNCH)
    if(APP AND (CMAKE_HOST_APPLE OR CMAKE_HOST_WIN32))
        set(LAUNCH ON)
    else()
        set(LAUNCH OFF)
    endif()
endif()
string(TOUPPER "${LAUNCH}" LAUNCH)
if(NOT LAUNCH MATCHES "^(ON|OFF)$" OR (LAUNCH AND NOT APP))
    message(FATAL_ERROR "LAUNCH must be ON or OFF, and launching requires APP=ON.")
endif()
if(LAUNCH AND NOT (CMAKE_HOST_APPLE OR CMAKE_HOST_WIN32))
    message(FATAL_ERROR "Automatic launch supports macOS and Windows; use -DLAUNCH=OFF elsewhere.")
endif()

get_filename_component(workspace_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
find_program(ctest_program NAMES ctest REQUIRED)
find_program(ninja_program NAMES ninja REQUIRED)
if(LAUNCH AND CMAKE_HOST_APPLE)
    find_program(launch_program NAMES open REQUIRED)
elseif(LAUNCH AND CMAKE_HOST_WIN32)
    find_program(launch_program NAMES powershell pwsh REQUIRED)
endif()

set(missions 01-test-tone 02-right-channel 03-mono-playback 04-playback-buzz
    05-saved-setup final-customer-report)
if(EX STREQUAL "final")
    set(exercise_id final)
    set(mission_index 5)
else()
    string(REGEX REPLACE "^0" "" exercise_number "${EX}")
    set(exercise_id "0${exercise_number}")
    math(EXPR mission_index "${exercise_number} - 1")
endif()
list(GET missions ${mission_index} mission)

if(APP)
    set(build_kind app)
else()
    set(build_kind core)
endif()
# One build folder serves every exercise and mode. Switching reconfigures it and
# recompiles only the project sources; JUCE is compiled once.
set(build_directory "${workspace_root}/build/workshop/${build_kind}")
# exercise/ holds one exercise's code at a time; scripts/working-set.cmake swaps it
# when the build is configured for another exercise. Back it up first, whichever
# exercise it holds.
include("${CMAKE_CURRENT_LIST_DIR}/working-set.cmake")
# The pre-workshop release of the attendee repository ships Exercise 1 only.
if(NOT IS_DIRECTORY "${workspace_root}/audio-core/.exercises/${mission}")
    escape_room_exercise_name("${mission}" missing_name)
    message(FATAL_ERROR "This repository doesn't include ${missing_name} yet. It will be "
        "added before the workshop: run git pull then to get it. Your code in exercise/ "
        "is not affected.")
endif()
escape_room_working_set_mission("${workspace_root}" held_mission)
string(TIMESTAMP snapshot_time "%Y%m%dT%H%M%SZ" UTC)
string(RANDOM LENGTH 8 ALPHABET 0123456789abcdef snapshot_suffix)
set(snapshot_directory "${workspace_root}/build/workshop/backups/${snapshot_time}-${snapshot_suffix}")
if(EXISTS "${workspace_root}/exercise")
    file(MAKE_DIRECTORY "${snapshot_directory}")
    file(COPY "${workspace_root}/exercise/" DESTINATION "${snapshot_directory}/exercise")
    file(WRITE "${snapshot_directory}/snapshot.txt"
        "Exercise requested: ${exercise_id}\nMode requested: ${MODE}\nexercise/ held: ${held_mission}\n")
    message(STATUS "Backup of exercise/: ${snapshot_directory}")
endif()

set(solution OFF)
if(MODE STREQUAL "solution")
    set(solution ON)
    message(STATUS "Solution mode builds the reference code; exercise/ is left as it is.")
elseif(RESET)
    # Forget this exercise's edits, so the build starts it again from its original code.
    file(REMOVE_RECURSE "${workspace_root}/build/workshop/saved/${mission}")
    if(held_mission STREQUAL mission)
        file(REMOVE "${workspace_root}/exercise/.exercise")
    endif()
    message(STATUS "Starting ${exercise_id} again from its original code.")
else()
    message(STATUS "Start mode keeps your edits: exercise/ holds your code for each exercise.")
endif()
set(configure_arguments -S "${workspace_root}" -B "${build_directory}" -G Ninja
    "-DCMAKE_MAKE_PROGRAM=${ninja_program}" -DCMAKE_BUILD_TYPE=Debug
    -DBUILD_TESTING=ON "-DESCAPE_ROOM_BUILD_APP=${APP}"
    "-DESCAPE_ROOM_MISSION=${mission}" "-DESCAPE_ROOM_USE_SOLUTION=${solution}")

function(run_step)
    execute_process(COMMAND ${ARGV} WORKING_DIRECTORY "${workspace_root}" RESULT_VARIABLE step_result)
    if(NOT step_result STREQUAL "0")
        message(FATAL_ERROR "Step failed (${step_result}). Your code is in exercise/; backups are in build/workshop/backups/.")
    endif()
endfunction()

run_step("${CMAKE_COMMAND}" ${configure_arguments})
run_step("${CMAKE_COMMAND}" --build "${build_directory}" --parallel 4)
# A failing check never stops the app opening: the app is how participants
# investigate. Unexpected failures are summarised at the end and the runner
# exits with an error. Only a failed build stops before launch.
set(unexpected_failures "")
function(run_check description)
    execute_process(COMMAND "${ctest_program}" --test-dir "${build_directory}"
        --output-on-failure ${ARGN} WORKING_DIRECTORY "${workspace_root}"
        RESULT_VARIABLE check_result)
    if(NOT check_result STREQUAL "0")
        set(unexpected_failures ${unexpected_failures} "${description}" PARENT_SCOPE)
    endif()
endfunction()

if(solution)
    run_check("solution checks" -E "^app_smoke$")
else()
    run_check("exercise source layout" -R "^exercise_source_layout$")
endif()

if(APP)
    run_check("app startup check" -R "^app_smoke$")
    set(application "${build_directory}/bin/Fix It Before Launch")
    if(CMAKE_HOST_APPLE)
        string(APPEND application ".app")
    elseif(CMAKE_HOST_WIN32)
        string(APPEND application ".exe")
    endif()
    message(STATUS "Application: ${application}")
    if(LAUNCH)
        if(CMAKE_HOST_APPLE)
            run_step("${launch_program}" -n "${application}")
        else()
            string(REPLACE "'" "''" escaped_application "${application}")
            run_step("${launch_program}" -NoProfile -NonInteractive -Command
                "Start-Process -FilePath '${escaped_application}'")
        endif()
    endif()
endif()

message(STATUS "Exercise ${exercise_id} ${MODE} ready: ${build_directory}")
# The focused check result goes last, so it is the first thing people read.
execute_process(COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIRECTORY=${build_directory}"
    -DSKIP_BUILD=ON -DRUNNER=${LAUNCH} -P "${CMAKE_CURRENT_LIST_DIR}/check.cmake"
    WORKING_DIRECTORY "${workspace_root}" RESULT_VARIABLE check_result)
if(NOT check_result STREQUAL "0")
    list(APPEND unexpected_failures "focused check could not run")
endif()
if(unexpected_failures)
    list(JOIN unexpected_failures ", " failure_list)
    set(opened "")
    if(APP AND LAUNCH)
        set(opened " The app was still opened so you can keep investigating.")
    endif()
    message(SEND_ERROR "Unexpected check failure: ${failure_list}.${opened} "
        "Review the output above, or ask a helper.")
endif()