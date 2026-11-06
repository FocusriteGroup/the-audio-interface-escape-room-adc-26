#include "App.h"

#include <app-state/AppState.h>

#include <JuceHeader.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

#if defined(ESCAPE_ROOM_COMPONENT_TESTS)
int runWorkshopComponentTests();
#endif

namespace
{
int runSmokeTest()
{
    try
    {
        constexpr auto minimumWavSize = 44;
        if (BinaryData::drumloop_wavSize <= minimumWavSize
            || std::memcmp(BinaryData::drumloop_wav, "RIFF", 4) != 0
            || std::memcmp(BinaryData::drumloop_wav + 8, "WAVE", 4) != 0)
        {
            throw std::runtime_error("embedded drum loop is missing or invalid");
        }
        const auto credits = juce::String::fromUTF8(BinaryData::README_md, BinaryData::README_mdSize);
        if (!credits.contains("CC BY 3.0") || !credits.contains("bart")
            || !credits.contains("https://opengameart.org"))
            throw std::runtime_error("embedded drum-loop attribution is missing");

        audio_core::AppState state;
        state.inputs[0].gainDecibels = 3.5f;
        state.inputs[0].phaseInverted = true;
        state.inputs[1].equalizer.midGainDecibels = -2.25f;
        state.routing[0][1] = 0.5f;
        state.outputs[1].muted = true;

        const auto serialized = state.serialize();
        const auto restored = audio_core::AppState::deserialize(serialized);
        if (restored.serialize() != serialized)
            throw std::runtime_error("app-state JSON roundtrip changed the state");

        std::cout << "App smoke test passed\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "App smoke test failed: " << error.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "App smoke test failed: unknown error\n";
    }

    return EXIT_FAILURE;
}

class EscapeRoomApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "Fix It Before Launch";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }

    void initialise(const juce::String& commandLine) override
    {
        #if defined(ESCAPE_ROOM_COMPONENT_TESTS)
         if (commandLine.trim() == "--component-test")
         {
             setApplicationReturnValue(runWorkshopComponentTests());
             quit();
             return;
         }
        #endif
        if (commandLine.trim() == "--smoke-test")
        {
            setApplicationReturnValue(runSmokeTest());
            quit();
            return;
        }

        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name,
                             juce::Colour(0xff202124),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new App(), true);
            setResizable(true, true);
            setResizeLimits(1380, 860, 2200, 1400);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};
}

START_JUCE_APPLICATION(EscapeRoomApplication)
