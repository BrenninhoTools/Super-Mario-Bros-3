# Super-Mario-Bros-3

A native port of Super Mario Bros. 3 (NES) written in C++ for Android, iOS, Windows, macOS and Linux.

The game is not emulated. The original 6502 code from the disassembly in `asm/` is assembled and statically translated into C++ at build time by `tools/smb3cpp.py`, one source file per MMC3 PRG bank, and runs on a native platform layer built on SDL2 (video, audio, input and touch controls).

## ROM

No Nintendo graphics or audio are included. You need your own dump of **Super Mario Bros. 3 (USA) (Rev 1)** (also called PRG1, `.nes` with iNES header). The program checks the PRG data and only accepts this version.

- **Windows / Linux:** drag and drop the ROM onto the window on first launch, or place it next to the executable as `baserom.nes`, or pass its path as the first argument. It is copied to the user data folder so it only needs to be done once.
- **macOS:** drag the ROM onto the window or onto the app icon. The app is ad-hoc signed, so the first launch needs right-click → Open.
- **Android:** the file picker opens on first launch. Select the ROM and it is stored in the app's private storage.
- **iOS:** the file picker opens on first launch, or copy the ROM as `baserom.nes` into the app's folder in the Files app (On My iPhone → Super Mario Bros. 3). The IPA is unsigned, so install it with AltStore, SideStore or TrollStore.

## Controls

| NES      | Keyboard              | Gamepad        |
|----------|-----------------------|----------------|
| D-Pad    | Arrows / WASD         | D-Pad / Left stick |
| A (jump) | X / K / Space         | A / Y          |
| B (run)  | Z / J / Left Shift    | B / X          |
| Start    | Enter                 | Start          |
| Select   | Right Shift / Tab     | Back           |

Extra keys: `F11` or `Alt+Enter` fullscreen window, `F4` switch between filling the screen and the original aspect ratio, `F2` reset, `P` pause, `F3` performance overlay, `Esc` quit.

The game fills the whole screen by default. The button in the top-right corner (shown when the mouse moves on desktop, always on mobile) switches back to the original aspect ratio, and the choice is saved.

On Android and iOS, NES-style touch controls are shown, with an analog stick for movement (8 directions, the finger that grabs it keeps control even outside the base). **A** jumps and **B** runs, attacks with the tail or throws fireballs while held. The yellow **C** button is a run toggle: tap it once and B stays held (a green ring and `RUN` appear), tap it again to release. In fill mode the controls become translucent so the game stays visible. Using a gamepad hides the touch controls, and touching the screen shows them again.

## Building

Clone with submodules:

```
git clone --recursive https://github.com/BrenninhoTools/Super-Mario-Bros-3
```

Requirements: CMake 3.16+, a C++17 compiler and Python 3.

### Windows / Linux

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Linux, install the usual SDL2 build dependencies (X11/Wayland, ALSA/PulseAudio). Use `-DSMB_SYSTEM_SDL=ON` to link against a system SDL2 instead of `external/SDL`.

### macOS

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --parallel
```

The app bundle is `build/SuperMarioBros3.app`.

### iOS

```
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-ios --config Release -- -sdk iphoneos CODE_SIGNING_ALLOWED=NO
```

Or open `build-ios/SuperMarioBros3.xcodeproj` in Xcode to sign and run it on a device.

### Android

Requires the Android SDK, NDK `27.2.12479018` and CMake `3.22.1`.

```
cd android
./gradlew assembleRelease
```

The APK is written to `android/app/build/outputs/apk/release/`.

## How it works

1. `tools/nesasm.py` is a NESASM-compatible assembler (macros, `.func`, local labels, banks) that rebuilds the 256 KB PRG ROM from the disassembly. Its output is byte-for-byte identical to the original Rev 1 cartridge, and the build embeds its CRC so only the matching ROM is accepted.
2. `tools/smb3cpp.py` turns every instruction into C++ with a label at its original address, one function per 8 KB bank. Branches and jumps inside a bank become `goto`. Calls, returns and jumps into another bank go through a dispatcher that looks up which bank the MMC3 currently maps at that address, so bank switching works exactly as on the cartridge. `rts` and `rti` use the real 6502 stack, which keeps the game's `DynJump` jump tables and stack tricks working.
3. Every instruction adds its cycle count, so the game runs the same amount of work per frame as the console, including lag frames. Busy-wait loops (waiting for V-Blank or for the NMI) are detected at build time.
4. Any address that was not marked as an entry point falls back to a small 6502 interpreter until it reaches translated code again. In normal play this fallback is never used.
5. The MMC3 is implemented natively: PRG and CHR bank switching, mirroring, 8 KB of work RAM and the scanline IRQ counter. The PPU renders scanline by scanline with full scroll register behaviour, so the status bar split, the raster effects and mid-frame CHR swaps are drawn correctly. The APU synthesizes both pulse channels, triangle, noise and the DMC sample channel used for the drums.

Level data, graphics and music are read from the user's ROM at runtime.

## Performance

- **Frame pacing:** when the display runs at a multiple of 60 Hz (60, 120, 240, including 59.94), the game advances exactly one frame every 1 or 2 vsyncs, so there are no doubled or skipped frames. Other refresh rates (90, 144 Hz) or a display without working vsync fall back to a precise timer.
- **Audio sync:** instead of dropping audio when the queue drifts, the output rate is nudged by up to ±0.5% to keep a small, constant buffer. This removes crackles and keeps latency low.
- **Speed:** a full frame (game logic, video and audio) takes about 0.25 ms on a desktop CPU.
- **Android:** the app asks for a 120 or 60 Hz display mode so frame pacing can lock, enables sustained performance mode when the device supports it, and keeps the screen on.

Press `F3` on desktop to see game FPS, display FPS, frame time, pacing mode and audio queue.

## Credits

- Disassembly by Southbird ([captainsouthbird/smb3](https://github.com/captainsouthbird/smb3)), in `asm/`.
- [SDL2](https://github.com/libsdl-org/SDL) for the platform layer.
- Super Mario Bros. 3 is © Nintendo. This project is not affiliated with Nintendo.
