/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_CLOCK_H
#define RECORD_PYROWAVE_CLOCK_H
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_pyrowave_clock record_pyrowave_clock_t;
typedef struct record_pyrowave_clock_position
{
   uint64_t generation, epoch, source_sample;
   bool valid;
} record_pyrowave_clock_position_t;
/* One audio owner; no clock reads, locks or allocation after construction.
 * Publish positions separately to the video owner. Device offsets must be
 * actual presentation ranges, including any inserted silence, not merely
 * the frontend's cumulative accepted-write count. */
record_pyrowave_clock_t *record_pyrowave_clock_new(unsigned capacity);
/* After the old device queue has been discarded or retired. Epochs increase
 * within a generation; resetting invalidates every old mapping. */
bool record_pyrowave_clock_reset(record_pyrowave_clock_t *clock, uint64_t generation, uint64_t epoch);
/* Reserve mapping capacity BEFORE accepting a device write. Retire with
 * poll; full capacity applies backpressure and never overwrites provenance. */
bool record_pyrowave_clock_room(const record_pyrowave_clock_t *clock);
unsigned record_pyrowave_clock_available(const record_pyrowave_clock_t *clock);
bool record_pyrowave_clock_commit(record_pyrowave_clock_t *clock,
      uint64_t device_first, uint32_t device_frames, uint64_t source_first, uint32_t source_frames,
      uint32_t block_frames, uint32_t block_offset);
/* source_first/source_frames describe the original resampling block;
 * block_offset and device_frames describe its accepted output slice. Keeping
 * the original ratio avoids rounding drift across partial device writes. */
/* Integer sample interpolation within an accepted resampled span. Unknown
 * silence/gaps and positions beyond accepted writes return invalid. A
 * backwards device cursor invalidates this epoch until explicit reset. */
bool record_pyrowave_clock_poll(record_pyrowave_clock_t *clock, uint64_t device_frame,
      record_pyrowave_clock_position_t *position);
/* End-of-stream proof after poll: the cursor passed the final accepted
 * range ending at source_end. This does not map intervening silence or
 * make an invalid position valid. A reset or backwards cursor revokes it. */
bool record_pyrowave_clock_drained(const record_pyrowave_clock_t *clock,
      uint64_t source_end);
void record_pyrowave_clock_free(record_pyrowave_clock_t *clock);
RETRO_END_DECLS
#endif
