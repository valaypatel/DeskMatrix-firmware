# DeskMatrix Desktop Emulator

A desktop SDL2 emulator for DeskMatrix (the ESP32-S3 LED matrix firmware).
It compiles the real, unmodified firmware `.cpp` files from
`firmware/DeskMatrix/` natively against a set of Arduino API shims, so
screens and clockfaces can be visually developed and iterated on without
the physical ESP32 board.

## Prerequisites

```bash
brew install sdl2
brew install llvm
```

`llvm` is required specifically because AppleClang's bundled libc++ is
missing `<cstdint>` on some machines. `emulator/CMakeLists.txt` looks for
Homebrew LLVM's `clang++` explicitly and will fail with a clear error
message if it isn't installed.

## Building and running

```bash
cmake -B emulator/build -S emulator
cmake --build emulator/build --target deskmatrix_emulator
./emulator/build/deskmatrix_emulator
```

The emulator can be run from any working directory (repo root,
`emulator/`, or `emulator/build/`) -- its asset and config paths resolve
relative to the executable's own location, not the process's current
working directory.

## Hotkeys

| Key      | Action                                  |
|----------|------------------------------------------|
| `1`      | Mario clockface                          |
| `2`      | Words clockface                          |
| `3`      | Pacman clockface                         |
| `4`      | NyanCat clockface (preset)               |
| `5`      | Canvas clockface (custom JSON)           |
| `d`      | DND screen                               |
| `b`      | BRB screen                               |
| `s`      | Screensaver                              |
| `c`      | Clock                                    |
| `p`      | Spotify screen                           |
| `Escape` | Quit                                     |

## Notes

- `emulator/build/` is a generated CMake build directory and is not
  committed to the repository (already covered by `.gitignore`).
