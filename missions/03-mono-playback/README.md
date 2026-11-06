# Exercise 3: No Sound When In Mono

**Reported setup:** stereo playback, headphones.

1. Play the supplied example in stereo.
2. Turn on **MONO AUDITION**, under the outputs.
3. Switch back to stereo and compare.

**Expected:** the same content remains audible.

**Actual:** silence in mono, although both outputs play in stereo.

**Frequency:** repeatable with this example.

## Investigation

The start uses identical 120 Hz, peak-0.4 signals with normal user controls.
Choose evidence that distinguishes a gain problem from a relationship between
channels. Locate the first unexpected change and verify both listening modes.
Investigate the complete working set in `exercise/`. Its files include
correct processing stages; use measurements to choose where to inspect.

```sh
cmake -DEX=3 -DMODE=start -P scripts/exercise.cmake
```

The focused check fails in the start state.
After each edit, run `cmake -P scripts/check.cmake`. It rebuilds just the tests
and prints a PASS/FAIL summary for this exercise's focused check. To hear the
change, close the app and run the command above again. See the
[shared run guide](../README.md).

**Pass:** aligned content survives mono; intentionally inverted content still
cancels. Both channel waveforms and user phase controls match the reference.

Open one hint at a time after recording your evidence: [Hint 1](hint-1.md) · [Hint 2](hint-2.md) · [Hint 3](hint-3.md).