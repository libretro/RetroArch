/* Generated pictures stay unchanged: enable an unused PPS capability. */
#ifndef HEVC_DEPENDENT_FIXTURE_H
#define HEVC_DEPENDENT_FIXTURE_H
#include <stdlib.h>
#include <string.h>
#include <retro_inline.h>
#include <formats/rmp4.h>

static INLINE uint8_t *hevc_dependent_fixture(const uint8_t *source, size_t size)
{
   rmp4_t *mp4 = rmp4_open_memory(source, size);
   uint8_t *copy = NULL;
   int track;
   if (!mp4) return NULL;
   for (track = 0; track < rmp4_num_tracks(mp4); track++)
   {
      const rmp4_track *t = rmp4_get_track(mp4, track);
      const uint8_t *config;
      size_t at, length, offset;
      unsigned array, arrays;
      if (!t || t->codec != RMP4_CODEC_H265) continue;
      config = t->codec_private;
      length = t->codec_private_size;
      if (!config || length < 23 || length > size) break;
      /* The demuxer owns a separate copy of the configuration. */
      for (offset = 0; offset <= size - length; offset++)
         if (!memcmp(source + offset, config, length)) break;
      if (offset > size - length) break;
      arrays = config[22];
      at = 23;
      for (array = 0; array < arrays && length - at >= 3; array++)
      {
         unsigned type = config[at++] & 63;
         unsigned n, count = ((unsigned)config[at] << 8) | config[at + 1];
         at += 2;
         for (n = 0; n < count && length - at >= 2; n++)
         {
            unsigned bytes = ((unsigned)config[at] << 8) | config[at + 1];
            at += 2;
            if (bytes > length - at) goto done;
            /* Fixture PPS/SPS ids are both zero (two leading ue(0)s).
             * The next bit is dependent_slice_segments_enabled_flag.
             * Its first slice is independent, so no slice bytes change. */
            if (type == 34 && bytes >= 3 && (config[at + 2] & 0xe0) == 0xc0)
            {
               if ((copy = (uint8_t*)malloc(size)))
               {
                  memcpy(copy, source, size);
                  copy[offset + at + 2] |= 0x20;
               }
               goto done;
            }
            at += bytes;
         }
      }
      break;
   }
done:
   rmp4_close(mp4);
   return copy;
}
#endif
