// emulator/EmulatorPaths.h
//
// Shared helper for resolving paths relative to the running executable's
// own directory, rather than the process's current working directory --
// so emulator binaries find their assets/config regardless of where they
// are launched from (repo root, emulator/, or emulator/build/).
#pragma once

#include <SDL2/SDL.h>
#include <string>

// relativeToEmulatorDir is a path relative to emulator/ (e.g. "assets" or
// "fake_config.json"). SDL_GetBasePath() returns the directory containing
// the executable (e.g. emulator/build/) with a trailing separator already
// included; assets/ and fake_config.json both live one level up, in
// emulator/.
inline std::string resolveEmulatorPath(const char* relativeToEmulatorDir) {
    std::string result = "./";  // fallback if SDL_GetBasePath() ever fails
    char* base = SDL_GetBasePath();
    if (base) {
        result = std::string(base) + "../";
        SDL_free(base);
    }
    result += relativeToEmulatorDir;
    return result;
}
