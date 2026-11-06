# Mission Guide

How to build, check and debug the exercises.

| Exercise | Brief | Runner |
| --- | --- | --- |
| 1 | [The test tone sounds distorted](01-test-tone/README.md) | `-DEX=1` |
| 2 | [No sound in the right channel](02-right-channel/README.md) | `-DEX=2` |
| 3 | [No sound when in mono](03-mono-playback/README.md) | `-DEX=3` |
| 4 | [A steady tone has a faint buzz](04-playback-buzz/README.md) | `-DEX=4` |
| 5 | [My saved setup sounds different](05-saved-setup/README.md) | `-DEX=5` |
| Combined | [My recording sounds wrong](final-customer-report/README.md) | `-DEX=final` |

Every exercise uses the same command from the repository root; only `EX` changes:

```sh
cmake -DEX=1 -DMODE=start -P scripts/exercise.cmake
```

Each exercise starts from its own code, not from your fixes to the one before, so
you can start any of them at any time.

The runner configures and builds the app and tests, checks the result, then opens
the app on macOS and Windows. Linux prints the executable path for manual launch.
The focused check at the end fails until you fix the exercise's bug; read the
failure to tell it apart from setup problems. A failing check never stops the
app opening, because the app is how you investigate.

Useful options, placed before `-P`:

- `-DLAUNCH=OFF`: build and check the app without opening a window.
- `-DAPP=OFF`: core-only build and tests, without JUCE or an audio device.
- `-DRESET=ON`: start the exercise again from its original code. Your edits are
  backed up first.
- `-DMODE=solution`: build and open the working version instead; see
  [Catch Up With A Solution](#catch-up-with-a-solution).

Dependencies are downloaded once through [CPM](https://github.com/cpm-cmake/CPM.cmake)
into `.cache/cpm/`. Set the `CPM_SOURCE_CACHE` environment variable to use a
different location, for example one cache shared by several checkouts.

## Your Code: `exercise/`

All your edits go in `exercise/` at the repository root. It holds the seven
processing files for the exercise you last ran: the processing chain, gain,
routing, output transfer, preset mapping, processor setup and recording metering.
Exercise 1 is a guided warm-up that tells you which file to edit. In later
exercises, correct code sits alongside the fault, so a file being there is not a
hint: use the signal path and the measurements to decide where to look.

The runner fills `exercise/` from each exercise's starting code. When you switch
exercise, it saves your edits to `build/workshop/saved/<exercise>/` and brings
them back when you return. Every run also backs up `exercise/` to
`build/workshop/backups/`. Copy anything you want to keep out of `build/` before
deleting it. Git ignores `exercise/`.

The [app overview](../docs/app-overview.md) shows where each of these files sits
in the signal path and when the app calls it.

Shared headers and supporting code stay in `audio-core/src/`. The hidden
`audio-core/.answers/` folder holds the correct versions of the seven files, which
solution mode builds. Comparing against it spoils the exercise.

## Check Your Fix

```sh
cmake -P scripts/check.cmake
```

Run this after every edit. It uses whichever exercise the runner last built,
rebuilds only the test programs (not the app), runs that exercise's focused check
and prints PASS or FAIL. Each failure starts with the test function and line it
came from, so you can open the check that failed. In VS Code, the same thing is
**Terminal > Run Task > Run focused check**. To hear a change, close the app and
run the exercise command again.

| Exercise | Focused check | Read |
| --- | --- | --- |
| 1 | `core_tests` | `tests/CoreTests.cpp`, the decibel checks in `main()` |
| 2 | `exercise_02_check` | `tests/MissionTests.cpp`, `testRouting()` |
| 3 | `exercise_03_check` | `tests/MissionTests.cpp`, `testMono()` |
| 4 | `exercise_04_check`, `exercise_04_render` | `tests/CoreTests.cpp`, `testOutputBuffer()`; `tests/ScenarioTests.cpp`, `verifyBuffering()` |
| 5 | `exercise_05_check` | `tests/MissionTests.cpp`, `testMigration()` |
| Combined | `final_check`, `final_recording_check`, `final_capture_check` | `tests/MissionTests.cpp`, `testRateLifecycle()`, `testRecording()`, `testCaptureClipCount()` |

The focused check is the exercise's regression test. Once it passes, read it and
convince yourself it would have caught the bug.

## Catch Up With A Solution

To see an exercise working, or to catch up after the review, close the app and
build the solution:

```sh
cmake -DEX=2 -DMODE=solution -P scripts/exercise.cmake
```

The app header shows `SOLUTION`. It keeps that exercise's inputs, controls and
setup, so you can repeat your reproduction against the working version. Solution
mode doesn't touch `exercise/`: run the same exercise with `-DMODE=start` to carry
on with your own code.

## The App

The runner opens the app on macOS and Windows. To open it yourself, or on Linux:

```sh
open -n "build/workshop/app/bin/Fix It Before Launch.app"
```

Windows PowerShell:

```powershell
& "build/workshop/app/bin/Fix It Before Launch.exe"
```

Linux:

```sh
"build/workshop/app/bin/Fix It Before Launch"
```

The path is the same for every exercise; the app header shows which one it is.
Confirm the header and **CALLBACK ACTIVE** before listening. Keep headphones at a
comfortable level and distorted listening brief. Close the app before rebuilding.

## Debugging In VS Code

You need two extensions: CodeLLDB (`vadimcn.vscode-lldb`), the debugger on macOS
and Linux, and Microsoft's C/C++ (`ms-vscode.cpptools`), the debugger on Windows.
This repository recommends both, under **Extensions: Show Recommended
Extensions**. Without them, F5 reports that the `lldb` or `cppvsdbg` debug type
isn't supported. Open the repository folder itself in VS Code, not a parent
folder.

Build an exercise with the runner first. Then choose a configuration in **Run and
Debug** and press F5. It rebuilds whichever exercise the runner last built, then
starts it under the debugger:

- **Debug app** opens the app. Set breakpoints in `exercise/`.
- **Debug focused check** runs the exercise's focused check. It repeats exactly
  the same steps every time and doesn't need an audio device.

On Windows, use the **(Windows)** configurations and start VS Code from a
Developer PowerShell with `code .`, so the build can find the compiler. Both
configurations use the app build, not a core-only `-DAPP=OFF` build.

Stopping at a breakpoint in the audio callback stops the audio, so expect a
dropout or device message when you continue. macOS may ask for permission to let
the debugger control the app the first time.

## CMake Presets

`CMakePresets.json` also defines presets such as `mission-01-debug` and
`core-debug`, for example for IDE integration. They build in their own
`build/<preset>` folders, so each compiles JUCE once more. Use the runner during
the workshop.
