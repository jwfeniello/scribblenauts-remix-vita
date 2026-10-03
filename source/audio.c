#include "audio.h"
#include "audio_core.h"
#include "utils/dialog.h"
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_FRAMES 1024
#define AUDIO_DIRECTORY DATA_PATH "res/raw"
#define AUDIO_SETTING DATA_PATH "sound.enabled"

static SceKernelLwMutexWork audio_lock;
static bool lock_ready;
static SceUID audio_thread = -1;
static int audio_port = -1;
static bool running, enabled = true, muted, music_looping, setting_dirty;
static int music_id = -1;
static unsigned music_generation;
static uint32_t pending_sfx;
static char pending_error[160];
static AudioMixer mixer;

// Early native constructors may queue callbacks before the worker is started.
static void lock(void) { if (lock_ready) sceKernelLockLwMutex(&audio_lock, 1, NULL); }
static void unlock(void) { if (lock_ready) sceKernelUnlockLwMutex(&audio_lock, 1); }
static void report_error(const char *message) {
    lock();
    if (!pending_error[0]) snprintf(pending_error, sizeof(pending_error), "%s", message);
    unlock();
}

static int audio_worker(SceSize argc, void *argv) {
    (void)argc; (void)argv;
    char error[160];
    if (!audio_mixer_load(&mixer, AUDIO_DIRECTORY, error, sizeof(error))) {
        report_error(error);
        return 0;
    }
    // Alternate buffers so hardware can finish the previous block while the
    // decoder fills the next one. No decoder or storage work runs on rendering.
    int16_t buffers[2][AUDIO_FRAMES] __attribute__((aligned(64)));
    unsigned buffer = 0, generation = 0;
    for (;;) {
        lock();
        bool active = running, silent = muted, looping = music_looping;
        bool save = setting_dirty, save_value = enabled;
        unsigned next_generation = music_generation;
        int next_id = music_id;
        uint32_t effects = pending_sfx;
        pending_sfx = 0;
        setting_dirty = false;
        unlock();
        if (!active) break;
        if (next_generation != generation) {
            generation = next_generation;
            audio_stream_close(&mixer.music);
            if (next_id >= 0) {
                char path[256];
                snprintf(path, sizeof(path), AUDIO_DIRECTORY "/%s.ogg", audio_music_name(next_id));
                if (!audio_stream_open(&mixer.music, path, looping)) {
                    snprintf(error, sizeof(error), "Cannot load music: %s.ogg", audio_music_name(next_id));
                    report_error(error);
                }
            }
        }
        for (unsigned id = 0; id < SCRIB_SFX_COUNT; ++id)
            if (effects & (1u << id)) audio_mixer_trigger(&mixer, id);
        audio_mixer_render(&mixer, buffers[buffer], AUDIO_FRAMES, silent);
        if (mixer.music.failed) {
            report_error("Cannot decode the music file. Recopy res/raw from your game data.");
            audio_stream_close(&mixer.music);
        }
        int result = sceAudioOutOutput(audio_port, buffers[buffer]);
        if (result < 0) {
            snprintf(error, sizeof(error), "Vita audio output failed: %08x", result);
            report_error(error);
            break;
        }
        buffer ^= 1;
        if (save) {
            FILE *file = fopen(AUDIO_SETTING, "wb");
            if (file) { fputc(save_value ? '1' : '0', file); fclose(file); }
        }
    }
    sceAudioOutOutput(audio_port, NULL);
    audio_mixer_destroy(&mixer);
    return 0;
}

static void audio_shutdown(void) {
    if (audio_thread < 0) return;
    lock(); running = false; unlock();
    sceKernelWaitThreadEnd(audio_thread, NULL, NULL);
    sceKernelDeleteThread(audio_thread);
    audio_thread = -1;
    sceAudioOutReleasePort(audio_port);
    audio_port = -1;
    sceKernelDeleteLwMutex(&audio_lock);
    lock_ready = false;
}

void scrib_audio_init(void) {
    FILE *file = fopen(AUDIO_SETTING, "rb");
    if (file) { enabled = fgetc(file) != '0'; fclose(file); }
    muted = !enabled;
    int result = sceKernelCreateLwMutex(&audio_lock, "scrib_audio", 0, 0, NULL);
    if (result < 0) fatal_error("Could not initialize audio: %08x", result);
    lock_ready = true;
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_FRAMES,
                                    SCRIB_AUDIO_RATE, SCE_AUDIO_OUT_MODE_MONO);
    if (audio_port < 0) fatal_error("Could not open Vita audio output: %08x", audio_port);
    int volume[2] = {SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB};
    sceAudioOutSetVolume(audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, volume);
    running = true;
    audio_thread = sceKernelCreateThread("scrib_audio", audio_worker, 0x10000100,
                                         128 * 1024, 0, 0, NULL);
    if (audio_thread < 0) fatal_error("Could not create audio thread: %08x", audio_thread);
    result = sceKernelStartThread(audio_thread, 0, NULL);
    if (result < 0) fatal_error("Could not start audio thread: %08x", result);
    atexit(audio_shutdown);
}

void scrib_audio_poll(void) {
    char error[sizeof(pending_error)];
    lock();
    memcpy(error, pending_error, sizeof(error));
    pending_error[0] = 0;
    unlock();
    if (error[0]) fatal_error("%s", error);
}
bool scrib_audio_enabled(void) {
    lock(); bool value = enabled; unlock(); return value;
}
void scrib_audio_mute(bool value) { lock(); muted = value; unlock(); }
void scrib_audio_music(int id, bool looping) {
    lock();
    music_id = (unsigned)id < SCRIB_MUSIC_COUNT ? id : 16;
    music_looping = looping;
    ++music_generation;
    unlock();
}
void scrib_audio_stop_music(void) {
    lock(); music_id = -1; ++music_generation; unlock();
}
void scrib_audio_sfx(int id) {
    if ((unsigned)id >= SCRIB_SFX_COUNT) return;
    lock(); pending_sfx |= 1u << id; unlock();
}
void scrib_audio_save_enabled(bool value) {
    lock();
    if (enabled != value) { enabled = value; setting_dirty = true; }
    muted = !value;
    unlock();
}
