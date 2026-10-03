#ifndef SCRIB_AUDIO_H
#define SCRIB_AUDIO_H
#include <stdbool.h>
void scrib_audio_init(void);
void scrib_audio_poll(void);
bool scrib_audio_enabled(void);
void scrib_audio_mute(bool muted);
void scrib_audio_music(int id, bool looping);
void scrib_audio_stop_music(void);
void scrib_audio_sfx(int id);
void scrib_audio_save_enabled(bool enabled);
#endif
