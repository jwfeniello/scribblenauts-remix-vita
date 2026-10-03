#ifndef SCRIB_PORT_H
#define SCRIB_PORT_H
#include <stdint.h>
#include <stdbool.h>
#include <falso_jni/FalsoJNI.h>
#include "reimpl/controls.h"

uintptr_t scrib_symbol(const char *name);
void scrib_check_data(void);
void scrib_start(void);
void scrib_render(void);
void scrib_back(void);
void scrib_touch(int id, float x, float y, ControlsAction action);
void scrib_analog(ControlsStickId stick, float x, float y);
void scrib_cancel_input(void);
unsigned scrib_sticks_active(void);
unsigned scrib_sticks_engaged(void);
bool scrib_sticks_available(void);
void scrib_poll_java(void);
bool scrib_keyboard_active(void);
void scrib_keyboard_show(void);
void scrib_keyboard_set(const char *text);
const char *scrib_keyboard_text(void);
void scrib_queue_initialize(void);
void scrib_queue_boot_complete(void);
void scrib_queue_joysticks(void);
void scrib_log_init(void);
#endif
