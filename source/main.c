#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include "reimpl/controls.h"
#include "scrib.h"
#include "audio.h"
#ifdef SCRIB_DIAGNOSTICS
#include "scrib_debug.h"
#include "scrib_perf.h"
#endif
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

int _newlib_heap_size_user = 256 * 1024 * 1024;
int sceLibcHeapSize = 4 * 1024 * 1024;
so_module so_mod;

int main(void) {
    scrib_log_init();
    l_info("Scribblenauts Remix build 0.9; music and effects");
    soloader_init_clocks();
    scrib_check_data();
    soloader_init_all();
    jint (*on_load)(JavaVM *, void *) = (void *)scrib_symbol("JNI_OnLoad");
    l_info("Calling JNI_OnLoad");
    jint version = on_load(&jvm, NULL);
    if (version < 0) fatal_error("JNI_OnLoad failed: %08x", version);
    l_info("JNI_OnLoad returned %08x", version);
    gl_init();
    scrib_audio_init();
#ifdef SCRIB_DIAGNOSTICS
    scrib_debug_self_test();
#endif
    scrib_start();
#ifdef SCRIB_DIAGNOSTICS
    unsigned frame = 0;
#endif
    for (;;) {
        uint64_t started = sceKernelGetProcessTimeWide();
        scrib_audio_poll();
        scrib_poll_java();
#ifdef SCRIB_DIAGNOSTICS
        bool testing = scrib_perf_controls(frame);
        if (!scrib_keyboard_active() && !testing) controls_poll();
        uint64_t input_done = sceKernelGetProcessTimeWide();
        unsigned active = scrib_sticks_active() | (controls_touch_count() ? 4 : 0);
        scrib_debug_begin(frame);
#else
        if (!scrib_keyboard_active()) controls_poll();
#endif
        scrib_render();
#ifdef SCRIB_DIAGNOSTICS
        uint64_t game_done = sceKernelGetProcessTimeWide();
        unsigned engaged = scrib_sticks_engaged();
        scrib_debug_end();
        uint64_t debug_done = sceKernelGetProcessTimeWide();
#endif
        gl_swap();
#ifdef SCRIB_DIAGNOSTICS
        uint64_t swap_done = sceKernelGetProcessTimeWide();
        if (frame == 0 || frame == 30 || frame == 300)
            l_info("Rendered frame %u", frame);
        ++frame;
#endif
        uint64_t elapsed = sceKernelGetProcessTimeWide() - started;
        if (elapsed < 33333) sceKernelDelayThread(33333 - elapsed);
#ifdef SCRIB_DIAGNOSTICS
        scrib_perf_sample(started, input_done, game_done, debug_done, swap_done,
                          sceKernelGetProcessTimeWide(), active, engaged);
#endif
    }
}

void controls_handler_key(int32_t key, ControlsAction action) {
    if (action == CONTROLS_ACTION_DOWN &&
        (key == AKEYCODE_BACK || key == AKEYCODE_BUTTON_START)) scrib_back();
}
void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    scrib_touch(id, x, y, action);
}
void controls_handler_analog(ControlsStickId stick, float x, float y, ControlsAction action) {
    scrib_analog(stick, x, y);
    (void)action;
}
