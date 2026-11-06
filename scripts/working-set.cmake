# The exercise working set: one folder, exercise/, holding the seven processing files
# for whichever exercise was configured last. Exercises never sit side by side, so
# comparing them can't point at a fault.
#
# Configuring an exercise build calls escape_room_use_working_set(). When the folder
# holds a different exercise, its files are saved to build/workshop/saved/<exercise>/
# and the new exercise's files come from its own saved copy, or from its starting
# code in audio-core/.exercises/<exercise>/ the first time.
#
# Script mode checks that the folder still holds the exercise a build folder was
# configured for, so building an old build folder can't compile the wrong code:
#
#     cmake -DWORKSHOP_ROOT=<repo> -DEXPECTED_MISSION=<mission> -P scripts/working-set.cmake

set(escape_room_working_files AudioProcessor.cpp Gain.cpp LegacyMapping.cpp OutputBuffer.cpp
    ProcessorSetup.cpp RecordingMeter.cpp Routing.cpp)

function(escape_room_exercise_name mission out)
    if(mission STREQUAL "final-customer-report")
        set(${out} "the combined exercise" PARENT_SCOPE)
    else()
        string(REGEX REPLACE "^0([1-9]).*" "Exercise \\1" name "${mission}")
        set(${out} "${name}" PARENT_SCOPE)
    endif()
endfunction()

function(escape_room_working_set_mission root out)
    set(mission "")
    if(EXISTS "${root}/exercise/.exercise")
        file(READ "${root}/exercise/.exercise" mission)
        string(STRIP "${mission}" mission)
    endif()
    set(${out} "${mission}" PARENT_SCOPE)
endfunction()

function(escape_room_use_working_set root mission)
    set(working "${root}/exercise")
    set(saved_root "${root}/build/workshop/saved")
    set(start "${root}/audio-core/.exercises/${mission}")
    escape_room_working_set_mission("${root}" current)

    if(current STREQUAL mission)
        # Same exercise: keep the edits, and only replace files that have gone missing.
        foreach(filename IN LISTS escape_room_working_files)
            if(NOT EXISTS "${working}/${filename}")
                file(COPY_FILE "${start}/${filename}" "${working}/${filename}")
                file(TOUCH "${working}/${filename}")
                message(STATUS "exercise/${filename} was missing; restored its starting code")
            endif()
        endforeach()
        return()
    endif()

    if(NOT current STREQUAL "")
        file(REMOVE_RECURSE "${saved_root}/${current}")
        file(MAKE_DIRECTORY "${saved_root}/${current}")
        foreach(filename IN LISTS escape_room_working_files)
            if(EXISTS "${working}/${filename}")
                file(COPY_FILE "${working}/${filename}" "${saved_root}/${current}/${filename}")
            endif()
        endforeach()
        escape_room_exercise_name("${current}" current_name)
        message(STATUS "Saved ${current_name}'s code to build/workshop/saved/${current}/")
    endif()

    set(source "${start}")
    set(origin "its starting code")
    set(have_saved ON)
    foreach(filename IN LISTS escape_room_working_files)
        if(NOT EXISTS "${saved_root}/${mission}/${filename}")
            set(have_saved OFF)
        endif()
    endforeach()
    if(have_saved)
        set(source "${saved_root}/${mission}")
        set(origin "your saved edits")
    endif()

    file(MAKE_DIRECTORY "${working}")
    foreach(filename IN LISTS escape_room_working_files)
        file(COPY_FILE "${source}/${filename}" "${working}/${filename}")
        # A new timestamp, so the build recompiles every file that changed.
        file(TOUCH "${working}/${filename}")
    endforeach()

    escape_room_exercise_name("${mission}" name)
    string(REGEX REPLACE "^0?([1-9]).*" "\\1" ex "${mission}")
    if(mission STREQUAL "final-customer-report")
        set(ex final)
    endif()
    file(WRITE "${working}/README.md" "# Your code for ${name}

These are the processing files the app and tests build for ${name}. Edit them
here, then rerun the runner or the focused check.

The runner swaps this folder when you change exercise. Your edits are saved
first, to build/workshop/saved/<exercise>/, and come back when you return to
that exercise. Every run also keeps a copy in build/workshop/backups/.

To start ${name} again from its original code (your current edits are backed
up first):

    cmake -DEX=${ex} -DMODE=start -DRESET=ON -P scripts/exercise.cmake
")
    file(WRITE "${working}/.exercise" "${mission}\n")
    message(STATUS "exercise/ now holds ${name}, from ${origin}")
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    escape_room_working_set_mission("${WORKSHOP_ROOT}" current)
    if(NOT current STREQUAL EXPECTED_MISSION)
        escape_room_exercise_name("${EXPECTED_MISSION}" expected_name)
        if(current STREQUAL "")
            set(holds "is empty")
        else()
            escape_room_exercise_name("${current}" current_name)
            set(holds "holds ${current_name}")
        endif()
        message(FATAL_ERROR "This build is for ${expected_name}, but exercise/ ${holds}. "
            "Use the runner to switch exercise: cmake -DEX=<n> -DMODE=start -P scripts/exercise.cmake")
    endif()
endif()
