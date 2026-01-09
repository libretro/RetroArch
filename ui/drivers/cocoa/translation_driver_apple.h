/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - RetroArch
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  See the COPYING file at the root of the source tree for the full license text.
 */

#ifndef __TRANSLATION_DRIVER_APPLE_H
#define __TRANSLATION_DRIVER_APPLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the Apple translation backend.
 * Checks OS version requirements (macOS 10.15+ / iOS 13+ for OCR).
 * @return true if available
 */
bool apple_translate_init(void);

/**
 * Check if on-device translation is available.
 * Translation requires macOS 14.0+ / iOS 17.4+.
 * @return true if translation is available
 */
bool apple_translate_can_translate(void);

/**
 * Callback for async translation completion.
 * Called on the main thread when translation is done.
 *
 * @param text        Translated text (caller must free with apple_translate_free_string)
 *                    or NULL if not available
 * @param image_data  Image data in PNG format with alpha channel for transparent overlay
 *                    (caller must free with apple_translate_free_data) or NULL if not available
 * @param image_size  Size of image_data in bytes
 * @param sound_data  Audio data in WAV format (caller must free with apple_translate_free_data)
 *                    or NULL if not available
 * @param sound_size  Size of sound_data in bytes
 * @param error       Error message (do not free) or NULL on success
 * @param userdata    User data passed to apple_translate_image
 */
typedef void (*apple_translate_callback_t)(
   char *text,
   void *image_data,
   size_t image_size,
   void *sound_data,
   size_t sound_size,
   const char *error,
   void *userdata
);

/**
 * Perform OCR on image data and optionally translate the result.
 * This is async - returns immediately and calls callback on main thread when done.
 *
 * Based on mode:
 * - Mode 0: Image overlay - returns transparent PNG with text boxes only
 * - Mode 1: Speech - generates audio using text-to-speech
 * - Mode 2: Narrator - returns text for on-device narrator
 * - Mode 3: Image + Speech - both transparent PNG overlay and audio
 *
 * @param image_data     Raw image data in BGR24 format (copied internally)
 * @param width          Image width in pixels
 * @param height         Image height in pixels
 * @param pitch          Bytes per row (typically width * 3)
 * @param source_lang    Source language hint (e.g., "ja") or NULL for auto
 * @param target_lang    Target language (e.g., "en") or NULL to skip translation
 * @param mode           Output mode (0=image, 1=speech, 2=narrator, 3=image+speech)
 * @param callback       Callback to invoke on completion (on main thread)
 * @param userdata       User data to pass to callback
 */
void apple_translate_image(
   const uint8_t *image_data,
   unsigned width,
   unsigned height,
   unsigned pitch,
   const char *source_lang,
   const char *target_lang,
   unsigned mode,
   apple_translate_callback_t callback,
   void *userdata
);

/**
 * Free a string returned via callback.
 */
void apple_translate_free_string(char *str);

/**
 * Free binary data (sound_data, image_data) returned via callback.
 */
void apple_translate_free_data(void *data);

/**
 * Log a message through RetroArch's logging system.
 * Called from Swift to route logs to retroarch.log.
 */
void apple_translate_log(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* __TRANSLATION_DRIVER_APPLE_H */
