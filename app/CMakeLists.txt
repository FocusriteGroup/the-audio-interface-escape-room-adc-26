juce_add_gui_app(EscapeRoomApp
    PRODUCT_NAME "Fix It Before Launch"
    COMPANY_NAME "Audio Interface Escape Room"
    BUNDLE_ID "com.audioescaperoom.fixitbeforelaunch"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/../assets/app-icon.svg"
)

juce_add_binary_data(EscapeRoomAssets
    SOURCES
        ../assets/audio/drum-loop.wav
        ../assets/audio/README.md
)

target_sources(EscapeRoomApp PRIVATE
    src/panels/AnalysisPanel.cpp
    src/panels/AnalysisPanel.h
    src/components/DiagnosticConsoleComponent.cpp
    src/components/DiagnosticConsoleComponent.h
    src/panels/DeviceDiagnosticsPanel.cpp
    src/panels/DeviceDiagnosticsPanel.h
    src/LookAndFeel.cpp
    src/LookAndFeel.h
    src/components/EqComponent.cpp
    src/components/EqComponent.h
    src/Header.cpp
    src/Header.h
    src/panels/InputPanel.cpp
    src/panels/InputPanel.h
    src/Main.cpp
    src/App.cpp
    src/App.h
    src/panels/OutputPanel.cpp
    src/panels/OutputPanel.h
    src/panels/RoutingPanel.cpp
    src/panels/RoutingPanel.h
    src/components/ScopeComponent.cpp
    src/components/ScopeComponent.h
    src/components/SpectrumComponent.cpp
    src/components/SpectrumComponent.h
    src/components/LevelMeterComponent.cpp
    src/components/LevelMeterComponent.h
)

target_compile_definitions(EscapeRoomApp PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

target_link_libraries(EscapeRoomApp PRIVATE
    EscapeRoomAssets
    escape_room_audio_core
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_generate_juce_header(EscapeRoomApp)

set(participant_app_directory "${CMAKE_BINARY_DIR}/bin")
if(APPLE)
    add_custom_command(TARGET EscapeRoomApp POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${participant_app_directory}"
        COMMAND "${CMAKE_COMMAND}" -E create_symlink
            "$<TARGET_BUNDLE_DIR:EscapeRoomApp>"
            "${participant_app_directory}/$<TARGET_BUNDLE_DIR_NAME:EscapeRoomApp>"
        VERBATIM
    )
else()
    add_custom_command(TARGET EscapeRoomApp POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${participant_app_directory}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:EscapeRoomApp>"
            "${participant_app_directory}/$<TARGET_FILE_NAME:EscapeRoomApp>"
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    target_sources(EscapeRoomApp PRIVATE ../tests/AppComponentTests.cpp)
    target_compile_definitions(EscapeRoomApp PRIVATE ESCAPE_ROOM_COMPONENT_TESTS=1)
    add_test(NAME app_smoke COMMAND $<TARGET_FILE:EscapeRoomApp> --smoke-test)
    if(ESCAPE_ROOM_MISSION STREQUAL "baseline" OR ESCAPE_ROOM_USE_SOLUTION)
        add_test(NAME app_components COMMAND $<TARGET_FILE:EscapeRoomApp> --component-test)
    endif()
endif()
