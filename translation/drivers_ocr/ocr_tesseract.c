#include <libretro.h>

#include "../ocr_driver.h"
#include "tesseract/wrapper/tess_get_text.h"

static void* ocr_tesseract_init(int game_character_set)
{
   bool pass                 = false;
   char* lang_data_dir       = NULL;
   const char* tess_char_set = NULL;

   switch (game_character_set)
   {
      case RETRO_LANGUAGE_JAPANESE:
         tess_char_set = "jpn";
         break;

      case RETRO_LANGUAGE_ENGLISH:
         tess_char_set = "eng";
         break;

      case RETRO_LANGUAGE_SPANISH:
         tess_char_set = "spa";
         break;
   }

   if (!tess_char_set)
      return NULL;

   /* TODO: get lang data from system dir */
   pass = tess_init(lang_data_dir, tess_char_set);

   /* data is unused by tesseract */
   if (pass)
      return (void*)32;

   return NULL;
}

static void ocr_tesseract_free(void* data)
{
}

char* ocr_tesseract_get_text(void* data, struct ocr_image_info image)
{
	tess_image temp_image;

	temp_image.width  = image.width;
	temp_image.height = image.height;
	temp_image.data   = image.data;

	switch (image.pixel_format)
	{
		case RETRO_PIXEL_FORMAT_0RGB1555:
		case RETRO_PIXEL_FORMAT_RGB565:
			temp_image.bytes_per_pixel = 2;
			break

		case RETRO_PIXEL_FORMAT_XRGB8888:
			temp_image.bytes_per_pixel = 4;
			break;

		default:
			/* unsupported format */
			return "";
	}

	return tess_get_text(temp_image);
}

const ocr_driver_t ocr_tesseract = {
   ocr_tesseract_init,
   ocr_tesseract_free,
   ocr_tesseract_get_text,
   "tesseract"
};
