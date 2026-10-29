#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H
#include <stdint.h>
#include "../../source/audio.h"

/* Portable software mixer behind the audio.h interface, for the 3DS, Wii U and Switch ports.
 * The platform opens a 44.1 kHz signed 16-bit stereo output and calls audio_render() from its
 * audio thread or callback; everything the game triggers is queued and picked up there, so no
 * locking is needed. Without an audio pack on the card audio_init() loads nothing and
 * audio_render() writes silence. */
#define AUDIO_RATE 44100
void audio_render(int16_t *out, int frames);   /* interleaved stereo */
#endif
