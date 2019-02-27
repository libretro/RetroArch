#include "../ocr_driver.h"

static void* ocr_null_init(int game_character_set)
{
	return NULL;
}

static void ocr_null_free(void* data)
{
}

char* ocr_null_get_text(void* data, struct ocr_image_info image)
{
	return "";
}

const ocr_driver_t ocr_null = {
   ocr_null_init,
   ocr_null_free,
   ocr_null_get_text,
   "null"
};