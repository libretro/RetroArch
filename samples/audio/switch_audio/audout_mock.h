#ifndef SWITCH_AUDIO_AUDOUT_MOCK_H
#define SWITCH_AUDIO_AUDOUT_MOCK_H

#include <stddef.h>

void   audout_mock_reset(void);
/* The service plays the n oldest appended buffers and releases them. */
void   audout_mock_play(unsigned n);
/* Buffers appended and not yet played. */
unsigned audout_mock_queued(void);
/* Bytes the service has been given, across every append. */
size_t audout_mock_bytes(void);
/* The data_size of the most recent append. */
size_t audout_mock_last_append(void);
/* Appends since the reset. */
unsigned audout_mock_appends(void);

#endif
