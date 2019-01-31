/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "interface/khronos/common/khrn_options.h"

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>

KHRN_OPTIONS_T khrn_options;

/* For now, we will use environment variables for the options.
   We might, at some point, want to use ini files perhaps */
static bool read_bool_option(const char *name, bool cur)
{
   char *val = getenv(name);

   if (val == NULL)
      return cur;

   if (val[0] == 't' || val[0] == 'T' || val[0] == '1')
      return true;

   return false;
}

static uint32_t read_uint32_option(const char *name, uint32_t cur)
{
   char *val = getenv(name);

   if (val == NULL)
      return cur;

   if (val[0] != '\0')
      return atoi(val);

   return 0;
}

void khrn_init_options(void)
{
   /* Default values are all 0 */
   memset(&khrn_options, 0, sizeof(KHRN_OPTIONS_T));

#ifndef DISABLE_OPTION_PARSING
   /* Now read the options */
   khrn_options.gl_error_assist        = read_bool_option(  "V3D_GL_ERROR_ASSIST",        khrn_options.gl_error_assist);
   khrn_options.double_buffer          = read_bool_option(  "V3D_DOUBLE_BUFFER",          khrn_options.double_buffer);
   khrn_options.no_bin_render_overlap  = read_bool_option(  "V3D_NO_BIN_RENDER_OVERLAP",  khrn_options.no_bin_render_overlap);
   khrn_options.reg_dump_on_lock       = read_bool_option(  "V3D_REG_DUMP_ON_LOCK",       khrn_options.reg_dump_on_lock);
   khrn_options.clif_dump_on_lock      = read_bool_option(  "V3D_CLIF_DUMP_ON_LOCK",      khrn_options.clif_dump_on_lock);
   khrn_options.force_dither_off       = read_bool_option(  "V3D_FORCE_DITHER_OFF",       khrn_options.force_dither_off);

   khrn_options.bin_block_size         = read_uint32_option("V3D_BIN_BLOCK_SIZE",         khrn_options.bin_block_size);
   khrn_options.max_bin_blocks         = read_uint32_option("V3D_MAX_BIN_BLOCKS",         khrn_options.max_bin_blocks);
#endif
}

void khrn_error_assist(GLenum error, const char *func)
{
   if (khrn_options.gl_error_assist && error != GL_NO_ERROR)
   {
      fprintf(stderr, "V3D ERROR ASSIST : ");
      switch (error)
      {
      case GL_INVALID_ENUM      : fprintf(stderr, "GL_INVALID_ENUM in %s\n", func); break;
      case GL_INVALID_VALUE     : fprintf(stderr, "GL_INVALID_VALUE in %s\n", func); break;
      case GL_INVALID_OPERATION : fprintf(stderr, "GL_INVALID_OPERATION in %s\n", func); break;
      case GL_OUT_OF_MEMORY     : fprintf(stderr, "GL_OUT_OF_MEMORY in %s\n", func); break;
      default                   : fprintf(stderr, "ERROR CODE %d in %s\n", (int)error, func); break;
      }
      fflush(stderr);
   }
}
