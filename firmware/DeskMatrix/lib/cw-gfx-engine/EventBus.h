#pragma once
#ifndef CW_EVENTBUS_H_INCLUDED
#define CW_EVENTBUS_H_INCLUDED

#include <Arduino.h>
#include "EventTask.h"
#include "Sprite.h"

class EventBus {
  private:
    EventTask* _subscriptions[5];
    uint8_t _subNum = 0;

  public:
    void broadcast(EventType event, Sprite* sender);
    void subscribe(EventTask* task);

};

#endif  // CW_EVENTBUS_H_INCLUDED