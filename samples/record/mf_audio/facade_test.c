/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_aac_audio.h"
/* Run the same native lifetime/clock contract through the shared owner. */
#define record_mf_audio_t record_aac_audio_t
#define record_mf_audio_packet_t record_aac_audio_packet_t
#define record_mf_audio_new record_aac_audio_new
#define record_mf_audio_input record_aac_audio_input
#define record_mf_audio_submit record_aac_audio_submit
#define record_mf_audio_pump record_aac_audio_pump
#define record_mf_audio_release record_aac_audio_release
#define record_mf_audio_config record_aac_audio_config
#define record_mf_audio_finish record_aac_audio_finish
#define record_mf_audio_cancel record_aac_audio_cancel
#define record_mf_audio_free record_aac_audio_free
#define record_mf_audio_error record_aac_audio_error
#define RECORD_MF_AUDIO_ERROR RECORD_AAC_AUDIO_ERROR
#define RECORD_MF_AUDIO_BUSY RECORD_AAC_AUDIO_BUSY
#define RECORD_MF_AUDIO_PROGRESS RECORD_AAC_AUDIO_PROGRESS
#define RECORD_MF_AUDIO_PACKET RECORD_AAC_AUDIO_PACKET
#define RECORD_MF_AUDIO_END RECORD_AAC_AUDIO_END
#include "encode_test.c"
