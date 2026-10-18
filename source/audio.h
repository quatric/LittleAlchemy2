#ifndef AUDIO_H
#define AUDIO_H
/* Little Alchemy 2 audio pack (Wii only). dir holds sfx/ and music/ with the original .ogg files. */
int audio_init(const char *dir);   /* returns how many sound effects were loaded */
int audio_sfx(int id);             /* plays an SFX_ effect from the pack; 0 when the pack lacks it */
void audio_music(int on);
void audio_note_discovery(void);   /* feeds the busy / calm choice of the next track */
void audio_shutdown(void);
#endif
