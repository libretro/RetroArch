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

#ifndef COMMON_MEM_H
#define COMMON_MEM_H

#ifndef MEM_DONT_USE_SMALL_ALLOC_POOL
#define MEM_USE_SMALL_ALLOC_POOL
#endif

#ifndef MEM_CHUNK_SIZE
#define MEM_CHUNK_SIZE           0x40000
#endif

#include <stdarg.h>

#include "vcinclude/common.h"

#ifdef _MSC_VER
   #define RCM_ALIGNOF(T) __alignof(T)
#else
   #define RCM_ALIGNOF(T) (sizeof(struct { T t; char ch; }) - sizeof(T))
#endif

#ifdef _MSC_VER
   #define RCM_INLINE __inline
#else
   #ifdef __LCC__
      #define RCM_INLINE
   #else
      #define RCM_INLINE inline
   #endif
#endif

/******************************************************************************
Global initialisation helpers
******************************************************************************/

/*
   Locate the relocatable heap partition, or malloc sufficient space, then
   call mem_init.
 */
int32_t rtos_common_mem_init( void );

/******************************************************************************
Pool management API
******************************************************************************/

/*
   Options for mem_compact.
*/

typedef enum
{
   /* these values are duplicated in rtos_common_mem.inc */

   MEM_COMPACT_NONE       = 0,   /* No compaction allowed */
   MEM_COMPACT_NORMAL     = 1,   /* Move unlocked blocks */
   MEM_COMPACT_DISCARD    = 2,   /* _NORMAL + discard blocks where possible */
   MEM_COMPACT_AGGRESSIVE = 4,   /* _NORMAL + move locked blocks where possible */
   MEM_COMPACT_ALL        = MEM_COMPACT_NORMAL | MEM_COMPACT_DISCARD | MEM_COMPACT_AGGRESSIVE,
   MEM_COMPACT_NOT_BLOCKING = 8,    /* don't retry if initial allocation fails */

   MEM_COMPACT_SHUFFLE    = 0x80 /* Move the lowest unlocked block up to the top
                                    (space permitting) - for testing */
} mem_compact_mode_t;

/*
   Get default values for memory pool defined in the platform makefile
*/

extern void mem_get_default_partition(void **mempool_base, uint32_t *mempool_size, void **mempool_handles_base, uint32_t *mempool_handles_size);

/*
   Initialize the memory subsystem, allocating a pool of a given size and
   with space for the given number of handles.
*/

extern int mem_init(void *mempool_base, uint32_t mempool_size, void *mempool_handles_base, uint32_t mempool_handles_size);

/*
   Terminate the memory subsystem, releasing the pool.
*/

extern void mem_term(void);

/*
   The heap is compacted to the maximum possible extent. If (mode & MEM_COMPACT_DISCARD)
   is non-zero, all discardable, unlocked, and unretained MEM_HANDLE_Ts are resized to size 0.
   If (mode & MEM_COMPACT_AGGRESSIVE) is non-zero, all long-term block owners (which are
   obliged to have registered a callback) are asked to unlock their blocks for the duration
   of the compaction.
*/

extern void mem_compact(mem_compact_mode_t mode);

/******************************************************************************
Movable memory core API
******************************************************************************/

/*
   A MEM_HANDLE_T refers to a movable block of memory.

   The only way to get a MEM_HANDLE_T is to call mem_alloc.

   The MEM_HANDLE_T you get is immutable and remains valid until its reference
   count reaches 0.

   A MEM_HANDLE_T has an initial reference count of 1. This can be incremented
   by calling mem_acquire and decremented by calling mem_release.
*/

/*
   MEM_ZERO_SIZE_HANDLE is a preallocated handle to a zero-size block of memory
   MEM_EMPTY_STRING_HANDLE is a preallocated handle to a block of memory containing the empty string

   MEM_HANDLE_INVALID is the equivalent of NULL for MEM_HANDLE_Ts -- no valid
   MEM_HANDLE_T will ever equal MEM_HANDLE_INVALID.
*/

typedef enum { /* enum to get stricter type checking on msvc */
#ifdef MEM_USE_SMALL_ALLOC_POOL
   MEM_ZERO_SIZE_HANDLE = 0x80000000,
   MEM_EMPTY_STRING_HANDLE = 0x80000001,
#else
   MEM_ZERO_SIZE_HANDLE = 1,
   MEM_EMPTY_STRING_HANDLE = 2,
#endif

   MEM_HANDLE_INVALID = 0,

   MEM_HANDLE_FORCE_32BIT = 0x80000000,

   // deprecated - for backward compatibility
   MEM_INVALID_HANDLE = MEM_HANDLE_INVALID
} MEM_HANDLE_T;

/*
   Flags are set once in mem_alloc -- they do not change over the lifetime of
   the MEM_HANDLE_T.
*/

typedef enum {
   MEM_FLAG_NONE = 0,

   /*
      If a MEM_HANDLE_T is discardable, the memory manager may resize it to size
      0 at any time when it is not locked or retained.
   */

   MEM_FLAG_DISCARDABLE = 1 << 0,

   /*
      MEM_FLAG_RETAINED should only ever be used when passing flags to
      mem_alloc. If it is set, the initial retain count is 1, otherwise it is 0.
   */

   MEM_FLAG_RETAINED = 1 << 9, /* shared with MEM_FLAG_ABANDONED. only used when passing flags to mem_alloc */

   /*
      Block must be kept within bottom 256M region of the relocatable heap.
      Specifying this flag means that an allocation will fail if the block
      cannot be allocated within that region, and the block will not be moved
      out of that range.
      (This is to support memory blocks used by the codec cache, which must
      have same top 4 bits; see HW-3058)
      This flag is ignored on non-VideoCore platforms.
   */

   MEM_FLAG_LOW_256M = 1 << 1,

   /*
      Define a mask to extract the memory alias used by the block of memory.
   */

   MEM_FLAG_ALIAS_MASK = 3 << 2,

   /*
      If a MEM_HANDLE_T is allocating (or normal), its block of memory will be
      accessed in an allocating fashion through the cache.
   */

   MEM_FLAG_NORMAL = 0 << 2,
   MEM_FLAG_ALLOCATING = MEM_FLAG_NORMAL,

   /*
      If a MEM_HANDLE_T is direct, its block of memory will be accessed
      directly, bypassing the cache.
   */

   MEM_FLAG_DIRECT = 1 << 2,

   /*
      If a MEM_HANDLE_T is coherent, its block of memory will be accessed in a
      non-allocating fashion through the cache.
   */

   MEM_FLAG_COHERENT = 2 << 2,

   /*
      If a MEM_HANDLE_T is L1-nonallocating, its block of memory will be accessed by
      the VPU in a fashion which is allocating in L2, but only coherent in L1.
   */

   MEM_FLAG_L1_NONALLOCATING = (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT),

   /*
      If a MEM_HANDLE_T is zero'd, its contents are set to 0 rather than
      MEM_HANDLE_INVALID on allocation and resize up.
   */

   MEM_FLAG_ZERO = 1 << 4,

   /*
      If a MEM_HANDLE_T is uninitialised, it will not be reset to a defined value
      (either zero, or all 1's) on allocation.
    */

   MEM_FLAG_NO_INIT = 1 << 5,

   /*
      The INIT flag is a placeholder, designed to make it explicit that
      initialisation is required, and to make it possible to change the sense
      of this bit at a later date.
    */

   MEM_FLAG_INIT    = 0 << 5,

   /*
      Hints.
   */

   MEM_FLAG_HINT_PERMALOCK = 1 << 6, /* Likely to be locked for long periods of time. */

   MEM_FLAG_HINT_ALL = 0xc0,

   MEM_FLAG_USER = 1 << 7,
   MEM_FLAG_HINT_GROW      = 1 << 7, /* Likely to grow in size over time. If this flag is specified, MEM_FLAG_RESIZEABLE must also be. */

   MEM_FLAG_UNUSED = 1 << 7,

   /*
      If a MEM_HANDLE_T is to be resized with mem_resize, this flag must be
      present. This flag prevents things from being allocated out of the small
      allocation pool.
   */
   MEM_FLAG_RESIZEABLE = 1 << 8,

   /*
      If the ABANDONED flag is set, because mem_abandon was called when the lock
      count was zero, the contents are undefined. This flag is cleared by
      mem_lock. Automatically set for blocks allocated with MEM_FLAG_NO_INIT.
   */

   MEM_FLAG_ABANDONED = 1 << 9, /* shared with MEM_FLAG_RETAINED. never used when passing flags to mem_alloc */

   /* There is currently room in MEM_HEADER_X_T for 10 flags */
   MEM_FLAG_ALL = 0x3ff
} MEM_FLAG_T;

/*
   A MEM_HANDLE_T may optionally have a terminator. This is a function that will
   be called just before the MEM_HANDLE_T becomes invalid: see mem_release.
*/

typedef void (*MEM_TERM_T)(void *, uint32_t);

/*
   A common way of storing a MEM_HANDLE_T together with an offset into it.
*/

typedef struct
{
   MEM_HANDLE_T mh_handle;
   uint32_t offset;
} MEM_HANDLE_OFFSET_T;

/*
   Attempts to allocate a movable block of memory of the specified size and
   alignment. size may be 0. A MEM_HANDLE_T with size 0 is special: see
   mem_lock/mem_unlock. mode specifies the types of compaction permitted,
   including MEM_COMPACT_NONE.

   Preconditions:

   - align is a power of 2.
   - flags only has set bits within the range specified by MEM_FLAG_ALL.
   - desc is NULL or a pointer to a null-terminated string.
   - the caller of this function is contracted to later call mem_release (or pass such responsibility on) if we don't return MEM_HANDLE_INVALID

   Postconditions:

   If the attempt succeeds:
   - A fresh MEM_HANDLE_T referring to the allocated block of memory is
     returned.
   - The MEM_HANDLE_T is unlocked, without a terminator, and has a reference
     count of 1.
   - If MEM_FLAG_RETAINED was specified, the MEM_HANDLE_T has a retain count of
     1, otherwise it is unretained.
   - If the MEM_FLAG_ZERO flag was specified, the block of memory is set to 0.
     Otherwise, each aligned word is set to MEM_HANDLE_INVALID.

   If the attempt fails:
   - MEM_HANDLE_INVALID is returned.
*/

extern MEM_HANDLE_T mem_alloc_ex(
   uint32_t size,
   uint32_t align,
   MEM_FLAG_T flags,
   const char *desc,
   mem_compact_mode_t mode);

#define mem_alloc(s,a,f,d) mem_alloc_ex(s,a,f,d,MEM_COMPACT_ALL)

#define MEM_WRAP_HACK

#ifdef MEM_WRAP_HACK
extern MEM_HANDLE_T mem_wrap(void *p, uint32_t size, uint32_t align, MEM_FLAG_T flags, const char *desc);

typedef void (*MEM_WRAP_TERM_T)(void * /*priv*/, MEM_HANDLE_T /*term_handle*/, void * /*p*/, int /*size*/);

/*
   int mem_wrap_set_on_term(MEM_HANDLE_T handle, MEM_WRAP_TERM_T term_cb, void *cb_priv);

   Adds an additional release callback for wrapped MEM_HANDLE_T.
   This allows the underlying allocator to release the wrapped memory.
   The MEM_HANDLE_T must NOT be accessed with mem_lock/mem_acquire etc
   from the callback context as the handle is already partially released.
*/
extern int mem_wrap_set_on_term(MEM_HANDLE_T handle, MEM_WRAP_TERM_T term_cb, void *cb_priv);

#endif

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The reference count of the MEM_HANDLE_T is incremented.
*/

extern void mem_acquire(
   MEM_HANDLE_T handle);

/*
   Calls mem_acquire and also calls mem_retain if the handle is discardable.
*/

extern void mem_acquire_retain(MEM_HANDLE_T handle);

/*
   If the reference count of the MEM_HANDLE_T is 1 and it has a terminator, the
   terminator is called with a pointer to and the size of the MEM_HANDLE_T's
   block of memory (or NULL/0 if the size of the MEM_HANDLE_T is 0). The
   MEM_HANDLE_T may not be used during the call.

   Preconditions:

   - handle is a valid MEM_HANDLE_T.
   - If its reference count is 1, it must not be locked or retained.

   Postconditions:

   If the reference count of the MEM_HANDLE_T was 1:
   - The MEM_HANDLE_T is now invalid. The associated block of memory has been
     freed.

   Otherwise:
   - The reference count of the MEM_HANDLE_T is decremented.
*/

extern void mem_release(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   If the reference count of the MEM_HANDLE_T was 1:
   - false is returned.
   - The reference count of the MEM_HANDLE_T is still 1.

   Otherwise:
   - true is returned.
   - The reference count of the MEM_HANDLE_T is decremented.
*/

extern int mem_try_release(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The total space used by the MEM_HANDLE_T is returned (this includes the
     header, the actual block, and padding).
   - sum_over_handles(mem_get_space(handle)) + mem_get_free_space() is constant
     over the lifetime of the pool.
*/

extern uint32_t mem_get_space(
   MEM_HANDLE_T handle);

/*
   uint32_t mem_get_size(MEM_HANDLE_T handle);

   The size of the MEM_HANDLE_T's block of memory is returned.
   This is consistent with the size requested in a mem_alloc call.

   Implementation notes:

   -

   Preconditions:

   handle is not MEM_HANDLE_INVALID

   Postconditions:

   result <= INT_MAX

   Invariants preserved:

   -
*/

extern uint32_t mem_get_size(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The minimum required alignment of the MEM_HANDLE_T's block of memory (as
     passed to mem_alloc) is returned.
   -
*/

extern uint32_t mem_get_align(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The MEM_HANDLE_T's flags (as passed to mem_alloc) are returned.
*/

extern MEM_FLAG_T mem_get_flags(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The MEM_HANDLE_T's reference count is returned.
*/

extern uint32_t mem_get_ref_count(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The MEM_HANDLE_T's lock count is returned.
*/

extern uint32_t mem_get_lock_count(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The MEM_HANDLE_T's retain count is returned.
*/

extern uint32_t mem_get_retain_count(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The MEM_HANDLE_T's description string is returned.
*/

extern const char *mem_get_desc(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.
   - desc is NULL or a pointer to a null-terminated string.

   Postconditions:

   - The MEM_HANDLE_T's description is set to desc.
*/

extern void mem_set_desc(
   MEM_HANDLE_T handle,
   const char *desc);

extern void mem_set_desc_vprintf(
   MEM_HANDLE_T handle,
   const char *fmt,
   va_list ap);

extern void mem_set_desc_printf(
   MEM_HANDLE_T handle,
   const char *fmt,
   ...);

/*
   void mem_set_term(MEM_HANDLE_T handle, MEM_TERM_T term)

   The MEM_HANDLE_T's terminator is set to term (if term was NULL, the
   MEM_HANDLE_T no longer has a terminator).
   The MEM_HANDLE_T's terminator is called just before the MEM_HANDLE_T becomes
   invalid: see mem_release.

   Preconditions:

   handle is a valid handle to a (possibly uninitialised or partially initialised*)
   object of type X

   This implies mem_get_size(handle) == sizeof(type X)

   memory management invariants for handle are satisfied

   term must be NULL or a pointer to a function compatible with the MEM_TERM_T
   type:

      void *term(void *v, uint32_t size)

   if term is not NULL, its precondition must be no stronger than the following:
       is only called from memory manager internals during destruction of an object of type X
       v is a valid pointer to a (possibly uninitialised or partially initialised*) object of type X
       memory management invariants for v are satisfied
       size == sizeof(type X)

   if term is not NULL, its postcondition must be at least as strong as the following:
       Frees any references to resources that are referred to by the object of type X

   Postconditions:

   -
*/

extern void mem_set_term(
   MEM_HANDLE_T handle,
   MEM_TERM_T term);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The MEM_HANDLE_T's terminator is returned, or NULL if there is none.
*/

extern MEM_TERM_T mem_get_term(
   MEM_HANDLE_T handle);

/*
   void mem_set_user_flag(MEM_HANDLE_T handle, int flag)

   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   - The MEM_HANDLE_T's user flag is set to 0 if flag is 0, or to 1 otherwise.
*/

extern void mem_set_user_flag(
   MEM_HANDLE_T handle, int flag);

/*
   Attempts to resize the MEM_HANDLE_T's block of memory. The attempt is
   guaranteed to succeed if the new size is less than or equal to the old size.
   size may be 0. A MEM_HANDLE_T with size 0 is special: see
   mem_lock/mem_unlock. mode specifies the types of compaction permitted,
   including MEM_COMPACT_NONE.

   Preconditions:

   - handle is a valid MEM_HANDLE_T.
   - It must not be locked.

   Postconditions:

   If the attempt succeeds:
   - true is returned.
   - The MEM_HANDLE_T's block of memory has been resized.
   - The contents in the region [0, min(old size, new size)) are unchanged. If
     the MEM_HANDLE_T is zero'd, the region [min(old size, new size), new size)
     is set to 0. Otherwise, each aligned word in the region
     [min(old size, new size), new size) is set to MEM_HANDLE_INVALID.

   If the attempt fails:
   - false is returned.
   - The MEM_HANDLE_T is still valid.
   - Its block of memory is unchanged.
*/

extern int mem_resize_ex(
   MEM_HANDLE_T handle,
   uint32_t size,
   mem_compact_mode_t mode);

#define mem_resize(h,s) mem_resize_ex(h,s,MEM_COMPACT_ALL)

/*
   A MEM_HANDLE_T with a lock count greater than 0 is considered to be locked
   and may not be moved by the memory manager.

   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   If the MEM_HANDLE_T's size is 0:
   - NULL is returned.
   - The MEM_HANDLE_T is completely unchanged.

   Otherwise:
   - A pointer to the MEM_HANDLE_T's block of memory is returned. The pointer is
     valid until the MEM_HANDLE_T's lock count reaches 0.
   - The MEM_HANDLE_T's lock count is incremented.
   - Clears MEM_FLAG_ABANDONED.
*/

/*@null@*/ extern void *mem_lock(
   MEM_HANDLE_T handle);

/*
   Lock a number of memory handles and store the results in an array of pointers.
   May be faster than calling mem_lock repeatedly as we only need to acquire the
   memory mutex once.
   For convenience you can also pass invalid handles in, and get out either null pointers or
   valid pointers (depending on the associated offset field)

   Preconditions:

   pointers is a valid pointer to n elements
   handles is a valid pointer to n elements
   For all 0 <= i < n
   - handles[i].mh_handle is MEM_HANDLE_INVALID or a valid MEM_HANDLE_T.
   - If handles[i] != MEM_HANDLE_INVALID then handles[i].offset <= handles[i].size

   Postconditions:

   For all 0 <= i < n
      If handles[i] == MEM_HANDLE_INVALID:
      - pointers[i] is set to offsets[i]

      Else if handles[i].mh_handle.size == 0:
      - pointers[i] is set to 0.
      - handles[i].mh_handle is completely unchanged.

      Otherwise:
      - pointers[i] is set to a pointer which is valid until handles[i].lockcount reaches 0
      - pointers[i] points to a block of size (handles[i].size - handles[i].offset)
      - handles[i].mh_handle.lockcount is incremented.
      - MEM_FLAG_ABANDONED is cleared in handles[i].mh_handle.x.flags
*/

extern void mem_lock_multiple(
   void **pointers,
   MEM_HANDLE_OFFSET_T *handles,
   uint32_t n);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.
   - If its size is not 0, it must be locked.

   Postconditions:

   If the MEM_HANDLE_T's size is 0:
   - The MEM_HANDLE_T is completely unchanged.

   Otherwise:
   - The MEM_HANDLE_T's lock count is decremented.
*/

extern void mem_unlock(
   MEM_HANDLE_T handle);

/*
   Unlock a number of memory handles.
   May be faster than calling mem_unlock repeatedly as we only need to acquire the
   memory mutex once.

   Preconditions:

   pointers is a valid pointer to n elements
   handles is a valid pointer to n elements
   For all 0 <= i < n
   - handles[i].mh_handle is a valid MEM_HANDLE_T.
   - If handles[i].mh_handle.size is not 0, it must be locked.

   Postconditions:

   For all 0 <= i < n
      If handles[i] == MEM_HANDLE_INVALID or handles[i].mh_handle.size == 0:
      - handles[i].mh_handle is completely unchanged.

      Otherwise:
      - handles[i].mh_handle.lockcount is decremented.
*/

extern void mem_unlock_multiple(
   MEM_HANDLE_OFFSET_T *handles,
   uint32_t n);

/*
   Like mem_unlock_multiple, but will unretain handles if they are discardable.
   Also releases handles.
*/

extern void mem_unlock_unretain_release_multiple(
   MEM_HANDLE_OFFSET_T *handles,
   uint32_t n);

/*
   Like mem_unlock_unretain_release_multiple, but without the unlocking.
   Also releases handles.
*/

extern void mem_unretain_release_multiple(
   MEM_HANDLE_OFFSET_T *handles,
   uint32_t n);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.

   Postconditions:

   If the MEM_HANDLE_T is not a small handle:
   - Sets MEM_FLAG_ABANDONED, which causes the data content to become undefined
    when the lock count reaches zero.
   - Sets MEM_FLAG_NO_INIT.

   Otherwise:
   - Does nothing.
*/

extern void mem_abandon(
   MEM_HANDLE_T handle);

/*
   A discardable MEM_HANDLE_T with a retain count greater than 0 is
   considered retained and may not be discarded by the memory manager.

   Preconditions:

   - handle is a valid MEM_HANDLE_T.
   - It must be discardable.

   Postconditions:

   - 0 is returned if the size of the MEM_HANDLE_T's block of memory is 0,
     otherwise 1 is returned.
   - The retain count of the MEM_HANDLE_T is incremented.
*/

extern int mem_retain(
   MEM_HANDLE_T handle);

/*
   Preconditions:

   - handle is a valid MEM_HANDLE_T.
   - It must be retained.

   Postconditions:

   - The retain count of the MEM_HANDLE_T is decremented.
*/

extern void mem_unretain(
   MEM_HANDLE_T handle);

/*
   A version of mem_lock which adds an indication that an aggressive compaction
   should not wait for the block to be unlocked.
*/

extern void *mem_lock_perma(
   MEM_HANDLE_T handle);

/*
   A version of mem_unlock which removes an indication that an aggressive
   compaction should not wait for the block to be unlocked.
*/

extern void mem_unlock_perma(
   MEM_HANDLE_T handle);

/******************************************************************************
Legacy memory blocks API
******************************************************************************/

/*
   Allocate a fixed-size block. The size of legacy blocks is a constant for the
   life of the memory subsystem (and may be a build-time constant). Legacy
   blocks are stored in offline chunks, so this is a veneer over mem_offline.

   Preconditions:

   - flags must specify a memory alias
     o MEM_FLAG_NORMAL
     o MEM_FLAG_COHERENT
     o MEM_FLAG_DIRECT
     o MEM_FLAG_L1_NONALLOCATING (VC4 only)
   - other permitted flags
     o MEM_FLAG_LOW_256M

   Postconditions:

   If the attempt succeeds:
   - A pointer to the legacy block is returned.

   If the attempt fails:
   - NULL (0) is returned.
*/

extern void *mem_alloc_legacy_ex(MEM_FLAG_T flags);

#define mem_alloc_legacy() mem_alloc_legacy_ex(MEM_FLAG_NORMAL)

/*
   Free a previously-allocated fixed-size block.

   Preconditions:

   - ptr must point to a block previously returned by mem_alloc_legacy.

   Postconditions:

   - None.
*/

extern void mem_free_legacy(void *ptr);

/*
   If count_max is positive then it sets a maximum number of legacy blocks which
   can be allocated, otherwise the maximum possible (capped at 32) are allowed.

   Preconditions:

   - align must be a power of two.
   - There must be no allocated legacy blocks.

   Postconditions:

   If the attempt succeeds:
   - Returns the maximum number of legacy blocks, assuming no other allocations.

   If the attempt fails:
   - Returns -1.
*/

extern int mem_init_legacy(uint32_t size, uint32_t align, int count_max);

/*
   Preconditions:

   - None.

   Postconditions:

   - Returns the size of the legacy blocks.
*/

extern uint32_t mem_get_legacy_size(void);

/*
   Preconditions:

   - None.

   Postconditions:

   - Returns the alignment of the legacy blocks.
*/

extern uint32_t mem_get_legacy_align(void);

/******************************************************************************
Offline chunks API
******************************************************************************/

/*
   Mark a contiguous chunk as being "offline". This is similar to a regular
   block which is allocated and locked, except that chunks are always a
   multiple of MEM_CHUNK_SIZE, and have no headers. Also, the allocator prefers
   chunks at higher addresses.
   mode specifies the types of compaction permitted, including MEM_COMPACT_NONE.
   If no contiguous range of chunks can be found, the heap will be compacted
   before retrying.

   Preconditions:

   - size must be an integer multiple of MEM_CHUNK_SIZE
   - flags must specify a memory alias
     o MEM_FLAG_NORMAL
     o MEM_FLAG_COHERENT
     o MEM_FLAG_DIRECT
     o MEM_FLAG_L1_NONALLOCATING (VC4 only)
   - other permitted flags
     o MEM_FLAG_LOW_256M

   Postconditions:

   If the attempt succeeds:
   - A pointer to the offline block is returned.

   If the attempt fails:
   - NULL (0) is returned.
*/

extern void *mem_offline(uint32_t size, MEM_FLAG_T flags,
   mem_compact_mode_t mode);

extern int mem_offline_chunks(uint32_t num_chunks, MEM_FLAG_T flags,
   mem_compact_mode_t mode);

/*
   Free a previously-allocated fixed-size block. Note that it is legal
   to take a large chunk offline and then bring a portion of it back
   online.

   Preconditions:

   - ptr must point to a block previously returned by mem_alloc_legacy,
     or a chunk-aligned section thereof.
   - size must be an integer multiple of MEM_CHUNK_SIZE.

   Postconditions:

   - None.
*/

extern void mem_online(void *ptr, uint32_t size);

extern void mem_online_chunks(int chunk, int num_chunks);

/*
   Retrieves various statistics about the heap chunks.
   Note that _used + _available may not equal _total in the case of
   overlapping areas.
 */

extern void mem_get_chunk_stats(uint32_t *total,
   uint32_t *legacy_used, uint32_t *legacy_available, uint32_t *legacy_total,
   uint32_t *offline_used, uint32_t *offline_available, uint32_t *offline_total);

/******************************************************************************
Long-term lock owners' API
******************************************************************************/

typedef enum
{
   /* An aggressive compaction is beginning. Any long-term locks should be released. */
   MEM_CALLBACK_REASON_UNLOCK,

   /* An aggressive compaction has completed. Any long-term locks can be reclaimed. */
   MEM_CALLBACK_REASON_RELOCK,

   /* The total amount of free memory has fallen below the threshold
    * defined by mem_set_low_memory_threshold.
    * To avoid repeated callbacks this callback is only invoked for the
    * allocation that caused the free memory threshold to be crossed.
    *
    * Caveats
    * The internal overheads of the heap are ignored.
    * Small allocs are ignored.
    * The available memory may be fragmented.
    */
   MEM_CALLBACK_REASON_LOW_MEMORY,

   MEM_CALLBACK_REASON_MAX
} mem_callback_reason_t;

typedef void (*mem_callback_func_t)(mem_callback_reason_t reason, uintptr_t context);

/* Returns 1 on success, 0 on failure. */
extern int mem_register_callback(mem_callback_func_t func, uintptr_t context);

/* Defines the threshold in bytes at which the
 * MEM_CALLBACK_REASON_LOW_MEMORY will be invoked.
 */
extern void mem_set_low_mem_threshold(uint32_t threshold);

extern void mem_unregister_callback(mem_callback_func_t func, uintptr_t context);

/******************************************************************************
Compaction notification API
******************************************************************************/

/** Type of compaction operation. */
typedef enum
{
   /* A compaction is about to begin. */
   MEM_COMPACT_OP_BEGIN,

   /* A compaction has just ended. */
   MEM_COMPACT_OP_END,

   MEM_COMPACT_OP_MAX
} mem_compact_op_t;

/**
 * Compaction notification function. Called when a relocatable heap compaction is
 * about to begin or has just ended.
 * This gives the callback the ability to delay the start of compaction.
 * Will be invoked independently from those registered via mem_register_callback().
 * @param op      Compaction operation.
 * @param context Context passed in compaction registration.
 * @param retries Number of retries remaining (assuming at least one callback returns non-zero).
 *                0 means the last call for this compaction.
 * @return non-zero to delay compaction; 0 if ok for compaction to start.
 */
typedef int (*mem_compact_cb_t)(mem_compact_op_t op, uintptr_t context, int retries);

/**
 * Register a callback function to be invoked at the start and end of every relocatable
 * heap compaction.
 * @return 1 on success; 0 on failure.
 * @note Do not call from a compaction notification function.
 */
int mem_register_compact_cb(mem_compact_cb_t func, uintptr_t context);

/**
 * Unregister a callback registered via mem_register_compact_cb().
 * @note Do not call from a compaction notification function.
 */
void mem_unregister_compact_cb(mem_compact_cb_t func, uintptr_t context);

/******************************************************************************
Movable memory helpers
******************************************************************************/

/*
   Enable/disable the memory shuffler.  Only has effect if the memory shuffler
   has been included in the build (by setting the MEM_SHUFFLE define).  The shuffler
   will be started and enabled by default.  A zero value of enable will disable
   compactions.  A non-zero value for enable will enable compactions.
*/
void rtos_common_mem_shuffle_enable(int enable);

extern MEM_HANDLE_T mem_strdup_ex(
   const char *str,
   mem_compact_mode_t mode);

#define mem_strdup(str) mem_strdup_ex((str),MEM_COMPACT_ALL)

/*
   Allocate a new buffer (with a single reference) with the same size and
   contents as the supplied buffer. This is intended to be used for data buffers
   rather than structures (structures would require adding a reference to each
   of their member handles). Hence the returned buffer does not have a
   terminator and we require that the supplied buffer doesn't have one either.

   At present, *all* flags are carried across to the new buffer. I'm not sure
   if this is wise. TODO: decide which we should allow, and whether any extra
   ones should be passed as arguments.

   Alignment is carried across too.

   A valid MEM_HANDLE_T must be passed. It must have no terminator.
*/

extern MEM_HANDLE_T mem_dup_ex(
   MEM_HANDLE_T handle,
   const char *desc,
   mem_compact_mode_t mode);

#define mem_dup(handle,desc) mem_dup_ex((handle),(desc),MEM_COMPACT_ALL)

extern void mem_print_state(void);
extern void mem_print_small_alloc_pool_state(void);
extern uint32_t mem_debug_get_alloc_count(void);

/*
   Retrieves various statistics about the allocated blocks.
 */

extern void mem_get_stats(uint32_t *blocks, uint32_t *bytes, uint32_t *locked);

/*
   Returns the size of the pool.
*/

extern uint32_t mem_get_total_space(void);

/*
   Returns the size of the largest possible allocation, assuming no fragmentation.
 */

extern uint32_t mem_get_free_space(void);

/*
   Returns the actual amount of free space. You won't actually be able to
   allocate anything this big due to overhead. If the heap is empty, the return
   value should equal mem_get_total_space()
*/

extern uint32_t mem_get_actual_free_space(void);

/*
   Returns the free space statistics. NULL pointers will not be written to.
 */

extern void mem_get_free_space_stats(uint32_t *total, uint32_t *max, uint32_t *count);

/*
   Returns the current estimate of available free space. Quicker than mem_get_free_space.
 */

extern uint32_t mem_get_low_mem_available_space(void);

/*
   Check the internal consistency of the heap. A corruption will result in
   a failed assert, which will cause a breakpoint in a debug build.

   If MEM_FILL_FREE_SPACE is defined for the build, then free space is also
   checked for corruption; this has a large performance penalty, but can help
   to track down random memory corruptions.

   Note that defining MEM_AUTO_VALIDATE will enable the automatic validation
   of the heap at key points - allocation, deallocation and compaction.

   Preconditions:

   - None.

   Postconditions:

   - None.
*/

extern void mem_validate(void);

/*
   Attempts to allocate a movable block of memory of the same size and alignment
   as the specified structure type.

   Implementation Notes:

   The returned object obeys the invariants of the memory subsystem only.  Invariants
   of the desired structure type may not yet be obeyed.
   The memory will be filled such that any handles in the structure would be
   interpreted as MEM_HANDLE_INVALID

   Preconditions:

   STRUCT is a structure type

   the caller of this macro is contracted to later call mem_release (or pass such responsibility on) if we don't return MEM_HANDLE_INVALID

   Postconditions:

   If the attempt succeeds:
   - A fresh MEM_HANDLE_T referring to the allocated block of memory is
     returned.
   - The MEM_HANDLE_T is unlocked, unretained, without a terminator, and has a
     reference count of 1.

   If the attempt fails:
   - MEM_HANDLE_INVALID is returned.
*/

#define MEM_ALLOC_STRUCT_EX(STRUCT, mode) mem_alloc_ex(sizeof(STRUCT), RCM_ALIGNOF(STRUCT), MEM_FLAG_NONE, #STRUCT, mode)
#define MEM_ALLOC_STRUCT(STRUCT) mem_alloc(sizeof(STRUCT), RCM_ALIGNOF(STRUCT), MEM_FLAG_NONE, #STRUCT)

/*
  Find out if a memory pointer is within the relocatable pool (excluding legacy blocks).
  Returns non-zero if it is.
 */

extern int mem_is_relocatable(const void *ptr);

#ifndef VCMODS_LCC
// LCC doesn't support inline so cannot define these functions in a header file

static RCM_INLINE void mem_assign(MEM_HANDLE_T *x, MEM_HANDLE_T y)
{
   if (y != MEM_HANDLE_INVALID)
      mem_acquire(y);
   if (*x != MEM_HANDLE_INVALID)
      mem_release(*x);

   *x = y;
}

/*
   MEM_ASSIGN(x, y)

   Overwrite a handle with another handle, managing reference counts appropriately

   Implementation notes:

   Always use the macro version rather than the inline function above

   Preconditions:

   each of x and y is MEM_HANDLE_INVALID or a handle to a block with non-zero ref_count
   if x is not MEM_HANDLE_INVALID and x != y and x.ref_count is 1, x.lock_count is zero
   is y is not MEM_HANDLE_INVALID there must at some point be a MEM_ASSIGN(x, MEM_HANDLE_INVALID)

   Postconditions:

   Invariants preserved:
*/

#define MEM_ASSIGN(x, y)            mem_assign(&(x), (y))

/*@null@*/ static RCM_INLINE void * mem_maybe_lock(MEM_HANDLE_T handle)
{
   if (handle == MEM_HANDLE_INVALID)
      return 0;
   else
      return mem_lock(handle);
}

static RCM_INLINE void mem_maybe_unlock(MEM_HANDLE_T handle)
{
   if (handle != MEM_HANDLE_INVALID)
      mem_unlock(handle);
}

#endif

extern void mem_assign_null_multiple(
   MEM_HANDLE_OFFSET_T *handles,
   uint32_t n);

struct MEM_MANAGER_STATS_T;
extern void mem_get_internal_stats(struct MEM_MANAGER_STATS_T *stats);

extern int mem_handle_acquire_if_valid(MEM_HANDLE_T handle);

void mem_compact_task_enable(int enable);

/******************************************************************************
API of memory access control using sandbox regions
    Users take the responsibility of assuring thread-safety
******************************************************************************/
#undef MEM_ACCESS_CTRL

#ifdef MEM_ACCESS_CTRL

#	if defined( NDEBUG ) || !defined( __BCM2708__ ) || defined(  __BCM2708A0__ )
#		error "BCM2708b0 or better needed"
#	endif

#define MEM_SSSR_PRIV_SECURE  (0x18)
#define MEM_SSSR_PRIV_SUPER   (0x08)
#define MEM_SSSR_PRIV_USER    (0x00)
#define MEM_SSSR_READ         (0x04)
#define MEM_SSSR_WRITE        (0x02)
#define MEM_SSSR_EXECUTE      (0x01)

#define MEM_ACCESSCTRL_BLOCKS_MAX		(7)  // Sandbox 0 is reserved for .text

typedef struct {
   uint32_t start;
   uint32_t size;
   uint32_t flags;
} MEM_ACCESSCTRL_BLOCK;

void mem_clr_accessctrl();
void mem_set_accessctrl(MEM_ACCESSCTRL_BLOCK *blocks, uint32_t n);
#define MEM_CLR_ACCESSCTRL		mem_clr_accessctrl
#define MEM_SET_ACCESSCTRL		mem_set_accessctrl

#else
#define MEM_CLR_ACCESSCTRL()				((void)0)
#define MEM_SET_ACCESSCTRL(BLOCKS, NUM)		((void)0)
#endif //#ifdef MEM_ACCESS_CTRL

#endif
