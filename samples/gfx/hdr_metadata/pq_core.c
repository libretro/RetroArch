#define retro_run fixture_run
#define retro_load_game fixture_load_game
#include "../../record/nvenc_frontend/core.c"
#undef retro_run
#undef retro_load_game

RETRO_API bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format format = RETRO_PIXEL_FORMAT_HDR10_2101010;
   (void)info;
   return environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
}

RETRO_API void retro_run(void)
{
   struct retro_framebuffer fb;
   uint8_t *base = (uint8_t*)pixels;
   size_t pitch = WIDTH * sizeof(uint32_t);
   unsigned x, y;
   bool lent = false;
   memset(&fb, 0, sizeof(fb));
   fb.width = WIDTH;
   fb.height = HEIGHT;
   fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
   if (getenv("FP16_LEND") && environment(
            RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &fb))
   {
      if (fb.format != RETRO_PIXEL_FORMAT_HDR10_2101010 || fb.pitch < pitch)
      {
         fprintf(stderr, "[pq] FAIL: lent format=%u pitch=%lu\n",
               (unsigned)fb.format, (unsigned long)fb.pitch);
         exit(1);
      }
      base = (uint8_t*)fb.data;
      pitch = fb.pitch;
      lent = true;
   }
   for (y = 0; y < HEIGHT; y++)
   {
      uint32_t *row = (uint32_t*)(base + y * pitch);
      for (x = 0; x < WIDTH; x++)
         row[x] = 0xc0000000u | (128u << 20) | (512u << 10) | 768u;
   }
   if (!frames)
      fprintf(stderr, "[pq] PASS: native PQ Rec.2020 negotiation; lent=%u pitch=%lu\n",
            lent ? 1 : 0, (unsigned long)pitch);
   frames++;
   video(base, WIDTH, HEIGHT, pitch);
}
