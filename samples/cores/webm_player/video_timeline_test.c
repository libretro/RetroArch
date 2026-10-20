/* Actual builtin core: delayed video must preserve continuous audio. */
#define main original_host_main
#define log_cb host_log_cb
#define video original_video
#define audio original_audio
#include "webm_player_test.c"
#undef main
#undef log_cb
#undef video
#undef audio
#include "../../../cores/libretro-media/media_core.c"

static uint32_t reference_pictures[128];
static unsigned reference_count, shown_count, last_shown, comparing, picture_base;
static int64_t first_video, reference_first;
static uint64_t signal_samples;
static uint32_t signal_hash, reference_audio;

static void video(const void *data, unsigned w, unsigned h, size_t pitch)
{
   unsigned i;
   original_video(data,w,h,pitch);
   if (!media_player.vshown && data)
   {
      const uint32_t *pixels = (const uint32_t*)data;
      for (i=0;i<w*h;i++) if (pixels[i] & 0xffffffu)
      { check(0,"video visible before its presentation interval"); break; }
   }
   if ((unsigned)media_player.vshown == last_shown) return;
   check(media_player.vshown == (int64_t)last_shown+1,"video skipped a fixture picture");
   last_shown=(unsigned)media_player.vshown;
   if (first_video<0) first_video=media_player.play_ns;
   if (shown_count<128)
   {
      if (comparing) check(picture_base+shown_count<reference_count && reference_pictures[picture_base+shown_count]==frame_hash,
            "video delay changed picture pixels");
      else reference_pictures[shown_count]=frame_hash;
   }
   shown_count++;
}
static size_t audio(const int16_t *data,size_t count)
{
   size_t i;
   for(i=0;i<count && signal_samples<192000;i++,signal_samples++)
   {
      signal_hash=(signal_hash^(uint16_t)data[2*i])*16777619u;
      signal_hash=(signal_hash^(uint16_t)data[2*i+1])*16777619u;
   }
   return original_audio(data,count);
}
static void capture_begin(unsigned compare)
{
   clear_capture();comparing=compare;shown_count=last_shown=picture_base=0;first_video=-1;
   signal_samples=0;signal_hash=2166136261u;
}
static void run_lane(const char *path,unsigned compare)
{
   struct retro_game_info game;
   unsigned i,pass;
   current=path;memset(&game,0,sizeof(game));game.path=path;
   capture_begin(compare);
   check(retro_load_game(&game),"cannot load timing fixture");
   if(!media_player.mp4vs)return;
   check(rmp4_video_stream_presentation_delay_ns(media_player.mp4vs)==(compare?1000000000:0),
         "stream did not preserve the selected track delay");
   for(pass=0;pass<(compare?2u:1u);pass++)
   {
      if(pass){retro_reset();capture_begin(compare);}
      for(i=0;i<128 && !shutdowns;i++)retro_run();
      check(shutdowns && shown_count==24,"video count/EOS changed");
      check(signal_samples==192000,"continuous audio truncated by delayed video");
      if(compare)
      {
         check(first_video-reference_first>=1000000000-media_player.frame_ns/2
               && first_video-reference_first<=1000000000+media_player.frame_ns/2,
               "leading empty video edit was ignored");
         check(signal_hash==reference_audio,"video delay changed audio PCM");
      }
      else {reference_count=shown_count;reference_first=first_video;reference_audio=signal_hash;}
      printf("Video timeline: compare=%u replay=%u first_ns=%ld pictures=%u audio=%lu\n",
            compare,pass,(long)first_video,shown_count,(unsigned long)signal_samples);
   }
   if(compare)
   {
      uint8_t state[40];
      uint32_t expected_hash;
      uint64_t expected_samples;
      unsigned expected_frames;
      retro_reset();capture_begin(1);
      check(media_seek(&media_player,500000000)==500000000,"seek into leading edit changed movie position");
      check(!media_player.vshown && media_player.play_ns==500000000,"seek decoded video before its interval");
      check(media_player.apos==24000,"seek aligned audio to future video instead of movie clock");
      for(i=0;i<6;i++)retro_run();
      check(!media_player.vshown,"video became visible during leading empty edit");
      check(retro_serialize(state,sizeof(state)),"cannot save state inside leading edit");
      capture_begin(1);retro_run();retro_run();
      expected_hash=signal_hash;expected_samples=samples;expected_frames=shown_count;
      check(retro_unserialize(state,sizeof(state)),"cannot restore state inside leading edit");
      capture_begin(1);retro_run();retro_run();
      check(expected_hash==signal_hash && expected_samples==samples && expected_frames==shown_count,
            "leading edit state restore changed pictures or PCM");
      printf("Leading edit seek/restore: pictures=%u audio=%lu PCM=%s\n",shown_count,
            (unsigned long)samples,expected_hash==signal_hash?"identical":"FAIL");
      {
         int64_t position=media_seek(&media_player,1500000000);
         check(position>=1500000000-media_player.frame_ns/2
               && position<=1500000000+media_player.frame_ns/2,
               "seek after leading edit used a raw track coordinate");
         check(media_player.apos>=71999 && media_player.apos<=72001,
               "seek after leading edit changed audio movie position");
         capture_begin(1);picture_base=last_shown=(unsigned)media_player.vshown;
         retro_run();retro_run();
         check(shown_count==2,"seek after leading edit failed to present two source pictures");
         printf("After-delay seek: base=%u pictures=%u audio_position=%ld\n",picture_base,
               shown_count,(long)media_player.apos);
      }
   }
   retro_unload_game();
}
int main(int argc,char **argv)
{
   if(argc!=3)return 2;
   check(!rmp4_video_stream_presentation_delay_ns(NULL),"NULL stream delay is nonzero");
   retro_set_environment(environment);retro_set_video_refresh(video);
   retro_set_audio_sample_batch(audio);retro_set_audio_sample(audio_sample);
   retro_set_input_poll(input_poll);retro_set_input_state(input_state);retro_init();
   run_lane(argv[1],0);run_lane(argv[2],1);retro_deinit();
   return failures?1:0;
}
