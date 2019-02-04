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

#if !defined( VC_DEBUG_SYM_H )
#define VC_DEBUG_SYM_H

/* ---- Include Files ----------------------------------------------------- */

#include <stddef.h>
#include "interface/vcos/vcos_stdint.h"

/* ---- Constants and Types ---------------------------------------------- */

typedef struct
{
    const char *label;
    uint32_t    addr;
    size_t      size;

} VC_DEBUG_SYMBOL_T;

/*
 * Debug header+params are constructed by makefiles/internals/memorymap.mk
 * (first .text section) and stored in the VC binary
 */
typedef struct
{
    uint32_t    symbolTableOffset;
    uint32_t    magic;
    uint32_t    paramSize;
} VC_DEBUG_HEADER_T;

typedef struct
{
    uint32_t    vcMemBase; /* base address of loaded binary */
    uint32_t    vcMemSize;
    uint32_t    vcEntryPoint;
    uint32_t    symbolTableLength;
} VC_DEBUG_PARAMS_T;

#define VC_DEBUG_HEADER_MAGIC  (('V' << 0) + ('C' << 8) + ('D' << 16) + ('H' << 24))

// Offset within the videocore memory map to get the address of the debug header.
#define VC_DEBUG_HEADER_OFFSET 0x2800

#if (defined( __VC4_BIG_ISLAND__ ) || defined (PLATFORM_RASPBERRYPI)) && !defined( __COVERITY__ )

    /* Use of the debug symbols only makes sense on machines which have
     * some type of shared memory architecture.
     */

#   define  USE_VC_DEBUG_SYMS   1
#else
#   define  USE_VC_DEBUG_SYMS   0
#endif

#if USE_VC_DEBUG_SYMS
#   define  VCDBG_PRAGMA(foo) pragma foo
#else
#   define  VCDBG_PRAGMA(foo)
#endif

/*
 * NOTE: The section name has to start with .init or the linker will
 *       purge it out of the image (since nobody has any references to these).
 */

#if USE_VC_DEBUG_SYMS
#define VC_DEBUG_SYMBOL(name,label,addr,size) \
    VCDBG_PRAGMA( Data(LIT, ".init.vc_debug_sym" ); ) \
    static const VC_DEBUG_SYMBOL_T vc_sym_##name = { label, (uint32_t)addr, size }; \
    VCDBG_PRAGMA( Data )
#else
#define VC_DEBUG_SYMBOL(name,label,addr,size)
#endif

#define VC_DEBUG_VAR(var)   VC_DEBUG_SYMBOL(var,#var,&var,sizeof(var))

/*
 * When using VC_DEBUG_VAR, you typically want to use uncached memory, otherwise 
 * it will go in cached memory and the host won't get a coherent view of the 
 * memory. So we take advantage of the .ucdata section which gets linked into 
 * the uncached memory space. 
 *  
 * Now this causes another problem, namely that the videocore linker ld/st 
 * instructions only have 27-bit relocations, and accessing a global from 
 * uncached memory is more than 27-bits away. So we create a couple of macros 
 * which can be used to declare a variable and put it into the .ucdata section 
 * and another macro which will dereference it as if it were a pointer. 
 *  
 * To use: 
 *  
 *      VC_DEBUG_DECLARE_UNCACHED_VAR( int, foo, 0xCafeBeef );
 *      #define foo VC_DEBUG_ACCESS_UNCACHED_VAR(foo)
 */

#define VC_DEBUG_DECLARE_UNCACHED_VAR(var_type,var_name,var_init) \
    VCDBG_PRAGMA( Data(".ucdata"); ) \
    var_type var_name = (var_init); \
    VCDBG_PRAGMA( Data(); ) \
    var_type *vc_var_ptr_##var_name = &var_name; \
    VC_DEBUG_VAR(var_name)

#define VC_DEBUG_EXTERN_UNCACHED_VAR(var_type,var_name) \
    extern var_type var_name; \
    static var_type *vc_var_ptr_##var_name = &var_name

#define VC_DEBUG_DECLARE_UNCACHED_STATIC_VAR(var_type,var_name,var_init) \
    VCDBG_PRAGMA( Data(".ucdata"); ) \
    static var_type var_name = (var_init); \
    VCDBG_PRAGMA( Data(); ) \
    static var_type *vc_var_ptr_##var_name = &var_name; \
    VC_DEBUG_VAR(var_name)

#define VC_DEBUG_DECLARE_UNCACHED_VAR_ZERO(var_type,var_name) \
    VCDBG_PRAGMA( Data(".ucdata"); ) \
    var_type var_name = {0}; \
    VCDBG_PRAGMA( Data(); ) \
    var_type *vc_var_ptr_##var_name = &var_name; \
    VC_DEBUG_VAR(var_name)

#define VC_DEBUG_DECLARE_UNCACHED_STATIC_VAR_ZERO(var_type,var_name) \
    VCDBG_PRAGMA( Data(".ucdata"); ) \
    static var_type var_name = {0}; \
    VCDBG_PRAGMA( Data(); ) \
    static var_type *vc_var_ptr_##var_name = &var_name; \
    VC_DEBUG_VAR(var_name)

#define VC_DEBUG_ACCESS_UNCACHED_VAR(var_name) (*vc_var_ptr_##var_name)

/*
 * Declare a constant character string. This doesn't need to be uncached
 * since it never changes.
 */

#define VC_DEBUG_DECLARE_STRING_VAR(var_name,var_init) \
    const char var_name[] = var_init; \
    VC_DEBUG_VAR(var_name)

/*
 * Since some variable aren't actually referenced by the videocore 
 * this variant uses a .init.* section to ensure that the variable 
 * doesn't get pruned by the linker.
 */

#define VC_DEBUG_DECLARE_UNCACHED_STATIC_VAR_NO_PTR(var_type,var_name,var_init) \
    VCDBG_PRAGMA( Data(".init.ucdata"); ) \
    static var_type var_name = (var_init); \
    VCDBG_PRAGMA( Data(); ) \
    VC_DEBUG_VAR(var_name)

/* ---- Variable Externs ------------------------------------------------- */

#if USE_VC_DEBUG_SYMS
extern VC_DEBUG_SYMBOL_T    __VC_DEBUG_SYM_START[];
extern VC_DEBUG_SYMBOL_T    __VC_DEBUG_SYM_END[];
#endif

/* ---- Function Prototypes ---------------------------------------------- */

#endif /* VC_DEBUG_SYM_H */

