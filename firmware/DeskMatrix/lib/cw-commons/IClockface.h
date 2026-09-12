#pragma once
#ifndef CW_ICLOCKFACE_H_INCLUDED
#define CW_ICLOCKFACE_H_INCLUDED

#include "CWDateTime.h"

class IClockface {
  public:
    // Virtual destructor: ClockScreen switches the active clockface at
    // runtime (config change) and deletes through this base pointer —
    // without a virtual destructor that would only run IClockface's
    // (trivial) destructor and skip the derived class's cleanup (e.g.
    // PacmanClockface frees heap-allocated Pacman/Ghost sprites in its own
    // destructor), which is undefined behavior.
    virtual ~IClockface() = default;

    //virtual void setup(DateTime *dateTime) = 0;
    virtual void setup(CWDateTime *dateTime) = 0;
    virtual void update() = 0;

};

#endif  // CW_ICLOCKFACE_H_INCLUDED