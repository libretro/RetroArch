#ifndef XENON360_MOCK_H
#define XENON360_MOCK_H

#include <stddef.h>

void   xenon_mock_reset(void);
/* The hardware plays this many bytes out of the queue. */
void   xenon_mock_play(size_t bytes);
/* Bytes submitted and not yet played; the most that ever were. */
size_t xenon_mock_unplayed(void);
size_t xenon_mock_peak(void);
/* Bytes submitted across every call. */
size_t xenon_mock_submitted(void);

#endif
