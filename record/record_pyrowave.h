/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_H
#define RECORD_PYROWAVE_H
#include <stddef.h>
typedef struct record_pyrowave_packet
{
   const void *data;
   size_t bytes;
} record_pyrowave_packet_t;
#endif
