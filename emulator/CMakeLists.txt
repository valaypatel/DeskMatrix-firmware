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

project(deskmatrix_emulator CXX)
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
