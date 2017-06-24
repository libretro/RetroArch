/* limits.h
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
*/

#ifndef TINYALSA_LIMITS_H
#define TINYALSA_LIMITS_H

#include "interval.h"

#include <limits.h>
#include <stdint.h>

#define TINYALSA_SIGNED_INTERVAL_MAX SSIZE_MAX
#define TINYALSA_SIGNED_INTERVAL_MIN SSIZE_MIN

#define TINYALSA_UNSIGNED_INTERVAL_MAX SIZE_MAX
#define TINYALSA_UNSIGNED_INTERVAL_MIN SIZE_MIN

#define TINYALSA_CHANNELS_MAX 32U
#define TINYALSA_CHANNELS_MIN 1U

#define TINYALSA_FRAMES_MAX (ULONG_MAX / (TINYALSA_CHANNELS_MAX * 4))
#define TINYALSA_FRAMES_MIN 0U

#if TINYALSA_FRAMES_MAX > TINYALSA_UNSIGNED_INTERVAL_MAX
#error "Frames max exceeds measurable value."
#endif

#if TINYALSA_FRAMES_MIN < TINYALSA_UNSIGNED_INTERVAL_MIN
#error "Frames min exceeds measurable value."
#endif

extern const struct tinyalsa_unsigned_interval tinyalsa_channels_limit;

extern const struct tinyalsa_unsigned_interval tinyalsa_frames_limit;

#endif /* TINYALSA_LIMITS_H */

