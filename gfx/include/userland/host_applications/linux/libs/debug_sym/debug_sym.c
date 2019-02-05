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

/**
*
*  @file    debug_sym.c
*
*  @brief   The usermode process which implements displays the messages.
*
****************************************************************************/

// ---- Include Files -------------------------------------------------------

#include "interface/vcos/vcos.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#if defined(WIN32)
# include <io.h>
#elif defined(__CYGWIN__)
# include <sys/mman.h>
#else
# include <sys/mman.h>
# include <sys/ioctl.h>
# include <vc_mem.h>
#endif

#include "debug_sym.h"
#include "vc_debug_sym.h"

// ---- Public Variables ----------------------------------------------------
// ---- Private Constants and Types -----------------------------------------

#define MAX_VC_SIZE  8     /* a sanity upper bound on memory size */

#ifndef PAGE_SIZE
# if defined(__CYGWIN__)
#  define PAGE_SIZE   65536 /* Cygwin mmap requires rounding to SYSTEM_INFO.dwAllocationGranularity */
# else
#  define PAGE_SIZE   4096
# endif
#endif
#ifndef PAGE_MASK
# define PAGE_MASK   (~(PAGE_SIZE - 1))
#endif

// Offset within the videocore memory map to get the address of the symbol
// table.
#define VC_SYMBOL_BASE_OFFSET       VC_DEBUG_HEADER_OFFSET

struct opaque_vc_mem_access_handle_t
{
    int                 memFd;
    int                 memFdBase;     /* The VideoCore address mapped to offset 0 of memFd */
    VC_MEM_ADDR_T       vcMemBase;     /* The VideoCore address of the start of the loaded image */
    VC_MEM_ADDR_T       vcMemLoad;     /* The VideoCore address of the start of the code image */
    VC_MEM_ADDR_T       vcMemEnd;      /* The VideoCore address of the end of the loaded image */
    VC_MEM_SIZE_T       vcMemSize;     /* The amount of memory used by the loaded image */
    VC_MEM_ADDR_T       vcMemPhys;     /* The VideoCore memory physical address */

    VC_MEM_ADDR_T       vcSymbolTableOffset;
    unsigned            numSymbols;
    VC_DEBUG_SYMBOL_T  *symbol;
    int                 use_vc_mem;    /* using mmap-ed memory rather than real file */
};

#if 1
   #define DBG( fmt, ... )    vcos_log_trace( "%s: " fmt, __FUNCTION__, ##__VA_ARGS__ )
   #define ERR( fmt, ... )    vcos_log_error( "%s: " fmt, __FUNCTION__, ##__VA_ARGS__ )
#else
   #define DBG( fmt, ... )    fprintf( stderr, "%s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__ )
   #define ERR( fmt, ... )    fprintf( stderr, "%s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__ )
#endif

typedef enum
{
    READ_MEM,
    WRITE_MEM,
} MEM_OP_T;

#if defined( WIN32 )
#define	open		_open
#define	close		_close
#define	O_SYNC		0
#endif

// ---- Private Variables ---------------------------------------------------

#define  VCOS_LOG_CATEGORY (&debug_sym_log_category)
static VCOS_LOG_CAT_T  debug_sym_log_category;

// ---- Private Function Prototypes -----------------------------------------

// ---- Functions -----------------------------------------------------------

/****************************************************************************
*
*   Get access to the videocore memory space. Returns zero if the memory was
*   opened successfully.
*
***************************************************************************/

int OpenVideoCoreMemory( VC_MEM_ACCESS_HANDLE_T *vcHandlePtr  )
{
    return OpenVideoCoreMemoryFile( NULL, vcHandlePtr );
}

/****************************************************************************
*
*   Get access to the videocore memory space. Returns zero if the memory was
*   opened successfully.
*
***************************************************************************/

struct fb_dmacopy {
	void *dst;
	uint32_t src;
	uint32_t length;
};
#define FBIODMACOPY _IOW('z', 0x22, struct fb_dmacopy)

static int vc_mem_copy(void *dst, uint32_t src, uint32_t length)
{
    const char *filename = "/dev/fb0";
    int memFd;
    struct fb_dmacopy ioparam;

    ioparam.dst = dst;
    ioparam.src = src;
    ioparam.length = length;

    if (( memFd = open( filename, O_RDWR | O_SYNC )) < 0 )
    {
        ERR( "Unable to open '%s': %s(%d)\n", filename, strerror( errno ), errno );
        return -errno;
    }

    if ( ioctl( memFd, FBIODMACOPY, &ioparam ) != 0 )
    {
        ERR( "Failed to get memory size via ioctl: %s(%d)\n",
            strerror( errno ), errno );
        close( memFd );
        return -errno;
    }
    close( memFd );
    return 0;
}

int OpenVideoCoreMemoryFile( const char *filename, VC_MEM_ACCESS_HANDLE_T *vcHandlePtr )
{
    return OpenVideoCoreMemoryFileWithOffset( filename, vcHandlePtr, 0 );
}

int OpenVideoCoreMemoryFileWithOffset( const char *filename, VC_MEM_ACCESS_HANDLE_T *vcHandlePtr, size_t loadOffset )
{
    int                     rc = 0;
    VC_MEM_ACCESS_HANDLE_T  newHandle;
    VC_DEBUG_SYMBOL_T       debug_sym;
    VC_MEM_ADDR_T           symAddr;
    size_t                  symTableSize;
    unsigned                symIdx;

    struct
    {
        VC_DEBUG_HEADER_T header;
        VC_DEBUG_PARAMS_T params;

    } vc_dbg;

    vcos_log_register( "debug_sym", &debug_sym_log_category );

    if (( newHandle = calloc( 1, sizeof( *newHandle ))) == NULL )
    {
        return -ENOMEM;
    }

    if ( filename == NULL )
    {
        newHandle->use_vc_mem = 1;
        filename = "/dev/vc-mem";
    }
    else
    {
        newHandle->use_vc_mem = 0;
    }

    if (( newHandle->memFd = open( filename, ( newHandle->use_vc_mem ? O_RDWR : O_RDONLY ) | O_SYNC )) < 0 )
    {
        ERR( "Unable to open '%s': %s(%d)\n", filename, strerror( errno ), errno );
        free(newHandle);
        return -errno;
    }
    DBG( "Opened %s memFd = %d", filename, newHandle->memFd );

    if ( newHandle->use_vc_mem )
    {
        newHandle->memFdBase = 0;

#if defined(WIN32) || defined(__CYGWIN__)
#define VC_MEM_SIZE  (128 * 1024 * 1024)
        newHandle->vcMemSize = VC_MEM_SIZE;
        newHandle->vcMemBase = 0;
        newHandle->vcMemLoad = 0;
        newHandle->vcMemPhys = 0;
#else
        if ( ioctl( newHandle->memFd, VC_MEM_IOC_MEM_SIZE, &newHandle->vcMemSize ) != 0 )
        {
            ERR( "Failed to get memory size via ioctl: %s(%d)\n",
                strerror( errno ), errno );
            free(newHandle);
            return -errno;
        }
        if ( ioctl( newHandle->memFd, VC_MEM_IOC_MEM_BASE, &newHandle->vcMemBase ) != 0 )
        {
            ERR( "Failed to get memory base via ioctl: %s(%d)\n",
                strerror( errno ), errno );
            free(newHandle);
            return -errno;
        }
        if ( ioctl( newHandle->memFd, VC_MEM_IOC_MEM_LOAD, &newHandle->vcMemLoad ) != 0 )
        {
            ERR( "Failed to get memory load via ioctl: %s(%d)\n",
                strerror( errno ), errno );
				/* Backward compatibility. */
            newHandle->vcMemLoad = newHandle->vcMemBase;
        }
        if ( ioctl( newHandle->memFd, VC_MEM_IOC_MEM_PHYS_ADDR, &newHandle->vcMemPhys ) != 0 )
        {
            ERR( "Failed to get memory physical address via ioctl: %s(%d)\n",
                strerror( errno ), errno );
            free(newHandle);
            return -errno;
        }
#endif
    }
    else
    {
        off_t len = lseek( newHandle->memFd, 0, SEEK_END );
        if ( len < 0 )
        {
            ERR( "Failed to seek to end of file: %s(%d)\n", strerror( errno ), errno );
            free(newHandle);
            return -errno;
        }
        newHandle->vcMemPhys = 0;
        newHandle->vcMemSize = len;
        newHandle->vcMemBase = 0;
        newHandle->vcMemLoad = loadOffset;
        newHandle->memFdBase = 0;
    }

    DBG( "vcMemSize = %08x", newHandle->vcMemSize );
    DBG( "vcMemBase = %08x", newHandle->vcMemBase );
    DBG( "vcMemLoad = %08x", newHandle->vcMemLoad );
    DBG( "vcMemPhys = %08x", newHandle->vcMemPhys );

    newHandle->vcMemEnd = newHandle->vcMemBase + newHandle->vcMemSize - 1;

    // See if we can detect the symbol table
    if ( !ReadVideoCoreMemory( newHandle,
                               &newHandle->vcSymbolTableOffset,
                               newHandle->vcMemLoad + VC_SYMBOL_BASE_OFFSET,
                               sizeof( newHandle->vcSymbolTableOffset )))
    {
        ERR( "ReadVideoCoreMemory @VC_SYMBOL_BASE_OFFSET (0x%08x) failed\n", VC_SYMBOL_BASE_OFFSET );
        rc = -EIO;
        goto err_exit;
    }

    // When reading from a file, the VC binary is read into a buffer and the effective base is 0.
    // But that may not be the actual base address the binary is intended to be loaded to.  The
    // following reads the debug header to find out what the actual base address is.
    if( !newHandle->use_vc_mem )
    {
        // Read the complete debug header
        if ( !ReadVideoCoreMemory( newHandle,
                                   &vc_dbg,
                                   newHandle->vcMemLoad + VC_SYMBOL_BASE_OFFSET,
                                   sizeof( vc_dbg )))
        {
            ERR( "ReadVideoCoreMemory @VC_SYMBOL_BASE_OFFSET (0x%08x) failed\n", VC_SYMBOL_BASE_OFFSET );
            rc = -EIO;
            goto err_exit;
        }
        // The vc_dbg header gives the "base" address of the VC binary,
        // which debug_sym calls the "load" address, so we need to adjust
        // it by loadOffset to find the base of the whole memory dump file
        newHandle->memFdBase = vc_dbg.params.vcMemBase - loadOffset;
        newHandle->vcMemBase = vc_dbg.params.vcMemBase - loadOffset;
        newHandle->vcMemLoad = vc_dbg.params.vcMemBase;
        newHandle->vcMemEnd = newHandle->memFdBase + newHandle->vcMemSize - 1;

        DBG( "Updated from debug header:" );
        DBG( "vcMemSize = %08x", newHandle->vcMemSize );
        DBG( "vcMemBase = %08x", newHandle->vcMemBase );
        DBG( "vcMemLoad = %08x", newHandle->vcMemLoad );
        DBG( "vcMemPhys = %08x", newHandle->vcMemPhys );
    }

    DBG( "vcSymbolTableOffset = 0x%08x", newHandle->vcSymbolTableOffset );

    // Make sure that the pointer points into the first few megabytes of
    // the memory space.
    if ( (newHandle->vcSymbolTableOffset - newHandle->vcMemLoad) > ( MAX_VC_SIZE * 1024 * 1024 ))
    {
        ERR( "newHandle->vcSymbolTableOffset (0x%x - 0x%x) > %dMB\n", newHandle->vcSymbolTableOffset, newHandle->vcMemLoad, MAX_VC_SIZE );
        rc = -EIO;
        goto err_exit;
    }

    // Make a pass to count how many symbols there are.

    symAddr = newHandle->vcSymbolTableOffset;
    newHandle->numSymbols = 0;
    do
    {
        if ( !ReadVideoCoreMemory( newHandle,
                                   &debug_sym,
                                   symAddr,
                                   sizeof( debug_sym )))
        {
            ERR( "ReadVideoCoreMemory @ symAddr(0x%08x) failed\n", symAddr );
            rc = -EIO;
            goto err_exit;
        }

        newHandle->numSymbols++;

        DBG( "Symbol %d: label: 0x%p addr: 0x%08x size: %zu",
             newHandle->numSymbols,
             debug_sym.label,
             debug_sym.addr,
             debug_sym.size );

        if ( newHandle->numSymbols > 1024 )
        {
            // Something isn't sane.

            ERR( "numSymbols (%d) > 1024 - looks wrong\n", newHandle->numSymbols );
            rc = -EIO;
            goto err_exit;
        }
        symAddr += sizeof( debug_sym );

    } while ( debug_sym.label != 0 );
    newHandle->numSymbols--;

    DBG( "Detected %d symbols", newHandle->numSymbols );

    // Allocate some memory to hold the symbols, and read them in.

    symTableSize = newHandle->numSymbols * sizeof( debug_sym );
    if (( newHandle->symbol = malloc( symTableSize )) == NULL )
    {
        rc = -ENOMEM;
        goto err_exit;
    }
    if ( !ReadVideoCoreMemory( newHandle,
                               newHandle->symbol,
                               newHandle->vcSymbolTableOffset,
                               symTableSize ))
    {
        ERR( "ReadVideoCoreMemory @ newHandle->vcSymbolTableOffset(0x%08x) failed\n", newHandle->vcSymbolTableOffset );
        rc = -EIO;
        goto err_exit;
    }

    // The names of the symbols are pointers in videocore space. We want
    // to have them available locally, so we make copies and fixup
    // the pointer.

    for ( symIdx = 0; symIdx < newHandle->numSymbols; symIdx++ )
    {
        VC_DEBUG_SYMBOL_T   *sym;
        char                 symName[ 256 ];

        sym = &newHandle->symbol[ symIdx ];

        DBG( "Symbol %d: label: 0x%p addr: 0x%08x size: %zu",
             symIdx,
             sym->label,
             sym->addr,
             sym->size );

        if ( !ReadVideoCoreMemory( newHandle,
                                   symName,
                                   TO_VC_MEM_ADDR(sym->label),
                                   sizeof( symName )))
        {
            ERR( "ReadVideoCoreMemory @ sym->label(0x%08x) failed\n", TO_VC_MEM_ADDR(sym->label) );
            rc = -EIO;
            goto err_exit;
        }
        symName[ sizeof( symName ) - 1 ] = '\0';
        sym->label = vcos_strdup( symName );

        DBG( "Symbol %d (@0x%p): label: '%s' addr: 0x%08x size: %zu",
             symIdx,
             sym,
             sym->label,
             sym->addr,
             sym->size );
    }

    *vcHandlePtr = newHandle;
    return 0;

err_exit:
    close( newHandle->memFd );
    free( newHandle );

    return rc;
}

/****************************************************************************
*
*   Returns the number of symbols which were detected.
*
***************************************************************************/

unsigned NumVideoCoreSymbols( VC_MEM_ACCESS_HANDLE_T vcHandle )
{
    return vcHandle->numSymbols;
}

/****************************************************************************
*
*   Returns the name, address and size of the i'th symbol.
*
***************************************************************************/

int GetVideoCoreSymbol( VC_MEM_ACCESS_HANDLE_T vcHandle, unsigned idx, char *labelBuf, size_t labelBufSize, VC_MEM_ADDR_T *vcMemAddr, size_t *vcMemSize )
{
    VC_DEBUG_SYMBOL_T   *sym;

    if ( idx >= vcHandle->numSymbols )
    {
        return -EINVAL;
    }
    sym = &vcHandle->symbol[ idx ];

    strncpy( labelBuf, sym->label, labelBufSize );
    labelBuf[labelBufSize - 1] = '\0';

    if ( vcMemAddr != NULL )
    {
        *vcMemAddr = (VC_MEM_ADDR_T)sym->addr;
    }
    if ( vcMemSize != NULL )
    {
        *vcMemSize = sym->size;
    }

    return 0;
}

/****************************************************************************
*
*   Looks up the named, symbol. If the symbol is found, it's value and size
*   are returned.
*
*   Returns  true if the lookup was successful.
*
***************************************************************************/

int LookupVideoCoreSymbol( VC_MEM_ACCESS_HANDLE_T vcHandle, const char *symbol, VC_MEM_ADDR_T *vcMemAddr, size_t *vcMemSize )
{
    unsigned        idx;
    char            symName[ 64 ];
    VC_MEM_ADDR_T   symAddr = 0;
    size_t          symSize = 0;

    for ( idx = 0; idx < vcHandle->numSymbols; idx++ )
    {
        GetVideoCoreSymbol( vcHandle, idx, symName, sizeof( symName ), &symAddr, &symSize );
        if ( strcmp( symbol, symName ) == 0 )
        {
            if ( vcMemAddr != NULL )
            {
                *vcMemAddr = symAddr;
            }
            if ( vcMemSize != 0 )
            {
                *vcMemSize = symSize;
            }

            DBG( "%s found, addr = 0x%08x size = %zu", symbol, symAddr, symSize );
            return 1;
        }
    }

    if ( vcMemAddr != NULL )
    {
        *vcMemAddr = 0;
    }
    if ( vcMemSize != 0 )
    {
        *vcMemSize = 0;
    }
    DBG( "%s not found", symbol );
    return 0;
}

/****************************************************************************
*
*   Looks up the named, symbol. If the symbol is found, and it's size is equal
*   to the sizeof a uint32_t, then true is returned.
*
***************************************************************************/

int LookupVideoCoreUInt32Symbol( VC_MEM_ACCESS_HANDLE_T vcHandle,
                                 const char *symbol,
                                 VC_MEM_ADDR_T *vcMemAddr )
{
    size_t  vcMemSize;

    if ( !LookupVideoCoreSymbol( vcHandle, symbol, vcMemAddr, &vcMemSize ))
    {
        return 0;
    }

    if ( vcMemSize != sizeof( uint32_t ))
    {
        ERR( "Symbol: '%s' has a size of %zu, expecting %zu", symbol, vcMemSize, sizeof( uint32_t ));
        return 0;
    }
    return 1;
}

/****************************************************************************
*
*   Does Reads or Writes on the videocore memory.
*
***************************************************************************/

static int AccessVideoCoreMemory( VC_MEM_ACCESS_HANDLE_T vcHandle,
                                  MEM_OP_T               mem_op,
                                  void                  *buf,
                                  VC_MEM_ADDR_T          vcMemAddr,
                                  size_t                 numBytes )
{
    VC_MEM_ADDR_T origVcMemAddr = vcMemAddr;
    DBG( "%s %zu bytes @ 0x%08x", mem_op == WRITE_MEM ? "Write" : "Read", numBytes, vcMemAddr );

    /*
     * Since we'll be passed videocore pointers, we need to deal with the high bits.
     *
     * We need to strip off the high 2 bits to convert to a physical address, except
     * for when the high 3 bits are equal to 011, which means that it corresponds to
     * a peripheral and isn't accessible.
     */

    if ( IS_ALIAS_PERIPHERAL( vcMemAddr ))
    {
        // This is a peripheral address.

        ERR( "Can't access peripheral address 0x%08x", vcMemAddr );
        return 0;
    }
    vcMemAddr = TO_VC_MEM_ADDR(ALIAS_NORMAL( vcMemAddr ));

    #if 0
    if ( (vcMemAddr < vcHandle->vcMemBase) ||
         (vcMemAddr > vcHandle->vcMemEnd) )
    {
        ERR( "Memory address 0x%08x is outside range 0x%08x-0x%08x", vcMemAddr,
             vcHandle->vcMemBase, vcHandle->vcMemEnd );
        return 0;
    }
    #endif
    if (( vcMemAddr + numBytes - 1) > vcHandle->vcMemEnd )
    {
        ERR( "Memory address 0x%08x + numBytes 0x%08zx is > memory end 0x%08x",
             vcMemAddr, numBytes, vcHandle->vcMemEnd );
        return 0;
    }

    vcMemAddr -= vcHandle->memFdBase;

#if defined( WIN32 )
    if ( mem_op != READ_MEM )
    {
        ERR( "Only reads are supported" );
        return 0;
    }
    if ( _lseek( vcHandle->memFd, vcMemAddr, SEEK_SET ) < 0 )
    {
        ERR( "_lseek position 0x%08x failed", vcMemAddr );
        return  0;
    }
    if ( _read( vcHandle->memFd, buf, numBytes ) < 0 )
    {
        ERR( "_read failed: %s(%d)", strerror( errno ), errno );
        return 0;
    }
#else
    // DMA allows memory to be accessed above 1008M and is more coherent so try this first
    if (mem_op == READ_MEM && vcHandle->use_vc_mem)
    {
        DBG( "AccessVideoCoreMemory: %p, %x, %d", buf, origVcMemAddr, numBytes );
        int s = vc_mem_copy(buf, (uint32_t)origVcMemAddr, numBytes);
        if (s == 0)
            return 1;
    }

    {
        uint8_t    *mapAddr;
        size_t      mapSize;
        size_t      memOffset;
        off_t       vcMapAddr;
        int         mmap_prot;

        if ( mem_op == WRITE_MEM )
        {
            mmap_prot = PROT_WRITE;
        }
        else
        {
            mmap_prot = PROT_READ;
        }

        // We can only map pages on 4K boundaries, so round the address down and the size up.

        memOffset = vcMemAddr & ~PAGE_MASK;

        vcMapAddr = vcMemAddr & PAGE_MASK;

        mapSize = ( memOffset + numBytes + PAGE_SIZE - 1 ) & PAGE_MASK;
        if (( mapAddr = mmap( 0, mapSize, mmap_prot, MAP_SHARED, vcHandle->memFd, vcMapAddr )) == MAP_FAILED )
        {
           ERR( "mmap failed: %s(%d)", strerror( errno ), errno );
           return 0;
        }
        if ( mem_op == WRITE_MEM )
        {
           memcpy( mapAddr + memOffset, buf, numBytes );
        }
        else
        {
           memcpy( buf, mapAddr + memOffset, numBytes );
        }

        munmap( mapAddr, mapSize );
    }
#endif

    return 1;
}

/****************************************************************************
*
*   Reads 'numBytes' from the videocore memory starting at 'vcMemAddr'. The
*   results are stored in 'buf'.
*
*   Returns true if the read was successful.
*
***************************************************************************/

int ReadVideoCoreMemory( VC_MEM_ACCESS_HANDLE_T vcHandle, void *buf, VC_MEM_ADDR_T vcMemAddr, size_t numBytes )
{
    return AccessVideoCoreMemory( vcHandle, READ_MEM, buf, vcMemAddr, numBytes );
}

/****************************************************************************
*
*   Reads 'numBytes' from the videocore memory starting at 'vcMemAddr'. The
*   results are stored in 'buf'.
*
*   Returns true if the read was successful.
*
***************************************************************************/

int ReadVideoCoreMemoryBySymbol( VC_MEM_ACCESS_HANDLE_T vcHandle, const char *symbol, void *buf, size_t bufSize )
{
    VC_MEM_ADDR_T   vcMemAddr;
    size_t          vcMemSize;

    if ( !LookupVideoCoreSymbol( vcHandle, symbol, &vcMemAddr, &vcMemSize ))
    {
        ERR( "Symbol not found: '%s'", symbol );
        return 0;
    }

    if ( vcMemSize > bufSize )
    {
        vcMemSize = bufSize;
    }

    if ( !ReadVideoCoreMemory( vcHandle, buf, vcMemAddr, vcMemSize ))
    {
        ERR( "Unable to read %zu bytes @ 0x%08x", vcMemSize, vcMemAddr );
        return 0;
    }
    return 1;
}

/****************************************************************************
*
*   Looks up a symbol and reads the contents into a user supplied buffer.
*
*   Returns true if the read was successful.
*
***************************************************************************/

int ReadVideoCoreStringBySymbol( VC_MEM_ACCESS_HANDLE_T vcHandle,
                                 const char *symbol,
                                 char *buf,
                                 size_t bufSize )
{
    VC_MEM_ADDR_T   vcMemAddr;
    size_t          vcMemSize;

    if ( !LookupVideoCoreSymbol( vcHandle, symbol, &vcMemAddr, &vcMemSize ))
    {
        ERR( "Symbol not found: '%s'", symbol );
        return 0;
    }

    if ( vcMemSize > bufSize )
    {
        vcMemSize = bufSize;
    }

    if ( !ReadVideoCoreMemory( vcHandle, buf, vcMemAddr, vcMemSize ))
    {
        ERR( "Unable to read %zu bytes @ 0x%08x", vcMemSize, vcMemAddr );
        return 0;
    }

    // Make sure that the result is null-terminated

    buf[vcMemSize-1] = '\0';
    return 1;
}

/****************************************************************************
*
*   Writes 'numBytes' into the videocore memory starting at 'vcMemAddr'. The
*   data is taken from 'buf'.
*
*   Returns true if the write was successful.
*
***************************************************************************/

int WriteVideoCoreMemory( VC_MEM_ACCESS_HANDLE_T vcHandle,
                          void *buf,
                          VC_MEM_ADDR_T vcMemAddr,
                          size_t numBytes )
{
    return AccessVideoCoreMemory( vcHandle, WRITE_MEM, buf, vcMemAddr, numBytes );
}

/****************************************************************************
*
*   Closes the memory space opened previously via OpenVideoCoreMemory.
*
***************************************************************************/

void CloseVideoCoreMemory( VC_MEM_ACCESS_HANDLE_T vcHandle )
{
    unsigned i;
    if ( vcHandle->symbol )
        for ( i = 0; i < vcHandle->numSymbols; i++ )
            free( (char *)vcHandle->symbol[i].label );
    free( vcHandle->symbol );

    if ( vcHandle->memFd >= 0 )
        close( vcHandle->memFd );

    free( vcHandle );
}

/****************************************************************************
*
*   Returns the base address of the videocore memory space.
*
***************************************************************************/

VC_MEM_ADDR_T GetVideoCoreMemoryBase( VC_MEM_ACCESS_HANDLE_T vcHandle )
{
    return vcHandle->vcMemBase;
}

/****************************************************************************
*
*   Returns the size of the videocore memory space.
*
***************************************************************************/

VC_MEM_SIZE_T GetVideoCoreMemorySize( VC_MEM_ACCESS_HANDLE_T vcHandle )
{
    return vcHandle->vcMemSize;
}

/****************************************************************************
*
*   Returns the videocore memory physical address.
*
***************************************************************************/

VC_MEM_ADDR_T GetVideoCoreMemoryPhysicalAddress( VC_MEM_ACCESS_HANDLE_T vcHandle )
{
    return vcHandle->vcMemPhys;
}
