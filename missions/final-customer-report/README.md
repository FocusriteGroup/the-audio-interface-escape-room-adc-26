# Combined Exercise: My Recording Sounds Wrong

**Reported setup:** internal loopback recording.

1. Start playback and a loopback recording at 48 kHz.
2. Change the app to 96 kHz during the session.
3. Stop recording and replay the captured audio.

**Expected:** the recording matches clean playback.

**Actual:** distorted recording, and playback crackles while recording; the capture
meter looks normal.

**Frequency:** reported after rate changes; trigger not isolated.

## Investigation

The supplied setup has a 240 Hz reference, a saved EQ setting, and Input 2 muted.
Keep these controls fixed. Establish a repeatable sequence, the first incorrect
stage, why the displayed capture level did not expose the symptom, and why
playback glitches while recording. The DEVICE tab shows callback timing: MAX is
the longest callback and MISSES counts callbacks that overran their budget.
Verify the repaired processing and the measurement independently.
Investigate the complete working set in `exercise/`, including correct
processing stages. Choose which paths to inspect from your reproduction and evidence.

```sh
cmake -DEX=final -DMODE=start -P scripts/exercise.cmake
```

The focused check fails in the start state.
After each edit, run `cmake -P scripts/check.cmake`. It rebuilds just the tests
and prints a PASS/FAIL summary for this exercise's focused check. To hear the
change, close the app and run the command above again. See the
[shared run guide](../README.md).

## Live Capture And Replay

Begin at 48 kHz, start **PLAY SIGNAL**, and enable
**LOOPBACK RECORD**. Change the output device setting to 96 kHz, allow a short
capture, disable recording, and enable **REPLAY CAPTURE** without changing rate.
The **CAPTURE** meter shows the recording level live. The readout beside it shows the
highest level reported for that capture, its length in frames and its count of
clipped samples.

Capture is bounded to 960,000 frames. A device-rate restart clears it and begins
a new segment if recording remains enabled. This avoids treating mixed-rate
frames as one WAV and keeps that separate problem out of the investigation.
Replay loops the latest segment without rerunning it through EQ. Stay at its
captured rate; changing rate clears the segment and disables replay. No virtual
driver, file recording, or feedback connection is required.

Device support varies, and Bluetooth headphones usually can't run at 96 kHz. Try
laptop speakers or wired headphones. If 96 kHz still isn't available, the focused
check reproduces the rate change without a device: read `testRateLifecycle()` in
`tests/MissionTests.cpp` and step through it with **Debug focused check**. Listen
briefly at a comfortable level. Keep mono audition off.

**Pass:** the same playback and rate-change sequence preserves the intended sound;
capture and replay stay bounded, the reported capture level matches the
recorded signal, and recording no longer causes deadline misses. Verify the signal and its displayed measurement independently.

Open one hint at a time after recording your evidence: [Hint 1](hint-1.md) · [Hint 2](hint-2.md) · [Hint 3](hint-3.md).