/*-------------------------------------------------------------

audio.h -- Audio subsystem

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

#ifndef __AUDIO_H__
#define __AUDIO_H__

/*! \file audio.h
\brief AUDIO subsystem

*/

#include <gctypes.h>

/*!
 * \addtogroup ai_stream_mode AI streaming modes
 * @{
 */

#define AI_STREAM_STOP			0x00000000		/*!< Stop streaming audio playback */
#define AI_STREAM_START			0x00000001		/*!< Start streaming audio playback */

/*!
 * @}
 */

/*!
 * \addtogroup ai_sample_rates AI sampling rates
 * @{
 */

#define AI_SAMPLERATE_32KHZ		0x00000000		/*!< AI sampling rate at 32kHz */
#define AI_SAMPLERATE_48KHZ		0x00000001		/*!< AI sampling rate at 48kHz */

/*!
 * @}
 */

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*!
 * \typedef void (*AIDCallback)(void)
 * \brief function pointer typedef for the user's Audio DMA interrupt callback
 *
 * \param none
 */
typedef void (*AIDCallback)(void);

/*!
 * \typedef void (*AISCallback)(u32 smp_cnt)
 * \brief function pointer typedef for the user's Audio Streaming interrupt callback
 *
 * \param smp_cnt AI sample count
 */
typedef void (*AISCallback)(u32 smp_cnt);

/*!
 * \fn AISCallback AUDIO_RegisterStreamCallback(AISCallback callback)
 * \brief Register a user callback function for the AUDIO streaming interface
 *
 * \param[in] callback pointer to the function which to call when AIS interrupt has triggered.
 *
 * \return pointer to old callback function or NULL respectively.
 */
AISCallback AUDIO_RegisterStreamCallback(AISCallback callback);

/*!
 * \fn void AUDIO_Init(u8 *stack)
 * \brief Initialize the AUDIO subsystem
 *
 * \param[in] stack pointer to a memory area to work as stack when calling the callbacks. May be NULL
 *
 * \return none
 */
void AUDIO_Init(u8 *stack);

/*!
 * \fn void AUDIO_SetStreamVolLeft(u8 vol)
 * \brief Set streaming volume on the left channel.
 *
 * \param[in] vol level of volume 0<= vol <=255
 *
 * \return none
 */
void AUDIO_SetStreamVolLeft(u8 vol);

/*!
 * \fn u8 AUDIO_GetStreamVolLeft()
 * \brief Get streaming volume of the left channel.
 *
 * \return level of volume.
 */
u8 AUDIO_GetStreamVolLeft();

/*!
 * \fn void AUDIO_SetStreamVolRight(u8 vol)
 * \brief set streaming volume of the right channel.
 *
 * \param[in] vol level of volume 0<= vol <=255
 *
 * \return none
 */
void AUDIO_SetStreamVolRight(u8 vol);

/*!
 * \fn u8 AUDIO_GetStreamVolRight()
 * \brief Get streaming volume of the right channel.
 *
 * \return level of volume.
 */
u8 AUDIO_GetStreamVolRight();

/*!
 * \fn void AUDIO_SetStreamSampleRate(u32 rate)
 * \brief Set streaming sample rate
 *
 * \param[in] rate streaming \ref ai_sample_rates "sample rate"
 *
 * \return none
 */
void AUDIO_SetStreamSampleRate(u32 rate);

/*!
 * \fn u32 AUDIO_GetStreamSampleRate()
 * \brief Get streaming sample rate
 *
 * \return \ref ai_sample_rates "sample rate"
 */
u32 AUDIO_GetStreamSampleRate();

/*!
 * \fn AIDCallback AUDIO_RegisterDMACallback(AIDCallback callback)
 * \brief Register a user callback function for the audio DMA interface.
 *
 *        This callback will be called whenever the audio DMA requests new data.<br>
 *        Internally the DMA buffers are double buffered.
 *
 * \param[in] callback pointer to the function which to call when AID interrupt has triggered.
 *
 * \return pointer to old callback function or NULL respectively.
 */
AIDCallback AUDIO_RegisterDMACallback(AIDCallback callback);

/*!
 * \fn void AUDIO_InitDMA(u32 startaddr,u32 len)
 * \brief Initialize an audio DMA transfer
 *
 * \param[in] startaddr start address of the memory region to load into the audio DMA. <b><i>NOTE:</i></b> Has to be aligned on a 32byte boundery!
 * \param[in] len lenght of data to load into the audio DMA. <b><i>NOTE:</i></b> Should be a multiple of 32
 *
 * \return none
 */
void AUDIO_InitDMA(u32 startaddr,u32 len);

/*!
 * \fn u16 AUDIO_GetDMAEnableFlag()
 * \brief Get the audio DMA flag
 *
 * \return state of the current DMA operation.
 */
u16 AUDIO_GetDMAEnableFlag();

/*!
 * \fn void AUDIO_StartDMA()
 * \brief Start the audio DMA operation.
 *
 *        Starts to transfer the data from main memory to the audio interface thru DMA.<br>
 *        This call should follow the call to AUDIO_InitDMA() which is used to initialize DMA transfers.
 *
 * \return none
 */
void AUDIO_StartDMA();

/*!
 * \fn void AUDIO_StopDMA()
 * \brief Stop the previously started audio DMA operation.
 *
 * \return none
 */
void AUDIO_StopDMA();

/*!
 * \fn u32 AUDIO_GetDMABytesLeft()
 * \brief Get the count of bytes, left to play, from the audio DMA interface
 *
 * \return count of bytes left to play.
 */
u32 AUDIO_GetDMABytesLeft();

/*!
 * \fn u32 AUDIO_GetDMALength()
 * \brief Get the DMA transfer length configured in the audio DMA interface.
 *
 * \return length of data loaded into the audio DMA interface.
 */
u32 AUDIO_GetDMALength();

/*!
 * \fn u32 AUDIO_GetDMAStartAddr()
 * \brief Get the main memory address for the DMA operation.
 *
 * \return start address of mainmemory loaded into the audio DMA interface.
 */
u32 AUDIO_GetDMAStartAddr();

/*!
 * \fn void AUDIO_SetStreamTrigger(u32 cnt)
 * \brief Set the sample count for the stream trigger
 *
 * \param[in] cnt count of samples when to trigger the audio stream.
 *
 * \return none
 */
void AUDIO_SetStreamTrigger(u32 cnt);

/*!
 * \fn void AUDIO_ResetStreamSampleCnt()
 * \brief Reset the stream sample count register.
 *
 * \return none
 */
void AUDIO_ResetStreamSampleCnt();

/*!
 * \fn void AUDIO_SetDSPSampleRate(u8 rate)
 * \brief Set the sampling rate for the DSP interface
 *
 * \param[in] rate sampling rate to set for the DSP (AI_SAMPLERATE_32KHZ,AI_SAMPLERATE_48KHZ)
 *
 * \return none
 */
void AUDIO_SetDSPSampleRate(u8 rate);

/*!
 * \fn u32 AUDIO_GetDSPSampleRate()
 * \brief Get the sampling rate for the DSP interface
 *
 * \return DSP sampling rate (AI_SAMPLERATE_32KHZ,AI_SAMPLERATE_48KHZ)
 */
u32 AUDIO_GetDSPSampleRate();

/*!
 * \fn void AUDIO_SetStreamPlayState(u32 state)
 * \brief Set the play state for the streaming audio interface
 *
 * \param[in] state playstate of the streaming audio interface (AI_STREAM_STOP,AI_STREAM_START)
 *
 * \return none
 */
void AUDIO_SetStreamPlayState(u32 state);

/*!
 * \fn u32 AUDIO_GetStreamPlayState()
 * \brief Get the play state from the streaming audio interface
 *
 * \return playstate (AI_STREAM_STOP,AI_STREAM_START)
 */
u32 AUDIO_GetStreamPlayState();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
