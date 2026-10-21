/* Compare bounded WebM Opus seek with the established preroll path. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <formats/audio.h>
#ifndef AUDIO_SEEK_TYPE
#define AUDIO_SEEK_TYPE AUDIO_TYPE_OPUS
#define AUDIO_SEEK_NAME "Opus"
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", path, __LINE__, #x); goto done; } } while (0)
static int read_exact(void *decoder, void *out, size_t frames,
      unsigned channels, int s16)
{
   size_t done = 0;
   unsigned steps;
   for (steps = 0; steps < 10000 && done < frames; steps++)
   {
      size_t got = 0;
      int ret = s16
         ? audio_transfer_read_s16(decoder, AUDIO_SEEK_TYPE,
               (int16_t*)out + done * channels, frames - done, &got)
         : audio_transfer_read_f32(decoder, AUDIO_SEEK_TYPE,
               (float*)out + done * channels, frames - done, &got);
      if (ret < 0 || got > frames - done || (!got && ret != AUDIO_PROCESS_NEXT))
         return 0;
      done += got;
   }
   return done == frames;
}
static int check_file(const char *path, int s16)
{
   FILE *f = NULL;
   void *seek = NULL, *legacy = NULL;
   uint8_t *bytes = NULL;
   float a[128], b[128], *linear = NULL;
   int16_t *linear16 = NULL;
   int16_t a16[128], b16[128];
   size_t size;
   long length;
   uint64_t target, total = 0;
   unsigned channels, rate;
   int probe, ret, steps, i, status = 1;
   CHECK((f = fopen(path, "rb")) != NULL);
   CHECK(!fseek(f, 0, SEEK_END) && (length = ftell(f)) > 0 && !fseek(f, 0, SEEK_SET));
   size = (size_t)length;
   CHECK((bytes = (uint8_t*)malloc(size)) != NULL);
   CHECK(fread(bytes, 1, size, f) == size);
   fclose(f); f = NULL;
   CHECK((seek = audio_transfer_new(AUDIO_SEEK_TYPE)) != NULL);
   CHECK((legacy = audio_transfer_new(AUDIO_SEEK_TYPE)) != NULL);
   audio_transfer_set_buffer_ptr(seek, AUDIO_SEEK_TYPE, bytes, size);
   audio_transfer_set_buffer_ptr(legacy, AUDIO_SEEK_TYPE, bytes, size);
   CHECK(audio_transfer_start(seek, AUDIO_SEEK_TYPE));
   CHECK(audio_transfer_start(legacy, AUDIO_SEEK_TYPE));
   CHECK(audio_transfer_info(seek, AUDIO_SEEK_TYPE, &channels, &rate, &total));
   CHECK(channels && channels <= 2 && rate == 48000 && total > 64);
   CHECK(total <= 1000000);
   if (s16)
   {
      CHECK((linear16 = (int16_t*)malloc((size_t)total * channels * sizeof(int16_t))) != NULL);
      CHECK(read_exact(legacy, linear16, (size_t)total, channels, 1));
   }
   else
   {
      CHECK((linear = (float*)malloc((size_t)total * channels * sizeof(float))) != NULL);
      CHECK(read_exact(legacy, linear, (size_t)total, channels, 0));
   }

   for (probe = 0; probe < 32; probe++)
   {
      target = probe == 31 ? total - 64
         : (uint64_t)probe * (total - 64) / 32;
      CHECK(audio_transfer_seek_begin_format(seek, AUDIO_SEEK_TYPE, 70000, s16 != 0));
      CHECK(audio_transfer_seek_begin_format(seek, AUDIO_SEEK_TYPE, target, s16 != 0));
      CHECK(!audio_transfer_seek_begin(seek, AUDIO_SEEK_TYPE, (uint64_t)-1));
      audio_transfer_set_avail(seek, AUDIO_SEEK_TYPE, 1);
      if (target) CHECK(audio_transfer_seek_step(seek, AUDIO_SEEK_TYPE) == 0);
      audio_transfer_set_avail(seek, AUDIO_SEEK_TYPE, size);
      ret = 0;
      for (steps = 0; steps < 10000 && ret == 0; steps++)
      {
         audio_transfer_set_avail(seek, AUDIO_SEEK_TYPE, size);
         ret = audio_transfer_seek_step(seek, AUDIO_SEEK_TYPE);
      }
      if (ret != 1)
         fprintf(stderr, "%s: probe=%d target=%lu steps=%d result=%d\n",
               path, probe, (unsigned long)target, steps, ret);
      CHECK(ret == 1);
      CHECK(audio_transfer_seek(legacy, AUDIO_SEEK_TYPE, target));
      if (s16)
      {
         CHECK(read_exact(seek, a16, 64, channels, 1));
         CHECK(read_exact(legacy, b16, 64, channels, 1));
         for (i = 0; i < (int)(64 * channels); i++)
         {
            CHECK(a16[i] == b16[i]);
            CHECK(abs((int)a16[i] - (int)linear16[(size_t)target * channels + i]) <= 33);
         }
      }
      else
      {
         CHECK(read_exact(seek, a, 64, channels, 0));
         CHECK(read_exact(legacy, b, 64, channels, 0));
         for (i = 0; i < (int)(64 * channels); i++)
         {
            CHECK(fabs(a[i] - b[i]) < (AUDIO_SEEK_TYPE == AUDIO_TYPE_OPUS ? 0.0000001 : 0.0001));
            CHECK(fabs(a[i] - linear[(size_t)target * channels + i]) < 0.001);
         }
      }
   }
   printf("%s: 32 bounded %s seeks match legacy %s preroll\n", path, AUDIO_SEEK_NAME, s16 ? "s16" : "float");
   status = 0;
done:
   if (f) fclose(f);
   audio_transfer_free(seek, AUDIO_SEEK_TYPE); audio_transfer_free(legacy, AUDIO_SEEK_TYPE);
   free(linear); free(linear16); free(bytes); return status;
}
int main(int argc, char **argv)
{
   int i;
   if (argc < 2) return 2;
   for (i = 1; i < argc; i++) if (check_file(argv[i], 0) || check_file(argv[i], 1)) return 1;
   return 0;
}
