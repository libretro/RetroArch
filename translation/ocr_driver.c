#include "ocr_driver.h"

static const ocr_driver_t *ocr_backends[] = {
#ifdef HAVE_TESSERACT
	&ocr_tesseract,
#endif
	&ocr_null,
	NULL
};

static const ocr_driver_t *current_ocr_backend = NULL;

bool  ocr_driver_init(void)
{	
	/*TODO: find name of active driver*/
	
	bool success = false;
	if (current_ocr_backend)
		success = (*current_ocr_backend->init)();
	return success;
}

void  ocr_driver_free(void)
{
	if (current_ocr_backend)
		(*current_ocr_backend->free)();
}

char* ocr_driver_get_text(struct ocr_image_info image)
{
	char* image_string = NULL;
	if (current_ocr_backend)
		image_string = (*current_ocr_backend->get_text)(image);
	return image_string;
}