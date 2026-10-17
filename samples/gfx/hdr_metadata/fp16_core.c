#define retro_run fixture_run
#define retro_load_game fixture_load_game
#include "../../record/nvenc_frontend/core.c"
#undef retro_run
#undef retro_load_game

static uint16_t half_pixels[HEIGHT][WIDTH * 4 + 28];

RETRO_API bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format format = RETRO_PIXEL_FORMAT_SCRGB_RGBA16F;
   (void)info;
   if (!environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format))
      return false;
   return true;
}

RETRO_API void retro_run(void)
{
   struct retro_hdr_metadata hdr;
   struct retro_framebuffer fb;
   unsigned x, y;
   unsigned width = WIDTH, height = HEIGHT;
   unsigned pitch = sizeof(half_pixels[0]);
   uint8_t *base = (uint8_t*)half_pixels;
   bool lent = false;
   bool stock = getenv("FP16_STOCK") != NULL;
   memset(&hdr, 0, sizeof(hdr));
   hdr.version = 1;
   hdr.size = sizeof(hdr);
   if (!environment(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr)
         || !(hdr.source_formats & RETRO_HDR_SOURCE_FORMAT_SCRGB_RGBA16F)
         || hdr.output_mode != 2)
      abort();
   if (getenv("FP16_RESIZE"))
   {
      if (frames >= 20 && frames < 40)
      {
         width = 300;
         height = 180;
      }
      if (frames == 20 || frames == 40)
      {
         struct retro_game_geometry geometry;
         memset(&geometry, 0, sizeof(geometry));
         geometry.base_width = width;
         geometry.base_height = height;
         geometry.max_width = WIDTH;
         geometry.max_height = HEIGHT;
         geometry.aspect_ratio = (float)width / height;
         if (!environment(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry))
            abort();
      }
   }
   memset(&fb, 0, sizeof(fb));
   fb.width = width;
   fb.height = height;
   fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
   if (getenv("FP16_LEND") && environment(
            RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &fb))
   {
      if (fb.format != RETRO_PIXEL_FORMAT_SCRGB_RGBA16F
            || fb.pitch < width * 8)
         abort();
      base = (uint8_t*)fb.data;
      pitch = (unsigned)fb.pitch;
      lent = true;
   }
   for (y = 0; y < height; y++)
   {
      uint16_t *row = (uint16_t*)(base + y * pitch);
      for (x = 0; x < width; x++)
      {
         /* -0.25, 0.5, 2.0: signed and above-white values must survive. */
         row[x * 4]     = stock ? 0x4113 : 0xb400;
         row[x * 4 + 1] = stock ? 0x4113 : 0x3800;
         row[x * 4 + 2] = stock ? 0x4113 : 0x4000;
         row[x * 4 + 3] = 0x3c00;
      }
   }
   if (!frames)
      fprintf(stderr, "[fp16] PASS: native scRGB negotiation; lent=%u pitch=%u\n",
            lent ? 1 : 0, pitch);
   frames++;
   video(base, width, height, pitch);
}
