
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <time/rtime.h>
#include "../../../runloop.h"
#include "../../../retroarch.h"
#include "../../../configuration.h"
#include "../../../command.h"
#include "../../../input/input_driver.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../runahead.h"
#include "../../../network/netplay/netplay.h"
#include "../../../menu/menu_driver.h"
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #c); failures++; } } while (0)
static bool blocked;
static unsigned calls, gates;
static void (*loaded_run)(void);
static char reply[256];
static void counted_run(void) { calls++; loaded_run(); }
static bool fail_save(void *data, size_t size) { (void)data; (void)size; return false; }
bool __real_netplay_driver_ctl(enum rarch_netplay_ctl_state, void*);
bool __wrap_netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{
   if (state == RARCH_NETPLAY_CTL_PRE_FRAME && blocked)
   { gates++; return false; }
   return __real_netplay_driver_ctl(state, data);
}
static void receive(command_t *cmd, const char *data, size_t n)
{
   (void)cmd;
   if (n >= sizeof(reply)) n = sizeof(reply)-1;
   memcpy(reply,data,n); reply[n]=0;
}
int main(int argc, char **argv)
{
   char cfg_path[512];
   char *args[7];
   FILE *cfg;
   unsigned i, before;
   int16_t axes[4];
   command_t cmd;
   runloop_state_t *loop;
   if (argc < 2) return 2;
   snprintf(cfg_path,sizeof(cfg_path),"%s.cfg",argv[0]);
   cfg=fopen(cfg_path,"wb"); if (!cfg) return 2;
   fputs("video_driver = \"null\"\naudio_driver = \"null\"\ninput_driver = \"null\"\ninput_joypad_driver = \"null\"\nmenu_driver = \"rgui\"\nvideo_threaded = \"false\"\nvideo_vsync = \"false\"\npause_nonactive = \"false\"\nconfig_save_on_exit = \"false\"\nthreaded_data_runloop_enable = \"false\"\n",cfg);
   fclose(cfg);
   config_file_set_io_default(config_file_io_filestream());
   rtime_init(); retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE,NULL);
   frontend_driver_init_first(NULL);
   args[0]=(char*)"retroarch"; args[1]=(char*)"--config"; args[2]=cfg_path;
   args[3]=(char*)"-L"; args[4]=argv[1]; args[5]=NULL;
   if (!retroarch_main_init(5,args)) return 2;
   for (i=0;i<5;i++) runloop_iterate();
   loop=runloop_state_get_ptr();
   if (loop->current_core_type == CORE_TYPE_DUMMY) return 2;
   loaded_run=loop->current_core.retro_run;
   loop->current_core.retro_run=counted_run;
   memset(&cmd,0,sizeof(cmd)); cmd.replier=receive;
   before=calls; blocked=true;
   if (!command_run_frames(&cmd,"3")) return 2;
   for (i=0;i<3;i++) runloop_iterate();
   printf("blocked: core_calls=%u gate_denials=%u pending=%u reply=%s\n",calls-before,gates,
         !!(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES),reply);
   CHECK(calls == before && gates == 3 && !reply[0]);
   CHECK(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES);
   blocked=false;
   before=calls; reply[0]=0;
   for (i=0;i<3;i++) runloop_iterate();
   printf("unblocked: core_calls=%u pending=%u reply=%s\n",calls-before,
         !!(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES),reply);

   CHECK(calls - before == 3 && !strcmp(reply, "RUN_FRAMES 3\n"));
   CHECK(!(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
   before=calls;
   for (i=0;i<3;i++) runloop_iterate();
   CHECK(calls == before && (loop->flags & RUNLOOP_FLAG_PAUSED));
   {
      settings_t *settings=config_get_ptr();
      uint32_t *state=(uint32_t*)loop->current_core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
      unsigned base=*state;
      before=calls; reply[0]=0;
      runahead_clear_variables(loop);
      settings->bools.run_ahead_enabled=true;
      settings->bools.run_ahead_secondary_instance=false;
      settings->uints.run_ahead_frames=2;
      if (!command_run_frames(&cmd,"3")) return 2;
      for (i=0;i<3;i++) runloop_iterate();
      printf("runahead: core_calls=%u logical_frames=%u pending=%u reply=%s\n",calls-before,*state-base,
            !!(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES),reply);
      CHECK(calls-before == 9 && *state-base == 3 && !strcmp(reply,"RUN_FRAMES 3\n"));
      settings->bools.run_ahead_enabled=false;
      settings->bools.preemptive_frames_enable=true;
      preempt_deinit(loop);
      if (!preempt_init(loop)) return 2;
      base=*state; before=calls; reply[0]=0;
      if (!command_run_frames(&cmd,"3")) return 2;
      for (i=0;i<3;i++) runloop_iterate();
      printf("preempt: core_calls=%u logical_frames=%u pending=%u reply=%s\n",calls-before,*state-base,
            !!(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES),reply);

      CHECK(calls-before == 3 && *state-base == 3 && !strcmp(reply,"RUN_FRAMES 3\n"));
      CHECK(command_set_input(&cmd,"1 b 1"));
      base=*state; before=calls; reply[0]=0;
      CHECK(command_run_frames(&cmd,"3"));
      for (i=0;i<3;i++) runloop_iterate();
      printf("preempt replay: core_calls=%u logical_frames=%u reply=%s\n",calls-before,*state-base,reply);
      CHECK(calls-before > 3 && *state-base == 3 && !strcmp(reply,"RUN_FRAMES 3\n"));
      {
         bool (*save)(void*,size_t)=loop->current_core.retro_serialize;
         loop->current_core.retro_serialize=fail_save;
         before=calls; reply[0]=0;
         CHECK(command_run_frames(&cmd,"1"));
         runloop_iterate();
         CHECK(calls == before && !reply[0] && (loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
         loop->current_core.retro_serialize=save;
         runloop_iterate();
         CHECK(calls-before == 1 && !strcmp(reply,"RUN_FRAMES 1\n"));
      }
      CHECK(command_release_input(&cmd,""));
      preempt_deinit(loop);
   }

   /* READ_CORE_MEMORY and WRITE_CORE_MEMORY answer with the address and
    * the bytes in hexadecimal, at a system address the core maps */
   reply[0]=0;
   CHECK(command_read_memory(&cmd,"1000 4")
         && !strcmp(reply,"READ_CORE_MEMORY 1000 00 01 02 03\n"));
   reply[0]=0;
   CHECK(command_read_memory(&cmd,"1004 1")
         && !strcmp(reply,"READ_CORE_MEMORY 1004 04\n"));
   reply[0]=0;
   CHECK(command_write_memory(&cmd,"1002 ff fe")
         && !strcmp(reply,"WRITE_CORE_MEMORY 1002 2\n"));
   reply[0]=0;
   CHECK(command_read_memory(&cmd,"1000 4")
         && !strcmp(reply,"READ_CORE_MEMORY 1000 00 01 FF FE\n"));
   /* every byte value formats as two uppercase digits */
   CHECK(command_write_memory(&cmd,"1000 00 0f 10 a5 ff"));
   reply[0]=0;
   CHECK(command_read_memory(&cmd,"1000 5")
         && !strcmp(reply,"READ_CORE_MEMORY 1000 00 0F 10 A5 FF\n"));
   /* a read past the region it is in stops at its end */
   reply[0]=0;
   CHECK(command_read_memory(&cmd,"100e 8")
         && !strcmp(reply,"READ_CORE_MEMORY 100e 0E 0F\n"));
   /* and one outside every region says so, as it always has */
   reply[0]=0;
   CHECK(!command_read_memory(&cmd,"20000 1")
         && strstr(reply,"-1"));
   /* GET_MEMORY_MAP answers the region READ_CORE_MEMORY takes */
   reply[0]=0;
   CHECK(command_get_memory_map(&cmd,"")
         && !strcmp(reply,
            /* the select mask is RetroArch's, worked out from the
             * region where the core leaves it zero */
            "1000\t10\t1ff0\t0\tsystem\trw\tlittle\tram\n"));

   /* several controls at once */
   reply[0]=0;
   CHECK(command_set_input(&cmd,"1 b+a 1") && !strcmp(reply,"1\tb a\tnone\n"));
   CHECK(command_release_input(&cmd,""));
   reply[0]=0;
   CHECK(command_set_input(&cmd,"1 +a++b+ 1") && !strcmp(reply,"1\tb a\tnone\n"));
   CHECK(command_release_input(&cmd,""));
   reply[0]=0;
   CHECK(command_set_input(&cmd,"1 left_x+right_y 2000")
         && !strcmp(reply,"1\tnone\tleft_x=2000 right_y=2000\n"));
   CHECK(command_release_input(&cmd,""));
   reply[0]=0;
   CHECK(!command_set_input(&cmd,"1 + 1") && strstr(reply,"SET_INPUT ERROR"));
   /* a combination with something wrong in it holds nothing: the next
    * request answers with only what it asked for */
   reply[0]=0;
   CHECK(!command_set_input(&cmd,"1 a+nope 1") && strstr(reply,"nope"));
   reply[0]=0;
   CHECK(!command_set_input(&cmd,"1 a+left_x 50")
         && strstr(reply,"SET_INPUT ERROR"));
   reply[0]=0;
   CHECK(command_set_input(&cmd,"1 start 1")
         && !strcmp(reply,"1\tstart\tnone\n"));
   CHECK(command_release_input(&cmd,""));

   /* PRESS holds, runs its frames and lets go, answered on the last */
   reply[0]=0;
   CHECK(command_press(&cmd,"1 b+a 2"));
   CHECK(!reply[0] && input_driver_command_held(0,axes) == 0x0101);
   runloop_iterate();
   CHECK(!reply[0] && input_driver_command_held(0,axes) == 0x0101);
   runloop_iterate();
   CHECK(!strcmp(reply,"PRESS 1 b+a 2\n")
         && input_driver_command_held(0,axes) == 0);
   /* the controls as resolved, not as written */
   reply[0]=0;
   CHECK(command_press(&cmd,"1 +a++a+ 1"));
   runloop_iterate();
   CHECK(!strcmp(reply,"PRESS 1 a+a 1\n"));
   /* an axis is held at a value, so it is SET_INPUT's */
   reply[0]=0;
   CHECK(!command_press(&cmd,"1 left_x 2") && strstr(reply,"PRESS ERROR"));
   CHECK(!command_press(&cmd,"1 a 0") && strstr(reply,"frame count"));
   /* a press holds input for every frame it runs, so it is bounded by
    * what a client waits for, not by RUN_FRAMES' ten minutes */
   reply[0]=0;
   CHECK(!command_press(&cmd,"1 a 601") && strstr(reply,"SET_INPUT"));
   CHECK(command_press(&cmd,"1 a 600"));
   /* A line interface waits as long as it likes, so RUN_FRAMES keeps
    * the ten minutes there; a structured one is told at once rather
    * than failing once the frames are already running. */
   reply[0]=0;
   CHECK(command_run_frames(&cmd,"36000")
         && input_driver_command_held(0,axes) == 0);
   command_run_frames_cancel("tidying up");
   CHECK(!strcmp(reply,"RUN_FRAMES ERROR tidying up"));
   cmd.structured=true;
   reply[0]=0;
   CHECK(!command_run_frames(&cmd,"36000")
         && strstr(reply,"at most 600 frames in one request"));
   CHECK(command_run_frames(&cmd,"600"));
   command_run_frames_cancel("tidying up");
   cmd.structured=false;
   /* one at a time: the second replaces the first, which is answered */
   reply[0]=0;
   CHECK(command_press(&cmd,"1 a 30"));
   CHECK(command_press(&cmd,"1 b 1")
         && strstr(reply,"PRESS ERROR replaced by PRESS"));
   /* and what the replaced one held was let go of */
   CHECK(input_driver_command_held(0,axes) == 0x0001);
   runloop_iterate();
   CHECK(!strcmp(reply,"PRESS 1 b 1\n")
         && input_driver_command_held(0,axes) == 0);
   /* a press over a button SET_INPUT holds leaves that hold in place */
   reply[0]=0;
   CHECK(command_set_input(&cmd,"1 a 1"));
   CHECK(command_press(&cmd,"1 a+b 1")
         && input_driver_command_held(0,axes) == 0x0101);
   runloop_iterate();
   CHECK(!strcmp(reply,"PRESS 1 a+b 1\n")
         && input_driver_command_held(0,axes) == 0x0100);
   CHECK(command_release_input(&cmd,""));
   /* RUN_FRAMES and PRESS are the same one frame job */
   reply[0]=0;
   CHECK(command_press(&cmd,"1 a 30"));
   CHECK(command_run_frames(&cmd,"1")
         && strstr(reply,"PRESS ERROR replaced by another RUN_FRAMES")
         && input_driver_command_held(0,axes) == 0);
   runloop_iterate();
   CHECK(!strcmp(reply,"RUN_FRAMES 1\n"));

   before=calls; reply[0]=0;
   CHECK(command_run_frames(&cmd,"3"));
   runloop_iterate();
   blocked=true;
   for(i=0;i<3;i++) runloop_iterate();
   CHECK(calls-before == 1 && !reply[0] && (loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
   blocked=false;
   runloop_iterate(); runloop_iterate();
   CHECK(calls-before == 3 && !strcmp(reply,"RUN_FRAMES 3\n"));
   before=calls; reply[0]=0;
   CHECK(command_run_frames(&cmd,"1"));
   loop->current_core.retro_run=NULL;
   runloop_iterate();
   CHECK(calls==before && !reply[0] && (loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
   loop->current_core.retro_run=counted_run;
   runloop_iterate();
   CHECK(calls-before==1 && !strcmp(reply,"RUN_FRAMES 1\n"));
   main_exit(NULL); remove(cfg_path);
   printf("command_frames: %s (%u failures)\n",failures?"FAIL":"PASS",failures);
   return failures ? 1 : 0;
}
