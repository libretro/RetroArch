#ifndef __TRANSLATION_DRIVER__H
#define __TRANSLATION_DRIVER__H

#include <boolean.h>
#include <retro_common_api.h>

#include "ocr_driver.h"

RETRO_BEGIN_DECLS

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
	/* returned char pointers do not need to be freed but are 1 time use, they may be destroyed on the next call to translate_image/text */
	/* NOTE: translate_image is allowed to call the ocr driver itself if it wants */
	char* (*translate_text)(void* data, const char* game_text);
	char* (*translate_image)(void* data, struct ocr_image_info image);

	const char *ident;
} translation_driver_t;

extern const translation_driver_t translation_cached_google;
extern const translation_driver_t translation_null;

bool  translation_driver_init(void);
void  translation_driver_free(void);

/* returned char pointers do not need to be freed but are 1 time use, they may be destroyed on the next call to translation_driver_translate_image */
char* translation_driver_translate_image(struct ocr_image_info image);

RETRO_END_DECLS

#endif
