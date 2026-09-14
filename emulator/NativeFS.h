// emulator/NativeFS.h
//
// Backs ScreensaverScreen.cpp's LittleFS.open()/.exists() calls with plain
// file I/O against a local directory (emulator/assets/) instead of the
// device's flash filesystem. Method names/signatures match exactly what
// ScreensaverScreen.cpp calls -- see gifOpen()/gifRead()/gifSeek() and
// loadPath() in that file.
#pragma once
#include <cstdio>
#include <cstdint>
#include <string>

class File {
public:
    File() = default;
    explicit File(FILE* f) : f_(f) {}

    operator bool() const { return f_ != nullptr; }

    size_t size() const {
        if (!f_) return 0;
        long pos = ftell(f_);
        fseek(f_, 0, SEEK_END);
        long end = ftell(f_);
        fseek(f_, pos, SEEK_SET);
        return static_cast<size_t>(end);
    }

    int read(uint8_t* buf, size_t len) {
        if (!f_) return 0;
        return static_cast<int>(fread(buf, 1, len, f_));
    }

    void seek(int32_t pos) {
        if (f_) fseek(f_, pos, SEEK_SET);
    }

    int32_t position() const {
        return f_ ? static_cast<int32_t>(ftell(f_)) : 0;
    }

    void close() {
        if (f_) { fclose(f_); f_ = nullptr; }
    }

private:
    FILE* f_ = nullptr;
};

class NativeFS {
public:
    // Matches LittleFS's real signature closely enough for this file's
    // call sites (mode is always "r" here).
    File open(const char* path, const char* mode) {
        std::string full = assetsDir_ + path;
        FILE* f = fopen(full.c_str(), mode);
        return File(f);
    }

    bool exists(const char* path) {
        std::string full = assetsDir_ + path;
        FILE* f = fopen(full.c_str(), "rb");
        if (f) { fclose(f); return true; }
        return false;
    }

    // Set once at emulator startup (main.cpp) to the emulator/assets/
    // directory, resolved relative to the running executable so it works
    // regardless of the current working directory.
    void setAssetsDir(const std::string& dir) { assetsDir_ = dir; }

private:
    std::string assetsDir_ = "./assets";
};

extern NativeFS LittleFS;
