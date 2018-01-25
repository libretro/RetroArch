#include "ocr_driver.h"

static const ocr_driver_t *ocr_backends[] = {
#ifdef HAVE_TESSERACT
	&ocr_tesseract,
#endif
	&ocr_null,
	NULL
};

static const ocr_driver_t *current_ocr_backend = NULL;
static void *ocr_data = NULL;

bool  ocr_driver_init(void)
{	
	/*TODO: find name of active driver*/
	
	ocr_data = NULL;
	if (current_ocr_backend)
		ocr_data = (*current_ocr_backend->init)();
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