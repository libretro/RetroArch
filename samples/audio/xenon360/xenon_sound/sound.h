/* A stand-in for libxenon's <xenon_sound/sound.h>, enough of it for
 * audio/drivers/xenon360_audio.c to build and run on the host over the
 * scripted queue in xenon_mock.c. */
#ifndef XENON360_MOCK_SOUND_H
#define XENON360_MOCK_SOUND_H

#include <stdint.h>

void xenon_sound_init(void);
void xenon_sound_submit(void *data, int len);
int  xenon_sound_get_free(void);
int  xenon_sound_get_unplayed(void);

#endif
