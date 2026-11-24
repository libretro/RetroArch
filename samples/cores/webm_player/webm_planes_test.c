/* The builtin core's software frames through the planes sink: every
 * picture the core would blit is handed over as its 8-bit 4:2:0 planes
 * instead, lent for the call, with a NULL video refresh in the blit's
 * place - and a hold repeats it with NULL too, never with the stale RGB
 * buffer. Converted with the colour flags the sink is given, the planes
 * are the pictures the core blits itself without a sink: byte for byte
 * where the blit is the shared converter (WebM), within the decoder's
 * own rounding where rmp4 converts (MP4). A sink that is not ready gets
 * none and the core blits; a frame it does not take leaves the last one
 * up.
 *
 * Usage: webm_planes_test FILE TOLERANCE */
#include "../../../cores/libretro-media/media_core.c"

#define MAX_FRAMES 64

static unsigned planes_failures;
#define PCHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL planes %u: %s\n", \
      (unsigned)__LINE__, #c); planes_failures++; } } while (0)

static uint32_t *frames[2][MAX_FRAMES];
static unsigned  nframes[2], fw, fh;
static unsigned  pass, rgb_calls, null_calls, sink_calls, refuse_every;
static unsigned  last_flags;
static bool      sink_ready;

static uint32_t *keep(unsigned w, unsigned h)
{
   uint32_t *f;
   if (pass > 1 || nframes[pass] >= MAX_FRAMES
         || !(f = (uint32_t*)calloc((size_t)w * h, sizeof(*f))))
      return NULL;
   frames[pass][nframes[pass]++] = f;
   return f;
}

/* The sink: each frame's planes converted as the GPU would, with the
 * flags it came with, and kept */
static bool planes_sink(void *user, const video_hw_frame_t *f)
{
   uint32_t *rgb;
   unsigned cw, ch;
   (void)user;
   if (!f)
      return sink_ready;
   PCHECK((pass == 1 || pass == 3) && f->planes[0] && f->planes[1] && f->planes[2]
         && f->chroma_step == 1 && f->pixfmt == IMAGE_PIXFMT_I420
         && !f->image && !f->abandon);
   PCHECK(f->width == fw && f->height == fh);
   sink_calls++;
   if (refuse_every && sink_calls % refuse_every == 0)
      return false;
   cw = (f->width + 1) / 2;
   ch = (f->height + 1) / 2;
   PCHECK(f->strides[0] >= f->width && f->strides[1] >= cw
         && f->strides[2] == f->strides[1]);
   (void)ch;
   last_flags = f->yuv;
   if ((rgb = keep(f->width, f->height)))
      image_yuv_420_to_rgb32(rgb, f->width, f->planes[0], f->strides[0],
            f->planes[1], f->strides[1], f->planes[2], f->strides[2], 1,
            f->width, f->height, f->yuv);
   return true;
}

static void planes_video(const void *data, unsigned w, unsigned h,
      size_t pitch)
{
   unsigned y;
   uint32_t *rgb;
   if (!data)
   {
      null_calls++;
      return;
   }
   rgb_calls++;
   PCHECK(pass != 1 || !sink_ready);
   if (pass != 0 || !(rgb = keep(w, h)))
      return;
   fw = w;
   fh = h;
   for (y = 0; y < h; y++)
      memcpy(rgb + (size_t)y * w,
            (const uint8_t*)data + (size_t)y * pitch, (size_t)w * 4);
}

static bool planes_env(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_XRGB8888;
   return false;
}
static void planes_poll(void) { }
static int16_t planes_input(unsigned p, unsigned d, unsigned i, unsigned id)
{ (void)p; (void)d; (void)i; (void)id; return 0; }
static size_t planes_audio(const int16_t *s, size_t n) { (void)s; return n; }

static void play(const char *path, unsigned runs)
{
   struct retro_game_info game;
   unsigned i;
   memset(&game, 0, sizeof(game));
   game.path   = path;
   rgb_calls   = null_calls = sink_calls = 0;
   PCHECK(retro_load_game(&game));
   for (i = 0; i < runs; i++)
      retro_run();
   retro_unload_game();
}

/* The largest difference of a channel between two pictures */
static int frame_diff(const uint32_t *a, const uint32_t *b)
{
   unsigned k, s;
   int worst = 0;
   for (k = 0; k < fw * fh; k++)
      for (s = 0; s < 24; s += 8)
      {
         int d = (int)((a[k] >> s) & 0xFF) - (int)((b[k] >> s) & 0xFF);
         if (d < 0)
            d = -d;
         if (d > worst)
            worst = d;
      }
   return worst;
}

int main(int argc, char **argv)
{
   unsigned i, j, n, tolerance;
   int worst = 0;
   if (argc != 3)
      return 2;
   tolerance = (unsigned)atoi(argv[2]);
   retro_set_environment(planes_env);
   retro_set_video_refresh(planes_video);
   retro_set_audio_sample_batch(planes_audio);
   retro_set_input_poll(planes_poll);
   retro_set_input_state(planes_input);
   retro_init();

   /* the core's own pictures */
   pass = 0;
   PCHECK(media_set_video_planes_refresh(NULL, NULL));
   play(argv[1], 40);
   PCHECK(nframes[0] >= 8 && !null_calls);

   /* the same through the sink: no RGB at all, holds included */
   pass       = 1;
   sink_ready = true;
   PCHECK(media_set_video_planes_refresh(planes_sink, NULL));
   play(argv[1], 40);
   PCHECK(!rgb_calls && null_calls == 40 && sink_calls == nframes[1]);
   /* each picture the sink got is the next one the core blitted; the
    * blitted run also holds pictures, which are passed over */
   n = nframes[1];
   PCHECK(n >= 8);
   for (i = 0, j = 0; i < n; i++)
   {
      int d = -1;
      while (j < nframes[0]
            && (d = frame_diff(frames[0][j], frames[1][i])) > (int)tolerance)
         j++;
      PCHECK(j < nframes[0]);
      if (j >= nframes[0])
         break;
      if (d > worst)
         worst = d;
      j++;
   }
   PCHECK(!(last_flags & IMAGE_YUV_FLAG_BT709));

   /* a sink that is not ready: the core blits */
   pass       = 2;
   sink_ready = false;
   play(argv[1], 40);
   PCHECK(rgb_calls == 40 && !null_calls && !sink_calls);

   /* frames not taken leave the last one up */
   pass         = 3;
   sink_ready   = true;
   refuse_every = 3;
   play(argv[1], 40);
   PCHECK(!rgb_calls && null_calls == 40 && sink_calls >= 8);

   PCHECK(media_set_video_planes_refresh(NULL, NULL));
   retro_deinit();
   for (pass = 0; pass < 2; pass++)
      for (i = 0; i < nframes[pass]; i++)
         free(frames[pass][i]);
   printf("%s: %u frames as planes, worst difference %d: %u failures\n",
         argv[1], n, worst, planes_failures);
   return planes_failures ? 1 : 0;
}
