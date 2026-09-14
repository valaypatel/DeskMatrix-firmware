# emulator/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Find Homebrew LLVM's clang++ before initializing the project
# (AppleClang's bundled libc++ is missing <cstdint> on this machine)
find_program(HOMEBREW_LLVM_CLANGXX clang++
  PATHS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
  NO_DEFAULT_PATH
)
if(NOT HOMEBREW_LLVM_CLANGXX)
  message(FATAL_ERROR
    "This project requires Homebrew LLVM's clang++ because AppleClang's "
    "bundled libc++ is missing <cstdint>. Install it with: brew install llvm"
  )
endif()

# Set the compiler BEFORE project() so it's used for detection
set(CMAKE_CXX_COMPILER ${HOMEBREW_LLVM_CLANGXX})

# PNGdec bundles its own small zlib-derived inflate implementation as
# plain C sources (adler32.c, crc32.c, inflate.c, ...) -- enable C too so
# deskmatrix_emulator can compile them directly instead of depending on
# a system zlib.
find_program(HOMEBREW_LLVM_CLANG clang
  PATHS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
  NO_DEFAULT_PATH
)
if(HOMEBREW_LLVM_CLANG)
  set(CMAKE_C_COMPILER ${HOMEBREW_LLVM_CLANG})
endif()

project(deskmatrix_emulator CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

set(ARDUINO_SHIM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/arduino_shim)
set(GFX_LIB_DIR "$ENV{HOME}/Documents/Arduino/libraries/Adafruit_GFX_Library")
set(ANIMATEDGIF_LIB_DIR "$ENV{HOME}/Documents/Arduino/libraries/AnimatedGIF")
find_path(ANIMATEDGIF_SRC_DIR AnimatedGIF.h PATHS ${ANIMATEDGIF_LIB_DIR}/src)

add_executable(smoke_test
    smoke_test.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
)
target_include_directories(smoke_test PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
)
target_compile_options(smoke_test PRIVATE -Wno-inconsistent-missing-override)

add_executable(panel_test
    panel_test.cpp
    NativePanel.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
)
target_include_directories(panel_test PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(panel_test PRIVATE SDL2::SDL2)

add_executable(screensaver_test
    screensaver_test.cpp
    NativePanel.cpp
    NativeFS.cpp
    PanelPresentNative.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
    ${ANIMATEDGIF_LIB_DIR}/src/AnimatedGIF.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix/ScreensaverScreen.cpp
)
target_include_directories(screensaver_test PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
    ${ANIMATEDGIF_LIB_DIR}/src
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix
)
target_link_libraries(screensaver_test PRIVATE SDL2::SDL2)

find_path(JPEGDEC_LIB_DIR JPEGDEC.h PATHS "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src")

add_executable(spotify_test
    spotify_test.cpp
    NativePanel.cpp
    PanelPresentNative.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src/JPEGDEC.cpp"
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix/SpotifyScreen.cpp
)
target_include_directories(spotify_test PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src"
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix
)
target_link_libraries(spotify_test PRIVATE SDL2::SDL2)

set(DESKMATRIX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/DeskMatrix)
# Header-only; CanvasClockface.h (pulled in transitively by ClockScreen.cpp)
# includes <ArduinoJson.h> directly for parsing canvasJson themes.
set(ARDUINOJSON_LIB_DIR "$ENV{HOME}/Documents/Arduino/libraries/ArduinoJson/src")

add_executable(deskmatrix_emulator
    main.cpp
    NativePanel.cpp
    NativeFS.cpp
    PanelPresentNative.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
    ${ANIMATEDGIF_LIB_DIR}/src/AnimatedGIF.cpp
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src/JPEGDEC.cpp"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/PNGdec.cpp"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/adler32.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/crc32.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/infback.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/inffast.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/inflate.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/inftrees.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/zutil.c"
    ${DESKMATRIX_DIR}/ClockScreen.cpp
    ${DESKMATRIX_DIR}/ScreensaverScreen.cpp
    ${DESKMATRIX_DIR}/DndScreen.cpp
    ${DESKMATRIX_DIR}/BrbScreen.cpp
    ${DESKMATRIX_DIR}/SpotifyScreen.cpp
    ${DESKMATRIX_DIR}/CWDateTime.cpp
    ${DESKMATRIX_DIR}/EventBus.cpp
    ${DESKMATRIX_DIR}/Locator.cpp
    ${DESKMATRIX_DIR}/Sprite.cpp
    ${DESKMATRIX_DIR}/MarioClockface.cpp
    ${DESKMATRIX_DIR}/MarioBlock.cpp
    ${DESKMATRIX_DIR}/MarioSprite.cpp
    ${DESKMATRIX_DIR}/WordsClockface.cpp
    ${DESKMATRIX_DIR}/PacmanClockface.cpp
    ${DESKMATRIX_DIR}/PacmanClockfaceAi.cpp
    ${DESKMATRIX_DIR}/PacmanClockfaceMap.cpp
    ${DESKMATRIX_DIR}/PacmanClockfacePathfinding.cpp
    ${DESKMATRIX_DIR}/PacmanEntity.cpp
    ${DESKMATRIX_DIR}/PacmanGhost.cpp
    ${DESKMATRIX_DIR}/PacmanSprite.cpp
    ${DESKMATRIX_DIR}/CanvasClockface.cpp
    ${DESKMATRIX_DIR}/EzTimeFormat.cpp
    FirmwareVtableStubs.cpp
)
target_include_directories(deskmatrix_emulator PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
    ${ANIMATEDGIF_LIB_DIR}/src
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src"
    ${ARDUINOJSON_LIB_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DESKMATRIX_DIR}
)
target_link_libraries(deskmatrix_emulator PRIVATE SDL2::SDL2)

# Development-only utility: preview any Canvas Clockface theme JSON file
# (e.g. from jnthas/clock-club's shared/ presets) through the real,
# unmodified CanvasClockface.cpp/ClockScreen.cpp. Same source list as
# deskmatrix_emulator (ClockScreen.cpp's non-canvas branches still
# reference Mario/Words/Pacman as fallback/case targets) with main.cpp
# swapped for canvas_preview.cpp.
add_executable(canvas_preview
    canvas_preview.cpp
    NativePanel.cpp
    NativeFS.cpp
    PanelPresentNative.cpp
    ${ARDUINO_SHIM_DIR}/Print.cpp
    ${GFX_LIB_DIR}/Adafruit_GFX.cpp
    ${ANIMATEDGIF_LIB_DIR}/src/AnimatedGIF.cpp
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src/JPEGDEC.cpp"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/PNGdec.cpp"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/adler32.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/crc32.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/infback.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/inffast.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/inflate.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/inftrees.c"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src/zutil.c"
    ${DESKMATRIX_DIR}/ClockScreen.cpp
    ${DESKMATRIX_DIR}/CWDateTime.cpp
    ${DESKMATRIX_DIR}/EventBus.cpp
    ${DESKMATRIX_DIR}/Locator.cpp
    ${DESKMATRIX_DIR}/Sprite.cpp
    ${DESKMATRIX_DIR}/MarioClockface.cpp
    ${DESKMATRIX_DIR}/MarioBlock.cpp
    ${DESKMATRIX_DIR}/MarioSprite.cpp
    ${DESKMATRIX_DIR}/WordsClockface.cpp
    ${DESKMATRIX_DIR}/PacmanClockface.cpp
    ${DESKMATRIX_DIR}/PacmanClockfaceAi.cpp
    ${DESKMATRIX_DIR}/PacmanClockfaceMap.cpp
    ${DESKMATRIX_DIR}/PacmanClockfacePathfinding.cpp
    ${DESKMATRIX_DIR}/PacmanEntity.cpp
    ${DESKMATRIX_DIR}/PacmanGhost.cpp
    ${DESKMATRIX_DIR}/PacmanSprite.cpp
    ${DESKMATRIX_DIR}/CanvasClockface.cpp
    ${DESKMATRIX_DIR}/EzTimeFormat.cpp
    FirmwareVtableStubs.cpp
)
target_include_directories(canvas_preview PRIVATE
    ${ARDUINO_SHIM_DIR}
    ${GFX_LIB_DIR}
    ${ANIMATEDGIF_LIB_DIR}/src
    "$ENV{HOME}/Documents/Arduino/libraries/JPEGDEC/src"
    "$ENV{HOME}/Documents/Arduino/libraries/PNGdec/src"
    ${ARDUINOJSON_LIB_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DESKMATRIX_DIR}
)
target_link_libraries(canvas_preview PRIVATE SDL2::SDL2)
