/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_avfoundation_worker_native.h"
#include "../../../record/record_avfoundation_session_native.h"
#include "../../../record/record_gpu_worker_vt_native.h"
/* Compiled against the real Apple SDK independently of hardware availability.
 * Runtime GPU->encoder->mux acceptance is a separate required gate. */
record_gpu_worker_vt_t *record_vt_worker_sdk_probe(record_gpu_queue_t *queue,
      record_gpu_capture_metal_t *capture,videotoolbox_encode_t *encoder)
{
   return record_gpu_worker_vt_new_native(queue,capture,encoder);
}
