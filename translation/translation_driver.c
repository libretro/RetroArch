#include <string/stdstring.h>

#include "translation_driver.h"
#include "ocr_driver.h"
#include "../configuration.h"

static const translation_driver_t *translation_backends[] = {
	&translation_cached_google,
	&ocr_null,
	NULL
};

static const translation_driver_t *current_translation_backend = NULL;
static void *translation_data = NULL;

static const translation_driver_t *translation_find_backend(
      const char* ident)
{
	unsigned i;

	for (i = 0; translation_backends[i]; i++)
	{
		if (string_is_equal(translation_backends[i]->ident, ident))
			return translation_backends[i];
	}

	return NULL;
}

bool  translation_driver_init(void)
{
	settings_t *settings = config_get_ptr();

   if (!settings)
      return false;

   current_translation_backend = translation_find_backend(
         settings->arrays.translation_driver);
	translation_data = NULL;

	if (current_translation_backend)
		translation_data = (*current_translation_backend->init)();
	return translation_data != NULL;
}

void  translation_driver_free(void)
{
   if (current_translation_backend && translation_data)
      (*current_translation_backend->free)(translation_data);
}

char* translation_driver_translate_image(struct ocr_image_info image)
{
   char* translated_text = NULL;

   if (current_translation_backend && translation_data)
   {
      if (current_translation_backend->translate_image)
         translated_text = (*current_translation_backend->translate_image)
            (translation_data, image);
      else
         translated_text = (*current_translation_backend->translate_text)
            (translation_data, ocr_driver_get_text(image));
   }

   return translated_text;
}
