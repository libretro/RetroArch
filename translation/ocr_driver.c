#include <string/stdstring.h>
#include <libretro.h>

#include "ocr_driver.h"
#include "../configuration.h"

static const ocr_driver_t *ocr_backends[] = {
#ifdef HAVE_TESSERACT
	&ocr_tesseract,
#endif
	&ocr_null,
	NULL
};

static const ocr_driver_t *current_ocr_backend = NULL;
static void *ocr_data = NULL;

static const ocr_driver_t *ocr_find_backend(const char* ident)
{
	unsigned i;

	for (i = 0; ocr_backends[i]; i++)
	{
		if (string_is_equal(ocr_backends[i]->ident, ident))
			return ocr_backends[i];
	}

	return NULL;
}

bool  ocr_driver_init(void)
{
	settings_t *settings = config_get_ptr();
	int game_char_set = RETRO_LANGUAGE_DUMMY;
	current_ocr_backend = ocr_find_backend(settings->arrays.ocr_driver);
	ocr_data = NULL;

	/* TODO: get game language */

	if (current_ocr_backend)
		ocr_data = (*current_ocr_backend->init)(game_char_set);
	return ocr_data != NULL;
}

void  ocr_driver_free(void)
{
	if (current_ocr_backend && ocr_data)
		(*current_ocr_backend->free)(ocr_data);
}

char* ocr_driver_get_text(struct ocr_image_info image)
{
	char* image_string = NULL;
	if (current_ocr_backend && ocr_data)
		image_string = (*current_ocr_backend->get_text)(ocr_data, image);
	return image_string;
}