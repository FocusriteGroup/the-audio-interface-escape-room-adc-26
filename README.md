# Fix It Before Launch - The Audio Interface Escape Room

An Audio Developer Conference 2026 workshop on diagnosing and fixing audio
software defects. A JUCE app models a fictional two-in, two-out audio interface
that is about to ship, and customers have already found some problems.

There are five exercises and a combined exercise. Each one starts from a customer
bug report. No audio interface, virtual driver or DAW is needed.

**This is the pre-workshop release.** It contains Exercise 1, so you can install
the tools, build once and check your audio before the day. The other exercises
will be added to this repository before the workshop; see
[Getting The Other Exercises](#getting-the-other-exercises).

## Before The Workshop

Please do all of this before the day. Conference Wi-Fi can be slow, and the
compiler is a large download.

Bring wired headphones, with an adapter if your laptop has no headphone jack.
Bluetooth headphones usually can't run at the 96 kHz sample rate or the
128-sample buffer some exercises use.

The workshop uses VS Code on every platform, with a command-line compiler
underneath. You don't need Xcode or the Visual Studio IDE.

### 1. Install a compiler

**macOS:** Apple's Command Line Tools.

```sh
xcode-select --install
```

Or download **Command Line Tools for Xcode** from
[Apple's developer downloads](https://developer.apple.com/download/all/?q=command%20line%20tools)
(needs a free Apple account).

**Windows:** [Build Tools for Visual Studio 2026](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2026)
with the **Desktop development with C++** workload. From PowerShell:

```powershell
winget install Microsoft.VisualStudio.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Or download the installer from the link above and tick
**Desktop development with C++**.

**Linux:** the compiler comes with the packages in step 2.

### 2. Install Git, CMake, Ninja and VS Code

**macOS**, with [Homebrew](https://brew.sh). Git comes with the Command Line Tools.

```sh
brew install cmake ninja
brew install --cask visual-studio-code
```

**Windows**, in PowerShell. Open a new PowerShell window afterwards so it finds
the new commands.

```powershell
winget install Git.Git Kitware.CMake Ninja-build.Ninja Microsoft.VisualStudioCode
```

**Linux (Ubuntu 24.04)**

```sh
sudo apt update
sudo apt install git cmake ninja-build g++ \
    libasound2-dev libjack-jackd2-dev ladspa-sdk \
    libfreetype-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
    libxrandr-dev libxrender-dev libxi-dev \
    libglu1-mesa-dev mesa-common-dev libegl-dev
sudo snap install code --classic
```

Older Ubuntu releases ship a CMake older than 3.28; install a newer one from
[cmake.org](https://cmake.org/download/). Other distributions need the
equivalent packages.

**Or download the installers.** On Windows, check `cmake --version` in
**Developer PowerShell** first: the Build Tools usually include CMake
and Ninja, so you may only need Git and VS Code.

| Tool | Download | After installing |
| --- | --- | --- |
| Git | [git-scm.com](https://git-scm.com/downloads) | Not needed on macOS: it comes with the Command Line Tools. |
| CMake | [cmake.org](https://cmake.org/download/) | Windows: choose **Add CMake to the system PATH**. macOS: open CMake.app and follow **Tools > How to Install For Command Line Use**. |
| Ninja | [GitHub releases](https://github.com/ninja-build/ninja/releases) | It's a single program. macOS: copy it with `sudo mkdir -p /usr/local/bin && sudo cp ninja /usr/local/bin/`, then allow it to run with `sudo xattr -d com.apple.quarantine /usr/local/bin/ninja`. Windows: put `ninja.exe` in a folder on your PATH. |
| VS Code | [code.visualstudio.com](https://code.visualstudio.com/download) | macOS: in VS Code, run **Shell Command: Install 'code' command in PATH** from the Command Palette. |

### 3. Install the VS Code extensions

The same on every platform:

```sh
code --install-extension vadimcn.vscode-lldb
code --install-extension ms-vscode.cpptools
```

Or install them from the Extensions view in VS Code:
[CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb)
and [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools).

CodeLLDB is the debugger on macOS and Linux. Microsoft's C/C++ is the debugger
on Windows, and gives code navigation everywhere.

### 4. Build once

On Windows, open **Developer PowerShell** (search the Start menu for it) and use
it for every command from here on; it sets up the compiler. Elsewhere, any
terminal works. Check the tools are found, then clone and build Exercise 1:

```sh
git --version
cmake --version
ninja --version
git clone https://github.com/FocusriteGroup/the-audio-interface-escape-room-adc-26.git
cd the-audio-interface-escape-room-adc-26
cmake -DEX=1 -DMODE=start -P scripts/exercise.cmake
```

The first run downloads dependencies and compiles JUCE, which takes a few minutes
and about 500 MB of disk. After that, everything works offline and switching
exercises takes seconds.

**The check at the end is meant to fail.** It is Exercise 1's bug, and the
output ends with:

```
+6.0206 dB should double amplitude: expected 2, got 4
```

Compiler, dependency or device errors are not part of the exercise; those are
what to sort out before the day.

### 5. Check your audio

On macOS and Windows the app opens automatically. On Linux, run it from
`build/workshop/app/bin/`.

1. Check the header shows Exercise 1.
2. In the **Audio Output** section, choose your headphones. The diagnostic
   should read **CALLBACK ACTIVE**.
3. Turn your volume down, then select **Play Signal**. You should hear a clean
   1 kHz tone. Select **Stop Signal** to stop it.

If the app reports **NO CALLBACKS / CHECK SYSTEM AUDIO**, see the troubleshooting
notes in the [Exercise 1 brief](missions/01-test-tone/README.md#run-the-application).

Finally, open the folder in VS Code with `code .`. On Windows, run that from the
Developer PowerShell too, so VS Code's builds can find the compiler. The
[mission guide](missions/README.md#debugging-in-vs-code) explains how to debug.

## The Workshop

Each exercise is a customer bug report. You reproduce it in the app, use the
oscilloscope, meters, spectrum and diagnostics to find the first stage where the
signal goes wrong, fix the C++, then prove the fix with the exercise's focused
check. Hints are there if you get stuck.

| Exercise | Customer Report | Available |
| --- | --- | --- |
| 1 | [The test tone sounds distorted](missions/01-test-tone/README.md) | Now |
| 2 | No sound in the right channel | Before the workshop |
| 3 | No sound when in mono | Before the workshop |
| 4 | A steady tone has a faint buzz | Before the workshop |
| 5 | My saved setup sounds different | Before the workshop |
| Combined | My recording sounds wrong | Before the workshop |

Exercise 1 is a short guided warm-up that opens the workshop. You're welcome to
look at it now, but there's no need to solve it beforehand.

All your code changes go in one folder, `exercise/`, which holds the processing
files for the exercise you last ran. The [mission guide](missions/README.md)
covers checking your fix, debugging in VS Code and the build options.

## Getting The Other Exercises

Before the workshop, update your copy from the repository folder:

```sh
git pull
cmake -DEX=1 -DMODE=start -P scripts/exercise.cmake
```

Your edits live in `exercise/`, which Git ignores, so pulling never overwrites
them. Rebuilding afterwards keeps the build up to date, so switching exercises
on the day takes seconds.

## Logging

The app's **LOG** tab shows your own instrumentation. These calls are safe to
use in the audio callback:

```cpp
#include <logging/DiagnosticLog.h>

audio_core::logDiagnostic("reached here");
audio_core::logDiagnosticValue("my value", someValue);
```

## Licence

The workshop code is released under the [MIT licence](LICENSE). The build
downloads [JUCE](https://juce.com) (AGPLv3 or commercial licence) and
[nlohmann/json](https://github.com/nlohmann/json) (MIT). The bundled drum loop is
"Tribal Loop 1" by bart, licensed CC BY 3.0; see
[assets/audio/README.md](assets/audio/README.md).
