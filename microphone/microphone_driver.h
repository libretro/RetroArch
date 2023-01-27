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
#include "audio/audio_defines.h"

#define MAX_SUPPORTED_MICROPHONES 8
#define MICROPHONE_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)

enum microphone_driver_state_flags
{
   /**
    * Indicates that the microphone driver is active.
    * Microphones can be opened, closed, and read.
    */
   MICROPHONE_DRIVER_FLAG_ACTIVE       = (1 << 0),
};

enum microphone_state_flags
{
   MICROPHONE_FLAG_USE_FLOAT    = (1 << 0)
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
    * Number of bytes that were copied into the buffer in the most recent flush.
    * Accounts for resampling
    **/
   size_t most_recent_copy_length;

   enum microphone_state_flags flags;

   /* May be enabled even before the driver is ready */
   bool pending_enabled;

   /**
    * True if this object represents a valid or pending microphone.
    * Mostly exists because retro_microphone is statically allocated,
    * so there's no reason to check it against NULL.
    */
   bool active;

   bool error;
};

/**
 * Defines the implementation of a microphone driver.
 * Similar to audio_driver_t.
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
    **/
   void *(*init)(void);


   /**
    * Frees the driver context and closes all microphones.
    * There is no need to call close_mic after calling this function.
    * Does nothing if \c driver_context is \c NULL.
    *
    * @param driver_context Pointer to the microphone driver context.
    * Provide the pointer that was returned by \c ::init(),
    * \em not one of the handles returned by \c ::mic_open().
    *
    * @post The provided driver context is invalid,
    * and all microphones are closed.
    */
   void (*free)(void *driver_context);

   /**
    * Read samples from the microphone into the provided buffer.
    *
    * Samples are provided in mono.
    * Since samples are in mono, a "frame" and a "sample" mean the same thing
    * in the context of microphone input.
    *
    * If \c use_float() returns \c true,
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
    */
   ssize_t (*read)(void *driver_context, void *mic_context, void *buffer, size_t buffer_size);

   /**
    * Resumes the driver from the paused state.
    * Microphones will resume activity \em if they were already active
    * before the driver was stopped.
    **/
   bool (*start)(void *data, bool is_shutdown);

   /**
    * Temporarily pauses all microphones.
    * The return value of \c get_mic_state will not be updated,
    * but all microphones will stop recording until \c start is called.
    *
    * @param driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @return \c true if microphone driver was successfully paused,
    * \c false if there was an error.
    **/
   bool (*stop)(void *driver_context);

   /**
    * Queries whether the driver is active.
    * This only queries the overall driver state,
    * not the state of individual microphones.
    *
    * @param driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @return \c true if the driver is active.
    * \c false if not or if there was an error.
    */
   bool (*alive)(void *driver_context);

   /**
    * Sets the nonblocking state of all microphones.
    * Primarily used for fast-forwarding.
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
    * Will be the value that was returned by \c init().
    * @param[in] devices Pointer to the device list
    * that was returned by \c device_list_new.
    */
   void (*device_list_free)(const void *driver_context, struct string_list *devices);

   /**
    * Initializes a microphone using the audio driver.
    * Cores that use microphone functionality will call this via
    * retro_microphone_interface::init_microphone.
    *
    * The core may request a microphone before the driver is fully initialized,
    * but driver implementations do not need to concern themselves with that;
    * when the driver is ready, it will call this function.
    *
    * @param data Handle to the driver context
    * that was originally returned by ::init.
    * @param device A specific device name (or other options)
    * to create the microphone with.
    * Each microphone driver interprets this differently,
    * and some may ignore it.
    * @param rate The requested sampling rate of the new microphone,
    * in Hz.
    * @param latency TODO
    * @param block_frames TODO
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
                            unsigned latency, unsigned block_frames, unsigned *new_rate);

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
    * Note that this describes the user-facing state of the microphone;
    * this function can still return \c true if the mic driver is paused
    * or muted.
    *
    * @param[in] driver_context Pointer to the driver context.
    * Will be the value that was returned by \c ::init().
    * @param[in] mic_context Pointer to a particular microphone's context.
    * Will be a value that was returned by \c ::open_mic().
    * @return \c true if the provided microphone is active,
    * \c false if not or if there was an error.
    */
   bool (*mic_alive)(const void *driver_context, const void *mic_context);

   bool (*start_mic)(void *driver_context, void *microphone_context);

   bool (*stop_mic)(void *driver_context, void *microphone_context);
} microphone_driver_t;

typedef struct
{
   struct string_list *devices_list;

   /**
    * A scratch buffer for audio input to be received.
    */
   void  *input_samples_buf;
   size_t input_samples_buf_length;

   /**
    * A scratch buffer for processed audio output to be converted to 16-bit,
    * so that it can be sent to the core.
    */
   int16_t *input_samples_conv_buf;
   size_t input_samples_conv_buf_length;

   /**
    * The current microphone driver.
    * Will be a pointer to one of the elements of \c microphone_drivers.
    */
   const microphone_driver_t *driver;

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

   /**
    * If \c true, all microphones return silence.
    */
   bool mute_enable;
} microphone_driver_state_t;

/**
 * Starts the active microphone driver.
 * @param is_shutdown TODO
 * @return \c true if the configured driver was started,
 * \c false if there was an error.
 */
bool microphone_driver_start(bool is_shutdown);

/**
 * Stops the active microphone driver.
 * Microphones will not receive any input
 * until \c microphone_driver_start is called again.
 * @return \c true if the driver was stopped,
 * \c false if there was an error.
 */
bool microphone_driver_stop(void);

/**
 * Driver function for opening a microphone.
 * Provided to retro_microphone_interface::init_microphone().
 * @return Pointer to the newly-opened microphone,
 * or \c NULL if there was an error.
 */
retro_microphone_t *microphone_driver_open_mic(void);

/**
 * Driver function for closing an open microphone.
 * Does nothing if the provided microphone is \c NULL.
 * @param microphone Pointer to the microphone to close.
 * Will be a value that was returned by \c microphone_driver_open_mic.
 */
void microphone_driver_close_mic(retro_microphone_t *microphone);

/**
 * TODO
 */
bool microphone_driver_set_mic_state(retro_microphone_t *microphone, bool state);

/**
 * TODO
 * @param microphone
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
 * @return Pointer to the global microphone driver state.
 */
microphone_driver_state_t *microphone_state_get_ptr(void);

/**
 * All microphone drivers available for use in this build.
 * The contents of this array depend on the build configuration
 * and target platform.
 */
extern microphone_driver_t *microphone_drivers[];


/**
 * TODO
 * @param stats
 * @return
 */
bool microphone_compute_buffer_statistics(audio_statistics_t *stats);

float microphone_driver_monitor_adjust_system_rates(
      double input_sample_rate,
      double input_fps,
      float video_refresh_rate,
      unsigned video_swap_interval,
      float audio_max_timing_skew);

bool microphone_driver_init_internal(void *settings_data);

bool microphone_driver_deinit(void);

bool microphone_driver_find_driver(
      void *settings_data,
      const char *prefix,
      bool verbosity_enabled);

#endif /* RETROARCH_MICROPHONE_DRIVER_H */