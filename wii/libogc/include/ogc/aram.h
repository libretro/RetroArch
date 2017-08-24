/*-------------------------------------------------------------

aram.h -- ARAM subsystem

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


#ifndef __ARAM_H__
#define __ARAM_H__

/*! 
 * \file aram.h 
 * \brief ARAM subsystem
 *
 */ 

#include <gctypes.h>


/*! 
 * \addtogroup dmamode ARAM DMA transfer direction
 * @{
 */

#define AR_MRAMTOARAM		0		/*!< direction: MRAM -> ARAM (write) */
#define AR_ARAMTOMRAM		1		/*!< direction: ARAM -> MRAM (read) */

/*!
 * @}
 */



/*!
 * \addtogroup memmode ARAM memory access modes
 * @{
 */

#define AR_ARAMINTALL		0		/*!< use all the internal ARAM memory */
#define AR_ARAMINTUSER		1		/*!< use only the internal user space of the ARAM memory */

/*!
 * @}
 */


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*! 
 * \typedef void (*ARCallback)(void)
 * \brief function pointer typedef for the user's ARAM interrupt callback
 *
 * \param none
 */
typedef void (*ARCallback)(void);


/*! 
 * \fn ARCallback AR_RegisterCallback(ARCallback callback)
 * \brief Register the given function as a DMA callback
 *
 *        Any existing callback is replaced unconditionally
 *
 * \param[in] callback to be invoked upon completion of DMA transaction
 *
 * \return pointer to the previously registered callback and NULL respectively
 */
ARCallback AR_RegisterCallback(ARCallback callback);


/*! 
 * \fn u32 AR_GetDMAStatus()
 * \brief Get current status of DMA
 *
 * \return zero if DMA is idle, non-zero if a DMA is in progress
 */
u32 AR_GetDMAStatus();


/*! 
 * \fn u32 AR_Init(u32 *stack_idx_array,u32 num_entries)
 * \brief Initializes ARAM subsystem.
 *
 *        Following tasks are performed:
 *
 *      - Disables ARAM DMA
 *      - Sets DMA callback to NULL
 *      - Initializes ARAM controller
 *      - Determines size of ARAM memory
 *      - Initializes the ARAM stack based memory allocation system<br>
 *
 *        The parameter u32 *stack_idx_array points to an array of u32 integers. The parameter u32 num_entries specifies the number of entries in this array.<br>
 *        The user application is responsible for determining how many ARAM blocks the device driver can allocate.<br>
 *        
 *        As an example, consider:
 * \code
 *        #define MAX_NUM_BLOCKS 10
 *
 *        u32 aram_blocks[MAX_NUM_BLOCKS];
 *        ...
 *        void func(void)
 *        {
 *           AR_Init(aram_blocks, MAX_NUM_BLOCKS);
 *        }
 * \endcode
 *        
 *        Here, we are telling AR that the application will allocate, at most, 10 blocks (of arbitrary size), and that AR should store addresses for those blocks in the array aram_blocks. Note that the array is simply storage supplied by the application so that AR can track the number and size of memory blocks allocated by AR_Alloc().
 *        AR_Free()also uses this array to release blocks.<br>
 *        If you do not wish to use AR_Alloc() and AR_Free() and would rather manage ARAM usage within your application, then call AR_Init() like so:<br>
 *
 *               AR_Init(NULL, 0);<br>
 *
 *        The AR_Init() function also calculates the total size of the ARAM aggregate. Note that this procedure is <b><i>destructive</i></b> - i.e., any data stored in ARAM will be corrupted.<br>
 *        AR_Init()may be invoked multiple times. This function checks the state of an initialization flag; if asserted, this function will simply exit on subsequent calls. To perform another initialization of the ARAM driver, call AR_Reset() before invoking AR_Init() again.
 *
 * \param[in] stack_idx_array pointer to an array of u32 integer
 * \param[in] num_entries number of entries in the specified array
 *
 * \return base address of the "user" ARAM area. As of this writing, the operating system reserves the bottom 16 KB of ARAM. Therefore, AR_Init() returns 0x04000 to indicate the starting location of the ARAM user area.
 */
u32 AR_Init(u32 *stack_idx_array,u32 num_entries);


/*! 
 * \fn void AR_StartDMA(u32 dir,u32 memaddr,u32 aramaddr,u32 len)
 * \brief Initiates a DMA between main memory and ARAM.
 *
 *        This function:
 *
 *      - Does <b><i>not</i></b> perform boundery-checking on addresses and lengths.
 *      - Will assert failure if a DMA is allready in progress.
 *      - Is provided for debugging purpose. Application programmers must use the ARQ API in order to access ARAM.
 *
 * \param[in] dir specifies the \ref dmamode "direction" of transfer.
 * \param[in] memaddr specifies main memory address for the transfer
 * \param[in] aramaddr specifies the ARAM address for the transfer. <b><i>NOTE:</i></b> Addresses are 27bits wide and refer to bytes
 * \param[in] len specifies the length of the block to transfer. <b><i>NOTE:</i></b> Must be in bytes and a multiple of 32
 *
 * \return none
 */
void AR_StartDMA(u32 dir,u32 memaddr,u32 aramaddr,u32 len);


/*! 
 * \fn u32 AR_Alloc(u32 len)
 * \brief Allocate a block of memory from ARAM having <i>len</i> bytes.
 *
 *        The <i>len</i> parameter <b><i>must</i></b> be a multiple of 32
 *
 * \param[in] len length of the specified block of memory in ARAM
 *
 * \return address of the block if successful, otherwise NULL
 */
u32 AR_Alloc(u32 len);


/*! 
 * \fn u32 AR_Free(u32 *len)
 * \brief Free a block from ARAM
 *
 * \param[out] len pointer to receive the length of the free'd ARAM block. This is optional and can be NULL.
 *
 * \return ARAM current baseaddress after free'ing the block
 */
u32 AR_Free(u32 *len);


/*!
 * \fn void AR_Clear(u32 flag)
 * \brief Clear ARAM memory
 *
 * \param[in] flag specifies the region of ARAM to clear
 *
 * \return none
 */
void AR_Clear(u32 flag);


/*! 
 * \fn BOOL AR_CheckInit()
 * \brief Get the ARAM subsystem initialization flag
 *
 * \return TRUE if the ARAM subsystem has been initialized(via AR_Init())<br>
 *         FALSE if the ARAM subsystem has not been initialized, or has been reset(via AR_Reset())
 */
BOOL AR_CheckInit();


/*!
 * \fn void AR_Reset()
 * \brief Clears the ARAM subsystem initialization flag.
 *
 *        Calling AR_Init() after this function will cause a "real" initialization of ARAM
 *
 * \return none
 */
void AR_Reset();


/*! 
 * \fn u32 AR_GetSize()
 * \brief Get the total size - in bytes - of ARAM as calculated during AR_Init()
 *
 * \return size of the specified ARAM block
 */
u32 AR_GetSize();


/*! 
 * \fn u32 AR_GetBaseAddress()
 * \brief Get the baseaddress of ARAM memory
 *
 * \return ARAM memory baseaddress
 */
u32 AR_GetBaseAddress();


/*! 
 * \fn u32 AR_GetInternalSize()
 * \brief Get the size of the internal ARAM memory
 *
 * \return ARAM internal memory size
 */
u32 AR_GetInternalSize();


/*! 
 * \def AR_StartDMARead(maddr,araddr,tlen)
 * \brief Wraps the DMA read operation done by AR_StartDMA()
 */
#define AR_StartDMARead(maddr,araddr,tlen)	\
	AR_StartDMA(AR_ARAMTOMRAM,maddr,araddr,tlen);


/*!
 * \def AR_StartDMAWrite(maddr,araddr,tlen)
 * \brief Wraps the DMA write operation done by AR_StartDMA()
 */
#define AR_StartDMAWrite(maddr,araddr,tlen)	\
	AR_StartDMA(AR_MRAMTOARAM,maddr,araddr,tlen);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif //__ARAM_H__
