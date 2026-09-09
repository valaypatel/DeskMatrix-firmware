#pragma once
#ifndef CW_EVENTTASK_H_INCLUDED
#define CW_EVENTTASK_H_INCLUDED

#include <Arduino.h>
#include "Sprite.h"

enum EventType {
    MOVE,
    COLLISION
};

class EventTask {
  public:
    virtual void execute(EventType event, Sprite* caller) = 0;
};

#endif  // CW_EVENTTASK_H_INCLUDED