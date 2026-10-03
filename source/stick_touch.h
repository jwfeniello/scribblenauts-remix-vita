#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "reimpl/controls.h"

typedef struct {
    uintptr_t owner;
    float center_x, center_y, radius_x, radius_y;
} StickTouchBounds;

typedef struct {
    bool down;
    StickTouchBounds bounds;
    float last_x, last_y;
} StickTouch;

typedef void (*StickTouchEmit)(int id, float x, float y, ControlsAction action);
void stick_touch_release(StickTouch *stick, int id, StickTouchEmit emit);
void stick_touch_update(StickTouch *stick, int id, float x, float y,
                        const StickTouchBounds *bounds, StickTouchEmit emit);
