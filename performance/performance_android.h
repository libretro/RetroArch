/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Copyright (C) 2010 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef RARCH_PERF_CPU_FEATURES_H
#define RARCH_PERF_CPU_FEATURES_H

#include <sys/cdefs.h>
#include <stdint.h>

#define ANDROID_CPU_FAMILY_UNKNOWN  0
#define ANDROID_CPU_FAMILY_ARM      1
#define ANDROID_CPU_FAMILY_X86      2
#define ANDROID_CPU_FAMILY_MIPS     3
#define ANDROID_CPU_FAMILY_MAX      4

#define ANDROID_CPU_ARM_FEATURE_ARMv7       (1)
#define ANDROID_CPU_ARM_FEATURE_VFPv3       (2)
#define ANDROID_CPU_ARM_FEATURE_NEON        (4)
#define ANDROID_CPU_ARM_FEATURE_LDREX_STREX (8)

#define ANDROID_CPU_X86_FEATURE_SSE3        (1)
#define ANDROID_CPU_X86_FEATURE_POPCNT      (2)
#define ANDROID_CPU_X86_FEATURE_MOVBE       (4)

extern unsigned rarch_perf_get_cpu_family(void);
extern uint64_t rarch_perf_get_cpu_features(void);
extern unsigned rarch_perf_get_cpu_count(void);

#endif /* RARCH_PERF_CPU_FEATURES_H */
