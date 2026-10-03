#include "scrib_perf.h"
#include "scrib.h"
#include "scrib_debug.h"
#include "utils/logger.h"
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    uint64_t wall, input, game, debug, swap;
    unsigned frames, max_us, draws, engaged;
} FrameStats;
static FrameStats stats[8];
static uint64_t last_finish, last_report, test_start;
static int test_phase = -1;

// Optional, bounded hardware replay using the same path as the physical sticks.
// Actual pad/touch input cancels immediately and returns control to the player.
bool scrib_perf_controls(unsigned frame) {
    uint64_t now = sceKernelGetProcessTimeWide();
    if (!test_start && frame % 30 == 0 && scrib_sticks_available()) {
        SceIoStat st;
        if (sceIoGetstat(DATA_PATH "stick-test.request", &st) >= 0) {
            sceIoRemove(DATA_PATH "stick-test.request");
            scrib_cancel_input();
            test_start = now;
            test_phase = -1;
            l_info("STICK TEST started: idle, left/right movement, camera, both sticks, release");
        }
    }
    if (!test_start) return false;
    SceCtrlData pad = {0};
    SceTouchData touch = {0};
    sceCtrlPeekBufferPositiveExt2(0, &pad, 1);
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
    if (scrib_keyboard_active() || pad.buttons || touch.reportNum ||
        abs((int)pad.lx - 128) > 28 || abs((int)pad.ly - 128) > 28 ||
        abs((int)pad.rx - 128) > 28 || abs((int)pad.ry - 128) > 28) {
        scrib_cancel_input();
        test_start = 0;
        l_info("STICK TEST cancelled by local input");
        return false;
    }
    unsigned phase = (now - test_start) / 2000000;
    if (phase >= 7) {
        scrib_cancel_input();
        test_start = 0;
        l_info("STICK TEST complete; synthetic touches released");
        return false;
    }
    if ((int)phase != test_phase) {
        test_phase = phase;
        l_info("STICK TEST phase=%u", phase);
    }
    float left = phase == 1 || phase == 5 ? 1 : phase == 2 ? -1 : 0;
    float right = phase == 3 ? 1 : phase == 4 || phase == 5 ? -1 : 0;
    scrib_analog(CONTROLS_STICK_LEFT, left, 0);
    scrib_analog(CONTROLS_STICK_RIGHT, right, 0);
    return true;
}

void scrib_perf_sample(uint64_t start, uint64_t input, uint64_t game,
                       uint64_t debug, uint64_t swap, uint64_t finish,
                       unsigned active, unsigned engaged) {
    if (!last_report) last_report = start;
    uint64_t elapsed = last_finish ? finish - last_finish : finish - start;
    last_finish = finish;
    // Explicit screen captures pause rendering and are not gameplay samples.
    if (scrib_debug_capturing()) return;
    FrameStats *s = &stats[active & 7];
    ++s->frames;
    s->wall += elapsed;
    s->input += input - start;
    s->game += game - input;
    s->debug += debug - game;
    s->swap += swap - debug;
    s->draws += scrib_debug_draw_count();
    if (engaged) ++s->engaged;
    if (elapsed > s->max_us) s->max_us = elapsed;
    if (finish - last_report < 3000000) return;
    for (unsigned i = 0; i < 8; ++i) {
        s = &stats[i];
        if (!s->frames) continue;
        float ms = 0.001f / s->frames;
        l_info("PERF input_mask=%u frames=%u fps=%.1f input=%.2fms game=%.2fms debug=%.2fms swap=%.2fms max=%.1fms draws=%.1f engaged=%u",
               i, s->frames, 1000000.0 * s->frames / s->wall,
               s->input * ms, s->game * ms, s->debug * ms, s->swap * ms,
               s->max_us * 0.001, (double)s->draws / s->frames, s->engaged);
    }
    memset(stats, 0, sizeof(stats));
    last_report = finish;
}
