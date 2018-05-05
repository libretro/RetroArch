/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (getopt.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_COMPAT_GETOPT_H
#define __LIBRETRO_SDK_COMPAT_GETOPT_H

#if defined(RARCH_INTERNAL) && defined(HAVE_CONFIG_H)
#include "../../../config.h"
#endif

/* Custom implementation of the GNU getopt_long for portability.
 * Not designed to be fully compatible, but compatible with
 * the features RetroArch uses. */

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
/* Avoid possible naming collisions during link since we
 * prefer to use the actual name. */
#define getopt_long(argc, argv, optstring, longopts, longindex) __getopt_long_retro(argc, argv, optstring, longopts, longindex)

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct option
{
   const char *name;
   int has_arg;
   int *flag;
   int val;
};

/* argv[] is declared with char * const argv[] in GNU,
 * but this makes no sense, as non-POSIX getopt_long
 * mutates argv (non-opts are moved to the end). */
int getopt_long(int argc, char *argv[],
      const char *optstring, const struct option *longopts, int *longindex);
extern char *optarg;
extern int optind, opterr, optopt;

RETRO_END_DECLS

/* If these are variously #defined, then we have bigger problems */
#ifndef no_argument
   #define no_argument 0
   #define required_argument 1
   #define optional_argument 2
#endif

/* HAVE_GETOPT_LONG */
#endif

/* pragma once */
#endif

