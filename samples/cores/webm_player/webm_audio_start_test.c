/* Real progressive MP4 core: metadata precedes the first audio packet. */
#include "../../../cores/libretro-media/media_core.c"
static unsigned startup_failures, startup_shutdowns;
static size_t startup_nonzero;
static size_t startup_frames;
#define START_CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL audio startup %u: %s\n",(unsigned)__LINE__,#c); startup_failures++; } } while (0)
static bool startup_environment(unsigned cmd,void *data)
{
   if(cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format*)data==RETRO_PIXEL_FORMAT_XRGB8888;
   if(cmd==RETRO_ENVIRONMENT_SHUTDOWN){startup_shutdowns++;return true;}
   return false;
}
static void startup_video(const void *data,unsigned w,unsigned h,size_t pitch)
{ (void)data;(void)w;(void)h;(void)pitch; }
static size_t startup_audio(const int16_t *data,size_t count)
{
   size_t i;
   for(i=0;i<count*2;i++)if(data[i])startup_nonzero++;
   startup_frames+=count;
   return count;
}
static void startup_poll(void) { }
static int16_t startup_input(unsigned p,unsigned d,unsigned i,unsigned id)
{ (void)p;(void)d;(void)i;(void)id;return 0; }
int main(int argc,char **argv)
{
   struct retro_game_info game;
   struct retro_system_av_info av;
   unsigned i,expected_rate=48000;
   double silence_error;
   if(argc!=2 && argc!=3)return 2;
   if(argc==3)expected_rate=(unsigned)strtoul(argv[2],NULL,10);
   memset(&game,0,sizeof(game));game.path=argv[1];
   retro_set_environment(startup_environment);retro_set_video_refresh(startup_video);
   retro_set_audio_sample_batch(startup_audio);retro_set_input_poll(startup_poll);
   retro_set_input_state(startup_input);retro_init();
   START_CHECK(retro_load_game(&game));
   if(!media_player.mp4vs)return 1;
   retro_get_system_av_info(&av);
   START_CHECK(media_player.actx && av.timing.sample_rate==(double)expected_rate);
   silence_error=(double)media_player.silence_frames-(double)expected_rate/media_player.fps;
   START_CHECK(silence_error>=0.0 && silence_error<=1.0);
   START_CHECK(!media_player.anpkts && !data_transfer_complete(media_player.dt));
   retro_run();
   START_CHECK(media_player.actx && !media_player.aeof && !media_player.anpkts);
   START_CHECK(!startup_nonzero && !startup_shutdowns);
   for(i=0;i<32 && !startup_nonzero && !startup_shutdowns;i++)retro_run();
   START_CHECK(startup_nonzero && !media_player.aeof && !startup_shutdowns);
   for(i=0;i<512 && !startup_shutdowns;i++)retro_run();
   START_CHECK(startup_shutdowns==1 && media_player.aeof);
   START_CHECK(media_player.atotal>0 && media_player.apos==media_player.atotal);
   printf("Audio timeline: rate=%u callbacks=%lu position=%ld total=%ld\n",
         expected_rate,(unsigned long)startup_frames,
         (long)media_player.apos,(long)media_player.atotal);
   START_CHECK(media_player.apos>=(int64_t)expected_rate*16 &&
         media_player.apos<=(int64_t)expected_rate*16+1024);
   START_CHECK(startup_frames>=(size_t)media_player.apos &&
         startup_frames<=(size_t)media_player.apos+media_player.silence_frames);
   retro_unload_game();retro_deinit();
   printf("Progressive MP4 audio startup: %u failures\n",startup_failures);
   return startup_failures?1:0;
}
