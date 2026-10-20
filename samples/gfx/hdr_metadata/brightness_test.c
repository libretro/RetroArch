#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <file/config_file.h>
#include <time/rtime.h>
#include <verbosity.h>
#include <formats/rpng.h>
#include "../../../gfx/video_shader_parse.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../configuration.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../gfx/video_driver.h"
#include "../../../audio/audio_driver.h"
#include "../../../tasks/tasks_internal.h"
static unsigned frames,gpu,cpu,failures;
static int probe_sdr;
static uint64_t samples;
#define CHECK(c) do { if(!(c)){fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#c);failures++;} } while(0)
static void video(const void *data,unsigned w,unsigned h,size_t pitch)
{
   if(data==RETRO_HW_FRAME_BUFFER_VALID)gpu++;
   else if(data)cpu++;
   frames++;
   video_driver_frame(data,w,h,pitch);
}
static size_t audio(const int16_t *data,size_t count)
{ samples+=count;return audio_driver_sample_batch(data,count); }
static void capture(unsigned index,const char *out)
{
 video_driver_state_t *v=video_state_get_ptr();
 struct video_viewport vp; struct rpng_hdr_metadata meta;
 uint16_t *p; unsigned w,h; char name[1024]; FILE *f;
 memset(&vp,0,sizeof(vp)); CHECK(video_driver_get_viewport_info(&vp));
 w=VIDEO_SCALE_W(vp.dims);h=VIDEO_SCALE_H(vp.dims);
 CHECK(w && h && w<4096 && h<4096 && v->current_video->read_viewport_hdr);
 if(!w || !h || w>=4096 || h>=4096 || !v->current_video->read_viewport_hdr)return;
 p=(uint16_t*)calloc((size_t)w*h,6);CHECK(p!=NULL);if(!p)return;
 memset(&meta,0,sizeof(meta));
 if(probe_sdr)
 {
    uint8_t *b=(uint8_t*)malloc((size_t)w*h*3);unsigned n;
    CHECK(b && v->current_video->read_viewport);
    if(!b || !v->current_video->read_viewport || !v->current_video->read_viewport(v->data,b,false)){CHECK(false);free(b);free(p);return;}
    for(n=0;n<w*h;n++){p[n*3]=(uint16_t)(b[n*3+2]*257);p[n*3+1]=(uint16_t)(b[n*3+1]*257);p[n*3+2]=(uint16_t)(b[n*3]*257);}
    free(b);meta.transfer_function=13;meta.colour_primaries=1;
 }
 else if(!v->current_video->read_viewport_hdr(v->data,p,false,&meta)){CHECK(false);free(p);return;}
 CHECK(meta.transfer_function==(probe_sdr?13:16));
 snprintf(name,sizeof(name),"%s/frame-%02u.bin",out,index);f=fopen(name,"wb");CHECK(f!=NULL);
 if(f){fwrite(p,6,(size_t)w*h,f);fclose(f);}
 printf("HDR_CAPTURE %u %u %u primaries=%u transfer=%u\n",index,w,h,meta.colour_primaries,meta.transfer_function);
 free(p);
}
int main(int argc,char **argv)
{
   char cfg_path[1024];char *args[8];FILE *f;unsigned i;
   runloop_state_t *loop;
   const char *overlay;
   if(argc!=9)return 2;
   probe_sdr=atoi(argv[7])==0;
   snprintf(cfg_path,sizeof(cfg_path),"%s/window.cfg",argv[3]);
   f=fopen(cfg_path,"wb");if(!f)return 2;
   fprintf(f,"video_driver = \"%s\"\n",argv[2]);
   fputs("audio_driver = \"null\"\ninput_driver = \"null\"\ninput_joypad_driver = \"null\"\nmenu_driver = \"rgui\"\nvideo_threaded = \"false\"\nvideo_vsync = \"true\"\nvideo_fullscreen = \"false\"\nvideo_scale = \"1\"\nvideo_smooth = \"false\"\npause_nonactive = \"false\"\nconfig_save_on_exit = \"false\"\nthreaded_data_runloop_enable = \"false\"\n",f);fclose(f);
   f=fopen(cfg_path,"ab");if(!f)return 2;
   fputs("video_window_custom_size_enable = \"true\"\nvideo_window_width = \"256\"\nvideo_window_height = \"192\"\nvideo_window_save_positions = \"false\"\nvideo_font_enable = \"false\"\n",f);fclose(f);
   f=fopen(cfg_path,"ab"); if(!f)return 2;
   fprintf(f,"video_hdr_mode = \"%s\"\nvideo_hdr_paper_white_nits = \"%s\"\nvideo_hdr_expand_gamut = \"%s\"\n",argv[7],argv[4],argv[5]); fclose(f);
   if(strcmp(argv[6],"none"))
   {
      f=fopen(cfg_path,"ab");if(!f)return 2;
      fputs("video_shader_enable = \"true\"\n",f);fclose(f);
   }
   else
   {
      f=fopen(cfg_path,"ab");if(!f)return 2;
      fputs("video_shader_enable = \"false\"\n",f);fclose(f);
   }
   overlay=getenv("HDR_PROBE_OVERLAY");
   if(overlay && *overlay)
   {
      f=fopen(cfg_path,"ab");if(!f)return 2;
      fprintf(f,"input_overlay = \"%s\"\ninput_overlay_enable = \"true\"\ninput_overlay_hide_when_gamepad_connected = \"false\"\ninput_overlay_opacity = \"1.0\"\nvideo_hdr_menu_nits = \"150\"\n",overlay);fclose(f);
   }
   verbosity_enable();
   config_file_set_io_default(config_file_io_filestream());rtime_init();retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE,NULL);frontend_driver_init_first(NULL);
   args[0]=(char*)"retroarch";args[1]=(char*)"--config";args[2]=cfg_path;args[3]=(char*)"-L";args[4]=argv[1];args[5]=NULL;
   if(strcmp(argv[6],"none")){args[5]=(char*)"--set-shader";args[6]=argv[6];args[7]=NULL;}
   if(!retroarch_main_init(strcmp(argv[6],"none")?7:5,args))return 2;
   loop=runloop_state_get_ptr();
   CHECK(strcmp(video_state_get_ptr()->current_video->ident,argv[2])==0);
   loop->current_core.retro_set_video_refresh(video);loop->current_core.retro_set_audio_sample_batch(audio);
   for(i=0;i<12;i++){CHECK(runloop_iterate()>=0);task_queue_check();fprintf(stderr,"phase capture %u\n",i);fflush(stderr);capture(i,argv[3]);}
   if(strcmp(argv[6],"none"))
   {
      video_shader_ctx_t shader;memset(&shader,0,sizeof(shader));
      CHECK(video_shader_driver_get_current_shader(&shader));
      CHECK(shader.data && shader.data->passes==(unsigned)atoi(argv[8]));
      if(shader.data)printf("PRESET_ACTIVE passes=%u\n",shader.data->passes);
   }
   CHECK(frames>=12);
   CHECK(cpu>=12);
   main_exit(NULL);
   printf("HDR brightness: %u GPU, %u CPU, %lu audio samples, %u failures\n",gpu,cpu,(unsigned long)samples,failures);
   return failures?1:0;
}
