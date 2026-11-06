# Exercise 5: My Saved Setup Sounds Different

**Reported setup:** existing preset; original version unknown.

1. Reopen the app and load the saved preset.
2. Play the same audio used with that preset.
3. Repeat the load and listen to each output.

**Expected:** the saved listening setup is restored.

**Actual:** audio comes from a different output.

**Frequency:** repeatable with this preset; others unchecked.

## Investigation

Compare saving/reloading a new preset with loading
[the supplied older preset](../../assets/presets/legacy-v1.json). Use signal evidence
as well as control state; a current roundtrip alone is not sufficient.
Investigate the complete working set in `exercise/`. Its files include
correct processing stages; use measurements to choose where to inspect.

```sh
cmake -DEX=5 -DMODE=start -P scripts/exercise.cmake
```

The focused check fails in the start state.
After each edit, run `cmake -P scripts/check.cmake`. It rebuilds just the tests
and prints a PASS/FAIL summary for this exercise's focused check. To hear the
change, close the app and run the command above again. In the app, choose
**LOAD PRESET** after restarting it;
this exercise tests an explicitly saved file, not automatic session restoration.
See the [shared run guide](../README.md) for commands and launch paths.

**Pass:** current controls roundtrip unchanged, older presets restore their
declared routes, and invalid/unsupported states are rejected without unsafe indexing.

Open one hint at a time after recording your evidence: [Hint 1](hint-1.md) · [Hint 2](hint-2.md) · [Hint 3](hint-3.md).