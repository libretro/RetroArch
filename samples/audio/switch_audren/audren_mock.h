#ifndef SWITCH_AUDREN_MOCK_H
#define SWITCH_AUDREN_MOCK_H

#include <stddef.h>
#include <pthread.h>

/* Starts the renderer thread: one queued wave buffer played per frame
 * of frame_us while the voice plays, the frame event raised each
 * frame. */
void     audren_mock_start(unsigned frame_us);
void     audren_mock_stop(void);
/* The thread allowed to call into the renderer; calls from any other
 * are counted. */
void     audren_mock_set_owner(pthread_t owner);
unsigned audren_mock_foreign_calls(void);
/* Bytes the voice has played. */
size_t   audren_mock_played_bytes(void);
unsigned audren_mock_updates(void);

#endif
