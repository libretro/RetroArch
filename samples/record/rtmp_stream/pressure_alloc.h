/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef STREAM_PRESSURE_ALLOC_H
#define STREAM_PRESSURE_ALLOC_H
/* Preserve retro_spsc.c feature selection before forcing a libc header. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE) \
      && !(defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) \
         && (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1050))
#define _DARWIN_C_SOURCE
#endif
#include <stdlib.h>
extern size_t pressure_bytes, pressure_calls, pressure_live;
void *pressure_malloc(size_t size);
void *pressure_calloc(size_t count, size_t size);
void *pressure_realloc(void *pointer, size_t size);
void pressure_free(void *pointer);
/* Forced into each fixture/production TU by this target alone. */
#define malloc pressure_malloc
#define calloc pressure_calloc
#define realloc pressure_realloc
#define free pressure_free
#endif
