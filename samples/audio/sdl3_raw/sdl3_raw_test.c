/* Shipping SDL3 driver with real unbound audio streams and injected API errors.
 * No audio device is opened. The return value always describes input accepted. */
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>
static bool fail_ratio, fail_gain, fail_format, fragmented, fail_queued;
static unsigned put_calls, fail_put;
static bool set_ratio(SDL_AudioStream *s, float value)
{ return !fail_ratio && SDL_SetAudioStreamFrequencyRatio(s, value); }
static bool set_gain(SDL_AudioStream *s, float value)
{ return !fail_gain && SDL_SetAudioStreamGain(s, value); }
static bool set_format(SDL_AudioStream *s, const SDL_AudioSpec *in, const SDL_AudioSpec *out)
{ return !fail_format && SDL_SetAudioStreamFormat(s, in, out); }
static bool put_data(SDL_AudioStream *s, const void *data, int len)
{ return ++put_calls != fail_put && SDL_PutAudioStreamData(s, data, len); }
static int test_queued(SDL_AudioStream *s)
{ return fail_queued ? -1 : fragmented ? 4096-64 : SDL_GetAudioStreamQueued(s); }
#define SDL_SetAudioStreamFrequencyRatio set_ratio
#define SDL_SetAudioStreamGain set_gain
#define SDL_SetAudioStreamFormat set_format
#define SDL_PutAudioStreamData put_data
#define SDL_GetAudioStreamQueued test_queued
#include "../../../audio/drivers/sdl3_audio.c"
#undef SDL_SetAudioStreamFrequencyRatio
#undef SDL_SetAudioStreamGain
#undef SDL_SetAudioStreamFormat
#undef SDL_PutAudioStreamData
#undef SDL_GetAudioStreamQueued
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
settings_t *config_get_ptr(void) { static settings_t settings; return &settings; }
static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %u: %s\n", __LINE__, #x); failures++; } } while (0)

int main(void)
{
   sdl3_audio_t s;
   int16_t input[512], saved[512], output[512];
   unsigned n;
   memset(&s, 0, sizeof(s));
   for (n=0;n<512;n++) input[n]=(int16_t)((int)n*71-16000);
   s.spec.format=SDL_AUDIO_S16; s.spec.channels=2; s.spec.freq=48000;
   s.stream=SDL_CreateAudioStream(&s.spec, &s.spec);
   if (!s.stream) { fprintf(stderr,"SDL stream: %s\n",SDL_GetError()); return 1; }
   s.buffer_size=4096; s.ratio=s.gain=1; s.nonblock=true;
   CHECK(sdl3_audio_write_raw(NULL,NULL,0,0,0,0)==0);
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,0.5,0.25f)==256);
   CHECK(SDL_GetAudioStreamQueued(s.stream)==sizeof(input));
   CHECK(SDL_GetAudioStreamFrequencyRatio(s.stream)==2.0f);
   CHECK(SDL_GetAudioStreamGain(s.stream)==0.25f);
   CHECK(s.ratio==2.0f && s.gain==0.25f);
   SDL_ClearAudioStream(s.stream); s.in_cap=320;
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,0.5,0.25f)==80);
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,0.5,0.25f)==0);
   SDL_ClearAudioStream(s.stream); s.in_cap=4096;
   put_calls=0; fail_put=1;
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,0.5,0.25f)==-1);
   CHECK(SDL_GetAudioStreamQueued(s.stream)==0);
   fail_queued=true; fail_put=0;
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,0.5,0.25f)==-1);
   fail_queued=false;
   put_calls=0; fail_put=2; fragmented=true;
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,0.5,0.25f)==16);
   CHECK(SDL_GetAudioStreamQueued(s.stream)==64);
   fragmented=false; fail_put=0; SDL_ClearAudioStream(s.stream);
   fail_ratio=true;
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,1,0.25f)==-1 && s.ratio==2);
   fail_ratio=false; fail_gain=true;
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,1,0.5f)==-1 && s.ratio==1 && s.gain==0.25f);
   fail_gain=false; fail_format=true;
   CHECK(sdl3_audio_write_raw(&s,input,256,48000,1,1)==-1 && s.raw_rate==24000);
   fail_format=false;
   CHECK(SDL_GetAudioStreamQueued(s.stream)==0);
   CHECK(sdl3_audio_write_raw(&s,input,SIZE_MAX,24000,1,1)==-1);
   CHECK(sdl3_audio_write_raw(&s,input,256,UINT_MAX,1,1)==-1);
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,0,1)==-1);
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,NAN,1)==-1);
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,INFINITY,1)==-1);
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,0.001,1)==-1);
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,1,-1)==-1);
   CHECK(sdl3_audio_write_raw(&s,input,256,24000,1,NAN)==-1);
   CHECK(SDL_GetAudioStreamQueued(s.stream)==0);
   /* Failure restoring ordinary output must leave caches matching SDL. */
   fail_gain=true;
   CHECK(sdl3_audio_write(&s,input,sizeof(input))==-1 && s.raw_rate==24000);
   fail_gain=false;
   CHECK(sdl3_audio_write(&s,input,sizeof(input))==sizeof(input));
   CHECK(!s.raw_rate && s.ratio==1 && s.gain==1);
   SDL_ClearAudioStream(s.stream);
   n=(unsigned)retro_atomic_load_relaxed_size(&s.consumed_frames);
   CHECK(sdl3_audio_write(&s,input,sizeof(input))==sizeof(input));
   CHECK(retro_atomic_load_relaxed_size(&s.consumed_frames)-n==256);
   memcpy(saved,input,sizeof(input)); memset(input,0,sizeof(input));
   CHECK(SDL_GetAudioStreamData(s.stream,output,sizeof(output))==sizeof(output));
   CHECK(!memcmp(saved,output,sizeof(output)));
   SDL_DestroyAudioStream(s.stream);
   SDL_Quit();
   printf("SDL3 raw contract: %u failures\n",failures);
   return failures ? 1 : 0;
}
