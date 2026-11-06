# Exercise 1: The Test Tone Sounds Distorted

The launch demo signal sounds harsh and the output waveform is flat-topped.

## Your Task

1. Reproduce the issue in the application.
2. Use the peak measurement and oscilloscope to identify where it begins.
3. Fix the defect in `exercise/Gain.cpp`.
4. Run the focused test to verify the repair.

From the repository root:

```sh
cmake -DEX=1 -DMODE=start -P scripts/exercise.cmake
```

This builds the app and tests, runs the focused test and opens the app on macOS
and Windows. On Linux it prints the app path instead. The focused test is
expected to fail before the repair:

```
+6.0206 dB should double amplitude: expected 2, got 4
```

After each edit, rerun just the test:

```sh
cmake -P scripts/check.cmake
```

It rebuilds the tests and prints a PASS/FAIL summary for this exercise's focused
check. To hear the change, close the app and run the exercise command again.

## Run The Application

If the app doesn't open by itself, it is in `build/workshop/app/bin/`:

| Platform | App |
| --- | --- |
| macOS | `Fix It Before Launch.app` (a shortcut; keep it inside the build folder) |
| Windows | `Fix It Before Launch.exe` |
| Linux | `Fix It Before Launch` |

The timestamp on the outer macOS `.app` folder may stay old when the executable
inside it is rebuilt.

Use the **Audio Output** section to select a device. Confirm that its diagnostic
reads `CALLBACK ACTIVE`, then use headphones at a comfortable level. Select
**Stop Signal** to pause playback at any point. If the oscilloscope remains
flat, check the selected output and callback diagnostic before investigating the
mission defect.

## Reproduction Steps

The app starts with both inputs at `0 dB` gain and **Input 2** muted, so only the
1 kHz test tone on **Input 1** reaches the outputs.

1. Select **Play Signal**. The tone peaks at `0.4` (about `-8 dBFS`) and sounds
   clean.
2. Turn **Input 1** gain up to `+6.0 dB`.
3. Expected: the amplitude doubles to a peak of about `0.8` (`-1.9 dBFS`), which
   is still below full scale, so the tone stays clean.
4. Actual: the output clips. The waveform is flat-topped and the tone sounds
   harsh.

For closer analysis, **Pause Scope** freezes the captured waveform while audio
and meters continue. **Time Scale** selects the visible history in milliseconds
and redraws the current capture, including while paused. **Amplitude Zoom**
magnifies only the drawing and does not change playback or measured levels.
Below 1.0x, dashed **FULL SCALE** lines mark 0 dBFS; anything drawn beyond them
turns amber, because the output device clips it.

If the app reports `NO CALLBACKS / CHECK SYSTEM AUDIO`, first confirm macOS can
play the fixture independently:

```sh
afplay assets/audio/mission-01-input.wav
```

An `AudioQueueStart failed ('stop')` result indicates a CoreAudio device failure,
not a workshop mission defect. Try another output first. If only one device
fails, reconnect or reset that device; if every device fails, quit conferencing
and audio applications, restart CoreAudio or the computer, verify `afplay`, and
then relaunch the app.

## Success Criteria

- `cmake -P scripts/check.cmake` reports the focused check passes.
- The output peak is approximately `-1.9 dBFS` at `+6.0 dB` gain.
- The clip indicator reports no clipped samples.
- You can explain why the original calculation applied too much gain.

Open one progressive hint at a time: [Hint 1](hint-1.md) · [Hint 2](hint-2.md) · [Hint 3](hint-3.md).
