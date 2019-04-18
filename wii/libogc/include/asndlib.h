/* ASNDLIB -> accelerated sound lib using the DSP

Copyright (c) 2008 Hermes <www.entuwii.net>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of
  conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list
  of conditions and the following disclaimer in the documentation and/or other
  materials provided with the distribution.
- The names of the contributors may not be used to endorse or promote products derived
  from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef __ASNDLIB_H__
#define __ASNDLIB_H__

#define __SNDLIB_H__

/*!
 * \file asndlib.h
 * \brief ASND library
 *
 */

#define ASND_LIB 0x100
#define SND_LIB  (ASND_LIB+2)

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup sndretvals SND return values
 * @{
 */
#define SND_OK               0
#define SND_INVALID         -1
#define SND_ISNOTASONGVOICE -2
#define SND_BUSY             1
/*! @} */

/*! \addtogroup sndiavretvals SND_IsActiveVoice additional return values
 * @{
 */
#define SND_UNUSED   0   /*!< This voice is available for use. */
#define SND_WORKING  1   /*!< This voice is currently in progress. */
#define SND_WAITING  2   /*!< This voice is currently in progress and waiting to one SND_AddVoice() function (the voice handler is called continuously) */
/*! @} */

/*! \addtogroup sndsetvoiceformats Voice format
 * @{
 */
#define VOICE_MONO_8BIT       0
#define VOICE_MONO_16BIT      1
#define VOICE_MONO_16BIT_BE   1
#define VOICE_STEREO_8BIT     2
#define VOICE_STEREO_16BIT    3
#define VOICE_STEREO_16BIT_BE 3
#define VOICE_MONO_8BIT_U     4
#define VOICE_MONO_16BIT_LE   5
#define VOICE_STEREO_8BIT_U   6
#define VOICE_STEREO_16BIT_LE 7
/*! @} */

/*! \addtogroup voicevol Voice volume
 * @{
 */
#define MIN_VOLUME 0
#define MID_VOLUME 127
#define MAX_VOLUME 255
/*! @} */

/*! \addtogroup pitchrange Pitch Range
 * @{
 */
#define MIN_PITCH      1      /*!< 1Hz */
#define F44100HZ_PITCH 44100  /*!< 44100Hz */
#define MAX_PITCH      144000 /*!< 144000Hz (more process time for pitch>48000) */
#define INIT_RATE_48000
/*! @} */

/*! \addtogroup notecode Note codification
 * @{
 */
enum
{
NOTE_DO=0,
NOTE_DOs,
NOTE_REb=NOTE_DOs,
NOTE_RE,
NOTE_REs,
NOTE_MIb=NOTE_REs,
NOTE_MI,
NOTE_FA,
NOTE_FAs,
NOTE_SOLb=NOTE_FAs,
NOTE_SOL,
NOTE_SOLs,
NOTE_LAb=NOTE_SOLs,
NOTE_LA,
NOTE_LAs,
NOTE_SIb=NOTE_LAs,
NOTE_SI
};

enum
{
NOTE_C=0,
NOTE_Cs,
NOTE_Db=NOTE_Cs,
NOTE_D,
NOTE_Ds,
NOTE_Eb=NOTE_Ds,
NOTE_E,
NOTE_F,
NOTE_Fs,
NOTE_Gb=NOTE_Fs,
NOTE_G,
NOTE_Gs,
NOTE_Ab=NOTE_Gs,
NOTE_A,
NOTE_As,
NOTE_Bb=NOTE_As,
NOTE_B
};

#define NOTE(note,octave) (note+(octave<<3)+(octave<<2)) /*!< Final note codification. Used in Note2Freq(). */
/*! @} */

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/*! \addtogroup sndlibcompat SNDLIB compatibility macros
 * \details These defines are for compatability with SNDLIB functions.
 * @{
 */
#define Note2Freq               ANote2Freq
#define SND_Init                ASND_Init
#define SND_End                 ASND_End
#define SND_Pause               ASND_Pause
#define SND_Is_Paused           ASND_Is_Paused
#define SND_GetTime             ASND_GetTime
#define SND_GetSampleCounter    ASND_GetSampleCounter
#define SND_GetSamplesPerTick   ASND_GetSamplesPerTick
#define SND_SetTime             ASND_SetTime
#define SND_SetCallback         ASND_SetCallback
#define SND_GetAudioRate        ASND_GetAudioRate
#define SND_SetVoice            ASND_SetVoice
#define SND_AddVoice            ASND_AddVoice
#define SND_StopVoice           ASND_StopVoice
#define SND_PauseVoice          ASND_PauseVoice
#define SND_StatusVoice         ASND_StatusVoice
#define SND_GetFirstUnusedVoice ASND_GetFirstUnusedVoice
#define SND_ChangePitchVoice    ASND_ChangePitchVoice
#define SND_ChangeVolumeVoice   ASND_ChangeVolumeVoice
#define SND_ChangeVolumeVoice   ASND_ChangeVolumeVoice
#define SND_GetTickCounterVoice ASND_GetTickCounterVoice
#define SND_GetTimerVoice       ASND_GetTimerVoice
#define SND_TestPointer         ASND_TestPointer
/*! @} */

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/** \brief Callback type for ASND_SetVoice(). */
typedef void (*ASNDVoiceCallback)(s32 voice);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/** \brief Initializes the SND lib and fixes the hardware sample rate.
 * \param[in] note \ref notecode to play. for example: NOTE(C,4) for note C and octave 4.
 * \param[in] freq_base Frequency base of the sample. For example 8000Hz.
 * \param[in] note_base \ref notecode of the sample. For example: NOTE(L, 3) for note L and octave 3 (LA 3).
 * \return Frequency, in Hz. */
int ANote2Freq(int note, int freq_base,int note_base);

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

/*! \addtogroup General sound functions
 * @{
 */

/*! \brief Initializes the ASND lib and fixes the hardware sample rate to 48000.
 * \return None. */
void ASND_Init();

/*! \brief De-initializes the ASND lib.
 * \return None. */
void ASND_End();

/*! \brief Used to pause (or unpause) the sound.
 * \note The sound starts paused when ASND_Init() is called.
 * \param[in] paused If 1, sound is paused; sound can be unpaused with 0.
 * \return None. */
void ASND_Pause(s32 paused);

/*! \brief Returns sound paused status.
 * \return 1 if paused, 0 if unpaused. */
s32 ASND_Is_Paused();

/*! \brief Returns the global time.
 * \details The time is updated from the IRQ.
 * \return The current time, in milliseconds. */
u32 ASND_GetTime();

/*! \brief Retrieves the global sample counter.
 * \details This counter is updated from the IRQ in steps of ASND_GetSamplesPerTick().
 * \note You can use this to implement one timer with high precision.
 * \return Current sample. */
u32 ASND_GetSampleCounter();

/*! \brief Retrieves the samples sent from the IRQ in one tick.
 * \return Samples per tick. */
u32 ASND_GetSamplesPerTick();

/*! \brief Set the global time.
 * \details This time is updated from the IRQ.
 * \param[in] time Fix the current time, in milliseconds.
 * \return None. */
void ASND_SetTime(u32 time);

/*! \brief Sets a global callback for general purposes.
 * \details This callback is called from the IRQ.
 * \param[in] callback Callback function to assign.
 * \return None. */
void ASND_SetCallback(void (*callback)());

/*! \brief Returns the current audio rate.
 * \note This function is implemented for compatibility with SNDLIB.
 * \return Audio rate (48000). */
s32 ASND_GetAudioRate();

/*! @} */

/*! \addtogroup voicefuncs Voice functions
 * @{
 */

// NOTE: I'm keeping this description around because I couldn't fully understand it; if someone else knows exactly what it's doing, they can come around
// and make sure the description is correct.
/* callback: can be NULL or one callback function is used to implement a double buffer use. When the second buffer is empty, the callback is called sending
          the voice number as parameter. You can use "void callback(s32 voice)" function to call ASND_AddVoice() and add one voice to the second buffer.
		  NOTE: When callback is fixed the voice never stops and it turn in SND_WAITING status if success one timeout condition.
*/

/*! \brief Sets a PCM voice to play.
 * \details This function stops one previous voice. Use ASND_StatusVoice() to test the status condition.
 * \note The voices are played in 16-bit stereo, regardless of the source format.<br><br>
 *
 * \note \a callback is used to implement a double buffer. When the second buffer is empty, the callback function is called with the voice number
 * as an argument. You can use <tt>void <i>callback</i> (s32 voice)</tt> to call ASND_AddVoice() and add one voice to the second buffer. When the callback
 * is non-NULL the, the voice never stops and returns SND_WAITING if successful on timeout condition.
 * \param[in] voice Voice slot to use for this sound; valid values are 0 to (MAX_SND_VOICES-1).
 * \param[in] format \ref sndsetvoiceformats to use for this sound.
 * \param[in] pitch Frequency to use, in Hz.
 * \param[in] delay Delay to wait before playing this voice; value is in milliseconds.
 * \param[in] snd Buffer containing samples to play back; the buffer <b>must</b> be aligned and padded to 32 bytes!
 * \param[in] size_snd Size of the buffer samples, in bytes.
 * \param[in] volume_l \ref voicevol of the left channel; value can be 0 - 255 inclusive.
 * \param[in] volume_r \ref voicevol of the right channel; value can be 0 - 255 inclusive.
 * \param[in] callback Optional callback function to use; set to NULL for no callback. See the note above for details.
 * \return SND_OK or SND_INVALID. */
s32 ASND_SetVoice(s32 voice, s32 format, s32 pitch,s32 delay, void *snd, s32 size_snd, s32 volume_l, s32 volume_r, ASNDVoiceCallback callback);

/*! \brief Sets a PCM voice to play infinitely.
 * \note See ASND_SetVoice() for a detailed description, as it is largely identical. */
s32 ASND_SetInfiniteVoice(s32 voice, s32 format, s32 pitch,s32 delay, void *snd, s32 size_snd, s32 volume_l, s32 volume_r);

/*! \brief Adds a PCM voice to play from the second buffer.
 * \note This function requires a call to ASND_SetVoice() and its subsequent return value to be a status other than SND_UNUSED prior to calling this one.
 * \param[in] voice Voice slot to attach this buffer to; value must be 0 to (MAX_SND_VOICES-1).
 * \param[in] snd Buffer containing the samples; it <b>must</b> be aligned and padded to 32 bytes AND have the same sample format as the first buffer.
 * \param[in] size_snd Size of the buffer samples, in bytes.
 * \return SND_OK or SND_INVALID; it may also return SND_BUSY if the second buffer is not empty and the voice cannot be added. */
s32 ASND_AddVoice(s32 voice, void *snd, s32 size_snd);

/*! \brief Stops the selected voice.
 * \details If the voice is used in song mode, you need to assign the samples with ASND_SetSongSampleVoice() again. In this case, use ANS_PauseSongVoice()
 * to stop the voice without loss of samples.
 * \param[in] voice Voice to stop, from 0 to (MAX_SND_VOICES-1).
 * \return SND_OK or SND_INVALID. */
s32 ASND_StopVoice(s32 voice);

/*! \brief Pauses the selected voice.
 * \param[in] voice Voice to pause, from 0 to (MAX_SND_VOICES-1).
 * \return SND_OK or SND_INVALID. */
s32 ASND_PauseVoice(s32 voice, s32 pause);

/*! \brief Returns the status of the selected voice.
 * \param[in] voice Voice slot to get the status from, from 0 to (MAX_SND_VOICES-1).
 * \return SND_INVALID if invalid, else a value from \ref sndiavretvals. */
s32 ASND_StatusVoice(s32 voice);

/*! \brief Returns the first unused voice.
 * \note Voice 0 is the last possible voice that can be returned. The idea is that this voice is reserved for a MOD/OGG/MP3/etc. player. With this in mind,
 * you can use this function to determine that the rest of the voices are working if the return value is < 1.
 * \return SND_INVALID or the first free voice from 0 to (MAX_SND_VOICES-1). */
s32 ASND_GetFirstUnusedVoice();

/*! \brief Changes the voice pitch in real-time.
 * \details This function can be used to create audio effects such as Doppler effect emulation.
 * \param[in] voice Voice to change the pitch of, from 0 to (MAX_SND_VOICES-1).
 * \return SND_OK or SND_INVALID. */
s32 ASND_ChangePitchVoice(s32 voice, s32 pitch);

/*! \brief Changes the voice volume in real-time.
 * \details This function can be used to create audio effects like distance attenuation.
 * \param[in] voice Voice to change the volume of, from 0 to (MAX_SND_VOICES-1).
 * \param[in] volume_l \ref voicevol to set the left channel to, from 0 to 255.
 * \param[in] volume_r \ref voicevol to set the right channel to, from 0 to 255.
 * \return SND_OK or SND_INVALID. */
s32 ASND_ChangeVolumeVoice(s32 voice, s32 volume_l, s32 volume_r);

/*! \brief Returns the voice tick counter.
 * \details This value represents the number of ticks since this voice started to play, sans delay time. It uses the same resolution as the internal
 * sound buffer. For example, if the lib is initialized with INIT_RATE_48000, a return value of 24000 is equal to 0.5 seconds. This value can be used, for
 * example, to synchronize audio and video.
 * \note This function does not return error codes.
 * \param[in] voice Voice to retrieve the counter value from, from 0 to (MAX_SND_VOICES-1).
 * \return Number of ticks since this voice started playing. */
u32 ASND_GetTickCounterVoice(s32 voice);

/*! \brief Returns the voice playback time.
 * \details This value represents the time, in milliseconds, since this voice started playing, sans delay time. This value can be used, for example, to
 * synchronize audio and video.
 * \note This function does not return error codes.
 * \param[in] voice Voice to retrieve the time value from, from 0 to (MAX_SND_VOICES-1).
 * \return Amount of time since this voice has started playing. */
u32 ASND_GetTimerVoice(s32 voice);

/*! \brief Tests if \a pointer is in use by \a voice as a buffer.
 * \param[in] voice Voice to test, from 0 to (MAX_SND_VOICES-1).
 * \param[in] pointer Address to test. This must be the same pointer sent to ASND_AddVoice() or ASND_SetVoice().
 * \return SND_INVALID if invalid
 * \return 0 if the pointer is unused
 * \return 1 if the pointer used as a buffer. */
s32 ASND_TestPointer(s32 voice, void *pointer);

/*! \brief Tests to determine if the \a voice is ready to receive a new buffer sample with ASND_AddVoice().
 * \details You can use this function to block a reader when double buffering is used. It works similarly to ASND_TestPointer() without needing to pass a
 * pointer.
 * \param[in] voice Voice to test, from 0 to (MAX_SND_VOICES-1).
 * \return SND_INVALID
 * \return 0 if voice is NOT ready to receive a new voice.
 * \return 1 if voice is ready to receive a new voice with ASND_AddVoice(). */
s32 ASND_TestVoiceBufferReady(s32 voice);

/*! @} */

/*! \addtogroup dspfuncs DSP functions
 * @{
 */

/*! \brief Returns the DSP usage.
 * \details The value is a percentage of DSP usage.
 * \return DSP usage, in percent. */
u32 ASND_GetDSP_PercentUse();

u32 ASND_GetDSP_ProcessTime();

/*! @} */

#ifdef __cplusplus
  }
#endif

#endif
