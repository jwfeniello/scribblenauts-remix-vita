#include "audio_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Preserve the Java AudioController's IDs, including its deliberate duplicates.
static const char *const music_names[SCRIB_MUSIC_COUNT] = {
    "jng_win", "mus_casual1", "mus_casual1", "mus_casual2", "mus_casual4",
    "mus_casual5", "mus_casual6", "mus_casual7", "mus_casual8", "mus_casual9",
    "mus_casual10", "mus_casual11", "mus_jumbledup1", "mus_jumbledup2",
    "mus_stuntworld1", "mus_stuntworld2", "mus_gameover", "mus_levelselect",
    "mus_online", "mus_tally", "mus_worldmap"
};
static const char *const sfx_names[SCRIB_SFX_COUNT] = {
    "sfx_buttonpress", "sfx_collision", "sfx_container", "sfx_destroy",
    "sfx_dig", "sfx_dig", "sfx_dig", "sfx_drop", "sfx_electric", "sfx_explode",
    "sfx_fire", "sfx_ice", "sfx_melee", "sfx_notepad", "sfx_ollars",
    "sfx_pickup", "sfx_shoot", "sfx_shoot", "sfx_shoot", "sfx_spawn",
    "sfx_splash", "sfx_starappear", "sfx_switch", "sfx_trash", "sfx_water"
};
const char *audio_music_name(int id) {
    return music_names[(unsigned)id < SCRIB_MUSIC_COUNT ? id : 16];
}
const char *audio_sfx_name(int id) {
    return (unsigned)id < SCRIB_SFX_COUNT ? sfx_names[id] : NULL;
}

void audio_stream_close(AudioStream *stream) {
    if (stream->opened) ov_clear(&stream->vorbis);
    memset(stream, 0, sizeof(*stream));
}

static bool next_sample(AudioStream *stream, int16_t *sample) {
    if (stream->decoded_pos == stream->decoded_count) {
        // Bound retries for damaged/empty input, including an empty loop.
        for (unsigned retry = 0; retry < 8; ++retry) {
            int section;
            long bytes = ov_read(&stream->vorbis, (char *)stream->decoded,
                                 sizeof(stream->decoded), 0, 2, 1, &section);
            if (bytes > 0) {
                stream->decoded_pos = 0;
                stream->decoded_count = (size_t)bytes / sizeof(int16_t);
                break;
            }
            if (bytes == OV_HOLE) continue;
            if (bytes == 0 && !stream->looping) return false;
            if (bytes != 0 || ov_pcm_seek(&stream->vorbis, 0) < 0) {
                stream->failed = true;
                return false;
            }
        }
        if (stream->decoded_pos == stream->decoded_count) {
            stream->failed = true;
            return false;
        }
    }
    *sample = stream->decoded[stream->decoded_pos++];
    return true;
}

bool audio_stream_open(AudioStream *stream, const char *path, bool looping) {
    audio_stream_close(stream);
    // This module and libvorbisfile both use newlib, not Android FILE objects.
    if (ov_fopen(path, &stream->vorbis) < 0) return false;
    stream->opened = true;
    vorbis_info *info = ov_info(&stream->vorbis, -1);
    // The original assets are mono at 32 kHz (music) or 44.1 kHz (effects).
    if (!info || info->channels != 1 || info->rate <= 0 ||
        info->rate > SCRIB_AUDIO_RATE || ov_streams(&stream->vorbis) != 1 ||
        ov_pcm_total(&stream->vorbis, -1) <= 0) {
        audio_stream_close(stream);
        return false;
    }
    stream->rate = (unsigned)info->rate;
    stream->looping = looping;
    stream->current_valid = next_sample(stream, &stream->current);
    stream->next_valid = next_sample(stream, &stream->next);
    if (!stream->current_valid || stream->failed) {
        audio_stream_close(stream);
        return false;
    }
    return true;
}

size_t audio_stream_read(AudioStream *stream, int16_t *out, size_t count) {
    size_t written = 0;
    while (written < count && stream->current_valid) {
        int next = stream->next_valid ? stream->next : stream->current;
        // Rational phase avoids cumulative pitch/timing drift across buffers.
        out[written++] = stream->current +
            (int64_t)(next - stream->current) * stream->phase / SCRIB_AUDIO_RATE;
        stream->phase += stream->rate;
        if (stream->phase >= SCRIB_AUDIO_RATE) {
            stream->phase -= SCRIB_AUDIO_RATE;
            stream->current_valid = stream->next_valid;
            stream->current = stream->next;
            if (stream->current_valid)
                stream->next_valid = next_sample(stream, &stream->next);
        }
    }
    return written;
}

bool audio_mixer_load(AudioMixer *mixer, const char *directory, char *error, size_t error_size) {
    for (unsigned id = 0; id < SCRIB_SFX_COUNT; ++id) {
        // Share PCM for duplicate assets, but keep each Android ID's own voice.
        unsigned original = 0;
        while (original < id && strcmp(sfx_names[id], sfx_names[original])) ++original;
        if (original < id) {
            mixer->clips[id] = mixer->clips[original];
            continue;
        }
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.ogg", directory, sfx_names[id]);
        AudioStream stream = {0};
        bool ok = audio_stream_open(&stream, path, false);
        if (ok) {
            ogg_int64_t frames = ov_pcm_total(&stream.vorbis, -1);
            // Reject malformed or unexpectedly huge effects before allocating.
            ok = frames <= 10 * (ogg_int64_t)stream.rate;
            if (ok) {
                size_t count = (frames * SCRIB_AUDIO_RATE + stream.rate - 1) / stream.rate;
                AudioClip *clip = &mixer->clips[id];
                clip->samples = malloc(count * sizeof(int16_t));
                ok = clip->samples != NULL;
                if (ok) {
                    clip->count = audio_stream_read(&stream, clip->samples, count);
                    ok = clip->count == count && !stream.failed;
                }
            }
        }
        audio_stream_close(&stream);
        if (!ok) {
            snprintf(error, error_size, "Cannot load audio: %s.ogg", sfx_names[id]);
            audio_mixer_destroy(mixer);
            return false;
        }
    }
    return true;
}

void audio_mixer_destroy(AudioMixer *mixer) {
    audio_stream_close(&mixer->music);
    for (unsigned id = 0; id < SCRIB_SFX_COUNT; ++id) {
        bool shared = false;
        for (unsigned earlier = 0; earlier < id; ++earlier)
            if (mixer->clips[id].samples == mixer->clips[earlier].samples) shared = true;
        if (!shared) free(mixer->clips[id].samples);
    }
    memset(mixer, 0, sizeof(*mixer));
}

void audio_mixer_trigger(AudioMixer *mixer, int id) {
    if ((unsigned)id >= SCRIB_SFX_COUNT) return;
    mixer->voices[id] = (AudioVoice){&mixer->clips[id], 0};
}

void audio_mixer_render(AudioMixer *mixer, int16_t *out, size_t count, bool muted) {
    size_t music_count = audio_stream_read(&mixer->music, out, count);
    memset(out + music_count, 0, (count - music_count) * sizeof(*out));
    for (size_t frame = 0; frame < count; ++frame) {
        int32_t mixed = out[frame];
        for (unsigned id = 0; id < SCRIB_SFX_COUNT; ++id) {
            AudioVoice *voice = &mixer->voices[id];
            if (voice->clip && voice->position < voice->clip->count)
                mixed += voice->clip->samples[voice->position++];
        }
        // Saturation prevents wraparound when effects overlap. Muting advances
        // playback, matching Android's volume-only mute behavior.
        out[frame] = muted ? 0 : mixed > 32767 ? 32767 : mixed < -32768 ? -32768 : mixed;
    }
}
