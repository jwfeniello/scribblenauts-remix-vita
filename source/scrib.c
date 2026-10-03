#include "scrib.h"
#include "audio.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include "stick_touch.h"
#include <so_util/so_util.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ime_dialog.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern so_module so_mod;
static void (*render_fn)(JNIEnv *, jclass, jboolean);
static void (*back_fn)(JNIEnv *, jclass);
static void (*touch_fn[3])(JNIEnv *, jclass, jint, jfloat, jfloat);
static void (*done_input_fn)(JNIEnv *, jclass);
static bool init_pending, boot_pending, joysticks_pending, keyboard_pending, keyboard_open;
static char keyboard_text[256];
static void (*clear_touches_fn)(JNIEnv *, jclass);
static void **game_instance;
static void *(*get_stick[2])(void *game);
static int (*process_status)(void *process);
static bool (*stick_engaged)(void *stick);
typedef struct { int16_t x, y, radius; } NativeCircle;
static const NativeCircle *stick_circle[2];
static const float *input_scale_x, *input_scale_y, *input_offset_x, *input_offset_y;
static StickTouch sticks[2];
// Vita front-touch IDs are bytes; reserve distinct IDs for both physical sticks.
#define STICK_TOUCH_ID 0x4000

uintptr_t scrib_symbol(const char *name) {
    uintptr_t symbol = so_symbol(&so_mod, name);
    if (!symbol) {
        l_fatal("Required native export missing: %s", name);
        fatal_error("Missing game function: %s", name);
    }
    return symbol;
}

void scrib_check_data(void) {
    uint64_t started = sceKernelGetProcessTimeWide();
    SceIoStat st;
    // Full hashes are checked by the PC's audit/packaging tools. On the device,
    // inspect metadata only; scanning the entire library delayed every launch.
    if (sceIoGetstat(SO_PATH, &st) < 0 || !SCE_S_ISREG(st.st_mode) || st.st_size != 47894606)
        fatal_error("Missing or incomplete libScribAndroid.so. Copy the prepared data folder first.");
    if (sceIoGetstat(DATA_PATH "1p", &st) < 0 || st.st_size != 155347587)
        fatal_error("Missing or incomplete game package: " DATA_PATH "1p");
    if (sceIoGetstat(DATA_PATH "1i", &st) < 0 || st.st_size != 119988)
        fatal_error("Missing or incomplete game index: " DATA_PATH "1i");
    l_info("STARTUP file checks passed in %.3fs; no launch-time checksum",
           (sceKernelGetProcessTimeWide() - started) / 1000000.0);
}

static void call_void(const char *name) {
    void (*fn)(JNIEnv *, jclass) = (void *)scrib_symbol(name);
    fn(&jni, NULL);
}
static void call_bool(const char *name, bool value) {
    void (*fn)(JNIEnv *, jclass, jboolean) = (void *)scrib_symbol(name);
    fn(&jni, NULL, value);
}
static void call_path(const char *name, const char *path) {
    void (*fn)(JNIEnv *, jclass, jstring) = (void *)scrib_symbol(name);
    jstring str = (*jni).NewStringUTF(&jni, path);
    fn(&jni, NULL, str);
    (*jni).DeleteLocalRef(&jni, str);
}

void scrib_start(void) {
    render_fn = (void *)scrib_symbol("Java_com_game_scrib_ScribRenderer_nativeRender");
    back_fn = (void *)scrib_symbol("Java_com_game_scrib_GameplayActivity_nativeOnBackPressed");
    touch_fn[CONTROLS_ACTION_UP] = (void *)scrib_symbol("Java_com_game_scrib_InputController_nativeTouchUp");
    touch_fn[CONTROLS_ACTION_DOWN] = (void *)scrib_symbol("Java_com_game_scrib_InputController_nativeTouchDown");
    touch_fn[CONTROLS_ACTION_MOVE] = (void *)scrib_symbol("Java_com_game_scrib_InputController_nativeTouchMove");
    done_input_fn = (void *)scrib_symbol("Java_com_game_scrib_InputController_nativeDoneInputting");
    clear_touches_fn = (void *)scrib_symbol("Java_com_game_scrib_InputController_nativeClearTouches");
    game_instance = (void *)scrib_symbol("_ZN6C_Game10pC_Game_smE");
    get_stick[0] = (void *)scrib_symbol("_ZNK6C_Game15GetLeftJoystickEv");
    get_stick[1] = (void *)scrib_symbol("_ZNK6C_Game16GetRightJoystickEv");
    process_status = (void *)scrib_symbol("_ZN2GE9I_Process9GetStatusEv");
    stick_engaged = (void *)scrib_symbol("_ZNK2GE24C_VirtualJoystickProcess9b_EngagedEv");
    stick_circle[0] = (void *)scrib_symbol("_ZN2GE24C_VirtualJoystickProcess23LeftJoystickInnerCircleE");
    stick_circle[1] = (void *)scrib_symbol("_ZN2GE24C_VirtualJoystickProcess24RightJoystickInnerCircleE");
    input_scale_x = (void *)scrib_symbol("_ZN2GE24M_GraphicsManagerAndroid17f_xContentScale_mE");
    input_scale_y = (void *)scrib_symbol("_ZN2GE24M_GraphicsManagerAndroid17f_yContentScale_mE");
    input_offset_x = (void *)scrib_symbol("_ZN2GE24M_GraphicsManagerAndroid18f_xContentOffset_mE");
    input_offset_y = (void *)scrib_symbol("_ZN2GE24M_GraphicsManagerAndroid18f_yContentOffset_mE");
    // Android's directory walker drops Vita's drive prefix. Prepare every
    // save directory before the game attempts to create its files.
    static const char *save_dirs[] = {
        "saves", "saves/SCRIBDATA", "saves/SCRIBDATA/PROFILE",
        "saves/SCRIBDATA/LEVELS", "saves/SCRIBDATA/MAIN",
        "saves/SCRIBDATA/MERITS", "saves/SCRIBDATA/PLAYGROUND",
        "saves/SCRIBDATA/AVATARUSAGE", "saves/SCRIBDATA/GOLDCROWN",
        "saves/SCRIBDATA/DOWNLOADS"
    };
    for (unsigned i = 0; i < sizeof(save_dirs) / sizeof(save_dirs[0]); ++i) {
        char path[256];
        snprintf(path, sizeof(path), "%s%s", DATA_PATH, save_dirs[i]);
        sceIoMkdir(path, 0777);
        SceIoStat st;
        if (sceIoGetstat(path, &st) < 0 || !SCE_S_ISDIR(st.st_mode))
            fatal_error("Could not prepare save directory: %s", path);
    }
    l_info("Setting save and package paths");
    call_path("Java_com_game_scrib_GameplayActivity_setFileStoragePath", DATA_PATH "saves");
    void (*assets)(JNIEnv *, jclass, jobject, jstring, jstring, jboolean) =
        (void *)scrib_symbol("Java_com_game_scrib_GameplayActivity_setAssetManager");
    jstring index = (*jni).NewStringUTF(&jni, DATA_PATH "1i");
    jstring package = (*jni).NewStringUTF(&jni, DATA_PATH "1p");
    assets(&jni, NULL, NULL, index, package, JNI_TRUE);
    (*jni).DeleteLocalRef(&jni, index);
    (*jni).DeleteLocalRef(&jni, package);
    call_path("Java_com_game_scrib_GameplayActivity_setExternalStoragePath", DATA_PATH);
    void (*screen)(JNIEnv *, jclass, jint, jint) =
        (void *)scrib_symbol("Java_com_game_scrib_GameplayActivity_sendScreenWidthHeight");
    screen(&jni, NULL, 960, 544);
    l_info("Calling nativeInit (game initialization)");
    call_void("Java_com_game_scrib_ScribRenderer_nativeInit");
    l_info("nativeInit returned; resizing renderer");
    void (*resize)(JNIEnv *, jclass, jint, jint) =
        (void *)scrib_symbol("Java_com_game_scrib_ScribRenderer_nativeResize");
    resize(&jni, NULL, 960, 544);
    scrib_cancel_input();
    l_info("Entering render loop; left stick=Maxwell, right stick=camera; touch and audio enabled");
}

void scrib_render(void) { render_fn(&jni, NULL, keyboard_open); }
void scrib_back(void) { back_fn(&jni, NULL); }
void scrib_touch(int id, float x, float y, ControlsAction action) {
    if ((unsigned)action < 3) touch_fn[action](&jni, NULL, id, x, y);
}

static bool stick_bounds(unsigned which, StickTouchBounds *bounds) {
    if (!game_instance || !*game_instance || scrib_keyboard_active()) return false;
    void *owner = get_stick[which](*game_instance);
    if (!owner || process_status(owner) != 1) return false;
    const NativeCircle *circle = stick_circle[which];
    if (circle->radius <= 0 || *input_scale_x <= 0 || *input_scale_y <= 0) return false;
    // Invert native convertPoint(): game = screen / scale - offset.
    // SetActiveJoystickPos clamps at 20 game pixels (verified in this binary).
    float radius = circle->radius < 20 ? circle->radius : 20;
    *bounds = (StickTouchBounds){(uintptr_t)owner,
        (circle->x + *input_offset_x) * *input_scale_x,
        (circle->y + *input_offset_y) * *input_scale_y,
        radius * *input_scale_x, radius * *input_scale_y};
    return true;
}

void scrib_analog(ControlsStickId which, float x, float y) {
    if ((unsigned)which > 1) return;
    StickTouchBounds bounds;
    bool available = stick_bounds(which, &bounds);
    bool was_down = sticks[which].down;
    stick_touch_update(&sticks[which], STICK_TOUCH_ID + which, x, y,
                      available ? &bounds : NULL, scrib_touch);
    if (!was_down && sticks[which].down)
        l_debug("Stick %u acquired at %.1f,%.1f radius %.1f,%.1f", which,
                bounds.center_x, bounds.center_y, bounds.radius_x, bounds.radius_y);
}

void scrib_cancel_input(void) {
    for (unsigned i = 0; i < 2; ++i) stick_touch_release(&sticks[i], STICK_TOUCH_ID + i, scrib_touch);
    if (clear_touches_fn) clear_touches_fn(&jni, NULL);
    controls_reset_touch();
}

unsigned scrib_sticks_active(void) { return sticks[0].down | (sticks[1].down << 1); }
unsigned scrib_sticks_engaged(void) {
    if (!game_instance || !*game_instance) return 0;
    unsigned result = 0;
    for (unsigned i = 0; i < 2; ++i) {
        void *stick = get_stick[i](*game_instance);
        if (stick && process_status(stick) == 1 && stick_engaged(stick)) result |= 1 << i;
    }
    return result;
}
bool scrib_sticks_available(void) {
    StickTouchBounds bounds;
    return stick_bounds(0, &bounds) && stick_bounds(1, &bounds);
}
void scrib_queue_initialize(void) { init_pending = true; }
void scrib_queue_boot_complete(void) { boot_pending = true; }
void scrib_queue_joysticks(void) { joysticks_pending = true; }
void scrib_keyboard_show(void) { keyboard_pending = true; }
bool scrib_keyboard_active(void) { return keyboard_open || keyboard_pending; }
const char *scrib_keyboard_text(void) { return keyboard_text; }
void scrib_keyboard_set(const char *text) {
    snprintf(keyboard_text, sizeof(keyboard_text), "%.192s", text ? text : "");
}

void scrib_poll_java(void) {
    // Java posts these callbacks to the game thread; avoid reentrancy from JNI.
    if (init_pending) {
        init_pending = false;
        l_info("Applying offline startup settings");
        void (*xml)(JNIEnv *, jclass, jint, jint, jint, jint, jint, jint, jint) =
            (void *)scrib_symbol("Java_com_game_scrib_GameplayActivity_setXMLValues");
        xml(&jni, NULL, 0, 0, 0, 0, 0, 0, 0);
        call_bool("Java_com_game_scrib_GameplayActivity_nativeSetSoundEnabled", scrib_audio_enabled());
        call_bool("Java_com_game_scrib_GameplayActivity_nativeSetJoysticksEnabled", true);
    }
    if (joysticks_pending) {
        joysticks_pending = false;
        call_bool("Java_com_game_scrib_GameplayActivity_nativeSetJoysticksEnabled", true);
    }
    if (boot_pending) {
        boot_pending = false;
        l_info("Completing Android boot prompts");
        call_void("Java_com_game_scrib_GameplayActivity_nativeBootPromptingComplete");
    }
    if (keyboard_pending && !keyboard_open) {
        keyboard_pending = false;
        scrib_cancel_input();
        int status = init_ime_dialog("Enter an object", keyboard_text);
        keyboard_open = status >= 0;
        l_info("Vita keyboard initialization: %08x", status);
        if (!keyboard_open) { keyboard_text[0] = 0; done_input_fn(&jni, NULL); }
    }
    if (keyboard_open) {
        char *text = get_ime_dialog_result();
        if (text) {
            scrib_keyboard_set(text);
            keyboard_open = false;
            done_input_fn(&jni, NULL);
        }
    }
}
