/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_file_win32.h"
struct record_file_win32
{
   const void *data;
   size_t size;
   uint64_t bytes;
   unsigned pending;
   uint32_t error;
   bool active;
};
extern unsigned file_delay, file_writes;
extern bool file_open_fail, file_push_fail, file_step_fail, file_finish_fail, file_cancel_fail;
