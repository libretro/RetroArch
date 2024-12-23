/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2023 Jesse Talavera-Greenberg
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RETROARCH_MICROPHONE_DRIVER_H
#define RETROARCH_MICROPHONE_DRIVER_H

#include <boolean.h>
#include <lists/string_list.h>
#include <retro_common_api.h>
#include <libretro.h>
#include <audio/audio_resampler.h>
#include <queues/fifo_queue.h>

/**
 * Flags that indicate the current state of the microphone driver.
 */
enum microphone_driver_state_flags
{
   /**
    * Indicates that the driver was successfully created
    * and is currently valid.
    * You may open microphones and query them for samples at any time.
    *
    * This flag does \em not mean that the core will receive audio;
    * the driver might be suspended.
    */
   MICROPHONE_DRIVER_FLAG_ACTIVE  = (1 << 0)
};

/**
 * Flags that indicate the current state of a particular microphone.
 */
enum microphone_state_flags
{
   /**
    * Indicates that the microphone was successfully created
    * and is currently valid.
    * You may query it for samples at any time.
    *
    * This flag does \em not mean that the core will receive anything,
    * as there are several situations where a mic will return silence.
    *
    * If this flag is not set, then the others are meaningless.
    * In that case, reads from this microphone will return an error.
    */
   MICROPHONE_FLAG_ACTIVE = (1 << 0),

   /**
    * Indicates that the core considers this microphone "on"
    * and ready to retrieve audio.
    *
    * Even if a microphone is opened, the user might not want it running constantly;
    * they might prefer to hold a button to use it.
    *
    * If this flag is not set, the microphone will not process input.
    * Reads from it will return an error.
    */
   MICROPHONE_FLAG_ENABLED = (1 << 1),

   /**
    * Indicates that this microphone was requested
    * before the microphone driver was initialized,
    * so the driver will need to create this microphone
    * when it's ready.
    *
    * This flag is also used to reinitialize microphones
    * that were closed as part of a driver reinit.
    *
    * If this flag is set, reads from this microphone return silence
    * of the requested length.
    */
   MICROPHONE_FLAG_PENDING = (1 << 2),

   /**
    * Indicates that the microphone provides floating-point samples,
    * as opposed to integer samples.
    *
    * All audio is sent through the resampler,
    * which operates on floating-point samples.
    *
    * If this flag is set, then the resampled output doesn't need
    * to be converted back to \c int16_t format.
    *
    * This won't significantly affect the audio that the core receives;
    * either way, it's supposed to receive \c int16_t samples.
    *
    * This flag won't be set if the selected microphone driver
    * doesn't support (or is configured to not use) \c float samples.
    *
    * @see microphone_driver_t::mic_use_float
    */
   MICROPHONE_FLAG_USE_FLOAT = (1 << 3),

   /**
    * Indicates that the microphone driver is not currently retrieving samples,
    * although it's valid and can be resumed.
    *
    * Usually set when RetroArch needs to simulate audio input
    * without actually rendering samples (e.g. runahead),
    * or when reinitializing the driver.
    *
    * If this flag is set, reads from this microphone return silence
    * of the requested length.
    *
    * This is different from \c MICROPHONE_FLAG_ACTIVE and \c MICROPHONE_FLAG_ENABLED;
    * \c MICROPHONE_FLAG_ACTIVE indicates that the microphone is valid,
    * and \c MICROPHONE_FLAG_ENABLED indicates that the core has the microphone turned on.
    */
   MICROPHONE_FLAG_SUSPENDED = (1 << 4)
};

/**
 * Driver object that tracks a microphone's state.
 * Pointers to this object are provided to cores
 * for use as opaque handles by \c retro_microphone_interface_t.
 */
struct retro_microphone
{
   /**
    * Pointer to the context object created by the underlying driver.
    * It will contain data that's specific to each driver,
    * such as device IDs or sample queues.
    */
   void *microphone_context;

   /**
    * Pointer to the data that will be copied to cores.
    */
   int16_t* sample_buffer;

   /**
    * Length of \c sample_buffer in bytes, \em not samples.
    */
   size_t sample_buffer_length;

   /**
    * Bit flags that describe the state of this microphone.
    *
    * @see microphone_state_flags
    */
   enum microphone_state_flags flags;

   /**
    * Samples that will be sent to the core.
    */
   fifo_buffer_t *outgoing_samples;

   /**
    * The requested microphone parameters,
    * taken from the core's open_mic call.
    */
   retro_microphone_params_t requested_params;

   /**
    * The parameters of the microphone as it was provided.
    */
   retro_microphone_params_t actual_params;

   /**
    * The parameters of the microphone after any resampling
    * or other changes.
    */
   retro_microphone_params_t effective_params;

   /**
    * Pointer to the configured resampler for microphones.
    * May be different than the audio driver's resampler.
    */
   const retro_resampler_t *resampler;

   /**
    * Pointer to the resampler-specific context.
    * Not shared with the audio driver's resampler.
    */
   void *resampler_data;

   /**
    * The ratio of the core-requested sample rate to the device's opened sample rate.
    * If this is (almost) equal to 1, then resampling will be skipped.
    */
   double original_ratio;
};

/**
 * Defines the implementation of a microphone driver.
 * All functions are mandatory unless otherwise noted.
 */
typedef struct microphone_driver
{
   /**
    * Initializes the microphone driver.
    * This function should not open any actual microphones;
    * instead, it should set up any prerequisites necessary
    * to create a microphone.
    *
    * After this function is called,
    * microphones can be opened with \c open_mic.
    *
    * @returns A handle to the microphone driver context,
    * or \c NULL if there was an error.
    *
    * @see microphone_driver_init_internal
    **/
   void *(*init)(void);


   /**
    * Frees the driver context.
    * There is no need to close the microphones in this function,
    * the microphone system will do that before calling this.
    * Does nothing if \c driver_context is \c NULL.
    *
    * @param driver_context Pointer to the microphone driver context.
    * Provide the pointer that was returned by \c ::init(),
    * \em not one of the handles returned by \c ::mic_open().
    *
    * @see microphone_driver_deinit
    */
   void (*free)(void *driver_context);

   /**
    * Read samples from the microphone into the provided buffer.
    *
    * Samples are provided in mono.
    * Since samples are in mono, a "frame" and a "sample" mean the same thing
    * in the context of microphone input.
    *
    * If \c ::mic_use_float returns \c true,
    * samples will be in 32-bit \c float format with a range of [-1.0, 1.0].
    * Otherwise, samples will be in signed 16-bit integer format.
    * Data will be in native byte order either way.
    *
    * All reads should block until all requested frames are provided,
    * unless set otherwise with set_nonblock_state().
    *
    * @param[in] driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @param[in] mic_context Pointer to the microphone context.
    * Will be a value that was returned by \c ::open_mic().
    * @param[out] buffer Pointer to the buffer in which the read samples should be stored.
    * @param buffer_size The available length of \c buffer in bytes,
    * \em not samples or frames.
    * @return The number of bytes that were successfully read,
    * or \c -1 if there was an error.
    * May be less than \c buffer_size if this microphone is non-blocking.
    * If this microphone is in non-blocking mode and no new data is available,
    * the driver should return 0 rather than -1.
    *
    * @note Do not apply resampling or up-channeling;
    * the microphone frontend will do so.
    * @note Do not return silence if unable to read samples;
    * instead, return an error.
    * The frontend will provide silence to the core in
    * non-erroneous situations where microphone input is unsupported
    * (such as in fast-forward or rewind).
    *
    * @see microphone_driver_read
    */
   int (*read)(void *driver_context, void *mic_context, void *buffer, size_t buffer_size);

   /**
    * Sets the nonblocking state of the driver.
    * If the driver is in blocking mode (the default),
    * \c ::read() will block the current thread
    * until all requested samples are provided.
    * Otherwise, \c ::read() will return as many samples as it can (which may be none)
    * and return without waiting.
    *
    * If a driver does not support nonblocking mode,
    * leave this function pointer as \c NULL.
    *
    * @param driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @param[in] nonblock \c true if the driver should be in nonblocking mode,
    * \c false if it should be in blocking mode.
    * */
   void (*set_nonblock_state)(void *driver_context, bool nonblock);

   /**
    * A human-readable name for this driver.
    * Shown to the user in the driver selection menu.
    */
   const char *ident;

   /**
    * Returns a list of all microphones (aka "capture devices")
    * that are currently available.
    * The user can select from these devices on the options menu.
    *
    * Optional, but must be implemented if \c device_list_free is implemented.
    * The list returned by this function must be freed with \c device_list_free.
    *
    * @param[in] driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @return Pointer to a list of available capture devices,
    * an empty list if no capture devices are available,
    * or \c NULL if there was an error.
    **/
   struct string_list *(*device_list_new)(const void *driver_context);

   /**
    * Frees the microphone list that was returned by \c device_list_new.
    * Optional, but must be provided if \c device_list_new is implemented.
    * Will do nothing if any parameter is \c NULL.
    *
    * @param[in] driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init.
    * @param[in] devices Pointer to the device list
    * that was returned by \c device_list_new.
    */
   void (*device_list_free)(const void *driver_context, struct string_list *devices);

   /**
    * Initializes a microphone.
    * Cores that use microphone functionality will call this via
    * \c retro_microphone_interface_t::open_mic.
    *
    * The core may request a microphone before the driver is fully initialized,
    * but driver implementations do not need to concern themselves with that;
    * when the driver is ready, it will call this function.
    *
    * Opened microphones must \em not be activated,
    * i.e. \c mic_alive on a newly-opened microphone should return \c false.
    *
    * @param data Handle to the driver context
    * that was originally returned by \c init.
    * @param device A specific device name (or other options)
    * to create the microphone with.
    * Each microphone driver interprets this differently,
    * and some may ignore it.
    * @param rate The requested sampling rate of the new microphone in Hz.
    * @param latency The desired latency of the new microphone, in milliseconds.
    * @param new_rate Pointer to the actual sample frequency,
    * if the microphone couldn't be initialized with the value given by rate.
    * If NULL, then the value will not be reported to the caller;
    * this is not an error.
    * @return An opaque handle to the newly-initialized microphone
    * if it was successfully created,
    * or \c NULL if there was an error.
    * May be \c NULL if no microphone is available,
    * or if the maximum number of microphones has been created.
    * The returned handle should be provided to the \c microphone_context
    * parameter of all other microphone functions.
    *
    * @note Your driver should keep track of the mic context
    */
   void *(*open_mic)(void *driver_context, const char *device, unsigned rate,
                            unsigned latency, unsigned *new_rate);

   /**
    * Releases the resources used by a particular microphone
    * and stops its activity.
    * Will do nothing if either \p driver_context or \p microphone_context is \c NULL.
    *
    * @param driver_context Opaque handle to the audio driver context
    * that was used to create the provided microphone.
    * Implementations may use this to help in cleaning up the microphone,
    * but the driver context itself must \em not be released.
    * @param microphone_context Opaque handle to the microphone that will be freed.
    * Implementations should stop any recording activity before freeing resources.
    *
    * @post \p driver_context will still be valid,
    * while \p microphone_context will not.
    */
   void (*close_mic)(void *driver_context, void *microphone_context);

   /**
    * Returns the active state of the provided microphone.
    * This is the state of the device itself,
    * not the user's desired on/off state.
    *
    * @param[in] driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @param[in] mic_context Pointer to a particular microphone's context.
    * Will be a value that was returned by \c ::open_mic().
    * @return \c true if the provided microphone is active,
    * \c false if not or if there was an error.
    */
   bool (*mic_alive)(const void *driver_context, const void *mic_context);

   /**
    * Begins capture activity on the provided microphone, if necessary.
    *
    * @param[in] driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @param[in] mic_context Pointer to a particular microphone's context.
    * Will be a value that was returned by \c ::open_mic().
    * @return \c true if the microphone was successfully started
    * or if it was already running. \c false if there was an error.
    */
   bool (*start_mic)(void *driver_context, void *microphone_context);

   /**
    * Pauses capture activity on the provided microphone, if necessary.
    * This function must not deallocate the microphone.
    *
    * @param[in] driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @param[in] mic_context Pointer to a particular microphone's context.
    * Will be a value that was returned by \c ::open_mic().
    * @return \c true if the microphone was successfully paused
    * or if it was already stopped. \c false if there was an error.
    */
   bool (*stop_mic)(void *driver_context, void *microphone_context);

   /**
    * Queries whether this microphone captures floating-point samples,
    * as opposed to 16-bit integer samples.
    *
    * Optional; if not provided, then \c int16_t samples are assumed.
    *
    * @param[in] driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @param[in] mic_context Pointer to a particular microphone's context.
    * Will be a value that was returned by \c ::open_mic().
    * @return \c true if this microphone provides floating-point samples.
    */
   bool (*mic_use_float)(const void *driver_context, const void *microphone_context);
} microphone_driver_t;

typedef struct microphone_driver_state
{
   /**
    * The buffer that receives samples from the microphone backend,
    * before they're processed.
    */
   void *input_frames;

   /**
    * The length of \c input_frames in bytes.
    */
   size_t input_frames_length;

   /**
    * The buffer that receives samples that have been
    * converted to floating-point format, if necessary.
    */
   float *converted_input_frames;

   /**
    * The length of \c converted_input_frames in bytes.
    */
   size_t converted_input_frames_length;

   /**
    * The buffer that stores microphone samples
    * after they've been converted to floating-point format
    * and up-channeled to dual-mono.
    */
   float *dual_mono_frames;

   /**
    * The length of \c dual_mono_frames in bytes.
    */
   size_t dual_mono_frames_length;

   /**
    * The buffer that stores microphone samples
    * after they've been converted to float,
    * up-channeled to dual-mono,
    * and resampled.
    */
   float *resampled_frames;

   /**
    * The length of \c resampled_frames in bytes.
    */
   size_t resampled_frames_length;


   /**
    * The buffer that stores microphone samples
    * after they've been resampled
    * and converted to mono.
    */
   float *resampled_mono_frames;

   /**
    * The length of \c resampled_mono_frames in bytes.
    */
   size_t resampled_mono_frames_length;

   /**
    * The buffer that contains the microphone input
    * after it's been totally processed and converted.
    * The contents of this buffer will be provided to the core.
    */
   int16_t *final_frames;

   /**
    * The length of \c final_frames in bytes.
    */
   size_t final_frames_length;

   /**
    * The current microphone driver.
    * Will be a pointer to one of the elements of \c microphone_drivers.
    */
   const microphone_driver_t *driver;

   struct string_list *devices_list;

   /**
    * Opaque handle to the driver-specific context.
    */
   void *driver_context;

   /**
    * The handle to the created microphone, if any.
    * The libretro API is designed to expose multiple microphones,
    * but RetroArch only supports one at a time for now.
    * PRs welcome!
    */
   retro_microphone_t microphone;

   enum microphone_driver_state_flags flags;

   enum resampler_quality resampler_quality;

   char resampler_ident[64];
} microphone_driver_state_t;

/**
 * Starts all enabled microphones,
 * and opens all pending microphones.
 * It is not an error to call this function
 * if the mic driver is already running.
 *
 * @return \c true if the configured driver was started
 * and pending microphones opened,
 * \c false if there was an error.
 */
bool microphone_driver_start(void);

/**
 * Stops all enabled microphones.
 * It is not an error to call this function
 * if the mic driver is already stopped,
 * or if there are no open microphones.
 *
 * Microphones will not receive any input
 * until \c microphone_driver_start is called again.
 *
 * @return \c true if the driver was stopped,
 * \c false if there was an error.
 */
bool microphone_driver_stop(void);

/**
 * Driver function for opening a microphone.
 * Provided to retro_microphone_interface::init_microphone().
 * @param[in] params Parameters for the newly-opened microphone
 * that the core requested.
 * May be \c NULL, in which case defaults will be selected.
 * @return Pointer to the newly-opened microphone,
 * or \c NULL if there was an error.
 */
retro_microphone_t *microphone_driver_open_mic(const retro_microphone_params_t *params);

/**
 * Driver function for closing an open microphone.
 * Does nothing if the provided microphone is \c NULL.
 * @param microphone Pointer to the microphone to close.
 * Will be a value that was returned by \c microphone_driver_open_mic.
 */
void microphone_driver_close_mic(retro_microphone_t *microphone);

/**
 * Enables or disables the microphone.
 *
 * @returns \c true if the microphone's active state was set,
 * \c false if there was an error.
 */
bool microphone_driver_set_mic_state(retro_microphone_t *microphone, bool state);

/**
 * Queries the active state of the microphone.
 * Inactive microphones return no audio,
 * and it is an error to read from them.
 *
 * @param microphone The microphone to query.
 * @return The active state of \c microphone.
 * \c true if the microphone is ready to accept input,
 * \c false if not.
 */
bool microphone_driver_get_mic_state(const retro_microphone_t *microphone);

/**
 * Reads a particular number of samples from a microphone
 * and stores it in a buffer.
 * This should be called by the core each frame if there's an active microphone.
 * Will block until the buffer is filled,
 * so don't ask for more than you'll use in a single frame.
 *
 * @param microphone The microphone from which samples will be read.
 * @param samples The buffer in which incoming samples will be stored.
 * @param num_samples The available size of the provided buffer,
 * in samples (\em not bytes).
 * @return The number of samples that were read, or -1 if there was an error.
 */
int microphone_driver_read(retro_microphone_t *microphone, int16_t* samples, size_t num_samples);

bool microphone_driver_get_effective_params(const retro_microphone_t *microphone, retro_microphone_params_t *params);

/**
 * A trivial backend with no functions and an identifier of "null".
 * Effectively disables mic support or serves as a stand-in
 * on platforms that lack mic backends.
 */
extern microphone_driver_t microphone_null;

/**
 * The ALSA-backed microphone driver.
 */
extern microphone_driver_t microphone_alsa;

/**
 * The multithreaded ALSA-backed microphone driver.
 */
extern microphone_driver_t microphone_alsathread;

/**
 * The SDL-backed microphone driver.
 */
extern microphone_driver_t microphone_sdl;

/**
 * The WASAPI-backed microphone driver.
 */
extern microphone_driver_t microphone_wasapi;

/**
 * The PipeWire-backed microphone driver.
 */
extern microphone_driver_t microphone_pipewire;

/**
 * @return Pointer to the global microphone driver state.
 */
microphone_driver_state_t *microphone_state_get_ptr(void);

/**
 * All microphone drivers available for use in this build.
 * The contents of this array depend on the build configuration
 * and target platform.
 */
extern microphone_driver_t *microphone_drivers[];

bool microphone_driver_init_internal(void *settings_data);

bool microphone_driver_deinit(bool is_reset);

bool microphone_driver_find_driver(
      void *settings_data,
      const char *prefix,
      bool verbosity_enabled);

bool microphone_driver_get_devices_list(void **ptr);

#endif /* RETROARCH_MICROPHONE_DRIVER_H */
