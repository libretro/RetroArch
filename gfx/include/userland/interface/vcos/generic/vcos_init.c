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

/*
The following header is included in order to provide one instance of the symbol vcos_deprecated_code.
Any other inclusions of this header (or "vcos_deprecated_code.inc" for assembly language) will cause the linker to warn
about multiple definitions of vcos_deprecated_code.
The idea is to include this header file for the source files which are deprecated.
Therefore the above warnning in a build indicates that the build is using deprecated code!
Contact the person named in the accompanying comment for advice - do not remove the inclusion.
*/
#include "vcos_deprecated.h"

#include "interface/vcos/vcos.h"

static int init_refcount;

VCOS_STATUS_T vcos_init(void)
{
   VCOS_STATUS_T st = VCOS_SUCCESS;

   vcos_global_lock();

   if (init_refcount++ == 0)
      st = vcos_platform_init();

   vcos_global_unlock();

   return st;
}

void vcos_deinit(void)
{
   vcos_global_lock();

   vcos_assert(init_refcount > 0);

   if (init_refcount > 0 && --init_refcount == 0)
      vcos_platform_deinit();

   vcos_global_unlock();
}

#if defined(__GNUC__) && (__GNUC__ > 2)

void vcos_ctor(void) __attribute__((constructor, used));

void vcos_ctor(void)
{
   vcos_init();
}

void vcos_dtor(void) __attribute__((destructor, used));

void vcos_dtor(void)
{
   vcos_deinit();
}

#endif
