/* Exercise the actual audio helpers and their allocation-failure contracts. */
#define record_ffmpeg audio_growth_driver
#include "../../../record/drivers/record_ffmpeg.c"
#undef record_ffmpeg

static unsigned fail_at, realloc_calls, pair_calls;
void *__real_av_realloc(void *ptr, size_t size);
void *__wrap_av_realloc(void *ptr, size_t size)
{
   if (fail_at && ++realloc_calls == fail_at)
      return NULL;
   return __real_av_realloc(ptr, size);
}
static void pair_copy(void *data, struct resampler_data *info)
{
   (void)data;
   pair_calls++;
   memcpy(info->data_out, info->data_in, info->input_frames * 2 * sizeof(float));
   info->output_frames = info->input_frames;
}
static void release_buffers(ffmpeg_t *ff)
{
   av_free(ff->audio.float_conv);
   av_free(ff->audio.resample_out);
   av_free(ff->audio.fixed_conv);
   av_free(ff->audio.planar_buf);
   av_free(ff->audio.buffer);
   av_free(ff->audio.pair_in);
   av_free(ff->audio.pair_out);
   if (ff->audio.resampler_int16)
      sinc_resampler_int16_free(ff->audio.resampler_int16);
   av_frame_free(&ff->audio.frame);
   avcodec_free_context(&ff->audio.codec);
   free(ff);
}
static int conversion_growth(unsigned stage, unsigned channels, bool integer)
{
   ffmpeg_t *ff = (ffmpeg_t*)calloc(1, sizeof(*ff));
   struct record_audio_data aud = {0};
   retro_resampler_t resampler = {0};
   int16_t input[64];
   unsigned i;
   bool wider = channels != 2;
   int failed = 0;
   if (!ff) return 1;
   for (i = 0; i < 64; i++) input[i] = (int16_t)(100 + i);
   ff->params.channels = channels;
   ff->audio.ratio = 1.0;
   ff->audio.use_float = !integer;
   if (wider)
   {
      resampler.process = pair_copy;
      ff->audio.resampler = &resampler;
   }
   if (integer)
   {
      ff->audio.resampler_int16 = sinc_resampler_int16_init(1.0, SINC_INT16_QUALITY_NORMAL);
      if (!ff->audio.resampler_int16) { release_buffers(ff); return 1; }
   }
   aud.data = input; aud.frames = 8;
   fail_at = stage; realloc_calls = pair_calls = 0;
   if (ffmpeg_push_audio_thread(ff, &aud, true) || !ff->encode_failed
         || realloc_calls != stage || (!wider && aud.data != input) || aud.frames != 8 || pair_calls)
      failed = 1;
   fail_at = 0;
   aud.data = input; aud.frames = 8;
   if (!ffmpeg_audio_resample(ff, &aud)) failed = 1;
   if (integer)
   {
      if (aud.data != ff->audio.fixed_conv || ff->audio.fixed_conv_frames < 24)
         failed = 1;
   }
   else
   {
      const float *output = (const float*)aud.data;
      if (aud.frames != 8 || (wider && pair_calls != (channels + 1) / 2)
            || !ff->audio.float_conv || ff->audio.float_conv_frames < 8
            || !ff->audio.resample_out || ff->audio.resample_out_frames < 24
            || !ff->audio.fixed_conv || ff->audio.fixed_conv_frames < 24)
         failed = 1;
      else for (i = 0; i < 8 * channels; i++)
         if (output[i] != input[i] / 32768.0f) failed = 1;
   }
   release_buffers(ff);
   if (failed) fprintf(stderr, "audio growth stage %u wider=%d integer=%d failed\n", stage, wider, integer);
   return failed;
}
static int planar_growth(unsigned channels, bool floating)
{
   ffmpeg_t *ff = (ffmpeg_t*)calloc(1, sizeof(*ff));
   unsigned i, ch;
   int failed = 0;
   if (!ff) return 1;
   ff->params.channels = channels;
   ff->audio.is_planar = true;
   ff->audio.use_float = floating;
   ff->audio.sample_size = floating ? sizeof(float) : sizeof(int16_t);
   ff->audio.frames_in_buffer = 8;
   ff->audio.frame = av_frame_alloc();
   ff->audio.codec = avcodec_alloc_context3(NULL);
   ff->audio.buffer = (uint8_t*)av_malloc(8 * channels * ff->audio.sample_size);
   if (!ff->audio.frame || !ff->audio.codec || !ff->audio.buffer)
   { release_buffers(ff); return 1; }
#if HAVE_CH_LAYOUT
   av_channel_layout_default(&ff->audio.codec->ch_layout, (int)channels);
#else
   ff->audio.codec->channels = channels;
#endif
   for (i = 0; i < 8 * channels; i++)
      if (floating)
         ((float*)ff->audio.buffer)[i] = (i + 1) / 64.0f;
      else
         ((int16_t*)ff->audio.buffer)[i] = (int16_t)i;
   fail_at = 1; realloc_calls = 0;
   if (encode_audio(ff, false) || !ff->encode_failed || ff->audio.planar_buf
         || ff->audio.planar_buf_frames) failed = 1;
   fail_at = 0;
   if (!planarize_audio(ff)) failed = 1;
   else
      for (ch = 0; ch < channels; ch++)
         for (i = 0; i < 8; i++)
         {
            size_t src = i * channels + ch;
            size_t dst = ch * 8 + i;
            if (floating)
            {
               if (((float*)ff->audio.planar_buf)[dst] != ((float*)ff->audio.buffer)[src]) failed = 1;
            }
            else if (((int16_t*)ff->audio.planar_buf)[dst] != ((int16_t*)ff->audio.buffer)[src]) failed = 1;
         }
   release_buffers(ff);
   if (failed) fprintf(stderr, "planar growth/order channels=%u float=%d failed\n", channels, floating);
   return failed;
}
int record_audio_growth_test_run(void)
{
   unsigned stage;
   int failed = 0;
   for (stage = 1; stage <= 3; stage++) failed |= conversion_growth(stage, 2, false);
   for (stage = 4; stage <= 5; stage++) failed |= conversion_growth(stage, 4, false);
   for (stage = 4; stage <= 5; stage++)
   {
      failed |= conversion_growth(stage, 1, false);
      failed |= conversion_growth(stage, 3, false);
      failed |= conversion_growth(stage, 6, false);
   }
   failed |= conversion_growth(1, 2, true);
   failed |= planar_growth(1, false);
   failed |= planar_growth(2, false);
   failed |= planar_growth(3, false);
   failed |= planar_growth(6, false);
   failed |= planar_growth(1, true);
   failed |= planar_growth(2, true);
   failed |= planar_growth(3, true);
   failed |= planar_growth(6, true);
   fail_at = 0;
   if (!failed) puts("[pass] audio buffer failures propagate and retries preserve samples");
   return failed;
}
