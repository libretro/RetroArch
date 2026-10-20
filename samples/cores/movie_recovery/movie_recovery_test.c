#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <file/config_file.h>
#include <time/rtime.h>
#include <features/features_cpu.h>
#include <verbosity.h>
#include "../../../retroarch.h"
#include "../../../command.h"
#ifdef HAVE_MENU
#include "../../../menu/menu_driver.h"
#endif
#include "../../../runloop.h"
#include "../../../configuration.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_thread_wrapper.h"
#include "../../../audio/audio_driver.h"
#include "../../../tasks/tasks_internal.h"
static unsigned saved_input_users;
/* No logical input ports: a null joypad request may fall back to a live
 * controller, and a source beside it can bypass a driver's state hook.
 * Keep real polling/lifecycle while excluding hotkeys from this probe. */
static void quiet_input_begin(void)
{
   settings_t *settings=config_get_ptr();
   saved_input_users=settings->uints.input_max_users;
   settings->uints.input_max_users=0;
}
static void quiet_input_end(void)
{
   config_get_ptr()->uints.input_max_users=saved_input_users;
}
static const video_poke_interface_t *real_poke;
static unsigned reject_next, rejected;
static enum video_texture_update reject_movie(void *data,const video_frame_import_t *frame)
{
   if(frame && reject_next){reject_next=0;rejected++;return VIDEO_TEXTURE_UPDATE_REFUSED;}
   return real_poke->import_movie_frame(data,frame);
}
static unsigned frames,gpu,cpu,failures;
static uint64_t samples;
static FILE *pcm_file;
enum movie_window_mode { MOVIE_WINDOW_STEADY, MOVIE_WINDOW_RESIZE, MOVIE_WINDOW_FULLSCREEN };
static enum movie_window_mode window_mode;
#define CHECK(c) do { if(!(c)){fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#c);failures++;} } while(0)
static void video(const void *data,unsigned w,unsigned h,size_t pitch)
{
   if(data==RETRO_HW_FRAME_BUFFER_VALID || (!data && video_state_get_ptr()->movie_frame))gpu++;
   else if(data)cpu++;
   frames++;
   video_driver_frame(data,w,h,pitch);
}
static size_t audio(const int16_t *data,size_t count)
{
   if(pcm_file)CHECK(fwrite(data,2*sizeof(*data),count,pcm_file)==count);
   samples+=count;return audio_driver_sample_batch(data,count);
}
static FILE *open_pcm(const char *out)
{
   char path[1024];
   snprintf(path,sizeof(path),"%s/audio.s16",out);
   return fopen(path,"wb");
}
static void capture(unsigned index,const char *out)
{
   video_driver_state_t *v=video_state_get_ptr();
   struct video_viewport vp;
   unsigned w,h,x,y;
   uint8_t *b;
   char name[1024];
   FILE *f;
   memset(&vp,0,sizeof(vp));
   CHECK(video_driver_get_viewport_info(&vp));
   w=VIDEO_SCALE_W(vp.dims);h=VIDEO_SCALE_H(vp.dims);
   if(window_mode==MOVIE_WINDOW_FULLSCREEN && index>=2 && index<7)
   {
#ifdef _WIN32
      HWND hwnd=(HWND)video_driver_window_get();
      MONITORINFO monitor;
      RECT rect;
      bool have_rect,have_monitor;
      memset(&rect,0,sizeof(rect));
      memset(&monitor,0,sizeof(monitor));monitor.cbSize=sizeof(monitor);
      have_rect=hwnd && GetWindowRect(hwnd,&rect);
      have_monitor=hwnd && GetMonitorInfo(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
      CHECK(have_rect && have_monitor);
      CHECK(hwnd && (GetWindowLongPtr(hwnd,GWL_STYLE)&WS_POPUP));
      CHECK(have_rect && have_monitor && EqualRect(&rect,&monitor.rcMonitor));
      CHECK(w>512 && h>384);
      printf("WINDOW_FULLSCREEN %u %ux%u\n",index,w,h);
#else
      CHECK(false);
#endif
   }
   else if(window_mode==MOVIE_WINDOW_RESIZE && index>=2 && index<7)
      CHECK(w==512 && h==384);
   else if(window_mode!=MOVIE_WINDOW_STEADY && (index==7 || index==8))
      CHECK(w==320 && h==240);
   else
      CHECK(w==256 && h==192);
   fprintf(stderr,"capture %u: driver=%s viewport=%ux%u reader=%u\n",index,v->current_video->ident,w,h,v->current_video->read_viewport?1:0);
   CHECK(w && h && w<=4096 && h<=4096 && v->current_video->read_viewport);
   if(!w || !h || w>4096 || h>4096 || !v->current_video->read_viewport)return;
   b=(uint8_t*)malloc((size_t)w*h*3);CHECK(b!=NULL);if(!b)return;
   if(!v->current_video->read_viewport(v->data,b,false)){CHECK(false);free(b);return;}
   snprintf(name,sizeof(name),"%s/frame-%02u.ppm",out,index);
   f=fopen(name,"wb");CHECK(f!=NULL);
   if(f)
   {
      fprintf(f,"P6\n%u %u\n255\n",w,h);
      for(y=0;y<h;y++)for(x=0;x<w;x++)
      {
         size_t p=((size_t)(h-1-y)*w+x)*3;
         uint8_t rgb[3];rgb[0]=b[p+2];rgb[1]=b[p+1];rgb[2]=b[p];
         fwrite(rgb,1,3,f);
      }
      fclose(f);
   }
   printf("frame %u: viewport %ux%u GPU=%u CPU=%u movie=%u\n",index,w,h,gpu,cpu,v->movie_frame?1:0);
   free(b);
}
int main(int argc,char **argv)
{
   char cfg_path[1024];char *args[6];FILE *f;unsigned i, restart_hold = 0, recovery_cpu = 0;
   bool threaded, continuous;
   int iterate_result;
   struct retro_system_av_info av;
   retro_time_t frame_us;
   runloop_state_t *loop;
   video_poke_interface_t fault_poke;
   if(argc<4 || argc>7)return 2;
   continuous=argc==7 && strcmp(argv[6],"continuous")==0;
   if(argc==7 && !continuous)return 2;
   threaded=argc>=5 && strcmp(argv[4],"true")==0;
   if(argc>=5 && strcmp(argv[4],"true")!=0 && strcmp(argv[4],"false")!=0)return 2;
   if(argc>=6)
   {
      if(strcmp(argv[5],"resize")==0)window_mode=MOVIE_WINDOW_RESIZE;
      else if(strcmp(argv[5],"fullscreen")==0)window_mode=MOVIE_WINDOW_FULLSCREEN;
      else if(strcmp(argv[5],"steady")!=0)return 2;
   }
   snprintf(cfg_path,sizeof(cfg_path),"%s/window.cfg",argv[3]);
   f=fopen(cfg_path,"wb");if(!f)return 2;
   fprintf(f,"video_driver = \"%s\"\n",argv[2]);
   fputs("audio_driver = \"null\"\ninput_driver = \"null\"\ninput_joypad_driver = \"null\"\nmenu_driver = \"rgui\"\nvideo_vsync = \"true\"\nvideo_fullscreen = \"false\"\nvideo_windowed_fullscreen = \"true\"\nvideo_scale = \"1\"\nvideo_smooth = \"false\"\npause_nonactive = \"false\"\nconfig_save_on_exit = \"false\"\nthreaded_data_runloop_enable = \"false\"\nvideo_hdr_mode = \"0\"\n",f);fclose(f);
   f=fopen(cfg_path,"ab");if(!f)return 2;
   fprintf(f,"video_threaded = \"%s\"\n",threaded?"true":"false");
   fputs("video_window_custom_size_enable = \"true\"\nvideo_window_width = \"256\"\nvideo_window_height = \"192\"\nvideo_window_save_positions = \"false\"\nvideo_font_enable = \"false\"\n",f);fclose(f);
   verbosity_enable();
   config_file_set_io_default(config_file_io_filestream());rtime_init();retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE,NULL);frontend_driver_init_first(NULL);
   args[0]=(char*)"retroarch";args[1]=(char*)"--config";args[2]=cfg_path;args[3]=argv[1];args[4]=NULL;
   if(!retroarch_main_init(4,args))return 2;
   quiet_input_begin();
   CHECK(video_driver_thread_wrapper_active()==threaded);
   loop=runloop_state_get_ptr();CHECK(loop->current_core_type==CORE_TYPE_WEBM);
   CHECK(strcmp(video_driver_get_ident(),argv[2])==0);
   pcm_file=open_pcm(argv[3]);CHECK(pcm_file!=NULL);
   loop->current_core.retro_set_video_refresh(video);loop->current_core.retro_set_audio_sample_batch(audio);
   memset(&av,0,sizeof(av));
   loop->current_core.retro_get_system_av_info(&av);
   CHECK(av.timing.fps>0.0 && av.timing.fps<=1000.0);
   if(av.timing.fps<=0.0 || av.timing.fps>1000.0)return 2;
   frame_us=(retro_time_t)(1000000.0/av.timing.fps+0.5);
   real_poke=video_state_get_ptr()->poke;CHECK(real_poke && real_poke->import_movie_frame);
   fault_poke=*real_poke;fault_poke.import_movie_frame=reject_movie;
   video_state_get_ptr()->poke=&fault_poke;
   for(i=0;i<12;i++)
   {
      unsigned cpu_before=cpu, gpu_before=gpu;
      if(window_mode!=MOVIE_WINDOW_STEADY && i==2)
         CHECK(video_driver_set_video_mode(window_mode==MOVIE_WINDOW_FULLSCREEN
                  ? video_driver_window_dims(true):VIDEO_SCALE_PACK(512,384),
                  window_mode==MOVIE_WINDOW_FULLSCREEN));
      if(window_mode!=MOVIE_WINDOW_STEADY && i==7)
         CHECK(video_driver_set_video_mode(VIDEO_SCALE_PACK(320,240),false));
      if(i==4)reject_next=1;
      /* CPU transition clears the frontend refusal latch. */
      if(i==9)
      {
         video_state_get_ptr()->poke=real_poke;
         quiet_input_end();
         CHECK(command_event(CMD_EVENT_REINIT,NULL));
         quiet_input_begin();
         CHECK(!video_state_get_ptr()->movie_refused);
         CHECK(video_driver_thread_wrapper_active()==threaded);
         loop->current_core.retro_set_video_refresh(video);
         loop->current_core.retro_set_audio_sample_batch(audio);
      }
      /* Null audio supplies no throttle. Pace this test at the core rate. */
      retro_sleep_until_us(cpu_features_get_time_usec()+frame_us);
      iterate_result=runloop_iterate();
      CHECK(iterate_result>=0);task_queue_check();
      CHECK(!(loop->flags&(RUNLOOP_FLAG_PAUSED|RUNLOOP_FLAG_FASTMOTION)));
      printf("ITER_STATE tick=%u rc=%d flags=%x focus=%u callbacks=%u samples=%lu\n",
            i,iterate_result,(unsigned)loop->flags,video_driver_has_focus()?1:0,frames,(unsigned long)samples);
#ifdef HAVE_MENU
      printf("ITER_MENU tick=%u flags=%x\n",i,(unsigned)menu_state_get_ptr()->flags);
#endif
      /* Test-only drain: viewport dimensions are queried after queued work.
       * Never advance the core merely to settle a resize. */
#ifdef HAVE_THREADS
      if(window_mode!=MOVIE_WINDOW_STEADY && threaded)video_thread_wait_idle();
#endif
      if(continuous && i>=9 && cpu>cpu_before)
      {
         CHECK(cpu==cpu_before+1 && gpu==gpu_before);
         recovery_cpu++;
         printf("RECOVERY_CPU tick=%u\n",i);
      }
      else if(i==9 && cpu==5 && gpu==5)
      {
         restart_hold=1;
         printf("RECOVERY_HOLD tick=9\n");
      }
      if(window_mode!=MOVIE_WINDOW_STEADY && (i==2 || i==7))
      {
         video_driver_cached_frame();
#ifdef HAVE_THREADS
         if(threaded)video_thread_wait_idle();
#endif
      }
      capture(i,argv[3]);
      if(continuous && i==11)CHECK(gpu>gpu_before);
   }
   CHECK(rejected==1);CHECK(cpu==4+restart_hold+recovery_cpu);CHECK(gpu==8-restart_hold-recovery_cpu);
   CHECK(frames>=12);
   if(strcmp(argv[2],"gl")==0){CHECK(gpu==0);CHECK(cpu>=12);}
   else {CHECK(gpu>=(continuous?6u:7u));CHECK(cpu>=4);}
   CHECK(samples>0);
   quiet_input_end();
   main_exit(NULL);
   if(pcm_file){CHECK(fclose(pcm_file)==0);pcm_file=NULL;}
   printf("window movie: %u GPU, %u CPU, %lu audio samples, %u failures\n",gpu,cpu,(unsigned long)samples,failures);
   return failures?1:0;
}
