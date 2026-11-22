/* Window starvation must never become an overread, EOF, or rewind.
 * The short heap allocation models the unreadable tail of a reserved file
 * window; ASan makes crossing its resident boundary an immediate failure. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio/audio_mixer.h>
#include <formats/audio.h>

static void put16(unsigned char *p, unsigned v)
{ p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void put32(unsigned char *p, unsigned v)
{ put16(p,v); put16(p+2,v>>16); }
struct fixture { unsigned char *data; size_t size, offset, unit, per_unit, frames, channels; };
static struct fixture fixture(unsigned format, unsigned bits, unsigned channels)
{
   struct fixture f;
   unsigned fmt = format == 17 ? 20 : format == 2 ? 50 : 16;
   size_t payload, i;
   f.offset = 28 + fmt;
   f.channels = channels;
   f.unit = format == 17 || format == 2 ? 8 : channels * bits/8;
   f.per_unit = format == 17 ? 9 : format == 2 ? 4 : 1;
   f.frames = f.per_unit > 1 ? f.per_unit*2 : 64;
   payload = f.frames/f.per_unit*f.unit;
   f.size = f.offset + payload;
   f.data = (unsigned char*)calloc(1,f.size);
   assert(f.data);
   memcpy(f.data,"RIFF",4); put32(f.data+4,(unsigned)f.size-8);
   memcpy(f.data+8,"WAVEfmt ",8); put32(f.data+16,fmt);
   put16(f.data+20,format); put16(f.data+22,channels);
   put32(f.data+24,48000); put32(f.data+28,(unsigned)(48000*f.unit/f.per_unit));
   put16(f.data+32,(unsigned)f.unit); put16(f.data+34,bits);
   if (fmt > 16) { put16(f.data+36,fmt-18); put16(f.data+38,(unsigned)f.per_unit); }
   if (format == 2)
   {
      static const int coefficients[14] = {256,0,512,-256,0,0,192,64,240,0,460,-208,392,-232};
      put16(f.data+40,7);
      for (i=0;i<14;i++) put16(f.data+42+i*2,(unsigned)coefficients[i]);
   }
   memcpy(f.data+20+fmt,"data",4); put32(f.data+24+fmt,(unsigned)payload);
   if (f.per_unit > 1)
   {
      if (format == 2) { put16(f.data+f.offset+1,16); put16(f.data+f.offset+9,16); }
   }
   else for (i=0;i<f.frames*channels;i++)
   {
      unsigned char *p = f.data+f.offset+i*bits/8;
      if (format == 3)
      {
         float value = (float)((int)(i%17)-8)/16.0f;
         uint32_t encoded;
         memcpy(&encoded,&value,4); put32(p,encoded);
      }
      else if (bits == 8) *p = (unsigned char)(i*13+31);
      else if (bits == 16) put16(p,(unsigned)(int16_t)((int)i*193-12000));
      else { unsigned v=(unsigned)((int)i*12345-700000); p[0]=(unsigned char)v; p[1]=(unsigned char)(v>>8); p[2]=(unsigned char)(v>>16); }
   }
   return f;
}
static int read_frames(void *x, bool s16, void *out, size_t frames, size_t *got)
{ return s16 ? audio_transfer_read_s16(x,AUDIO_TYPE_WAV,(int16_t*)out,frames,got)
             : audio_transfer_read_f32(x,AUDIO_TYPE_WAV,(float*)out,frames,got); }
static void transfer_case(unsigned format, unsigned bits, unsigned channels, bool s16)
{
   struct fixture f = fixture(format,bits,channels);
   static float expected_f[256], output_f[256];
   static int16_t expected_i[256], output_i[256];
   void *expected = s16 ? (void*)expected_i : (void*)expected_f;
   void *output = s16 ? (void*)output_i : (void*)output_f;
   size_t sample_size = s16 ? sizeof(int16_t) : sizeof(float);
   size_t prefix = f.offset + (f.per_unit > 1 ? f.unit-1 : 16*f.unit+(f.unit>1 ? 1 : 0));
   size_t first = f.per_unit > 1 ? 0 : 16, got = 0, total;
   uint64_t duration = 0;
   unsigned char *resident = (unsigned char*)malloc(prefix);
   void *ref = audio_transfer_new(AUDIO_TYPE_WAV);
   void *x = audio_transfer_new(AUDIO_TYPE_WAV);
   assert(ref && x && resident);
   audio_transfer_set_buffer_ptr(ref,AUDIO_TYPE_WAV,f.data,f.size);
   assert(audio_transfer_start(ref,AUDIO_TYPE_WAV));
   read_frames(ref,s16,expected,f.frames,&got); assert(got == f.frames);
   audio_transfer_free(ref,AUDIO_TYPE_WAV);
   memcpy(resident,f.data,prefix);
   audio_transfer_set_buffer_ptr(x,AUDIO_TYPE_WAV,resident,f.size);
   audio_transfer_set_avail(x,AUDIO_TYPE_WAV,prefix);
   assert(audio_transfer_start(x,AUDIO_TYPE_WAV));
   assert(audio_transfer_info(x,AUDIO_TYPE_WAV,NULL,NULL,&duration) && duration == f.frames);
   assert(read_frames(x,s16,output,f.frames,&got) == AUDIO_PROCESS_NEXT && got == first);
   assert(!memcmp(output,expected,first*channels*sample_size));
   total = got;
   assert(read_frames(x,s16,output,f.frames,&got) == AUDIO_PROCESS_NEXT && !got);
   if (f.per_unit > 1)
   {
      prefix = f.offset+f.unit;
      resident = (unsigned char*)realloc(resident,prefix); assert(resident);
      memcpy(resident,f.data,prefix);
      audio_transfer_set_buffer_ptr(x,AUDIO_TYPE_WAV,resident,f.size);
      audio_transfer_set_avail(x,AUDIO_TYPE_WAV,prefix);
      assert(read_frames(x,s16,output,f.frames,&got) == AUDIO_PROCESS_NEXT && got == f.per_unit);
      assert(!memcmp(output,expected,got*channels*sample_size)); total += got;
   }
   resident = (unsigned char*)realloc(resident,f.size); assert(resident);
   memcpy(resident,f.data,f.size);
   audio_transfer_set_buffer_ptr(x,AUDIO_TYPE_WAV,resident,f.size);
   audio_transfer_set_avail(x,AUDIO_TYPE_WAV,f.size);
   read_frames(x,s16,output,f.frames,&got); assert(got == f.frames-total);
   assert(!memcmp(output,(const unsigned char*)expected+total*channels*sample_size,got*channels*sample_size));
   assert(read_frames(x,s16,output,1,&got) == AUDIO_PROCESS_END && !got);
   assert(audio_transfer_seek(x,AUDIO_TYPE_WAV,0));
   assert(read_frames(x,s16,output,f.frames,&got) == AUDIO_PROCESS_NEXT && got == first);
   assert(audio_transfer_seek(x,AUDIO_TYPE_WAV,0));
   audio_transfer_set_avail(x,AUDIO_TYPE_WAV,prefix);
   read_frames(x,s16,output,f.frames,&got);
   assert(got == (f.per_unit > 1 ? f.per_unit : first));
   assert(audio_transfer_seek(x,AUDIO_TYPE_WAV,f.frames/2));
   assert(read_frames(x,s16,output,f.frames,&got) == AUDIO_PROCESS_NEXT && !got);
   audio_transfer_set_avail(x,AUDIO_TYPE_WAV,f.size);
   read_frames(x,s16,output,f.frames,&got); assert(got == f.frames-f.frames/2);
   assert(!memcmp(output,(const unsigned char*)expected+(f.frames/2)*channels*sample_size,got*channels*sample_size));
   audio_transfer_set_avail(x,AUDIO_TYPE_WAV,0);
   assert(audio_transfer_seek(x,AUDIO_TYPE_WAV,0));
   read_frames(x,s16,output,f.frames,&got); assert(got == f.frames);
   assert(!memcmp(output,expected,got*channels*sample_size));
   audio_transfer_free(x,AUDIO_TYPE_WAV); free(resident); free(f.data);
}
static unsigned finished;
static void stopped(audio_mixer_sound_t *sound, unsigned reason)
{ (void)sound; if(reason == AUDIO_MIXER_SOUND_FINISHED) finished++; }
static void mixer_case(bool s16)
{
   struct fixture f=fixture(1,16,2);
   audio_mixer_sound_t *sound;
   audio_mixer_voice_t *voice;
   static float out_f[128]; static int16_t out_i[128];
   unsigned i;
   audio_mixer_init(48000); finished=0;
   sound=audio_mixer_load_wav_stream(f.data,f.size); assert(sound);
   audio_mixer_sound_set_avail(sound,f.offset+16*f.unit);
   voice=s16 ? audio_mixer_play_s16(sound,false,AUDIO_MIXER_GAIN_UNITY,"nearest",RESAMPLER_QUALITY_NORMAL,stopped)
             : audio_mixer_play(sound,false,1.0f,"nearest",RESAMPLER_QUALITY_NORMAL,stopped);
   assert(voice);
   memset(out_f,0,sizeof(out_f)); memset(out_i,0,sizeof(out_i));
   if(s16) audio_mixer_mix_s16(out_i,64,AUDIO_MIXER_GAIN_UNITY,false); else audio_mixer_mix(out_f,64,1.0f,false);
   assert(!finished && audio_mixer_voice_buffer_tell(voice)==f.offset+16*f.unit);
   for(i=0;i<128;i++)
   {
      int16_t sample=i<32 ? (int16_t)((int)i*193-12000) : 0;
      if(s16) assert(out_i[i]==sample); else assert(out_f[i]==sample/32768.0f);
   }
   memset(out_f,0,sizeof(out_f)); memset(out_i,0,sizeof(out_i));
   if(s16) audio_mixer_mix_s16(out_i,64,AUDIO_MIXER_GAIN_UNITY,false); else audio_mixer_mix(out_f,64,1.0f,false);
   assert(!finished && audio_mixer_voice_buffer_tell(voice)==f.offset+16*f.unit);
   for(i=0;i<128;i++) assert(s16 ? out_i[i]==0 : out_f[i]==0.0f);
   audio_mixer_voice_set_avail(voice,f.size);
   if(s16) audio_mixer_mix_s16(out_i,64,AUDIO_MIXER_GAIN_UNITY,false); else audio_mixer_mix(out_f,64,1.0f,false);
   for(i=0;i<128;i++)
   {
      int16_t sample=i<96 ? (int16_t)((int)(i+32)*193-12000) : 0;
      if(s16) assert(out_i[i]==sample); else assert(out_f[i]==sample/32768.0f);
   }
   assert(finished==1);
   audio_mixer_stop(voice); audio_mixer_destroy(sound); audio_mixer_done();
}
int main(void)
{
   unsigned s16;
   for(s16=0;s16<2;s16++)
   {
      transfer_case(1,8,1,s16!=0); transfer_case(1,16,2,s16!=0);
      transfer_case(1,24,2,s16!=0); transfer_case(3,32,2,s16!=0);
      transfer_case(6,8,1,s16!=0); transfer_case(7,8,1,s16!=0);
      transfer_case(17,4,1,s16!=0); transfer_case(2,4,1,s16!=0);
      mixer_case(s16!=0);
   }
   puts("PASS: WAV-only resident bounds, short allocations, partial PCM/ADPCM, starvation, growth, rewind and float/S16 mixer resume");
   return 0;
}
