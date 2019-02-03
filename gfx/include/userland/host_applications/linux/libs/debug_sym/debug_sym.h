/*
Copyright (c) 2013, Broadcom Europe Ltd
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

#if !defined( DEBUG_SYM_H )
#define DEBUG_SYM_H

/* ---- Include Files ----------------------------------------------------- */

#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Constants and Types ---------------------------------------------- */

typedef struct opaque_vc_mem_access_handle_t *VC_MEM_ACCESS_HANDLE_T;

/* Since the VC might have a different memory model from the host
 * (32-bit vs 64-bit), define fixed-width types that match the
 * VC memory types */
typedef uint32_t VC_MEM_ADDR_T;    /* equivalent to uintptr_t */
typedef uint32_t VC_MEM_SIZE_T;    /* equivalent to size_t */
typedef uint32_t VC_MEM_PTRDIFF_T; /* equivalent to ptrdiff_t */

#define TO_VC_MEM_ADDR(ptr)    ((VC_MEM_ADDR_T)(unsigned long)(ptr))

/* ---- Variable Externs ------------------------------------------------- */

/* ---- Function Prototypes ---------------------------------------------- */

/*
 * The following were taken from vcinclude/hardware_vc4_bigisland.h
 */

#define ALIAS_NORMAL(x)             ((VC_MEM_ADDR_T)(((unsigned long)(x)&~0xc0000000uL)|0x00000000uL)) // normal cached data (uses main 128K L2 cache)
#define IS_ALIAS_PERIPHERAL(x)      (((unsigned long)(x)>>29)==0x3uL)

/*
 * Get access to the videocore memory space. Returns zero if the memory was
 * opened successfully, or a negative value (-errno) if the access could not
 * be obtained.
 */
int OpenVideoCoreMemory( VC_MEM_ACCESS_HANDLE_T *handle  );

/*
 * Get access to the videocore space from a file. The file might be /dev/mem, or
 * it might be saved image on disk.
 */
int OpenVideoCoreMemoryFile( const char *filename, VC_MEM_ACCESS_HANDLE_T *vcHandlePtr );

/*
 * Get access to the videocore space from a file, explicitly giving the
 * offset of the load address (the start of the VC binary) relative to the
 * start of the file.
 * loadOffset is ignored if reading from memory instead of a saved image.
 */
int OpenVideoCoreMemoryFileWithOffset( const char *filename,
                                       VC_MEM_ACCESS_HANDLE_T *vcHandlePtr,
                                       size_t loadOffset );

/*
 * Returns the number of symbols which were detected.
 */
unsigned NumVideoCoreSymbols( VC_MEM_ACCESS_HANDLE_T handle );

/*
 * Returns the name, address and size of the i'th symbol.
 */
int GetVideoCoreSymbol( VC_MEM_ACCESS_HANDLE_T handle,
                        unsigned idx,
                        char *nameBuf,
                        size_t nameBufSize,
                        VC_MEM_ADDR_T *vcMemAddr,
                        size_t *vcMemSize );

/*
 * Looks up the named, symbol. If the symbol is found, it's value and size
 * are returned.
 *
 * Returns  true if the lookup was successful.
 */
int LookupVideoCoreSymbol( VC_MEM_ACCESS_HANDLE_T handle,
                           const char *symbol,
                           VC_MEM_ADDR_T *vcMemAddr,
                           size_t *vcMemSize );

/*
 * Looks up the named, symbol. If the symbol is found, and it's size is equal
 * to the sizeof a uint32_t, then true is returned.
 */
int LookupVideoCoreUInt32Symbol( VC_MEM_ACCESS_HANDLE_T handle,
                                 const char *symbol,
                                 VC_MEM_ADDR_T *vcMemAddr );

/*
 * Reads 'numBytes' from the videocore memory starting at 'vcMemAddr'. The
 * results are stored in 'buf'.
 *
 * Returns true if the read was successful.
 */
int ReadVideoCoreMemory( VC_MEM_ACCESS_HANDLE_T handle,
                         void *buf,
                         VC_MEM_ADDR_T vcMemAddr,
                         size_t numBytes );

/*
 * Reads an unsigned 32-bit value from videocore memory.
 */
VCOS_STATIC_INLINE int ReadVideoCoreUInt32( VC_MEM_ACCESS_HANDLE_T handle,
                                       uint32_t *val,
                                       VC_MEM_ADDR_T vcMemAddr )
{
    return ReadVideoCoreMemory( handle, val, vcMemAddr, sizeof( *val ));
}

/*
 * Reads a block of memory using the address associated with a symbol.
 */
int ReadVideoCoreMemoryBySymbol( VC_MEM_ACCESS_HANDLE_T vcHandle,
                                 const char            *symbol,
                                 void                  *buf,
                                 size_t                 numBytes );
/*
 * Reads an unsigned 32-bit value from videocore memory.
 */
VCOS_STATIC_INLINE int ReadVideoCoreUInt32BySymbol( VC_MEM_ACCESS_HANDLE_T handle,
                                               const char *symbol,
                                               uint32_t *val )
{
    return ReadVideoCoreMemoryBySymbol( handle, symbol, val, sizeof( *val ));
}

/*
 * Looksup a string symbol by name, and reads the contents into a user
 * supplied buffer.
 */
int ReadVideoCoreStringBySymbol( VC_MEM_ACCESS_HANDLE_T handle,
                                 const char *symbol,
                                 char *buf,
                                 size_t bufSize );

/*
 * Writes 'numBytes' into the videocore memory starting at 'vcMemAddr'. The
 * data is taken from 'buf'.
 *
 * Returns true if the write was successful.
 */
int WriteVideoCoreMemory( VC_MEM_ACCESS_HANDLE_T handle,
                          void *buf,
                          VC_MEM_ADDR_T vcMemAddr,
                          size_t numBytes );

/*
 * Writes an unsigned 32-bit value into videocore memory.
 */
VCOS_STATIC_INLINE int WriteVideoCoreUInt32( VC_MEM_ACCESS_HANDLE_T handle,
                                        uint32_t val,
                                        VC_MEM_ADDR_T vcMemAddr )
{
    return WriteVideoCoreMemory( handle, &val, vcMemAddr, sizeof( val ));
}

/*
 * Closes the memory space opened previously via OpenVideoCoreMemory.
 */
void CloseVideoCoreMemory( VC_MEM_ACCESS_HANDLE_T handle );

/*
 * Returns the base address of the videocore memory space.
 */
VC_MEM_ADDR_T GetVideoCoreMemoryBase( VC_MEM_ACCESS_HANDLE_T handle );

/*
 * Returns the size of the videocore memory space.
 */
VC_MEM_SIZE_T GetVideoCoreMemorySize( VC_MEM_ACCESS_HANDLE_T handle );

/*
 * Returns the videocore memory physical address.
 */
VC_MEM_ADDR_T GetVideoCoreMemoryPhysicalAddress( VC_MEM_ACCESS_HANDLE_T handle );

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_SYM_H */
