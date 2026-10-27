/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_AAC_PROBE_H
#define RECORD_AAC_PROBE_H
#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
#define RECORD_AAC_PROBE_FRAMES 8192u
typedef struct record_aac_probe record_aac_probe_t;
/* Recording I/O owner only. The encoded prefix must remain in the file for
 * decoder dependencies, then be excluded by its presentation edit. */
record_aac_probe_t *record_aac_probe_new(unsigned rate, unsigned channels);
const int16_t *record_aac_probe_input(record_aac_probe_t *probe,
      size_t offset, size_t *frames);
bool record_aac_probe_config(record_aac_probe_t *probe,
      const void *asc, size_t bytes);
/* One borrowed AAC packet; decode only the bounded prefix. */
bool record_aac_probe_packet(record_aac_probe_t *probe,
      const void *data, size_t bytes);
/* At most16 delay candidates per call. -1 failure,0 waiting for packets,
 * 1 more matching work,2 ready. No I/O, waits or allocation. */
int record_aac_probe_step(record_aac_probe_t *probe);
bool record_aac_probe_delay(const record_aac_probe_t *probe, unsigned *delay);
void record_aac_probe_free(record_aac_probe_t *probe);
RETRO_END_DECLS
#endif
