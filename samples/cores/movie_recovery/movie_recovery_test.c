#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <file/config_file.h>
#include <time/rtime.h>
#include <verbosity.h>
#include "../../../retroarch.h"
#include "../../../command.h"
#include "../../../runloop.h"
#include "../../../configuration.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../gfx/video_driver.h"
#include "../../../audio/audio_driver.h"
#include "../../../tasks/tasks_internal.h"
static const video_poke_interface_t *real_poke;
static unsigned reject_next, rejected;
static enum video_texture_update reject_movie(void *data,const video_frame_import_t *frame)
{
   if(frame && reject_next){reject_next=0;rejected++;return VIDEO_TEXTURE_UPDATE_REFUSED;}
   return real_poke->import_movie_frame(data,frame);
}
static unsigned frames,gpu,cpu,failures;
static uint64_t samples;
#define CHECK(c) do { if(!(c)){fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#c);failures++;} } while(0)
static void video(const void *data,unsigned w,unsigned h,size_t pitch)
{
   if(data==RETRO_HW_FRAME_BUFFER_VALID || (!data && video_state_get_ptr()->movie_frame))gpu++;
   else if(data)cpu++;
   frames++;
   video_driver_frame(data,w,h,pitch);
}
static size_t audio(const int16_t *data,size_t count)
{ samples+=count;return audio_driver_sample_batch(data,count); }
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
   fprintf(stderr,"capture %u: driver=%s viewport=%ux%u reader=%u\n",index,v->current_video->ident,w,h,v->current_video->read_viewport?1:0);
   CHECK(w && h && w<=2048 && h<=2048 && v->current_video->read_viewport);
   if(!w || !h || w>2048 || h>2048 || !v->current_video->read_viewport)return;
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
   char cfg_path[1024];char *args[6];FILE *f;unsigned i;
   runloop_state_t *loop;
   video_poke_interface_t fault_poke;
   if(argc!=4)return 2;
   snprintf(cfg_path,sizeof(cfg_path),"%s/window.cfg",argv[3]);
   f=fopen(cfg_path,"wb");if(!f)return 2;
   fprintf(f,"video_driver = \"%s\"\n",argv[2]);
   fputs("audio_driver = \"null\"\ninput_driver = \"null\"\ninput_joypad_driver = \"null\"\nmenu_driver = \"rgui\"\nvideo_threaded = \"false\"\nvideo_vsync = \"false\"\nvideo_fullscreen = \"false\"\nvideo_scale = \"1\"\nvideo_smooth = \"false\"\npause_nonactive = \"false\"\nconfig_save_on_exit = \"false\"\nthreaded_data_runloop_enable = \"false\"\n",f);fclose(f);
   f=fopen(cfg_path,"ab");if(!f)return 2;
   fputs("video_window_custom_size_enable = \"true\"\nvideo_window_width = \"256\"\nvideo_window_height = \"192\"\nvideo_window_save_positions = \"false\"\nvideo_font_enable = \"false\"\n",f);fclose(f);
   verbosity_enable();
   config_file_set_io_default(config_file_io_filestream());rtime_init();retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE,NULL);frontend_driver_init_first(NULL);
   args[0]=(char*)"retroarch";args[1]=(char*)"--config";args[2]=cfg_path;args[3]=argv[1];args[4]=NULL;
   if(!retroarch_main_init(4,args))return 2;
   loop=runloop_state_get_ptr();CHECK(loop->current_core_type==CORE_TYPE_WEBM);
   CHECK(strcmp(video_state_get_ptr()->current_video->ident,argv[2])==0);
   loop->current_core.retro_set_video_refresh(video);loop->current_core.retro_set_audio_sample_batch(audio);
   real_poke=video_state_get_ptr()->poke;CHECK(real_poke && real_poke->import_movie_frame);
   fault_poke=*real_poke;fault_poke.import_movie_frame=reject_movie;
   video_state_get_ptr()->poke=&fault_poke;
   for(i=0;i<12;i++)
   {
      if(i==4)reject_next=1;
      /* CPU transition clears the frontend refusal latch. */
      if(i==9)
      {
         video_state_get_ptr()->poke=real_poke;
         CHECK(command_event(CMD_EVENT_REINIT,NULL));
         CHECK(!video_state_get_ptr()->movie_refused);
         loop->current_core.retro_set_video_refresh(video);
         loop->current_core.retro_set_audio_sample_batch(audio);
      }
      CHECK(runloop_iterate()>=0);task_queue_check();capture(i,argv[3]);
   }
   CHECK(rejected==1);CHECK(cpu==4);CHECK(gpu==8);
   CHECK(frames>=12);
   if(strcmp(argv[2],"gl")==0){CHECK(gpu==0);CHECK(cpu>=12);}
   else {CHECK(gpu>=7);CHECK(cpu>=4);}
   CHECK(samples>0);
   main_exit(NULL);
   printf("window movie: %u GPU, %u CPU, %lu audio samples, %u failures\n",gpu,cpu,(unsigned long)samples,failures);
   return failures?1:0;
}
