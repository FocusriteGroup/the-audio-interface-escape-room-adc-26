add_executable(escape_room_core_tests CoreTests.cpp)
target_link_libraries(escape_room_core_tests PRIVATE escape_room_audio_core)
target_compile_definitions(escape_room_core_tests PRIVATE
	ESCAPE_ROOM_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/assets"
)

add_test(NAME core_tests COMMAND escape_room_core_tests)
add_test(NAME exercise_04_check COMMAND escape_room_core_tests --exercise-04)

add_executable(escape_room_scenario_tests ScenarioTests.cpp)
target_link_libraries(escape_room_scenario_tests PRIVATE escape_room_audio_core)

# Exercises 1-5 ship the recording code as stubs and share the combined exercise's
# sample-rate setup, so those files don't change when exercise/ moves on to the
# combined exercise. Recording tests would fail there, so they only run where that
# code is meant to work: the combined exercise, the baseline and solutions.
set(escape_room_has_recording OFF)
if(ESCAPE_ROOM_USE_SOLUTION OR NOT ESCAPE_ROOM_MISSION MATCHES "^0[1-5]-")
	set(escape_room_has_recording ON)
endif()
if(escape_room_has_recording)
	add_test(NAME reference_scenarios COMMAND escape_room_scenario_tests)
else()
	add_test(NAME reference_scenarios COMMAND escape_room_scenario_tests --without-recording)
endif()
add_test(NAME exercise_04_render COMMAND escape_room_scenario_tests --exercise-04)

add_executable(escape_room_mission_tests MissionTests.cpp)
target_link_libraries(escape_room_mission_tests PRIVATE escape_room_audio_core)
target_compile_definitions(escape_room_mission_tests PRIVATE
	ESCAPE_ROOM_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/assets"
)
add_test(NAME exercise_02_check COMMAND escape_room_mission_tests 02)
add_test(NAME exercise_03_check COMMAND escape_room_mission_tests 03)
add_test(NAME exercise_05_check COMMAND escape_room_mission_tests 05)
if(escape_room_has_recording)
	add_test(NAME final_check COMMAND escape_room_mission_tests final)
	add_test(NAME final_recording_check COMMAND escape_room_mission_tests final-recording)
	add_test(NAME final_capture_check COMMAND escape_room_mission_tests final-capture)
endif()

# One program that runs the built exercise's focused check, for the VS Code
# debugger. Each test file is compiled again through a generated wrapper that
# renames its main(); the originals above are unchanged.
set(focused_check_sources FocusedCheck.cpp)
foreach(suite Core Mission Scenario)
	set(wrapper "${CMAKE_CURRENT_BINARY_DIR}/focused-check/${suite}Tests.cpp")
	file(CONFIGURE OUTPUT "${wrapper}"
		CONTENT "#include \"${CMAKE_CURRENT_SOURCE_DIR}/${suite}Tests.cpp\"\n")
	set_source_files_properties("${wrapper}" PROPERTIES
		COMPILE_DEFINITIONS "ESCAPE_ROOM_TEST_MAIN=run${suite}Tests")
	list(APPEND focused_check_sources "${wrapper}")
endforeach()
add_executable(escape_room_focused_check ${focused_check_sources})
target_link_libraries(escape_room_focused_check PRIVATE escape_room_audio_core)
target_compile_definitions(escape_room_focused_check PRIVATE
	ESCAPE_ROOM_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/assets"
)

add_test(NAME exercise_source_layout
	COMMAND "${CMAKE_COMMAND}"
		"-DWORKSHOP_ROOT=${PROJECT_SOURCE_DIR}"
		"-DACTIVE_MISSION=${ESCAPE_ROOM_MISSION}"
		"-DUSE_SOLUTION=${ESCAPE_ROOM_USE_SOLUTION}"
		"-DUSE_START_CODE=${ESCAPE_ROOM_USE_START_CODE}"
		"-DCOMPILED_SOURCES=$<JOIN:$<TARGET_PROPERTY:escape_room_audio_core,SOURCES>,|>"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/ExerciseLayoutTests.cmake"
)
