/*-------------------------------------------------------------

exi.h -- EXI subsystem

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


#ifndef __EXI_H__
#define __EXI_H__

/*! 
\file exi.h 
\brief EXI subsystem

*/


#include "gctypes.h"

/*!
 * \addtogroup exi_tx_mode EXI tranfer types
 * @{
 */

#define EXI_READ					0			/*!< EXI transfer type read */
#define EXI_WRITE					1			/*!< EXI transfer type write */
#define EXI_READWRITE				2			/*!< EXI transfer type read-write */

/*!
 * @}
 */


/*!
 * \addtogroup exi_channels EXI channels
 * @{
 */

#define EXI_CHANNEL_0				0			/*!< EXI channel 0 (memory card slot A) */
#define EXI_CHANNEL_1				1			/*!< EXI channel 1 (memory card slot B) */
#define EXI_CHANNEL_2				2			/*!< EXI channel 2 (other EXI devices connected, e.g. BBA) */
#define EXI_CHANNEL_MAX				3			/*!< _Termination */

/*!
 * @}
 */


/*!
 * \addtogroup exi_devices EXI devices
 * @{
 */

#define EXI_DEVICE_0				0			/*!< EXI device 0 */
#define EXI_DEVICE_1				1			/*!< EXI device 1 */
#define EXI_DEVICE_2				2			/*!< EXI device 2 */
#define EXI_DEVICE_MAX				3			/*!< _Termination */

/*!
 * @}
 */


/*!
 * \addtogroup exi_speed EXI device frequencies
 * @{
 */

#define EXI_SPEED1MHZ				0			/*!< EXI device frequency 1MHz */
#define EXI_SPEED2MHZ				1			/*!< EXI device frequency 2MHz */
#define EXI_SPEED4MHZ				2			/*!< EXI device frequency 4MHz */
#define EXI_SPEED8MHZ				3			/*!< EXI device frequency 8MHz */
#define EXI_SPEED16MHZ				4			/*!< EXI device frequency 16MHz */
#define EXI_SPEED32MHZ				5			/*!< EXI device frequency 32MHz */

/*!
 * @}
 */


/*!
 * \addtogroup exi_flags EXI device operation flags
 * @{
 */

#define EXI_FLAG_DMA				0x0001		/*!< EXI DMA mode transfer in progress */
#define EXI_FLAG_IMM				0x0002		/*!< EXI immediate mode transfer in progress */
#define EXI_FLAG_SELECT				0x0004		/*!< EXI channel and device selected */
#define EXI_FLAG_ATTACH				0x0008		/*!< EXI device on selected channel and device attached */
#define EXI_FLAG_LOCKED				0x0010		/*!< EXI channel and device locked for device operations */

/*!
 * @}
 */


/*!
 * \addtogroup exi_mcident EXI memory card identifier
 * @{
 */

#define EXI_MEMCARD59				0x00000004	/*!< Nintendo memory card:   64/  4/ 0.5 (blocks/Mbits/MB). 3rd party vendors do have the same identification */
#define EXI_MEMCARD123				0x00000008	/*!< Nintendo memory card:  128/  8/ 1.0 (blocks/Mbits/MB). 3rd party vendors do have the same identification */
#define EXI_MEMCARD251				0x00000010	/*!< Nintendo memory card:  256/ 16/ 2.0 (blocks/Mbits/MB). 3rd party vendors do have the same identification */
#define EXI_MEMCARD507				0x00000020	/*!< Nintendo memory card:  512/ 32/ 4.0 (blocks/Mbits/MB). 3rd party vendors do have the same identification */
#define EXI_MEMCARD1019				0x00000040	/*!< Nintendo memory card: 1024/ 64/ 8.0 (blocks/Mbits/MB). 3rd party vendors do have the same identification */
#define EXI_MEMCARD2043				0x00000080	/*!< Nintendo memory card: 2048/128/16.0 (blocks/Mbits/MB). 3rd party vendors do have the same identification */

/*!
 * @}
 */


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*! \typedef s32 (*EXICallback)(s32 chn,s32 dev)
\brief function pointer typedef for the user's EXI callback
\param chn EXI channel
\param dev EXI device
*/
typedef s32 (*EXICallback)(s32 chn,s32 dev);


/*! \fn s32 EXI_ProbeEx(s32 nChn)
\brief Performs an extended probe of the EXI channel
\param[in] nChn EXI channel to probe

\return 1 on success, <=0 on error
*/
s32 EXI_ProbeEx(s32 nChn);


/*! \fn s32 EXI_Probe(s32 nChn)
\brief Probes the EXI channel
\param[in] nChn EXI channel to probe

\return 1 on success, <=0 on error
*/
s32 EXI_Probe(s32 nChn);


/*! \fn s32 EXI_Lock(s32 nChn,s32 nDev,EXICallback unlockCB)
\brief Try to lock the desired EXI channel on the given device.
\param[in] nChn EXI channel to lock
\param[in] nDev EXI device to lock
\param[in] unlockCB pointer to callback to call when EXI_Unlock() is called. Thus allowing us a small way of mutual exclusion.

\return 1 on success, <=0 on error
*/
s32 EXI_Lock(s32 nChn,s32 nDev,EXICallback unlockCB);


/*! \fn s32 EXI_Unlock(s32 nChn)
\brief Unlock the desired EXI channel.
\param[in] nChn EXI channel to unlock

\return 1 on success, <=0 on error
*/
s32 EXI_Unlock(s32 nChn);


/*! \fn s32 EXI_Select(s32 nChn,s32 nDev,s32 nFrq)
\brief Selects the spedified EXI channel on the given device with the given frequency
\param[in] nChn EXI channel to select
\param[in] nDev EXI device to select
\param[in] nFrq EXI frequency to select

\return 1 on success, <=0 on error
*/
s32 EXI_Select(s32 nChn,s32 nDev,s32 nFrq);


/*! \fn s32 EXI_SelectSD(s32 nChn,s32 nDev,s32 nFrq)
\brief Performs a special select, for SD cards or adapters respectively, on the given device with the given frequence
\param[in] nChn EXI channel to select
\param[in] nDev EXI device to select
\param[in] nFrq EXI frequency to select

\return 1 on success, <=0 on error
*/
s32 EXI_SelectSD(s32 nChn,s32 nDev,s32 nFrq);


/*! \fn s32 EXI_Deselect(s32 nChn)
\brief Deselects the EXI channel.
\param[in] nChn EXI channel to deselect

\return 1 on success, <=0 on error
*/
s32 EXI_Deselect(s32 nChn);


/*! \fn s32 EXI_Sync(s32 nChn)
\brief Synchronize or finish respectively the last EXI transfer.
\param[in] nChn EXI channel to select

\return 1 on success, <=0 on error
*/
s32 EXI_Sync(s32 nChn);


/*! \fn s32 EXI_Imm(s32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb)
\brief Initializes an immediate mode EXI transfer.
\param[in] nChn EXI channel to select
\param[in,out] pData pointer to a buffer to read/copy from/to data.
\param[in] nLen lenght of data to transfer <=4.
\param[in] nMode direction of transferoperation(EXI_READ,EXI_WRITE,EXI_READWRITE)
\param[in] tc_cb pointer to a callback to call when transfer has completed. May be NULL.

\return 1 on success, <=0 on error
*/
s32 EXI_Imm(s32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb);


/*! \fn s32 EXI_ImmEx(s32 nChn,void *pData,u32 nLen,u32 nMode)
\brief Initializes an extended immediate mode EXI transfer.
\param[in] nChn EXI channel to select
\param[in,out] pData pointer to a buffer to read/copy from/to data.
\param[in] nLen lenght of data to transfer.
\param[in] nMode direction of transferoperation(EXI_READ,EXI_WRITE,EXI_READWRITE)

\return 1 on success, <=0 on error
*/
s32 EXI_ImmEx(s32 nChn,void *pData,u32 nLen,u32 nMode);


/*! \fn s32 EXI_Dma(s32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb)
\brief Initializes a DMA mode EXI transfer.
\param[in] nChn EXI channel to select
\param[in,out] pData pointer to a buffer to read/copy from/to data.
\param[in] nLen lenght of data to transfer.
\param[in] nMode direction of transferoperation(EXI_READ,EXI_WRITE,EXI_READWRITE)
\param[in] tc_cb pointer to a callback to call when transfer has completed. May be NULL.

\return 1 on success, <=0 on error
*/
s32 EXI_Dma(s32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb);


/*! \fn s32 EXI_GetState(s32 nChn)
\brief Get the EXI state
\param[in] nChn EXI channel to select

\return EXI channels state flag.
*/
s32 EXI_GetState(s32 nChn);


/*! \fn s32 EXI_GetID(s32 nChn,s32 nDev,u32 *nId)
\brief Get the ID of the connected EXI device on the given channel
\param[in] nChn EXI channel to select
\param[in] nDev EXI device to select
\param[out] nId EXI device ID to return.

\return 1 on success, <=0 on error
*/
s32 EXI_GetID(s32 nChn,s32 nDev,u32 *nId);


/*! \fn s32 EXI_Attach(s32 nChn,EXICallback ext_cb)
\brief Attach the device on the given channel
\param[in] nChn EXI channel to select
\param[in] ext_cb pointer to callback to call when device is physically removed.

\return 1 on success, <=0 on error
*/
s32 EXI_Attach(s32 nChn,EXICallback ext_cb);


/*! \fn s32 EXI_Detach(s32 nChn)
\brief Detach the device on the given channel
\param[in] nChn EXI channel to select

\return 1 on success, <=0 on error
*/
s32 EXI_Detach(s32 nChn);


/*! \fn void EXI_ProbeReset()
\brief Resets certain internal flags and counters and performs a probe on all 3 channels.

\return nothing
*/
void EXI_ProbeReset();


/*! \fn EXICallback EXI_RegisterEXICallback(s32 nChn,EXICallback exi_cb)
\brief Register a callback function in the EXI driver for the EXI interrupt.
\param[in] nChn EXI channel to select
\param[in] exi_cb pointer to the function which to call when EXI interrupt has triggered.

\return old callback function pointer or NULL
*/
EXICallback EXI_RegisterEXICallback(s32 nChn,EXICallback exi_cb);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
