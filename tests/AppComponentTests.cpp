#include "../app/src/App.h"

#include <Analysis.h>
#include <app-state/AppState.h>
#include <mission-setup/MissionSetup.h>
#include <EscapeRoomMission.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

int runWorkshopComponentTests();

namespace
{
// A silent stereo output with fixed rates and sizes, so the DEVICE tab can be
// laid out with its sample rate and buffer size controls and no real hardware.
class FakeOutputDevice final : public juce::AudioIODevice
{
public:
    FakeOutputDevice() : juce::AudioIODevice("Fake output", "Fake") {}
    juce::StringArray getOutputChannelNames() override { return { "Left", "Right" }; }
    juce::StringArray getInputChannelNames() override { return {}; }
    juce::Array<double> getAvailableSampleRates() override { return { 44'100.0, 48'000.0, 96'000.0 }; }
    juce::Array<int> getAvailableBufferSizes() override { return { 64, 128, 256, 512 }; }
    int getDefaultBufferSize() override { return 128; }
    juce::String open(const juce::BigInteger&, const juce::BigInteger& outputs,
                      double rate, int size) override
    {
        activeOutputs = outputs;
        sampleRate = rate;
        bufferSize = size;
        opened = true;
        return {};
    }
    void close() override { opened = false; }
    bool isOpen() override { return opened; }
    void start(juce::AudioIODeviceCallback*) override { playing = true; }
    void stop() override { playing = false; }
    bool isPlaying() override { return playing; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return bufferSize; }
    double getCurrentSampleRate() override { return sampleRate; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return activeOutputs; }
    juce::BigInteger getActiveInputChannels() const override { return {}; }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }

private:
    juce::BigInteger activeOutputs;
    double sampleRate = 48'000.0;
    int bufferSize = 128;
    bool opened = false;
    bool playing = false;
};

class FakeOutputDeviceType final : public juce::AudioIODeviceType
{
public:
    FakeOutputDeviceType() : juce::AudioIODeviceType("Fake") {}
    void scanForDevices() override {}
    juce::StringArray getDeviceNames(bool wantInputNames) const override
    {
        return wantInputNames ? juce::StringArray {} : juce::StringArray { "Fake output" };
    }
    int getDefaultDeviceIndex(bool) const override { return 0; }
    int getIndexOfDevice(juce::AudioIODevice* device, bool asInput) const override
    {
        return device != nullptr && !asInput ? 0 : -1;
    }
    bool hasSeparateInputsAndOutputs() const override { return true; }
    juce::AudioIODevice* createDevice(const juce::String& outputName, const juce::String&) override
    {
        return outputName.isEmpty() ? nullptr : new FakeOutputDevice();
    }
};
}

class WorkshopComponentTests
{
public:
    static int run()
    {
        try
        {
            auto component = std::make_unique<App>(false);
            component->stopTimer();
            auto& app = *component;
            auto& analysis = app.analysisPanel;
            auto& scope = analysis.oscilloscope;
            auto& diagnostics = app.deviceDiagnostics;
            scope.stopTimer();
            app.appState = {};
            app.appState.inputs[0].gainDecibels = 6.0206f;
            app.prepareToPlay(480, 48'000.0);
            app.publishAppState();
            app.isPlaying.store(true);
            for (std::size_t channel = 0; channel < 2; ++channel)
            {
                app.inputPanels[channel].frequencySlider.setValue(1'000.0, juce::sendNotificationSync);
                app.inputPanels[channel].amplitudeSlider.setValue(channel == 0 ? 0.4 : 0.2,
                                                        juce::sendNotificationSync);
            }
            juce::AudioBuffer<float> output(2, 480);
            const auto render = [&]
            {
                app.getNextAudioBlock({ &output, 0, output.getNumSamples() });
            };

            analysis.analysisTapSelector.setSelectedId(1, juce::sendNotificationSync);
            scope.timerCallback();
            render();
            near(analysis.pendingAnalysisPeak.load(), 0.4f, "input tap must meter input, not output gain");
            near(app.pendingOutputPeaks[0].load(), 0.8f, "output gain must remain independent of tap");
            scope.timerCallback();
            for (int sample = 0; sample < output.getNumSamples(); ++sample)
            {
                const auto expected = static_cast<float>(std::sin(
                    2.0 * std::numbers::pi * 1'000.0 * sample / 48'000.0));
                near(scope.history[static_cast<std::size_t>(sample)][0], 0.4f * expected,
                     "scope channel 1 must preserve generator frequency and peak");
                near(scope.history[static_cast<std::size_t>(sample)][1], 0.2f * expected,
                     "scope channel 2 must share channel 1's timeline");
            }
            analysis.analysisChannelSelector.setSelectedId(2, juce::sendNotificationSync);
            render();
            near(analysis.pendingAnalysisPeak.load(), 0.2f, "selected-channel peak must follow channel 2");

            app.appState = {};
            app.appState.inputs[1].phaseInverted = true;
            app.publishAppState();
            app.inputPanels[1].amplitudeSlider.setValue(0.4, juce::sendNotificationSync);
            app.resetSourcesRequested.store(true);
            analysis.analysisTapSelector.setSelectedId(5, juce::sendNotificationSync);
            app.monoAuditionButton.setToggleState(true, juce::dontSendNotification);
            app.monoAuditionButton.onClick();
            app.pendingOutputPeaks[0].store(0.0f);
            app.pendingOutputPeaks[1].store(0.0f);
            render();
            near(output.getMagnitude(0, 480), 0.0f, "mono must cancel opposite-phase sources");
            near(analysis.pendingAnalysisPeak.load(), 0.0f, "mono tap peak must show cancellation");
            near(app.pendingOutputPeaks[0].load(), 0.0f, "output peak must be measured after mono");
            analysis.refresh(true, juce::Time::getMillisecondCounter());
            require(analysis.peakValueLabel.getText().contains("-inf"), "silence must display negative infinity");

            app.resetSourcesButton.onClick();
            const auto setup = audio_core::setupForMission(ESCAPE_ROOM_MISSION_NAME);
            for (std::size_t channel = 0; channel < 2; ++channel)
            {
                near(static_cast<float>(app.sourceFrequencies[channel].load()), setup.frequencies[channel],
                     "reset must restore mission frequencies");
                near(app.sourceAmplitudes[channel].load(), setup.amplitudes[channel],
                     "reset must restore mission peak levels");
            }
            app.sourceTypes[0].store(App::SourceType::impulse);
            app.inputPanels[0].amplitudeSlider.setValue(0.25, juce::sendNotificationSync);
            app.resetSourcesRequested.store(true);
            render();
            near(app.inputScratch[0][0], 0.25f, "impulse peak must be adjustable to the routing reference");
            near(app.inputScratch[0][1], 0.0f, "impulse must last exactly one sample");
            require(app.sourceAudio.getNumSamples() > 0, "bundled audio must decode");
            near(app.sourceAudio.getMagnitude(0, app.sourceAudio.getNumSamples()), 1.0f,
                 "bundled audio must be normalized before source level is applied");
            app.inputPanels[0].sourceSelector.setSelectedId(static_cast<int>(App::SourceType::bundledWav),
                                                  juce::sendNotificationSync);
            require(!app.inputPanels[0].frequencySlider.isEnabled(), "loop source must not imply frequency changes its tempo");
            for (const auto rate : { 44'100.0, 48'000.0, 96'000.0 })
            {
                app.prepareToPlay(480, rate);
                app.inputPanels[0].amplitudeSlider.setValue(0.25, juce::sendNotificationSync);
                float peak = 0.0f;
                for (int block = 0; block < 32; ++block)
                {
                    render();
                    peak = std::max(peak, audio_core::analyze(
                        std::span<const float>(app.inputScratch[0]).first(480)).peak);
                }
                require(peak > 0.01f && peak <= 0.2501f,
                        "drum loop must produce bounded non-silent input at each workshop rate");
            }

            // Matching the two inputs one control at a time, as a participant
            // would, must still give identical signals (Exercise 3's mono check).
            app.prepareToPlay(480, 48'000.0);
            const auto requireMatchingInputs = [&](const char* message)
            {
                for (std::size_t sample = 0; sample < 480; ++sample)
                    near(app.inputScratch[1][sample], app.inputScratch[0][sample], message);
            };
            for (std::size_t channel = 0; channel < 2; ++channel)
            {
                app.inputPanels[channel].sourceSelector.setSelectedId(
                    static_cast<int>(App::SourceType::sine), juce::sendNotificationSync);
                app.inputPanels[channel].amplitudeSlider.setValue(0.4, juce::sendNotificationSync);
                app.inputPanels[channel].frequencySlider.setValue(channel == 0 ? 300.0 : 500.0,
                                                                  juce::sendNotificationSync);
            }
            render();
            app.inputPanels[0].frequencySlider.setValue(120.0, juce::sendNotificationSync);
            render();
            app.inputPanels[1].frequencySlider.setValue(120.0, juce::sendNotificationSync);
            render();
            requireMatchingInputs("sines set to the same frequency one at a time must stay in phase");
            app.inputPanels[0].sourceSelector.setSelectedId(
                static_cast<int>(App::SourceType::bundledWav), juce::sendNotificationSync);
            render();
            app.inputPanels[1].sourceSelector.setSelectedId(
                static_cast<int>(App::SourceType::bundledWav), juce::sendNotificationSync);
            render();
            requireMatchingInputs("drum loops selected one at a time must play in step");

            diagnostics.onMarkState();
            app.appState.routing = {{ {{ 0.0f, 1.0f }}, {{ 1.0f, 0.0f }} }};
            app.publishAppState();
            require(diagnostics.stateView.getText().toStdString()
                        == app.appState.serialize(),
                    "state view must show the current product JSON");
            require(diagnostics.differenceView.getText().contains("/routing/"),
                    "diff view must identify changed routes");
            diagnostics.resetTimingButton.onClick();
            diagnostics.publishAudioCallback(9.0, 128, 48'000.0);
            diagnostics.refresh(true);
            diagnostics.publishAudioCallback(0.01, 128, 48'000.0);
            diagnostics.refresh(true);
            const auto traceText = diagnostics.traceLines.joinIntoString("\n");
            require(traceText.contains("[UI]") && traceText.contains("[AUDIO]"),
                    "trace must retain both thread domains");
            require(diagnostics.displayedTraceLines.isEmpty() && diagnostics.traceChanged,
                    "hidden trace must collect events without updating its display");
            diagnostics.refreshTrace(1'000, true);
            require(diagnostics.displayedTraceLines == diagnostics.traceLines
                        && !diagnostics.traceChanged,
                    "opening trace must display pending events immediately");
            const auto initialTraceRows = diagnostics.getNumRows();
            diagnostics.appendTraceLine("throttled event");
            diagnostics.refreshTrace(1'199, true);
            require(diagnostics.getNumRows() == initialTraceRows && diagnostics.traceChanged,
                    "visible trace must not update more than five times per second");
            diagnostics.refreshTrace(1'200, true);
            require(diagnostics.displayedTraceLines == diagnostics.traceLines,
                    "visible trace must publish pending events after 200 milliseconds");
            diagnostics.refreshTrace(1'201, false);
            for (int event = 0; event < 10'000; ++event)
                diagnostics.appendTraceLine("retained event " + juce::String(event));
            diagnostics.refreshTrace(1'202, false);
            require(diagnostics.traceLines.size() == 256
                        && diagnostics.getNumRows() == initialTraceRows + 1,
                    "hidden trace history must stay bounded without changing displayed rows");
            diagnostics.refreshTrace(1'203, true);
            require(diagnostics.getNumRows() == 256
                        && diagnostics.displayedTraceLines[0] == "retained event 9744"
                        && diagnostics.displayedTraceLines[255] == "retained event 9999",
                    "reopening trace must immediately show the latest 256 events");
            diagnostics.pauseTraceButton.setToggleState(true, juce::dontSendNotification);
            diagnostics.pauseTraceButton.onClick();
            const auto heldTrace = diagnostics.displayedTraceLines;
            diagnostics.appendTraceLine("event while paused");
            diagnostics.refreshTrace(5'000, true);
            require(diagnostics.displayedTraceLines == heldTrace
                        && diagnostics.traceStatusLabel.getText().contains("1 NEW"),
                    "paused trace must hold its rows while still counting new events");
            diagnostics.pauseTraceButton.setToggleState(false, juce::dontSendNotification);
            diagnostics.pauseTraceButton.onClick();
            require(diagnostics.displayedTraceLines == diagnostics.traceLines
                        && diagnostics.displayedTraceLines[255] == "event while paused",
                    "resuming trace must show the events collected while paused");
            require(diagnostics.peakCallbackMilliseconds.load() >= 9.0,
                    "maximum duration must survive UI refreshes");
            diagnostics.resetTimingButton.onClick();
            near(static_cast<float>(diagnostics.peakCallbackMilliseconds.load()), 0.0f,
                 "timing reset must clear the maximum");

            app.appState = setup.product;
            app.updateAppStateControls();
            app.resetSourcesButton.onClick();
            app.monoAuditionButton.setToggleState(false, juce::dontSendNotification);
            app.monoAuditionButton.onClick();
            analysis.analysisChannelSelector.setSelectedId(1, juce::sendNotificationSync);
            app.prepareToPlay(480, 48'000.0);
            app.publishAppState();
            for (std::size_t channel = 0; channel < 2; ++channel)
            {
                app.pendingInputPeaks[channel].store(0.0f);
                app.pendingOutputPeaks[channel].store(0.0f);
                app.inputMeterBallistics[channel] = {};
                app.outputMeterBallistics[channel] = {};
            }
            analysis.resetMeasurements();
            analysis.smoothedCorrelation = 0.0f;
            analysis.displayedCorrelation = 0.0f;
            scope.timerCallback();
            for (int block = 0; block < 40; ++block)
            {
                render();
                scope.timerCallback();
                analysis.spectrum.timerCallback();
            }
            app.timerCallback();
            const auto waveform = scope.createComponentSnapshot(scope.getLocalBounds());
            int signalPixels = 0;
            for (int row = 20; row < waveform.getHeight(); ++row)
                for (int column = 44; column < waveform.getWidth(); ++column)
                {
                    const auto pixel = waveform.getPixelAt(column, row);
                    if ((pixel.getRed() > 160 && pixel.getGreen() < 130)
                        || (pixel.getGreen() > 140 && pixel.getRed() < 140))
                        ++signalPixels;
                }
            require(signalPixels > 200, "scope snapshot must contain a waveform, not just axes");

            // Exercise 1 sends a sine peaking near 1.6 to the device. Zoomed out, the
            // part above full scale must stand out, since the device clips it.
            const auto countOverFullScalePixels = [&](float zoom)
            {
                juce::AudioBuffer<float> loud(2, 960);
                for (int sample = 0; sample < loud.getNumSamples(); ++sample)
                {
                    const auto phase = 2.0 * std::numbers::pi * 1'000.0 * sample / 48'000.0;
                    loud.setSample(0, sample, 1.6f * static_cast<float>(std::sin(phase)));
                    loud.setSample(1, sample, 0.2f * static_cast<float>(std::sin(phase)));
                }
                scope.clear();
                scope.timerCallback();
                scope.setVerticalZoom(zoom);
                scope.pushBuffer(loud, 0, loud.getNumSamples());
                scope.timerCallback();
                const auto image = scope.createComponentSnapshot(scope.getLocalBounds());
                int pixels = 0;
                for (int row = 0; row < image.getHeight(); ++row)
                    for (int column = 0; column < image.getWidth(); ++column)
                    {
                        const auto pixel = image.getPixelAt(column, row);
                        if (pixel.getRed() > 200 && pixel.getGreen() > 150
                            && pixel.getGreen() < 215 && pixel.getBlue() < 120)
                            ++pixels;
                    }
                return std::pair { pixels, image };
            };
            const auto [zoomedOutPixels, zoomedOut] = countOverFullScalePixels(0.5f);
            require(zoomedOutPixels > 50,
                    "zoomed-out scope must mark signal above full scale");
            require(countOverFullScalePixels(1.0f).first == 0,
                    "at 1x zoom full scale is the plot edge, so nothing is marked");
            scope.clear();
            scope.timerCallback();
            const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("adc-workshop-component-tests").getChildFile(ESCAPE_ROOM_MISSION_NAME);
            require(directory.createDirectory().wasOk(), "snapshot directory must be writable");
            saveImage(zoomedOut, directory.getChildFile("scope-full-scale.png"));
            for (const auto size : { juce::Point<int>(1380, 860), juce::Point<int>(1440, 920),
                                     juce::Point<int>(1920, 1080) })
            {
                app.setSize(size.x, size.y);
                for (const auto& inputPanel : app.inputPanels)
                    require(app.getLocalBounds().contains(
                                app.getLocalArea(&inputPanel, inputPanel.frequencySlider.getBounds())),
                            "generator controls must remain inside the window");
                saveSnapshot(app, directory.getChildFile(juce::String(size.x) + "x"
                                                          + juce::String(size.y) + ".png"));
            }
            app.setSize(1440, 920);
            for (const auto tab : { 2, 3, 4 })
            {
                diagnostics.diagnosticsTabs.setCurrentTabIndex(tab);
                saveSnapshot(app, directory.getChildFile("tab-" + juce::String(tab) + ".png"));
            }
            // Exercise 2's setup routes Input 1 only to Output R, so Output L is
            // correctly silent there. Measure the Input tap, which always carries
            // the playing source, for the FFT checks.
            analysis.analysisTapSelector.setSelectedId(1, juce::sendNotificationSync);
            for (int block = 0; block < 40; ++block)
            {
                render();
                analysis.spectrum.timerCallback();
            }
            auto& spectrum = analysis.spectrum;
            const auto hasSpectrum = [&]
            {
                return std::any_of(spectrum.magnitudes.begin(), spectrum.magnitudes.end(),
                                   [](float level) { return level > -80.0f && level < 0.0f; });
            };
            const auto spectrumIsClear = [&]
            {
                return std::all_of(spectrum.magnitudes.begin(), spectrum.magnitudes.end(),
                                   [](float level) { return std::abs(level + 100.0f) < 0.0001f; });
            };
            require(hasSpectrum(), "playing generator must populate the FFT");
            app.header.onPlayToggled(false);
            app.timerCallback();
            require(spectrumIsClear(), "stopping the generator must clear the FFT display");
            require(spectrum.collectedSamples == 0 && spectrum.fifo.getNumReady() == 0,
                    "stopping must discard queued samples and the partial FFT frame");
            spectrum.pushSamples(std::span<const float>(app.inputScratch[0]).first(480));
            spectrum.timerCallback();
            require(spectrumIsClear() && spectrum.collectedSamples == 0
                        && spectrum.fifo.getNumReady() == 0,
                    "late samples must not restore the stopped FFT");
            app.header.onPlayToggled(true);
            app.timerCallback();
            for (int block = 0; block < 40; ++block)
            {
                render();
                spectrum.timerCallback();
            }
            require(hasSpectrum(), "restarting the generator must resume FFT analysis");
            app.header.onReplayToggled(true);
            app.header.onPlayToggled(false);
            app.timerCallback();
            spectrum.clear();
            for (int block = 0; block < 20; ++block)
            {
                spectrum.pushSamples(std::span<const float>(app.inputScratch[0]).first(480));
                spectrum.timerCallback();
            }
            require(hasSpectrum(), "replay must keep FFT analysis active with the generator stopped");
            app.header.onReplayToggled(false);
            app.timerCallback();
            require(spectrumIsClear(), "stopping replay must clear the FFT display");
            app.deviceManager.addAudioDeviceType(std::make_unique<FakeOutputDeviceType>());
            app.deviceManager.setCurrentAudioDeviceType("Fake", true);
            require(app.deviceManager.getCurrentAudioDevice() != nullptr, "fake output must open");
            app.deviceManager.dispatchPendingMessages();
            app.setSize(1380, 860);
            // JUCE's device selector only lists the device types that existed when it
            // was built, so the app's own panel can't select the fake type. Where there
            // are several types, as on Windows, it then shows no settings at all. Lay a
            // panel built after the fake type was added over the app's one instead.
            DeviceDiagnosticsPanel freshDiagnostics { app.deviceManager };
            app.addAndMakeVisible(freshDiagnostics);
            freshDiagnostics.setBounds(diagnostics.getBounds());
            freshDiagnostics.diagnosticsTabs.setCurrentTabIndex(0);
            auto& devicePage = freshDiagnostics.devicePage;
            std::vector<juce::ComboBox*> deviceControls;
            std::function<void(juce::Component&)> findControls = [&](juce::Component& parent)
            {
                for (auto* child : parent.getChildren())
                {
                    if (auto* comboBox = dynamic_cast<juce::ComboBox*>(child); comboBox != nullptr
                        && comboBox->isVisible())
                        deviceControls.push_back(comboBox);
                    findControls(*child);
                }
            };
            findControls(devicePage);
            saveSnapshot(app, directory.getChildFile("device-tab.png"));
            require(deviceControls.size() >= 3,
                    "DEVICE must always show output, sample rate and buffer size");
            for (auto* control : deviceControls)
                require(devicePage.getLocalBounds().contains(
                            devicePage.getLocalArea(control, control->getLocalBounds())),
                        "device settings must fit inside the DEVICE tab at the smallest window size");
            app.removeChildComponent(&freshDiagnostics);
            std::cout << "App component tests passed; snapshots: " << directory.getFullPathName() << '\n';
            return EXIT_SUCCESS;
        }
        catch (const std::exception& error)
        {
            std::cerr << "App component test failed: " << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }

private:
    static void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    static void near(float actual, float expected, const char* message)
    {
        require(std::isfinite(actual) && std::abs(actual - expected) < 0.0001f, message);
    }

    static void saveSnapshot(App& app, const juce::File& file)
    {
        saveImage(app.createComponentSnapshot(app.getLocalBounds()), file);
    }

    static void saveImage(const juce::Image& image, const juce::File& file)
    {
        const auto stream = file.createOutputStream();
        require(stream != nullptr, "snapshot output must open");
        require(stream->setPosition(0) && stream->truncate().wasOk(), "snapshot must replace previous output");
        juce::PNGImageFormat format;
        require(format.writeImageToStream(image, *stream), "snapshot must encode");
    }
};

int runWorkshopComponentTests()
{
    return WorkshopComponentTests::run();
}