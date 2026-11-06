# Correct versions of the exercise working-set files. They live in a hidden
# folder so they aren't the first thing participants see next to the headers.
# Exercises compile the copies in exercise/ instead, which
# scripts/working-set.cmake fills from audio-core/.exercises/<exercise>/.
set(processing_sources
    .answers/AudioProcessor.cpp
    .answers/Gain.cpp
    .answers/LegacyMapping.cpp
    .answers/OutputBuffer.cpp
    .answers/ProcessorSetup.cpp
    .answers/RecordingMeter.cpp
    .answers/Routing.cpp
)
set(missions baseline 01-test-tone 02-right-channel 03-mono-playback 04-playback-buzz
    05-saved-setup final-customer-report)
if(NOT ESCAPE_ROOM_MISSION IN_LIST missions)
    message(FATAL_ERROR "Unknown ESCAPE_ROOM_MISSION: ${ESCAPE_ROOM_MISSION}")
endif()

set(use_working_set OFF)
if(NOT ESCAPE_ROOM_USE_SOLUTION AND NOT ESCAPE_ROOM_MISSION STREQUAL "baseline")
    # The pre-workshop release of the attendee repository ships Exercise 1 only.
    if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.exercises/${ESCAPE_ROOM_MISSION}")
        message(FATAL_ERROR "This repository doesn't include ${ESCAPE_ROOM_MISSION} yet. "
            "It will be added before the workshop: run git pull then to get it.")
    endif()
    include("${PROJECT_SOURCE_DIR}/scripts/working-set.cmake")
    if(ESCAPE_ROOM_USE_START_CODE)
        # Release packages ship the exercise as it starts, whatever is in exercise/.
        set(source_directory "${CMAKE_CURRENT_SOURCE_DIR}/.exercises/${ESCAPE_ROOM_MISSION}")
    else()
        set(use_working_set ON)
        escape_room_use_working_set("${PROJECT_SOURCE_DIR}" "${ESCAPE_ROOM_MISSION}")
        set(source_directory "${PROJECT_SOURCE_DIR}/exercise")
    endif()
    set(processing_sources)
    foreach(filename IN LISTS escape_room_working_files)
        list(APPEND processing_sources "${source_directory}/${filename}")
    endforeach()
endif()

add_library(escape_room_audio_core STATIC
    src/analysis/Analysis.cpp
    src/logging/DiagnosticLog.cpp
    ${processing_sources}
    src/LoopbackCapture.cpp
    src/app-state/AppState.cpp
    src/mission-setup/ReferenceScenario.cpp
    src/state-exchange/StateExchange.cpp
    src/audio-processing/SignalGenerator.cpp
    src/mission-setup/MissionSetup.cpp
)

target_include_directories(escape_room_audio_core PUBLIC src src/analysis src/app-state)

if(use_working_set)
    # Another exercise's configure may have swapped exercise/ since this build folder
    # was configured. Stop before compiling the wrong exercise's code.
    add_custom_target(escape_room_working_set_check
        COMMAND "${CMAKE_COMMAND}" "-DWORKSHOP_ROOT=${PROJECT_SOURCE_DIR}"
            "-DEXPECTED_MISSION=${ESCAPE_ROOM_MISSION}"
            -P "${PROJECT_SOURCE_DIR}/scripts/working-set.cmake"
        VERBATIM)
    add_dependencies(escape_room_audio_core escape_room_working_set_check)
endif()

target_compile_features(escape_room_audio_core PUBLIC cxx_std_20)
# The exercise selection lives in a generated header rather than in compile
# definitions. Only project sources include it, so switching exercise in one
# build folder recompiles those files but not JUCE.
if(ESCAPE_ROOM_MISSION STREQUAL "baseline")
    set(mission_macro ESCAPE_ROOM_PRODUCT_BASELINE)
elseif(ESCAPE_ROOM_MISSION STREQUAL "final-customer-report")
    set(mission_macro ESCAPE_ROOM_MISSION_FINAL)
else()
    string(SUBSTRING "${ESCAPE_ROOM_MISSION}" 0 2 mission_number)
    set(mission_macro ESCAPE_ROOM_MISSION_${mission_number})
endif()
set(solution_definition "")
if(ESCAPE_ROOM_USE_SOLUTION)
    set(solution_definition "#define ESCAPE_ROOM_SOLUTION 1")
endif()
set(generated_directory "${CMAKE_BINARY_DIR}/generated")
file(CONFIGURE OUTPUT "${generated_directory}/EscapeRoomMission.h" CONTENT [=[
#pragma once

// Generated from ESCAPE_ROOM_MISSION and ESCAPE_ROOM_USE_SOLUTION. Do not edit.
#define ESCAPE_ROOM_MISSION_NAME "@ESCAPE_ROOM_MISSION@"
#define @mission_macro@ 1
@solution_definition@
]=] @ONLY)
target_include_directories(escape_room_audio_core PUBLIC "${generated_directory}")
target_link_libraries(escape_room_audio_core PUBLIC nlohmann_json::nlohmann_json Threads::Threads)
