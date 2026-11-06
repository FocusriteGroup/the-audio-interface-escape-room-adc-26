# Exercise 4: A Steady Tone Has A Faint Buzz

## Report

**Reported setup:** headphones, default signal and levels.

1. Start the supplied steady tone.
2. Try another buffer size in the device settings.
3. Switch back and compare the sound.

**Expected:** a clean, steady tone in either setting.

**Actual:** a faint buzz; its character changes with the setting.

**Frequency:** repeatable; CPU usage looks normal.

## Your Task

- Establish a repeatable reproduction and the first incorrect processing stage.
- Choose a useful diagnostic signal and record numerical evidence.
- Rule out a competing explanation before changing code.
- Repair the cause and verify it against independently specified samples.
- Explain or extend a regression case; do not just repeat the implementation.

This is an independent mission, not a continuation of earlier participant fixes.
Investigate the complete working set in `exercise/`, including correct
processing stages. The app and the tests share the same selected audio core.

## Build, Test And Run

Prerequisites: CMake 3.28+, Ninja, Git, and a C++20 compiler. From the repository
root:

```sh
cmake -DEX=4 -DMODE=start -P scripts/exercise.cmake
```

This builds the app and tests, runs the focused checks and opens the app on macOS
and Windows (Linux prints the app path). The checks fail in the start state.
Establish the first incorrect stage using signal evidence, then explain or extend
a test with an independent expected signal. After each edit, rerun the checks:

```sh
cmake -P scripts/check.cmake
```

It rebuilds the tests and prints a PASS/FAIL summary for both focused checks. To
hear the change, close the app and run the exercise command again.

Without an audio device or JUCE, add `-DAPP=OFF` before `-P` for a core-only
build in `build/workshop/core/`.

## In The App

Confirm the header says **EXERCISE 04**, select an output device, and check
**CALLBACK ACTIVE**. Use the default **Sine** sources, unity gains, neutral EQ,
and mono audition off. Listen briefly at a comfortable level. In **Audio Output**,
try two supported buffer sizes at the same sample rate; availability depends on
the device. The focused checks cover exact sizes even when a device does not.

Use the analysis tap/channel selectors, **Pause Scope**, and **Time Scale** to
inspect a capture. Output is measured after the transfer into the device buffer;
Post-Mix is before that transfer and the output controls.

## Choosing What To Measure

Rely on measurements rather than a musical impression. In the app, the spectrum
is the clearest view: a fault that repeats once per block adds extra lines either
side of the tone, spaced by the block rate (sample rate divided by buffer size),
which move when you change the buffer size. Compare the Post-Mix and Output taps.

Single wrong samples are harder to see on a scope, because a sine can hide one
near a zero crossing. Pause the scope and shorten **Time Scale** to look for brief
dips at the Output tap that aren't at Post-Mix.

The focused checks use signals that can't hide one: `testOutputBuffer()` in
`tests/CoreTests.cpp` copies alternating and ramp values through
`copyOutputBlock` at block sizes from 1 to 257 samples, and `verifyBuffering()` in
`tests/ScenarioTests.cpp` renders a constant 0.25 channel at the same sizes. Read
them, and use **Debug focused check** in VS Code to inspect individual samples.

## Success Criteria

- `cmake -P scripts/check.cmake` reports both focused checks pass.
- Output matches Post-Mix sample for sample on both channels, at any buffer size.
- Different partitions of the same input produce the same output.
- Empty, one-sample, odd-sized, and partial final blocks are handled safely.
- Destination guard samples remain unchanged; unused output capacity is silent.
- The audible symptom is checked again without a gain, smoothing, or buffer-size workaround.

Open one staged hint at a time after recording your evidence: [Hint 1](hint-1.md) ·
[Hint 2](hint-2.md) · [Hint 3](hint-3.md).