# Exercise 2: No Sound In The Right Channel

**Reported setup:** headphones; audio on both inputs.

1. Start playback on both inputs.
2. Route Input 1 to Output R only.
3. Listen to each headphone channel.

**Expected:** Input 1 on the right; nothing on the left.

**Actual:** the right is silent and the left plays Input 2, which isn't routed.

**Frequency:** every attempt with these settings.

## Investigation

The start app loads the reported route, Input 1 to Output R, in the **ROUTING
MATRIX** panel in the middle of the app: rows are outputs, columns are inputs.
Both inputs play their own tone, Input 1 at 1 kHz and Input 2 at 250 Hz, so you
can tell which input reaches which output by ear, meter or spectrum. Keep gain
and EQ neutral. Find the first incorrect stage, rule out a competing explanation, and
verify both wanted and unwanted outputs with an independent reference.
Investigate the complete working set in `exercise/`. Its files include
correct processing stages; use measurements to choose where to inspect.

```sh
cmake -DEX=2 -DMODE=start -P scripts/exercise.cmake
```

The focused check fails in the start state.
After each edit, run `cmake -P scripts/check.cmake`. It rebuilds just the tests
and prints a PASS/FAIL summary for this exercise's focused check. To hear the
change, close the app and run the command above again. The full suite and app
paths are in the [shared run guide](../README.md).

**Pass:** all four individual routes reach only their intended destination;
the selected route is audible and its unwanted output stays silent.

Open one hint at a time after recording your evidence: [Hint 1](hint-1.md) · [Hint 2](hint-2.md) · [Hint 3](hint-3.md).