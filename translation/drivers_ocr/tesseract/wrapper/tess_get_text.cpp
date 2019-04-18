#include <boolean.h>
#include <tesseract/baseapi.h>
#include "tess_get_text.h"

#define ERROR_BUFFER_LENGTH 1000

static tesseract::TessBaseAPI *api;
static char* one_time_return_pointer = NULL;

char tess_last_error[ERROR_BUFFER_LENGTH];

bool tess_init(const char* lang_data_dir, const char* language)
{
   api = new tesseract::TessBaseAPI();

   snprintf(tess_last_error, ERROR_BUFFER_LENGTH, "No errors!\n");

   if (api->Init(lang_data_dir, language))
   {
      snprintf(tess_last_error, ERROR_BUFFER_LENGTH,
            "Could not initialize tesseract.\n");
      return false;
   }

   return true;
}

void tess_deinit(void)
{
   if (one_time_return_pointer)
      delete [] one_time_return_pointer;

   if (api)
      api->End();
}

char* tess_get_text(tess_image image)
{
   if (one_time_return_pointer)
      delete [] one_time_return_pointer;

   api->SetImage(image.data, image.width, image.height,
         image.bytes_per_pixel, image.width * image.bytes_per_pixel);
   one_time_return_pointer = api->GetUTF8Text();
   return one_time_return_pointer;
}
