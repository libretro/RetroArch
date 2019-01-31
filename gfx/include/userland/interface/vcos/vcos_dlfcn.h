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

/*=============================================================================
VCOS - abstraction over dynamic library opening
=============================================================================*/

#ifndef VCOS_DLFCN_H
#define VCOS_DLFCN_H

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 *
 * Loading dynamic libraries. See also dlfcn.h.
 */

/** Open a dynamic library.
  *
  * @param name  name of the library
  * @param mode  Load lazily or immediately (VCOS_DL_LAZY, VCOS_DL_NOW, VCOS_DL_LOCAL, VCOS_DL_GLOBAL).
  *
  * @return A handle for use in subsequent calls.
  */
VCOSPRE_ void * VCOSPOST_ vcos_dlopen(const char *name, int mode);

/** Look up a symbol.
  *
  * @param handle Handle to open
  * @param name   Name of function
  *
  * @return Function pointer, or NULL.
  */
VCOSPRE_ void VCOSPOST_ (*vcos_dlsym(void *handle, const char *name))(void);

/** Close a library
  *
  * @param handle Handle to close
  */
VCOSPRE_ int VCOSPOST_ vcos_dlclose (void *handle);

/** Return error message from library.
  *
  * @param err  On return, set to non-zero if an error has occurred
  * @param buf  Buffer to write error to
  * @param len  Size of buffer (including terminating NUL).
  */
VCOSPRE_ int VCOSPOST_ vcos_dlerror(int *err, char *buf, size_t buflen);


#ifdef __cplusplus
}
#endif
#endif


