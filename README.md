# ESP32-S3 Hello World

An Arduino C++ project built with PlatformIO and edited in Zed.

## Quick reference

See [QUICKSTART.txt](QUICKSTART.txt) for everyday commands, configuration notes,
and BOOT/RESET instructions. Run `bash esp32.sh help` for short command shortcuts.
The working RTS/DTR settings are saved in `platformio.ini`, so
`bash esp32.sh monitor` opens the monitor without extra flags.

## Files

- `src/main.cpp`: firmware source; prints a greeting every second.
- `platformio.ini`: board, framework, USB flags, and serial port.
- `.zed/tasks.json`: build, upload, monitor, clean, list ports, and compile database tasks.
- `.pio/`: generated build output and dependencies (ignored by Git).

PlatformIO builds sources in `src/`.

## Run from Zed

Open this folder. In the command palette, choose `task: spawn`:

1. **ESP32: Build** compiles firmware; the board need not be connected.
2. Connect your ESP32-S3 through the connector labeled **USB**.
3. **ESP32: List ports** checks the connected device.
4. **ESP32: Upload** builds and writes firmware to the board.
5. **ESP32: Monitor** opens serial output at 115200 baud.

Expected output, once per second:

```text
Hello, world from ESP32-S3!
```

Stop the monitor with Ctrl+C before uploading again. If uploading cannot enter
download mode, hold BOOT, press and release RESET, release BOOT, then retry.
If the board remains in download mode after upload, release BOOT and press
and release RESET to start the program.
If the device path differs, update `upload_port` in `platformio.ini`;
monitor and test ports inherit that value.

Equivalent terminal commands, from this directory:

```sh
pio run
pio run --target upload
pio device monitor
```

PlatformIO Core must be on your PATH. If Zed cannot find `pio`, launch Zed from
a terminal that can run `pio --version`, or set the task command to the full
path to your installation (currently `/home/kaleb/.local/bin/pio`).

## How building works

A desktop command like `g++ main.cpp -o hello` compiles and links a program for
your computer. This board instead needs a cross compiler: a compiler that runs
on Linux but generates instructions for the ESP32-S3's Xtensa processor.

`pio run` reads `platformio.ini`, obtains the selected platform's toolchain and
Arduino framework, and uses SCons to coordinate these steps:

1. Preprocess headers and the USB definitions supplied through `build_flags`.
2. Compile your C++ source and required framework/library source into object files.
3. Link those objects and framework libraries using the board's memory layout.
   This produces `firmware.elf`, containing the executable and debugging information.
4. Convert the executable to `firmware.bin`, the image written to flash memory.

Output is under `.pio/build/esp32-s3-devkitc-1/`. Subsequent builds reuse
unchanged outputs. Use `pio run -v` to see the actual compiler and linker commands.

You do not need to write a CMake file or invoke GCC manually for this project.
PlatformIO supplies the build recipes; `platformio.ini` selects their inputs.
Zed simply launches the commands.

Uploading uses Espressif's flashing tool to write the firmware and required boot
images to the board. Monitoring only reads serial output; it does not compile
or upload code.

Arduino supplies the startup code and `main()`: it calls your `setup()` once,
then repeatedly calls `loop()`. Our example initializes USB serial in
`setup()` and prints every second in `loop()`.

The Espressif platform is pinned to 6.12.0 for repeatable builds. No external
library is required for Hello World. Add ESP32Servo to `lib_deps` when you
start using servos.

For C/C++ editor navigation, run **ESP32: Generate compile commands** after a
successful build. This generates `compile_commands.json` for clangd.
Regenerate it after adding libraries or changing the board/build configuration.
The project settings in `.zed/settings.json` allow clangd to query the ESP32-S3
compiler for system headers; `.clangd` filters unsupported GCC parsing options.
After changing these settings, run `editor: restart language server` in Zed's
command palette. If moving the project to another machine, update the compiler
path in `.zed/settings.json`.

Documentation:
- https://docs.platformio.org/en/latest/projectconf/index.html
- https://docs.platformio.org/en/latest/core/userguide/cmd_run.html
- https://zed.dev/docs/tasks
