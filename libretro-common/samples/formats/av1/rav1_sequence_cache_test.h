/* SPDX-License-Identifier: MIT */
/* Included by the MP4 and WebM reader fixtures after their backend helpers. */
#ifdef SEQUENCE_CACHE_WEBM
#define CACHE_STREAM rwebm_video_stream_t
#define CACHE_OPEN() rwebm_video_stream_open_hw_avail(bytes, used, used, NULL)
#define CACHE_SET_HW rwebm_video_stream_set_hw
#define CACHE_NEXT rwebm_video_stream_next
#define CACHE_FRAME rwebm_video_stream_hw_frame
#define CACHE_AVAIL rwebm_video_stream_set_avail
#define CACHE_SEEK rwebm_video_stream_seek_begin_ns
#define CACHE_STEP rwebm_video_stream_seek_step
#define CACHE_CLOSE rwebm_video_stream_close
#else
#define CACHE_STREAM rmp4_video_stream_t
#define CACHE_OPEN()                                                           \
   rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL)
#define CACHE_SET_HW rmp4_video_stream_set_hw
#define CACHE_NEXT rmp4_video_stream_next
#define CACHE_FRAME rmp4_video_stream_hw_frame
#define CACHE_AVAIL rmp4_video_stream_set_avail
#define CACHE_SEEK rmp4_video_stream_seek_begin_ns
#define CACHE_STEP rmp4_video_stream_seek_step
#define CACHE_CLOSE rmp4_video_stream_close
#endif

static void test_sequence_reopen(int inband)
{
   CACHE_STREAM *reader;
   static rav1_sequence_t original, alternate;
   rav1_obu_t obu;
   video_hw_frame_t frame;
   const uint32_t *marker = NULL;
   uint8_t candidate[sizeof(config) - 4];
   size_t offset = 0, byte, changed = 0;
   unsigned bit, found = 0, i;
   int ret = 0;
   CHECK(rav1_obu_next(&obu, config + 4, sizeof(config) - 4, &offset, true)
         == 1);
   CHECK(rav1_sequence_parse(&original, obu.payload, obu.payload_size));
   for (byte = 2; byte < sizeof(candidate) && !found; byte++)
   {
      for (bit = 0; bit < 8; bit++)
      {
         memcpy(candidate, config + 4, sizeof(candidate));
         candidate[byte] ^= (uint8_t)(1u << bit);
         if (!rav1_sequence_parse(
                   &alternate, candidate + 2, sizeof(candidate) - 2))
            continue;
         alternate.enable_filter_intra ^= 1;
         if (!memcmp(&alternate, &original, sizeof(original)))
         {
            changed = byte;
            found   = 1u << bit;
            break;
         }
      }
   }
   CHECK(found);
   if (!found)
      return;
#ifdef SEQUENCE_CACHE_WEBM
   make_webm_clip(inband, 0, 0, 0, 3, 0, 4);
#else
   (void)inband;
   make_mp4_clip(1, 4);
#endif
   bytes[(inband ? media_at : config_at + 4) + changed] ^= (uint8_t)found;
   reader = CACHE_OPEN();
   CHECK(reader);
   if (!reader)
      return;
   if (!CACHE_SET_HW(reader, 1))
   {
      CHECK(0);
      CACHE_CLOSE(reader);
      return;
   }
   CHECK(admitted_sequence.enable_filter_intra != original.enable_filter_intra);
   for (i = 0; i < 512 && !marker; i++)
      marker = CACHE_NEXT(reader, NULL);
   CHECK(marker && CACHE_FRAME(reader, &frame));
   if (marker)
      video_hw_frame_abandon(&frame);
   CHECK(!memcmp(&admitted_sequence, &original, sizeof(original)));
   /* Reopen while initial packet bytes are temporarily unavailable. */
   CACHE_AVAIL(reader, media_at);
   CHECK(CACHE_SEEK(reader, 40000000));
   CHECK(!memcmp(&admitted_sequence, &original, sizeof(original)));
   CACHE_AVAIL(reader, used);
   for (i = 0; i < 512 && !ret; i++)
      ret = CACHE_STEP(reader, NULL);
   CHECK(ret == 1);
   marker = CACHE_NEXT(reader, NULL);
   CHECK(marker);
   if (marker)
   {
      ret = CACHE_FRAME(reader, &frame);
      CHECK(ret);
      if (ret)
         video_hw_frame_abandon(&frame);
   }
   CHECK(!memcmp(&admitted_sequence, &original, sizeof(original)));
   CACHE_CLOSE(reader);
   CHECK(opens == closes && exports == abandons);
}

#undef CACHE_STREAM
#undef CACHE_OPEN
#undef CACHE_SET_HW
#undef CACHE_NEXT
#undef CACHE_FRAME
#undef CACHE_AVAIL
#undef CACHE_SEEK
#undef CACHE_STEP
#undef CACHE_CLOSE
