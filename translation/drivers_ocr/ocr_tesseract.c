#include "../ocr_driver.h"

static void* ocr_tesseract_init()
{
	return NULL;
}
	
static void ocr_tesseract_free(void* data)
{
}

char* ocr_tesseract_get_text(struct ocr_image_info image)
{
	return "";
}
	
const ocr_driver_t ocr_tesseract = {
   ocr_tesseract_init,
   ocr_tesseract_free,
   ocr_tesseract_get_text,
   "tesseract"
};