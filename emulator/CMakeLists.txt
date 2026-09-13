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
