/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_mixer.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <audio/audio_mixer.h>
#include <audio/audio_resampler.h>

#include <formats/rwav.h>
#include <memalign.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_STB_VORBIS
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_CRT

#include <stb/stb_vorbis.h>
#endif

#ifdef HAVE_DR_FLAC
#define DR_FLAC_IMPLEMENTATION
#include <dr/dr_flac.h>
#endif

#ifdef HAVE_DR_MP3
#define DR_MP3_IMPLEMENTATION
#include <retro_assert.h>
#define DRMP3_ASSERT(expression) retro_assert(expression)
#include <dr/dr_mp3.h>
#endif

#ifdef HAVE_IBXM
#include <ibxm/ibxm.h>
#endif

#define AUDIO_MIXER_MAX_VOICES      8
#define AUDIO_MIXER_TEMP_BUFFER 8192

struct audio_mixer_sound
{
   enum audio_mixer_type type;

   union
   {
      struct
      {
         /* wav */
         unsigned frames;
         const float* pcm;
      } wav;

#ifdef HAVE_STB_VORBIS
      struct
      {
         /* ogg */
         unsigned size;
         const void* data;
      } ogg;
#endif

#ifdef HAVE_DR_FLAC
      struct
      {
          /* flac */
         unsigned size;
         const void* data;
      } flac;
#endif

#ifdef HAVE_DR_MP3
      struct
      {
          /* mp */
         unsigned size;
         const void* data;
      } mp3;
#endif

#ifdef HAVE_IBXM
      struct
      {
         /* mod/s3m/xm */
         unsigned size;
         const void* data;
      } mod;
#endif
   } types;
};

struct audio_mixer_voice
{
   bool     repeat;
   unsigned type;
   float    volume;
   audio_mixer_sound_t *sound;
   audio_mixer_stop_cb_t stop_cb;

   union
   {
      struct
      {
         unsigned position;
      } wav;

#ifdef HAVE_STB_VORBIS
      struct
      {
         unsigned    position;
         unsigned    samples;
         unsigned    buf_samples;
         float*      buffer;
         float       ratio;
         stb_vorbis *stream;
         void       *resampler_data;
         const retro_resampler_t *resampler;
      } ogg;
#endif

#ifdef HAVE_DR_FLAC
      struct
      {
         unsigned    position;
         unsigned    samples;
         unsigned    buf_samples;
         float*      buffer;
         float       ratio;
         drflac      *stream;
         void        *resampler_data;
         const retro_resampler_t *resampler;
      } flac;
#endif

#ifdef HAVE_DR_MP3
      struct
      {
         unsigned    position;
         unsigned    samples;
         unsigned    buf_samples;
         float*      buffer;
         float       ratio;
         drmp3       stream;
         void        *resampler_data;
         const retro_resampler_t *resampler;
      } mp3;
#endif

#ifdef HAVE_IBXM
      struct
      {
         unsigned          position;
         unsigned          samples;
         unsigned          buf_samples;
         int*              buffer;
         struct replay*    stream;
         struct module*    module;
      } mod;
#endif
   } types;
};

static struct audio_mixer_voice s_voices[AUDIO_MIXER_MAX_VOICES] = {{0}};
static unsigned s_rate = 0;

static bool wav2float(const rwav_t* wav, float** pcm, size_t samples_out)
{
   size_t i;
   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes */
   float *f           = (float*)memalign_alloc(16,
         ((samples_out + 15) & ~15) * sizeof(float));

   if (!f)
      return false;

   *pcm = f;

   if (wav->bitspersample == 8)
   {
      float sample      = 0.0f;
      const uint8_t *u8 = (const uint8_t*)wav->samples;

      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            sample = (float)*u8++ / 255.0f;
            sample = sample * 2.0f - 1.0f;
            *f++   = sample;
            *f++   = sample;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            sample = (float)*u8++ / 255.0f;
            sample = sample * 2.0f - 1.0f;
            *f++   = sample;
            sample = (float)*u8++ / 255.0f;
            sample = sample * 2.0f - 1.0f;
            *f++   = sample;
         }
      }
   }
   else
   {
      /* TODO/FIXME note to leiradel - can we use audio/conversion/s16_to_float
       * functions here? */

      float sample       = 0.0f;
      const int16_t *s16 = (const int16_t*)wav->samples;

      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            sample = (float)((int)*s16++ + 32768) / 65535.0f;
            sample = sample * 2.0f - 1.0f;
            *f++   = sample;
            *f++   = sample;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            sample = (float)((int)*s16++ + 32768) / 65535.0f;
            sample = sample * 2.0f - 1.0f;
            *f++   = sample;
            sample = (float)((int)*s16++ + 32768) / 65535.0f;
            sample = sample * 2.0f - 1.0f;
            *f++   = sample;
         }
      }
   }

   return true;
}

static bool one_shot_resample(const float* in, size_t samples_in,
      unsigned rate, float** out, size_t* samples_out)
{
   struct resampler_data info;
   void* data                         = NULL;
   const retro_resampler_t* resampler = NULL;
   float ratio                        = (double)s_rate / (double)rate;

   if (!retro_resampler_realloc(&data, &resampler, NULL,
            RESAMPLER_QUALITY_DONTCARE, ratio))
      return false;

   /*
    * Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes. We
    * add four more samples in the formula below just as safeguard, because
    * resampler->process sometimes reports more output samples than the
    * formula below calculates. Ideally, audio resamplers should have a
    * function to return the number of samples they will output given a
    * count of input samples.
    */
   *samples_out                       = samples_in * ratio + 4;
   *out                               = (float*)memalign_alloc(16,
         ((*samples_out + 15) & ~15) * sizeof(float));

   if (*out == NULL)
      return false;

   info.data_in                       = in;
   info.data_out                      = *out;
   info.input_frames                  = samples_in / 2;
   info.output_frames                 = 0;
   info.ratio                         = ratio;

   resampler->process(data, &info);
   resampler->free(data);
   return true;
}

void audio_mixer_init(unsigned rate)
{
   unsigned i;

   s_rate = rate;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++)
      s_voices[i].type = AUDIO_MIXER_TYPE_NONE;
}

void audio_mixer_done(void)
{
   unsigned i;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++)
      s_voices[i].type = AUDIO_MIXER_TYPE_NONE;
}

audio_mixer_sound_t* audio_mixer_load_wav(void *buffer, int32_t size)
{
   /* WAV data */
   rwav_t wav;
   /* WAV samples converted to float */
   float* pcm                 = NULL;
   size_t samples             = 0;
   /* Result */
   audio_mixer_sound_t* sound = NULL;
   enum rwav_state rwav_ret   = rwav_load(&wav, buffer, size);

   if (rwav_ret != RWAV_ITERATE_DONE)
      return NULL;

   samples       = wav.numsamples * 2;

   if (!wav2float(&wav, &pcm, samples))
      return NULL;

   if (wav.samplerate != s_rate)
   {
      float* resampled           = NULL;

      if (!one_shot_resample(pcm, samples,
               wav.samplerate, &resampled, &samples))
         return NULL;

      memalign_free((void*)pcm);
      pcm = resampled;
   }

   sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
   {
      memalign_free((void*)pcm);
      return NULL;
   }

   sound->type             = AUDIO_MIXER_TYPE_WAV;
   sound->types.wav.frames = (unsigned)(samples / 2);
   sound->types.wav.pcm    = pcm;

   rwav_free(&wav);

   return sound;
}

audio_mixer_sound_t* audio_mixer_load_ogg(void *buffer, int32_t size)
{
#ifdef HAVE_STB_VORBIS
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_OGG;
   sound->types.ogg.size = size;
   sound->types.ogg.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_flac(void *buffer, int32_t size)
{
#ifdef HAVE_DR_FLAC
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_FLAC;
   sound->types.flac.size = size;
   sound->types.flac.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_mp3(void *buffer, int32_t size)
{
#ifdef HAVE_DR_MP3
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_MP3;
   sound->types.mp3.size = size;
   sound->types.mp3.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_mod(void *buffer, int32_t size)
{
#ifdef HAVE_IBXM
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_MOD;
   sound->types.mod.size = size;
   sound->types.mod.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

void audio_mixer_destroy(audio_mixer_sound_t* sound)
{
   void *handle = NULL;
   if (!sound)
      return;

   switch (sound->type)
   {
      case AUDIO_MIXER_TYPE_WAV:
         handle = (void*)sound->types.wav.pcm;
         if (handle)
            memalign_free(handle);
         break;
      case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_STB_VORBIS
         handle = (void*)sound->types.ogg.data;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
         handle = (void*)sound->types.mod.data;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_DR_FLAC
         handle = (void*)sound->types.flac.data;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_DR_MP3
         handle = (void*)sound->types.mp3.data;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_NONE:
         break;
   }

   free(sound);
}

static bool audio_mixer_play_wav(audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice, bool repeat, float volume,
      audio_mixer_stop_cb_t stop_cb)
{
   voice->types.wav.position = 0;
   return true;
}

#ifdef HAVE_STB_VORBIS
static bool audio_mixer_play_ogg(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      audio_mixer_stop_cb_t stop_cb)
{
   stb_vorbis_info info;
   int res                         = 0;
   float ratio                     = 1.0f;
   unsigned samples                = 0;
   void *ogg_buffer                = NULL;
   void *resampler_data            = NULL;
   const retro_resampler_t* resamp = NULL;
   stb_vorbis *stb_vorbis          = stb_vorbis_open_memory(
         (const unsigned char*)sound->types.ogg.data,
         sound->types.ogg.size, &res, NULL);

   if (!stb_vorbis)
      return false;

   info                    = stb_vorbis_get_info(stb_vorbis);

   if (info.sample_rate != s_rate)
   {
      ratio = (double)s_rate / (double)info.sample_rate;

      if (!retro_resampler_realloc(&resampler_data,
               &resamp, NULL, RESAMPLER_QUALITY_DONTCARE,
               ratio))
         goto error;
   }

   samples                         = (unsigned)(AUDIO_MIXER_TEMP_BUFFER * ratio);
   ogg_buffer                      = (float*)memalign_alloc(16,
         ((samples + 15) & ~15) * sizeof(float));

   if (!ogg_buffer)
   {
      if (resamp && resampler_data)
         resamp->free(resampler_data);
      goto error;
   }

   /* "system" menu sounds may reuse the same voice without freeing anything first, so do that here if needed */
   if (voice->types.ogg.stream)
      stb_vorbis_close(voice->types.ogg.stream);
   if (voice->types.ogg.resampler && voice->types.ogg.resampler_data)
      voice->types.ogg.resampler->free(voice->types.ogg.resampler_data);
   if (voice->types.ogg.buffer)
      memalign_free(voice->types.ogg.buffer);

   voice->types.ogg.resampler      = resamp;
   voice->types.ogg.resampler_data = resampler_data;
   voice->types.ogg.buffer         = (float*)ogg_buffer;
   voice->types.ogg.buf_samples    = samples;
   voice->types.ogg.ratio          = ratio;
   voice->types.ogg.stream         = stb_vorbis;
   voice->types.ogg.position       = 0;
   voice->types.ogg.samples        = 0;

   return true;

error:
   stb_vorbis_close(stb_vorbis);
   return false;
}
#endif

#ifdef HAVE_IBXM
static bool audio_mixer_play_mod(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      audio_mixer_stop_cb_t stop_cb)
{
   struct data data;
   char message[64];
   int buf_samples               = 0;
   int samples                   = 0;
   void *mod_buffer              = NULL;
   struct module* module         = NULL;
   struct replay* replay         = NULL;

   data.buffer                   = (char*)sound->types.mod.data;
   data.length                   = sound->types.mod.size;
   module                        = module_load(&data, message);

   if (!module)
   {
      printf("audio_mixer_play_mod module_load() failed with error: %s\n", message);
      goto error;
   }

   if (voice->types.mod.module)
      dispose_module(voice->types.mod.module);

   voice->types.mod.module = module;

   replay = new_replay(module, s_rate, 1);

   if (!replay)
   {
      printf("audio_mixer_play_mod new_replay() failed\n");
      goto error;
   }

   buf_samples = calculate_mix_buf_len(s_rate);
   mod_buffer  = memalign_alloc(16, ((buf_samples + 15) & ~15) * sizeof(int));

   if (!mod_buffer)
   {
      printf("audio_mixer_play_mod cannot allocate mod_buffer !\n");
      goto error;
   }

   samples = replay_calculate_duration(replay);

   if (!samples)
   {
      printf("audio_mixer_play_mod cannot retrieve duration !\n");
      goto error;
   }

   /* FIXME: stopping and then starting a mod stream will crash here in dispose_replay (ASAN says struct replay is misaligned?) */
   if (voice->types.mod.stream)
      dispose_replay(voice->types.mod.stream);
   if (voice->types.mod.buffer)
      memalign_free(voice->types.mod.buffer);

   voice->types.mod.buffer         = (int*)mod_buffer;
   voice->types.mod.buf_samples    = buf_samples;
   voice->types.mod.stream         = replay;
   voice->types.mod.position       = 0;
   voice->types.mod.samples        = 0; /* samples; */

   return true;

error:
   if (mod_buffer)
      memalign_free(mod_buffer);
   if (module)
      dispose_module(module);
   return false;

}
#endif

#ifdef HAVE_DR_FLAC
static bool audio_mixer_play_flac(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      audio_mixer_stop_cb_t stop_cb)
{
   float ratio                     = 1.0f;
   unsigned samples                = 0;
   void *flac_buffer                = NULL;
   void *resampler_data            = NULL;
   const retro_resampler_t* resamp = NULL;
   drflac *dr_flac          = drflac_open_memory((const unsigned char*)sound->types.flac.data,sound->types.flac.size);

   if (!dr_flac)
      return false;
   if (dr_flac->sampleRate != s_rate)
   {
      ratio = (double)s_rate / (double)(dr_flac->sampleRate);

      if (!retro_resampler_realloc(&resampler_data,
               &resamp, NULL, RESAMPLER_QUALITY_DONTCARE,
               ratio))
         goto error;
   }

   samples                         = (unsigned)(AUDIO_MIXER_TEMP_BUFFER * ratio);
   flac_buffer                      = (float*)memalign_alloc(16,
         ((samples + 15) & ~15) * sizeof(float));

   if (!flac_buffer)
   {
      if (resamp && resamp->free)
         resamp->free(resampler_data);
      goto error;
   }

   if (voice->types.flac.stream)
      drflac_close(voice->types.flac.stream);
   if (voice->types.flac.resampler && voice->types.flac.resampler_data)
      voice->types.flac.resampler->free(voice->types.flac.resampler_data);
   if (voice->types.flac.buffer)
      memalign_free(voice->types.flac.buffer);

   voice->types.flac.resampler      = resamp;
   voice->types.flac.resampler_data = resampler_data;
   voice->types.flac.buffer         = (float*)flac_buffer;
   voice->types.flac.buf_samples    = samples;
   voice->types.flac.ratio          = ratio;
   voice->types.flac.stream         = dr_flac;
   voice->types.flac.position       = 0;
   voice->types.flac.samples        = 0;

   return true;

error:
   drflac_close(dr_flac);
   return false;
}
#endif

#ifdef HAVE_DR_MP3
static bool audio_mixer_play_mp3(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      audio_mixer_stop_cb_t stop_cb)
{
   float ratio                     = 1.0f;
   unsigned samples                = 0;
   void *mp3_buffer                = NULL;
   void *resampler_data            = NULL;
   const retro_resampler_t* resamp = NULL;
   bool res;

   if (voice->types.mp3.stream.pData)
   {
      drmp3_uninit(&voice->types.mp3.stream);
      memset(&voice->types.mp3.stream, 0, sizeof(voice->types.mp3.stream));
   }

   res = drmp3_init_memory(&voice->types.mp3.stream, (const unsigned char*)sound->types.mp3.data, sound->types.mp3.size, NULL);

   if (!res)
      return false;

   if (voice->types.mp3.stream.sampleRate != s_rate)
   {
      ratio = (double)s_rate / (double)(voice->types.mp3.stream.sampleRate);

      if (!retro_resampler_realloc(&resampler_data,
               &resamp, NULL, RESAMPLER_QUALITY_DONTCARE,
               ratio))
         goto error;
   }

   samples                         = (unsigned)(AUDIO_MIXER_TEMP_BUFFER * ratio);
   mp3_buffer                      = (float*)memalign_alloc(16,
         ((samples + 15) & ~15) * sizeof(float));

   if (!mp3_buffer)
   {
      if (resamp && resampler_data)
         resamp->free(resampler_data);
      goto error;
   }

   /* "system" menu sounds may reuse the same voice without freeing anything first, so do that here if needed */
   if (voice->types.mp3.resampler && voice->types.mp3.resampler_data)
      voice->types.mp3.resampler->free(voice->types.mp3.resampler_data);
   if (voice->types.mp3.buffer)
      memalign_free(voice->types.mp3.buffer);

   voice->types.mp3.resampler      = resamp;
   voice->types.mp3.resampler_data = resampler_data;
   voice->types.mp3.buffer         = (float*)mp3_buffer;
   voice->types.mp3.buf_samples    = samples;
   voice->types.mp3.ratio          = ratio;
   voice->types.mp3.position       = 0;
   voice->types.mp3.samples        = 0;

   return true;

error:
   drmp3_uninit(&voice->types.mp3.stream);
   return false;
}
#endif

audio_mixer_voice_t* audio_mixer_play(audio_mixer_sound_t* sound, bool repeat,
      float volume, audio_mixer_stop_cb_t stop_cb)
{
   unsigned i;
   bool res                   = false;
   audio_mixer_voice_t* voice = s_voices;

   if (!sound)
      return NULL;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++, voice++)
   {
      if (voice->type != AUDIO_MIXER_TYPE_NONE)
         continue;

      switch (sound->type)
      {
         case AUDIO_MIXER_TYPE_WAV:
            res = audio_mixer_play_wav(sound, voice, repeat, volume, stop_cb);
            break;
         case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_STB_VORBIS
            res = audio_mixer_play_ogg(sound, voice, repeat, volume, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
            res = audio_mixer_play_mod(sound, voice, repeat, volume, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_DR_FLAC
            res = audio_mixer_play_flac(sound, voice, repeat, volume, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_DR_MP3
            res = audio_mixer_play_mp3(sound, voice, repeat, volume, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_NONE:
            break;
      }

      break;
   }

   if (res)
   {
      voice->type     = sound->type;
      voice->repeat   = repeat;
      voice->volume   = volume;
      voice->sound    = sound;
      voice->stop_cb  = stop_cb;
   }
   else
      voice = NULL;

   return voice;
}

void audio_mixer_stop(audio_mixer_voice_t* voice)
{
   audio_mixer_stop_cb_t stop_cb = NULL;
   audio_mixer_sound_t* sound    = NULL;

   if (voice)
   {
      stop_cb = voice->stop_cb;
      sound   = voice->sound;

      voice->type = AUDIO_MIXER_TYPE_NONE;

      if (stop_cb)
         stop_cb(sound, AUDIO_MIXER_SOUND_STOPPED);
   }
}

static void audio_mixer_mix_wav(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   unsigned buf_free                = (unsigned)(num_frames * 2);
   const audio_mixer_sound_t* sound = voice->sound;
   unsigned pcm_available           = sound->types.wav.frames
      * 2 - voice->types.wav.position;
   const float* pcm                 = sound->types.wav.pcm +
      voice->types.wav.position;

again:
   if (pcm_available < buf_free)
   {
      for (i = pcm_available; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      if (voice->repeat)
      {
         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

         buf_free                  -= pcm_available;
         pcm_available              = sound->types.wav.frames * 2;
         pcm                        = sound->types.wav.pcm;
         voice->types.wav.position  = 0;
         goto again;
      }

      if (voice->stop_cb)
         voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

      voice->type = AUDIO_MIXER_TYPE_NONE;
   }
   else
   {
      for (i = buf_free; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      voice->types.wav.position += buf_free;
   }
}

#ifdef HAVE_STB_VORBIS
static void audio_mixer_mix_ogg(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   struct resampler_data info = { 0 };
   float temp_buffer[AUDIO_MIXER_TEMP_BUFFER] = { 0 };
   unsigned buf_free                = (unsigned)(num_frames * 2);
   unsigned temp_samples            = 0;
   float* pcm                       = NULL;

   if (voice->types.ogg.position == voice->types.ogg.samples)
   {
again:
      temp_samples = stb_vorbis_get_samples_float_interleaved(
            voice->types.ogg.stream, 2, temp_buffer,
            AUDIO_MIXER_TEMP_BUFFER) * 2;

      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

            stb_vorbis_seek_start(voice->types.ogg.stream);
            goto again;
         }
         else
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

            voice->type = AUDIO_MIXER_TYPE_NONE;
            return;
         }
      }

      info.data_in              = temp_buffer;
      info.data_out             = voice->types.ogg.buffer;
      info.input_frames         = temp_samples / 2;
      info.output_frames        = 0;
      info.ratio                = voice->types.ogg.ratio;

      if (voice->types.ogg.resampler)
      {
        voice->types.ogg.resampler->process(voice->types.ogg.resampler_data, &info);
      }
      else
        memcpy(voice->types.ogg.buffer, temp_buffer, temp_samples * sizeof(float));
      voice->types.ogg.position = 0;
      voice->types.ogg.samples  = voice->types.ogg.buf_samples;
   }

   pcm = voice->types.ogg.buffer + voice->types.ogg.position;

   if (voice->types.ogg.samples < buf_free)
   {
      for (i = voice->types.ogg.samples; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      buf_free -= voice->types.ogg.samples;
      goto again;
   }
   else
   {
      int i;
      for (i = buf_free; i != 0; --i )
         *buffer++ += *pcm++ * volume;

      voice->types.ogg.position += buf_free;
      voice->types.ogg.samples  -= buf_free;
   }
}
#endif

#ifdef HAVE_IBXM
static void audio_mixer_mix_mod(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   float samplef                    = 0.0f;
   int samplei                      = 0;
   unsigned temp_samples            = 0;
   unsigned buf_free                = (unsigned)(num_frames * 2);
   int* pcm                         = NULL;

   if (voice->types.mod.position == voice->types.mod.samples)
   {
again:
      temp_samples = replay_get_audio(
            voice->types.mod.stream, voice->types.mod.buffer );

      temp_samples *= 2; /* stereo */

      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

            replay_seek( voice->types.mod.stream, 0);
            goto again;
         }
         else
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

            voice->type = AUDIO_MIXER_TYPE_NONE;
            return;
         }
      }

      voice->types.mod.position = 0;
      voice->types.mod.samples  = temp_samples;
   }
   pcm = voice->types.mod.buffer + voice->types.mod.position;

   if (voice->types.mod.samples < buf_free)
   {
      for (i = voice->types.mod.samples; i != 0; i--)
      {
         samplei     = *pcm++ * volume;
         samplef     = (float)((int)samplei + 32768) / 65535.0f;
         samplef     = samplef * 2.0f - 1.0f;
         *buffer++  += samplef;
      }

      buf_free -= voice->types.mod.samples;
      goto again;
   }
   else
   {
      int i;
      for (i = buf_free; i != 0; --i )
      {
         samplei     = *pcm++ * volume;
         samplef     = (float)((int)samplei + 32768) / 65535.0f;
         samplef     = samplef * 2.0f - 1.0f;
         *buffer++  += samplef;
      }

      voice->types.mod.position += buf_free;
      voice->types.mod.samples  -= buf_free;
   }
}
#endif

#ifdef HAVE_DR_FLAC
static void audio_mixer_mix_flac(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   struct resampler_data info = { 0 };
   float temp_buffer[AUDIO_MIXER_TEMP_BUFFER] = { 0 };
   unsigned buf_free                = (unsigned)(num_frames * 2);
   unsigned temp_samples            = 0;
   float* pcm                       = NULL;

   if (voice->types.flac.position == voice->types.flac.samples)
   {
again:
      temp_samples = (unsigned)drflac_read_f32( voice->types.flac.stream, AUDIO_MIXER_TEMP_BUFFER, temp_buffer);
      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

            drflac_seek_to_sample(voice->types.flac.stream,0);
            goto again;
         }
         else
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

            voice->type = AUDIO_MIXER_TYPE_NONE;
            return;
         }
      }

      info.data_in              = temp_buffer;
      info.data_out             = voice->types.flac.buffer;
      info.input_frames         = temp_samples / 2;
      info.output_frames        = 0;
      info.ratio                = voice->types.flac.ratio;

      if (voice->types.flac.resampler)
      {
        voice->types.flac.resampler->process(voice->types.flac.resampler_data, &info);
      }
      else
        memcpy(voice->types.flac.buffer, temp_buffer, temp_samples * sizeof(float));
      voice->types.flac.position = 0;
      voice->types.flac.samples  = voice->types.flac.buf_samples;
   }

   pcm = voice->types.flac.buffer + voice->types.flac.position;

   if (voice->types.flac.samples < buf_free)
   {
      for (i = voice->types.flac.samples; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      buf_free -= voice->types.flac.samples;
      goto again;
   }
   else
   {
      int i;
      for (i = buf_free; i != 0; --i )
         *buffer++ += *pcm++ * volume;

      voice->types.flac.position += buf_free;
      voice->types.flac.samples  -= buf_free;
   }
}
#endif

#ifdef HAVE_DR_MP3
static void audio_mixer_mix_mp3(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   struct resampler_data info = { 0 };
   float temp_buffer[AUDIO_MIXER_TEMP_BUFFER] = { 0 };
   unsigned buf_free                = (unsigned)(num_frames * 2);
   unsigned temp_samples            = 0;
   float* pcm                       = NULL;

   if (voice->types.mp3.position == voice->types.mp3.samples)
   {
again:
      temp_samples = (unsigned)drmp3_read_f32(&voice->types.mp3.stream, AUDIO_MIXER_TEMP_BUFFER/2, temp_buffer) * 2;

      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

            drmp3_seek_to_frame(&voice->types.mp3.stream,0);
            goto again;
         }
         else
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

            voice->type = AUDIO_MIXER_TYPE_NONE;
            return;
         }
      }

      info.data_in              = temp_buffer;
      info.data_out             = voice->types.mp3.buffer;
      info.input_frames         = temp_samples / 2;
      info.output_frames        = 0;
      info.ratio                = voice->types.mp3.ratio;

      if (voice->types.mp3.resampler)
        voice->types.mp3.resampler->process(voice->types.mp3.resampler_data, &info);
      else
        memcpy(voice->types.mp3.buffer, temp_buffer, temp_samples * sizeof(float));
      voice->types.mp3.position = 0;
      voice->types.mp3.samples  = voice->types.mp3.buf_samples;
   }

   pcm = voice->types.mp3.buffer + voice->types.mp3.position;

   if (voice->types.mp3.samples < buf_free)
   {
      for (i = voice->types.mp3.samples; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      buf_free -= voice->types.mp3.samples;
      goto again;
   }
   else
   {
      int i;
      for (i = buf_free; i != 0; --i )
         *buffer++ += *pcm++ * volume;

      voice->types.mp3.position += buf_free;
      voice->types.mp3.samples  -= buf_free;
   }
}
#endif

void audio_mixer_mix(float* buffer, size_t num_frames, float volume_override, bool override)
{
   unsigned i;
   size_t j                   = 0;
   float* sample              = NULL;
   audio_mixer_voice_t* voice = s_voices;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++, voice++)
   {
      float volume = (override) ? volume_override : voice->volume;

      switch (voice->type)
      {
         case AUDIO_MIXER_TYPE_WAV:
            audio_mixer_mix_wav(buffer, num_frames, voice, volume);
            break;
         case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_STB_VORBIS
            audio_mixer_mix_ogg(buffer, num_frames, voice, volume);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
            audio_mixer_mix_mod(buffer, num_frames, voice, volume);
#endif
            break;
         case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_DR_FLAC
            audio_mixer_mix_flac(buffer, num_frames, voice, volume);
#endif
            break;
            case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_DR_MP3
            audio_mixer_mix_mp3(buffer, num_frames, voice, volume);
#endif
            break;
         case AUDIO_MIXER_TYPE_NONE:
            break;
      }
   }

   for (j = 0, sample = buffer; j < num_frames; j++, sample++)
   {
      if (*sample < -1.0f)
         *sample = -1.0f;
      else if (*sample > 1.0f)
         *sample = 1.0f;
   }
}

float audio_mixer_voice_get_volume(audio_mixer_voice_t *voice)
{
   if (!voice)
      return 0.0f;

   return voice->volume;
}

void audio_mixer_voice_set_volume(audio_mixer_voice_t *voice, float val)
{
   if (!voice)
      return;

   voice->volume = val;
}
