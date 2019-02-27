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
VideoCore OS Abstraction Layer - initialization routines
=============================================================================*/

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \file
  *
  * Some OS support libraries need some initialization. To support this, call
  * vcos_init() function at the start of day; vcos_deinit() at the end.
  */

/**
 * vcos initialization. Call this function before using other vcos functions.
 * Calls can be nested within the same process; they are reference counted so
 * that only a call from uninitialized state has any effect.
 * @note On platforms/toolchains that support it, gcc's constructor attribute or
 *       similar is used to invoke this function before main() or equivalent.
 * @return Status of initialisation.
 */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_init(void);

/**
 * vcos deinitialization. Call this function when vcos is no longer required,
 * in order to free resources.
 * Calls can be nested within the same process; they are reference counted so
 * that only a call that decrements the reference count to 0 has any effect.
 * @note On platforms/toolchains that support it, gcc's destructor attribute or
 *       similar is used to invoke this function after exit() or equivalent.
 * @return Status of initialisation.
 */
VCOSPRE_ void VCOSPOST_ vcos_deinit(void);

/**
 * Acquire global lock. This must be available independent of vcos_init()/vcos_deinit().
 */
VCOSPRE_ void VCOSPOST_ vcos_global_lock(void);

/**
 * Release global lock. This must be available independent of vcos_init()/vcos_deinit().
 */
VCOSPRE_ void VCOSPOST_ vcos_global_unlock(void);

/** Pass in the argv/argc arguments passed to main() */
VCOSPRE_ void VCOSPOST_ vcos_set_args(int argc, const char **argv);

/** Return argc. */
VCOSPRE_ int VCOSPOST_ vcos_get_argc(void);

/** Return argv. */
VCOSPRE_ const char ** VCOSPOST_ vcos_get_argv(void);

/**
 * Platform-specific initialisation.
 * VCOS internal function, not part of public API, do not call from outside
 * vcos. vcos_init()/vcos_deinit() reference count calls, so this function is
 * only called from an uninitialized state, i.e. there will not be two
 * consecutive calls to vcos_platform_init() without an intervening call to
 * vcos_platform_deinit().
 * This function is called with vcos_global_lock held.
 * @return Status of initialisation.
 */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_platform_init(void);

/**
 * Platform-specific de-initialisation.
 * VCOS internal function, not part of public API, do not call from outside
 * vcos.
 * See vcos_platform_init() re reference counting.
 * This function is called with vcos_global_lock held.
 */
VCOSPRE_ void VCOSPOST_ vcos_platform_deinit(void);

#ifdef __cplusplus
}
#endif
