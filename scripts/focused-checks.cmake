# Each exercise's focused check: the CTest tests that fail until its bug is fixed,
# its title and where to read them. Used by check.cmake.

set(focused_check_title_01-test-tone "Exercise 1: The test tone sounds distorted")
set(focused_check_tests_01-test-tone core_tests)
set(focused_check_source_01-test-tone "tests/CoreTests.cpp, the decibel checks in main()")

set(focused_check_title_02-right-channel "Exercise 2: No sound in the right channel")
set(focused_check_tests_02-right-channel exercise_02_check)
set(focused_check_source_02-right-channel "tests/MissionTests.cpp, testRouting()")

set(focused_check_title_03-mono-playback "Exercise 3: No sound when in mono")
set(focused_check_tests_03-mono-playback exercise_03_check)
set(focused_check_source_03-mono-playback "tests/MissionTests.cpp, testMono()")

set(focused_check_title_04-playback-buzz "Exercise 4: A steady tone has a faint buzz")
set(focused_check_tests_04-playback-buzz exercise_04_check exercise_04_render)
set(focused_check_source_04-playback-buzz
    "tests/CoreTests.cpp, testOutputBuffer(); tests/ScenarioTests.cpp, verifyBuffering()")

set(focused_check_title_05-saved-setup "Exercise 5: My saved setup sounds different")
set(focused_check_tests_05-saved-setup exercise_05_check)
set(focused_check_source_05-saved-setup "tests/MissionTests.cpp, testMigration()")

set(focused_check_title_final-customer-report "Combined exercise: My recording sounds wrong")
set(focused_check_tests_final-customer-report final_check final_recording_check final_capture_check)
set(focused_check_source_final-customer-report
    "tests/MissionTests.cpp, testRateLifecycle(), testRecording() and testCaptureClipCount()")
