#include "../translation_driver.h"

static void* translation_cached_google_init(const struct translation_driver_info *params)
{
	return NULL;
}

static void translation_cached_google_free(void* data)
{
}

static char* translation_cached_google_translate_text(void* data, const char* game_text)
{
	return "";
}

static char* translation_cached_google_translate_image(void* data, struct ocr_image_info image)
{
	return "";
}

const translation_driver_t translation_cached_google = {
   translation_cached_google_init,
   translation_cached_google_free,
   translation_cached_google_translate_text,
   translation_cached_google_translate_image,
   "cached_google"
};