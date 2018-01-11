#ifndef __TRANSLATION_DRIVER__H
#define __TRANSLATION_DRIVER__H

#include "ocr_driver.h"

enum translation_init_errors
{
   TRANSLATION_INIT_SUCCESS = 0,
   TRANSLATION_INIT_UNSUPPORTED_DEVICE_LANGUAGE,
   TRANSLATION_INIT_UNSUPPORTED_GAME_LANGUAGE,
   TRANSLATION_INIT_UNKNOWN_DEVICE_LANGUAGE,
   TRANSLATION_INIT_UNKNOWN_GAME_LANGUAGE
};

struct translation_driver_info
{
	int device_language;
	int game_language;
};

typedef struct translation_driver
{
	void* (*init)(const struct translation_driver_info *params);
	void  (*free)(void* data);
	
	/* use translate_image if non NULL else run image through ocr driver then run translate_text */
	/* NOTE: translate_image is allowed to call the ocr driver itself if it wants */
	char* (*translate_text)(const char* game_text);
	char* (*translate_image)(struct ocr_image_info image);
	
	const char *ident;
} translation_driver_t;

extern const translation_driver_t translation_cached_google;
extern const translation_driver_t translation_null;

char* translation_translate_image(struct ocr_image_info image);

#endif