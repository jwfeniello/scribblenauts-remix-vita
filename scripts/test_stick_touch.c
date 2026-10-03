#include "stick_touch.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

typedef struct { int id; float x, y; ControlsAction action; } Event;
static Event events[32];
static unsigned count;
static void emit(int id, float x, float y, ControlsAction action) {
    assert(count < 32);
    events[count++] = (Event){id, x, y, action};
}
static void expect(unsigned index, int id, float x, float y, ControlsAction action) {
    assert(index < count);
    assert(events[index].id == id && events[index].action == action);
    assert(fabsf(events[index].x - x) < 0.001f);
    assert(fabsf(events[index].y - y) < 0.001f);
}

int main(void) {
    StickTouch left = {0}, right = {0};
    StickTouchBounds a = {1, 180, 467, 40, 30};
    StickTouchBounds b = {2, 786, 467, 40, 30};
    stick_touch_update(&left, 0x4000, 0, 0, &a, emit);
    assert(!count && !left.down);
    stick_touch_update(&left, 0x4000, 1, 0, &a, emit);
    expect(0, 0x4000, 180, 467, CONTROLS_ACTION_DOWN);
    stick_touch_update(&left, 0x4000, 1, 0, &a, emit);
    expect(1, 0x4000, 220, 467, CONTROLS_ACTION_MOVE);
    stick_touch_update(&right, 0x4001, -1, 0, &b, emit);
    expect(2, 0x4001, 786, 467, CONTROLS_ACTION_DOWN);
    stick_touch_update(&right, 0x4001, -1, 0, &b, emit);
    expect(3, 0x4001, 746, 467, CONTROLS_ACTION_MOVE);
    assert(left.down && right.down);
    stick_touch_update(&left, 0x4000, 1, 1, &a, emit);
    expect(4, 0x4000, 180 + 40 / sqrtf(2), 467 + 30 / sqrtf(2), CONTROLS_ACTION_MOVE);
    stick_touch_update(&left, 0x4000, 0, 0, &a, emit);
    expect(5, 0x4000, events[4].x, events[4].y, CONTROLS_ACTION_UP);
    stick_touch_update(&left, 0x4000, 0, 0, &a, emit);
    assert(count == 6 && !left.down && right.down);
    // Hidden controls / a keyboard or screen transition release held input.
    stick_touch_update(&right, 0x4001, -1, 0, NULL, emit);
    expect(6, 0x4001, 746, 467, CONTROLS_ACTION_UP);
    stick_touch_update(&right, 0x4001, -1, 0, NULL, emit);
    assert(count == 7 && !right.down);
    // A recreated native joystick gets a fresh center press after the old UP.
    stick_touch_update(&left, 0x4000, 0.5f, -0.5f, &a, emit);
    stick_touch_update(&left, 0x4000, 0.5f, -0.5f, &a, emit);
    expect(8, 0x4000, 200, 452, CONTROLS_ACTION_MOVE);
    a.owner = 3;
    a.center_x = 200;
    stick_touch_update(&left, 0x4000, 0.5f, -0.5f, &a, emit);
    expect(9, 0x4000, 200, 452, CONTROLS_ACTION_UP);
    expect(10, 0x4000, 200, 467, CONTROLS_ACTION_DOWN);
    stick_touch_update(&left, 0x4000, NAN, 0, &a, emit);
    expect(11, 0x4000, 200, 467, CONTROLS_ACTION_UP);
    stick_touch_release(&left, 0x4000, emit);
    assert(count == 12 && !left.down && !right.down);
    puts("Stick mapping checks passed: acquisition, both IDs, analog range, diagonal clamp, release, transitions.");
    return 0;
}
