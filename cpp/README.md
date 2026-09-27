# Capitol Defense (C++ / raylib)

A native desktop version of the game in `../index.html`, written in C++20 with [raylib](https://www.raylib.com/). It plays the same way: same towers, mobs, waves and balance.

## Build and run

You need a C++ compiler, CMake 3.16+ and Git. CMake downloads and builds raylib 5.5 automatically the first time, so there is nothing else to install.

```sh
cd cpp
cmake -B build
cmake --build build --config Release
```

Then run it:

| System | Command |
|---|---|
| Linux / macOS | `./build/capitol_defense` |
| Windows | `build\Release\capitol_defense.exe` |

### Getting the tools

- **Windows:** install [Visual Studio Community](https://visualstudio.microsoft.com/) with the "Desktop development with C++" workload (it includes CMake), and [Git](https://git-scm.com/). Run the commands above in the "Developer PowerShell for VS".
- **macOS:** `xcode-select --install`, then `brew install cmake`.
- **Linux (Debian/Ubuntu/Raspberry Pi OS):**
  `sudo apt install build-essential cmake git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`

## Controls

| Action | Mouse / keyboard | Gamepad |
|---|---|---|
| Choose a tower | Click a shop button, or 1–4 | LB / RB |
| Move the build cursor | Mouse, or arrow keys | D-pad |
| Build / select a tower | Click, or Enter | A |
| Build several in a row | Hold Shift | |
| Upgrade selected tower | Upgrade button, or U | X |
| Sell selected tower | Sell button, or Delete | |
| Cancel | Esc | B |
| Next wave | Next Wave button, or Space | Start |
| Speed 1×/2×/3× | Speed button, or F | Y |
| Pause | Pause button, or P | Select |

The window can be resized. The game scales to fit.

## Headless playtest

`./build/capitol_defense --sim` plays all 15 waves with a scripted player and prints the approval rating after each wave. It's useful for checking balance after changing numbers in `main.cpp`.
