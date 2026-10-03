#include "audio_core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_mixing(void) {
    AudioMixer mixer = {0};
    int16_t positive[] = {30000, 30000, 30000};
    int16_t negative[] = {-30000, -30000, -30000};
    int16_t out[8];
    mixer.clips[0] = (AudioClip){positive, 3};
    mixer.clips[1] = (AudioClip){positive, 3};
    mixer.clips[2] = (AudioClip){negative, 3};
    mixer.clips[3] = (AudioClip){negative, 3};
    audio_mixer_trigger(&mixer, -1);
    audio_mixer_trigger(&mixer, SCRIB_SFX_COUNT);
    audio_mixer_render(&mixer, out, 8, false);
    for (unsigned i = 0; i < 8; ++i) assert(out[i] == 0);
    audio_mixer_trigger(&mixer, 0);
    audio_mixer_trigger(&mixer, 1);
    audio_mixer_render(&mixer, out, 1, false);
    assert(out[0] == 32767); // No signed overflow on overlap.
    audio_mixer_trigger(&mixer, 0); // Retrigger only this ID.
    audio_mixer_render(&mixer, out, 3, false);
    assert(out[0] == 32767 && out[1] == 32767 && out[2] == 30000);
    audio_mixer_trigger(&mixer, 2);
    audio_mixer_trigger(&mixer, 3);
    audio_mixer_render(&mixer, out, 1, false);
    assert(out[0] == -32768);
    audio_mixer_render(&mixer, out, 2, true);
    assert(out[0] == 0 && out[1] == 0);
    audio_mixer_render(&mixer, out, 8, false);
    for (unsigned i = 0; i < 8; ++i) assert(out[i] == 0); // Muted voices still finish.
}

static void check_stream(const char *directory, int id) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ogg", directory, audio_music_name(id));
    AudioStream stream = {0};
    assert(audio_stream_open(&stream, path, false));
    assert(stream.rate == 32000);
    uint64_t original = ov_pcm_total(&stream.vorbis, -1);
    uint64_t expected = (original * SCRIB_AUDIO_RATE + stream.rate - 1) / stream.rate;
    uint64_t count = 0, energy = 0;
    int16_t output[1024];
    size_t got;
    while ((got = audio_stream_read(&stream, output, 1024))) {
        count += got;
        for (size_t i = 0; i < got; ++i) energy += abs(output[i]);
    }
    assert(!stream.failed && count == expected && energy > 0);
    audio_stream_close(&stream);
    if (id != 0) return;

    // Read boundaries must not change pitch, duration, or sample sequence.
    AudioStream other = {0};
    assert(audio_stream_open(&stream, path, false));
    assert(audio_stream_open(&other, path, false));
    int16_t singles[1024];
    assert(audio_stream_read(&stream, output, 1024) == 1024);
    for (unsigned i = 0; i < 1024; ++i)
        assert(audio_stream_read(&other, &singles[i], 1) == 1);
    assert(!memcmp(output, singles, sizeof(output)));
    audio_stream_close(&stream);
    audio_stream_close(&other);

    // The win jingle's exact duration is integral at 48 kHz. Its loop must
    // join directly to the start, with no silence, dropped sample, or drift.
    assert(original * SCRIB_AUDIO_RATE % 32000 == 0);
    assert(audio_stream_open(&stream, path, true));
    assert(audio_stream_read(&stream, singles, 1024) == 1024);
    count = 1024;
    while (count < expected) {
        size_t wanted = expected - count < 1024 ? expected - count : 1024;
        assert(audio_stream_read(&stream, output, wanted) == wanted);
        count += wanted;
    }
    assert(audio_stream_read(&stream, output, 1024) == 1024);
    assert(!memcmp(output, singles, sizeof(output)));
    audio_stream_close(&stream);
    assert(audio_stream_read(&stream, output, 1024) == 0); // Explicit stop.
}

int main(int argc, char **argv) {
    assert(argc == 2);
    check_mixing();
    assert(!audio_sfx_name(-1) && !audio_sfx_name(SCRIB_SFX_COUNT));
    assert(!strcmp(audio_music_name(-1), "mus_gameover"));
    assert(!strcmp(audio_music_name(SCRIB_MUSIC_COUNT), "mus_gameover"));
    AudioStream invalid = {0};
    assert(!audio_stream_open(&invalid, "/dev/null", true));
    assert(!audio_stream_open(&invalid, "/does-not-exist/sound.ogg", false));
    AudioMixer mixer = {0};
    char error[160];
    assert(audio_mixer_load(&mixer, argv[1], error, sizeof(error)));
    size_t samples = 0;
    for (unsigned id = 0; id < SCRIB_SFX_COUNT; ++id) {
        assert(mixer.clips[id].count && mixer.clips[id].samples);
        samples += mixer.clips[id].count;
    }
    assert(mixer.clips[4].samples == mixer.clips[5].samples);
    assert(mixer.clips[16].samples == mixer.clips[18].samples);
    audio_mixer_destroy(&mixer);
    assert(!audio_mixer_load(&mixer, "/does-not-exist", error, sizeof(error)));
    audio_mixer_destroy(&mixer); // Partial initialization and repeated cleanup.
    for (int id = 0; id < SCRIB_MUSIC_COUNT; ++id) check_stream(argv[1], id);
    printf("Audio tests passed: 21 music IDs, 25 effect IDs (%zu PCM bytes including aliases), resampling, loops, overlap, retrigger, saturation, mute, stop, invalid inputs.\n", samples * 2);
    return 0;
}
