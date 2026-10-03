#include "stick_touch.h"
#include <math.h>

void stick_touch_release(StickTouch *stick, int id, StickTouchEmit emit) {
    if (!stick->down) return;
    emit(id, stick->last_x, stick->last_y, CONTROLS_ACTION_UP);
    stick->down = false;
}

void stick_touch_update(StickTouch *stick, int id, float x, float y,
                        const StickTouchBounds *bounds, StickTouchEmit emit) {
    if (!bounds || !isfinite(x) || !isfinite(y) || (x == 0 && y == 0)) {
        stick_touch_release(stick, id, emit);
        return;
    }
    if (stick->down && stick->bounds.owner != bounds->owner)
        stick_touch_release(stick, id, emit);
    if (!stick->down) {
        stick->bounds = *bounds;
        stick->last_x = bounds->center_x;
        stick->last_y = bounds->center_y;
        stick->down = true;
        // Let the game acquire the virtual stick at its center for one frame.
        emit(id, stick->last_x, stick->last_y, CONTROLS_ACTION_DOWN);
        return;
    }
    float length = sqrtf(x * x + y * y);
    if (length > 1.0f) { x /= length; y /= length; }
    stick->last_x = stick->bounds.center_x + x * stick->bounds.radius_x;
    stick->last_y = stick->bounds.center_y + y * stick->bounds.radius_y;
    emit(id, stick->last_x, stick->last_y, CONTROLS_ACTION_MOVE);
}
