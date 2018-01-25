#include "translation_driver.h"
#include "ocr_driver.h"

static const translation_driver_t *translation_backends[] = {
	&translation_cached_google,
	&ocr_null,
	NULL
};

static const translation_driver_t *current_translation_backend = NULL;
static void *translation_data = NULL;

bool  translation_driver_init(void)
{
	/*TODO: find name of active driver*/
	
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
			translated_text = (*current_translation_backend->translate_image)(translation_data, image);
		else 
			translated_text = (*current_translation_backend->translate_text)(translation_data, ocr_driver_get_text(image));
	}
	return translated_text;
}