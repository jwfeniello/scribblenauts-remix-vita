#ifndef SCRIB_AUDIO_CORE_H
#define SCRIB_AUDIO_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

#define SCRIB_AUDIO_RATE 48000
#define SCRIB_SFX_COUNT 25
#define SCRIB_MUSIC_COUNT 21

typedef struct {
    OggVorbis_File vorbis;
    bool opened, looping, current_valid, next_valid, failed;
    unsigned rate, phase;
    int16_t current, next, decoded[2048];
    size_t decoded_count, decoded_pos;
} AudioStream;

typedef struct { int16_t *samples; size_t count; } AudioClip;
typedef struct { const AudioClip *clip; size_t position; } AudioVoice;
typedef struct {
    AudioStream music;
    AudioClip clips[SCRIB_SFX_COUNT];
    AudioVoice voices[SCRIB_SFX_COUNT];
} AudioMixer;

const char *audio_music_name(int id);
const char *audio_sfx_name(int id);
bool audio_stream_open(AudioStream *stream, const char *path, bool looping);
void audio_stream_close(AudioStream *stream);
size_t audio_stream_read(AudioStream *stream, int16_t *out, size_t count);
bool audio_mixer_load(AudioMixer *mixer, const char *directory, char *error, size_t error_size);
void audio_mixer_destroy(AudioMixer *mixer);
void audio_mixer_trigger(AudioMixer *mixer, int id);
void audio_mixer_render(AudioMixer *mixer, int16_t *out, size_t count, bool muted);

#endif
