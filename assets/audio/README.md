# Audio Fixtures

## Bundled Drum Loop

"Tribal Loop 1" from **2 tribal drum loops**, by **bart**, is distributed under
**CC BY 3.0**. No royalty payments are required; retain this attribution and the
licence link when redistributing the audio or application.

- Author: https://opengameart.org/users/bart
- Source: https://opengameart.org/content/2-tribal-drum-loops
- Original WAV: https://opengameart.org/sites/default/files/tribal_loop_1_1.wav
- Licence: https://creativecommons.org/licenses/by/3.0/
- OpenGameArt: https://opengameart.org/
- Local asset: `drum-loop.wav`, an unmodified copy of the original download.
- SHA256: `b7315223d850c42735dccfdc304d997fd85c6757b2abd6e3040a24c70db8910e`

The original is stereo float32, 44.1 kHz, 302,400 frames (6.857143 seconds).
At load time the app averages the channels and normalizes that mono signal to
unit peak. Playback applies the selected source peak and linear sample-rate
conversion. The original file is not changed. Source frequency affects Sine,
not the loop's tempo. Both virtual inputs can select the same mono loop.

The app embeds these credits in the diagnostics CREDITS tab. Distribution
archives also include this file. The loop is useful for transient and musical
testing, but it does not replace the exact generated exercise references.

## Calibration Fixture

`mission-01-input.wav` is a one-second, 48 kHz, mono PCM16 1 kHz sine wave at
0.4 linear amplitude, generated with the known-good gain stage at 0 dB. (The
offline renderer that wrote it has since been removed; any equivalent sine will do
if it ever needs regenerating.)

The app's Sine source generates the calibration signal directly in floating
point. Its frequency and peak are adjustable; RESET SOURCES restores the mission
defaults. This WAV remains a separate reference fixture, not the bundled loop.
