#include "../translation_driver.h"

static void* translation_null_init(const struct translation_driver_info *params)
{
	return NULL;
}

static void translation_null_free(void* data)
{
}

static char* translation_null_translate_text(const char* game_text)
{
	return "";
}

const translation_driver_t translation_null = {
   translation_null_init,
   translation_null_free,
   translation_null_translate_text,
   NULL,
   "null"
};