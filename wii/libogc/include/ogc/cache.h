/*-------------------------------------------------------------

cache.h -- Cache interface

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __CACHE_H__
#define __CACHE_H__

/*! \file cache.h
\brief Cache subsystem

*/

#include <gctypes.h>

#define LC_BASEPREFIX		0xe000
#define LC_BASE				(LC_BASEPREFIX<<16)

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*!
 * \fn void DCEnable()
 * \brief Enable L1 d-cache
 *
 * \return none
 */
void DCEnable();

/*!
 * \fn void DCDisable()
 * \brief Disable L1 d-cache
 *
 * \return none
 */
void DCDisable();

/*!
 * \fn void DCFreeze()
 * \brief Current contents of the L1 d-cache are locked down and will not be cast out.
 *
 *        Hits are still serviced, but misses go straight to L2 or 60x bus.  Most cache operations, such as DCFlushRange(), will still execute regardless of whether the cache is frozen.<br>
 *	      <b><i>NOTE:</i></b> In PowerPC architecture jargon, this feature is referred to as "locking" the data cache.  We use the word "freeze" to distinguish it from the locked cache and DMA features.
 *
 * \return none
 */
void DCFreeze();

/*!
 * \fn void DCUnfreeze()
 * \brief Undoes actions of DCFreeze().
 *
 *        Old cache blocks will now be cast out on subsequent L1 misses.<br>
 * 	      <b><i>NOTE:</i></b> In PowerPC architecture jargon, this feature is referred to as "locking" the data cache.  We use the word "freeze" to distinguish it from the locked cache and DMA features.
 *
 * \return none
 */
void DCUnfreeze();

/*!
 * \fn void DCFlashInvalidate()
 * \brief Invalidate L1 d-cache.
 *
 *        An invalidate operation is issued that marks the state of each data cache block as invalid without writing back modified cache blocks to memory.<br>
 *        Cache access is blocked during this time.Bus accesses to the cache are signaled as a miss during invalidate-all operations.
 *
 * \return none
 */
void DCFlashInvalidate();

/*!
 * \fn void DCInvalidateRange(void *startaddress,u32 len)
 * \brief Invalidates a given range of the d-cache.
 *
 *        If any part of the range hits in the d-cache, the corresponding block will be invalidated.
 *
 * \param[in] startaddress pointer to the startaddress of the memory range to invalidate. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery
 * \param[in] len length of the range to invalidate. <b><i>NOTE:</i></b> Should be a multiple of 32
 *
 * \return none
 */
void DCInvalidateRange(void *startaddress,u32 len);

/*!
 * \fn void DCFlushRange(void *startaddress,u32 len)
 * \brief Flushes a given range.
 *
 *        If any part of the range hits in the d-cache the corresponding block will be flushed to main memory and invalidated.<br>
 *        <b><i>NOTE:</i></b> This function invokes a "sync" after flushing the range. This means the function will stall until the CPU knows that the data has been writen to main memory
 *
 * \param[in] startaddress pointer to the startaddress of the memory range to flush. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery
 * \param[in] len length of range to be flushed. <b><i>NOTE:</i></b> Should be a multiple of 32
 *
 *\return none
 */
void DCFlushRange(void *startaddress,u32 len);

/*!
 * \fn void DCStoreRange(void *startaddress,u32 len)
 * \brief Ensures a range of memory is updated with any modified data in the cache.
 *
 *        <b><i>NOTE:</i></b> This function invokes a "sync" after storing the range. This means the function will stall until the CPU knows that the data has been writen to main memory
 *
 * \param[in] startaddress pointer to the startaddress of the memory range to store. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery
 * \param[in] len length of the range to store. <b><i>NOTE:</i></b> Should be a multiple of 32
 *
 * \return none
 */
void DCStoreRange(void *startaddress,u32 len);

/*!
 * \fn void DCFlushRangeNoSync(void *startaddress,u32 len)
 * \brief Flushes a given range.
 *
 *        If any part of the range hits in the d-cache the corresponding block will be flushed to main memory and invalidated.<br>
 *        <b><i>NOTE:</i></b> This routine does not perform a "sync" to ensure that the range has been flushed to memory.  That is, the cache blocks are sent to the bus interface unit for storage to main memory, but by the time this function returns, you are not guaranteed that the blocks have been written to memory.
 *
 * \param[in] startaddress pointer to the startaddress of the memory range to flush. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery
 * \param[in] len length of range to be flushed. <b><i>NOTE:</i></b> Should be a multiple of 32
 *
 * \return none
 */
void DCFlushRangeNoSync(void *startaddress,u32 len);

/*!
 * \fn void DCStoreRangeNoSync(void *startaddress,u32 len)
 * \brief Ensures a range of memory is updated with any modified data in the cache.
 *
 *        <b><i>NOTE:</i></b> This routine does not perform a "sync" to ensure that the range has been flushed to memory.  That is, the cache blocks are sent to the bus interface unit for storage to main memory, but by the time this function returns, you are not guaranteed that the blocks have been written to memory
 *
 * \param[in] startaddress pointer to the startaddress of the memory range to store. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery
 * \param[in] len length of the range to store. <b><i>NOTE:</i></b> Should be a multiple of 32
 *
 * \return none
 */
void DCStoreRangeNoSync(void *startaddress,u32 len);

/*!
 * \fn void DCZeroRange(void *startaddress,u32 len)
 * \brief Loads a range of memory into cache and zeroes all the cache lines.
 *
 * \param[in] startaddress pointer to the startaddress of the memory range to load/zero. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery
 * \param[in] len length of the range to load/zero. <b><i>NOTE:</i></b> Should be a multiple of 32
 *
 * \return none
 */
void DCZeroRange(void *startaddress,u32 len);

/*!
 * \fn void DCTouchRange(void *startaddress,u32 len)
 * \brief Loads a range of memory into cache.
 *
 * \param[in] startaddress pointer to the startaddress of the memory range to load. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery
 * \param[in] len length of the range to load. <b><i>NOTE:</i></b> Should be a multiple of 32
 *
 * \return none
 */
void DCTouchRange(void *startaddress,u32 len);

/*!
 * \fn void ICSync()
 * \brief Performs an instruction cache synchronization.
 *
 *        This ensures that all instructions preceding this instruction have completed before this instruction completes.
 *
 * \return none
 */
void ICSync();

/*!
 * \fn void ICFlashInvalidate()
 * \brief Invalidate the L1 i-cache.
 *
 *        An invalidate operation is issued that marks the state of each instruction cache block as invalid without writing back modified cache blocks to memory.<br>
 *        Cache access is blocked during this time. Bus accesses to the cache are signaled as a miss during invalidate-all operations.
 *
 * \return none
 */
void ICFlashInvalidate();

/*!
 * \fn void ICEnable()
 * \brief Enable L1 i-cache
 *
 * \return none
 */
void ICEnable();

/*!
 * \fn void ICDisable()
 * \brief Disable L1 i-cache
 *
 * \return none
 */
void ICDisable();

/*!
 * \fn void ICFreeze()
 * \brief Current contents of the L1 i-cache are locked down and will not be cast out.
 *
 *        Hits are still serviced, but misses go straight to L2 or 60x bus.<br>
 *	      <b><i>NOTE:</i></b> In PowerPC architecture jargon, this feature is referred to as "locking" the data cache.  We use the word "freeze" to distinguish it from the locked cache and DMA features.
 *
 * \return none
 */
void ICFreeze();

/*!
 * \fn void ICUnfreeze()
 * \brief Undoes actions of ICFreeze().
 *
 *        Old cache blocks will now be cast out on subsequent L1 misses.<br>
 *	      <b><i>NOTE:</i></b> In PowerPC architecture jargon, this feature is referred to as "locking" the data cache.  We use the word "freeze" to distinguish it from the locked cache and DMA features.
 *
 * \return none
 */
void ICUnfreeze();

/*!
 * \fn void ICBlockInvalidate(void *startaddress)
 * \brief Invalidates a block in the i-cache.
 *
 *        If the block hits in the i-cache, the corresponding block will be invalidated.
 *
 * \param[in] startaddress pointer to the startaddress of the memory block to invalidate. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery
 *
 *\return none
 */
void ICBlockInvalidate(void *startaddress);

/*!
 * \fn void ICInvalidateRange(void *startaddress,u32 len)
 * \brief Invalidate a range in the L1 i-cache.
 *
 *        If any part of the range hits in the i-cache, the corresponding block will be invalidated.
 *
 * \param[in] startaddress pointer to the startaddress of the memory range to invalidate. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery
 * \param[in] len length of the range to invalidate. <b><i>NOTE:</i></b> Should be a multiple of 32
 *
 * \return none
 */
void ICInvalidateRange(void *startaddress,u32 len);

/*!
 * \fn void L2Enhance()
 * \brief Turn on extra L2 cache features
 *
 *        Sets the following bits in the HID4 register which affect the L2 cache:
 *          - L2FM=01 (64-byte fetch mode)
 *          - BCO=1 (dual 64-byte castout buffers)
 *          - L2MUM=1 (configured as 2-deep miss-under-miss cache)
 *        Since these features can't be enabled safely, any HID4 writes in the HBC stub will be removed.
 *
 * \return none
 */
#ifdef HW_RVL
void L2Enhance();
#endif

void LCEnable();
void LCDisable();
void LCLoadBlocks(void *,void *,u32);
void LCStoreBlocks(void *,void *,u32);
u32 LCLoadData(void *,void *,u32);
u32 LCStoreData(void *,void *,u32);
u32 LCQueueLength();
u32 LCQueueWait(u32);
void LCFlushQueue();
void LCAlloc(void *,u32);
void LCAllocNoInvalidate(void *,u32);
void LCAllocOneTag(BOOL,void *);
void LCAllocTags(BOOL,void *,u32);

#define LCGetBase()		((void*)LC_BASE)
#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
