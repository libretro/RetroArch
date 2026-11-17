/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/image.h>
#include "../../../gfx/gfx_anim_preview.h"

static void fixture(const char *path, enum image_type_enum type,
      enum video_audio_codec codec, unsigned count)
{
   FILE *file = fopen(path, "rb");
   long length;
   uint8_t *data;
   void *stream, *reference;
   gfx_anim_preview_t *preview;
   video_audio_track_t track, empty;
   const uint32_t *pixels, *expected;
   unsigned i, width = 0, height = 0;
   int duration = 0, expected_duration = 0;
   assert(file && !fseek(file, 0, SEEK_END));
   length = ftell(file);
   assert(length > 0 && !fseek(file, 0, SEEK_SET));
   data = (uint8_t*)malloc((size_t)length);
   assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
   assert(!fclose(file));
   stream = image_transfer_anim_stream_new_avail(data, (size_t)length,
         (size_t)length, type, NULL, NULL, NULL);
   reference = image_transfer_anim_stream_new_avail(data, (size_t)length,
         (size_t)length, type, NULL, NULL, NULL);
   assert(stream && reference);
   preview = gfx_anim_preview_wrap(stream, type, NULL, data, (size_t)length, false, path);
   assert(preview && preview->audio_track_count == count);
   for (i = 0; i < count; i++)
   {
      static const enum video_audio_codec mixed[] = {
         VIDEO_AUDIO_OPUS, VIDEO_AUDIO_VORBIS, VIDEO_AUDIO_AAC, VIDEO_AUDIO_FLAC
      };
      assert(image_transfer_anim_stream_audio_track(stream, type, i, &track));
      assert(track.index == (int)i + 1 && track.channels == 1
            && track.sample_rate == 48000 && track.codec_id && *track.codec_id);
      assert(track.codec == (codec ? codec : mixed[i]));
      assert(preview->audio_tracks[i].info.index == track.index
            && preview->audio_tracks[i].info.codec == track.codec);
      if (count == 2)
      {
         const char *language = i ? "jpn" : "eng";
         assert(track.language);
         assert(!strcmp(preview->audio_tracks[i].language, language)
               && preview->audio_tracks[i].info.language
                  == (const uint8_t*)preview->audio_tracks[i].language);
         if (type == IMAGE_TYPE_MP4)
         {
            unsigned packed = ((unsigned)track.language[0] << 8) | track.language[1];
            assert(track.language_size == 2
                  && track.language_kind == VIDEO_AUDIO_LANGUAGE_MP4_PACKED);
            assert(((packed >> 10) & 31) + 96 == (unsigned)language[0]
                  && ((packed >> 5) & 31) + 96 == (unsigned)language[1]
                  && (packed & 31) + 96 == (unsigned)language[2]);
         }
         else
            assert(track.language_kind == VIDEO_AUDIO_LANGUAGE_TEXT
                  && track.language_size == 3 && !memcmp(track.language, language, 3));
      }
   }
   if (type == IMAGE_TYPE_MP4)
   {
      gfx_anim_preview_t *invalid;
      size_t offset;
      uint8_t saved[2];
      assert(image_transfer_anim_stream_audio_track(stream, type, 0, &track));
      assert((uintptr_t)track.language >= (uintptr_t)data
            && (uintptr_t)track.language - (uintptr_t)data <= (size_t)length - 2);
      offset = (size_t)((uintptr_t)track.language - (uintptr_t)data);
      memcpy(saved, data + offset, 2);
      memset(data + offset, 0, 2);
      invalid = gfx_anim_preview_wrap(stream, type, NULL, data, (size_t)length, false, path);
      assert(invalid && !*invalid->audio_tracks[0].language);
      /* The earlier snapshot survives source-page changes. */
      assert(!strcmp(preview->audio_tracks[0].language, "eng"));
      gfx_anim_preview_release(invalid);
      memcpy(data + offset, saved, 2);
   }
   memset(&empty, 0, sizeof(empty));
   memset(&track, 0x55, sizeof(track));
   assert(!image_transfer_anim_stream_audio_track(stream, type, count, &track));
   assert(!memcmp(&track, &empty, sizeof(track)));
   assert(!image_transfer_anim_stream_audio_track(stream, type, ~0U, &track));
   assert(!image_transfer_anim_stream_audio_track(NULL, type, 0, &track));
   assert(!image_transfer_anim_stream_audio_track(stream, IMAGE_TYPE_PNG, 0, &track));
   assert(!image_transfer_anim_stream_audio_track(stream, type, 0, NULL));
   image_transfer_anim_stream_get_info(stream, type, &width, &height, NULL, NULL);
   assert(width == 16 && height == 16);
   pixels = image_transfer_anim_stream_next(stream, type, &duration);
   expected = image_transfer_anim_stream_next(reference, type, &expected_duration);
   assert(pixels && expected && duration == expected_duration
         && !memcmp(pixels, expected, width * height * sizeof(*pixels)));
   assert(image_transfer_anim_stream_audio_track(stream, type, 1, &track)
         && track.index == 2);
   gfx_anim_preview_release(preview);
   image_transfer_anim_stream_free(stream, type);
   image_transfer_anim_stream_free(reference, type);
   free(data);
   printf("PASS borrowed audio metadata, demux indices and unchanged video cursor: %s\n", path);
}

int main(void)
{
   const char *base = "../../../libretro-common/samples/formats/audio_transfer_tracks/fixtures/";
   char path[256];
   static const char *names[] = {"aac-mp4.bin", "aac-webm.bin", "opus.bin",
      "vorbis.bin", "flac.bin", "mixed.bin"};
   static const enum video_audio_codec codecs[] = {VIDEO_AUDIO_AAC,
      VIDEO_AUDIO_AAC, VIDEO_AUDIO_OPUS, VIDEO_AUDIO_VORBIS,
      VIDEO_AUDIO_FLAC, VIDEO_AUDIO_UNKNOWN};
   unsigned i;
   for (i = 0; i < sizeof(names) / sizeof(*names); i++)
   {
      assert(strlen(base) + strlen(names[i]) < sizeof(path));
      strcpy(path, base);
      strcat(path, names[i]);
      fixture(path, i ? IMAGE_TYPE_WEBM : IMAGE_TYPE_MP4, codecs[i], i == 5 ? 4 : 2);
   }
   return 0;
}
