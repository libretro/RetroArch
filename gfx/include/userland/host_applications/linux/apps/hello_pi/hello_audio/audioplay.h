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

//  API for host applications to deliver raw PCM samples to rendered on VideoCore

#ifndef AUDIOPLAY_H
#define AUDIOPLAY_H

/**
 * \file 
 *
 * \brief This API allows the host to provide PCM samples to be
 * rendered via <DFN>audio_render</DFN>.
 * 
 * This file describes a simple API for host applications to play sound
 * using VideoCore.  It includes the functionality to:
 * 
 * \li open/close
 * \li set pcm parameters
 * \li set buffer size parameters
 * \li retrieve empty buffer available to use
 * \li send full buffer to be played
 * \li retrieve current buffering level
 * 
 * This API has no thread context of it's own, so the caller must be
 * aware that the IL API will be used in context.  This has
 * implications on executing calls inside callback contexts, and on
 * the minimum size of stack that the caller requires.  See the
 * <DFN>ilclient_stack_size()</DFN> function for assistance.
 * 
 * This API will use a single <DFN>audio_render</DFN> IL component, and
 * supply buffers to the input port using the OpenMAX IL base profile mode.
 ********************************************************************************/

struct AUDIOPLAY_STATE_T;

/**
 * The <DFN>AUDIOPLAY_STATE_T</DFN> is an opaque type that represents the
 * audioplus engine handle.
 *******************************************************************************/
typedef struct AUDIOPLAY_STATE_T AUDIOPLAY_STATE_T;

/**
 * The <DFN>audioplay_create()</DFN> function creates the audioplay object.
 * 
 * @param  handle       On success, this is filled in with a handle to use in other
 *                      API functions.
 * 
 * @param  sample_rate  The sample rate, in samples per second, for the PCM data.
 *                      This shall be between 8000 and 96000.
 * 
 * @param  num_channels The number of channels for the PCM data.  Must be 1, 2, 4, or 8.
 *                      Channels must be sent interleaved.
 * 
 * @param  bit_depth    The bitdepth per channel per sample.  Must be 16 or 32.
 * 
 * @param  num_buffers  The number of buffers that will be created to write PCM
 *                      samples into.
 * 
 * @param  buffer_size  The size in bytes of each buffer that will be used to write
 *                      PCM samples into.  Note that small buffers of less than a few
 *                      Kb in size may be faster than larger buffers, although this is
 *                      platform dependent.
 *                     
 * @return              0 on success, -1 on failure.
 *********************************************************************************/
VCHPRE_ int32_t VCHPOST_ audioplay_create(AUDIOPLAY_STATE_T **handle,
                                          uint32_t sample_rate,
                                          uint32_t num_channels,
                                          uint32_t bit_depth,
                                          uint32_t num_buffers,
                                          uint32_t buffer_size);

/**
 * The <DFN>audioplay_delete()</DFN> function deletes the audioplay object.
 * 
 * @param  handle       Must be a handle previously created by
 *                      <DFN>audioplay_create()</DFN>.  After calling this
 *                      function that handle is no longer valid.  Any
 *                      buffers provided by <DFN>audioplay_get_buffer()</DFN>
 *                      are also no longer valid and must not be referenced.
 * 
 * @return              0 on success, -1 on failure.
 ********************************************************************************/
VCHPRE_ int32_t VCHPOST_ audioplay_delete(AUDIOPLAY_STATE_T *handle);

/**
 * The <DFN>audioplay_get_buffer()</DFN> function requests an empty
 * buffer.  Any buffer returned will have the valid size indicated in
 * the call to <DFN>audioplay_create()</DFN>.
 * 
 * @param handle        Must be a handle previously created by
 *                      <DFN>audioplay_create()</DFN>.  
 * 
 * @return              A pointer to an available buffer.  If no buffers are
 *                      available, then <DFN>NULL</DFN> will be returned.
 *********************************************************************************/
VCHPRE_ uint8_t * VCHPOST_ audioplay_get_buffer(AUDIOPLAY_STATE_T *handle);


/**
 * The <DFN>audioplay_play_buffer()</DFN> sends a buffer containing
 * raw samples to be rendered.
 * 
 * @param handle         Must be a handle previously created by
 *                       <DFN>audioplay_create()</DFN>.  
 * 
 * @param buffer         Must be a pointer previously returned by
 *                       <DFN>audioplay_get_buffer()</DFN>.  After calling this function
 *                       the buffer pointer must not be referenced until returned
 *                       again by another call to <DFN>audioplay_get_buffer()</DFN>.
 * 
 * @param length         Length in bytes of valid data.  Must be a whole number of 
 *                       samples, ie a multiple of (num_channels*bit_depth/8).
 * 
 * @return               0 on success, -1 on failure
 ********************************************************************************/
VCHPRE_ int32_t VCHPOST_ audioplay_play_buffer(AUDIOPLAY_STATE_T *handle,
                                               uint8_t *buffer,
                                               uint32_t length);

/**
 * The <DFN>audioplay_get_latency()</DFN> requests the current audio
 * playout buffer size in samples, which is the latency until the next
 * sample supplied is to be rendered.
 * 
 * @param handle         Must be a handle previously created by
 *                       <DFN>audioplay_create()</DFN>.  
 * 
 * @return               Number of samples currently buffered.
 *********************************************************************************/
VCHPRE_ uint32_t VCHPOST_ audioplay_get_latency(AUDIOPLAY_STATE_T *handle);


#endif /* AUDIOPLAY_H */
