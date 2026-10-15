
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string/stdstring.h>
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
/* ram[7] as the core has it now. Read rather than worked out from the
 * frame counter: a runahead or preempt restore moves the counter back
 * without moving the byte. */
static unsigned ram7(command_t *cmd)
{
   char *sp;
   reply[0]=0;
   if (!command_read_memory(cmd,"1007 1") || !(sp=strrchr(reply,' ')))
      return 0x100;
   return (unsigned)strtoul(sp+1,NULL,16);
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

   /* STATE answers as STATE, not as the command it runs */
   reply[0]=0;
   CHECK(!command_state(&cmd,"lo") && !strcmp(reply,"STATE ERROR one of save|load"));
   reply[0]=0;
   CHECK(!command_state(&cmd,"loads 1") && !strcmp(reply,"STATE ERROR one of save|load"));
   reply[0]=0;
   CHECK(!command_state(&cmd,"load") && !strcmp(reply,"STATE ERROR a slot number, not nothing"));
   strlcpy(loop->name.savestate,"command_frames_test.state",
         sizeof(loop->name.savestate));
   reply[0]=0;
   CHECK(command_state(&cmd," save  1"));
   for (i=0;i<8 && !reply[0];i++) { runloop_iterate(); task_queue_check(); }
   CHECK(!strcmp(reply,"STATE save 1"));
   remove("command_frames_test.state1");

   /* the other families answer under their own names too */
   reply[0]=0;
   CHECK(!command_replay(&cmd,"seek 600") && !strncmp(reply,"REPLAY ERROR ",13));
   cmd.structured=true;
   reply[0]=0;
   CHECK(command_menu(&cmd,"up") && !strcmp(reply,"MENU up pressed\n"));
   /* a command that answers nothing of its own answers as the tool */
   reply[0]=0;
   CHECK(command_reinit(&cmd,"audio") && !strcmp(reply,"REINIT audio\n"));
   /* this core has no disks: the hotkeys would do nothing */
   reply[0]=0;
   CHECK(!command_disk(&cmd,"next")
         && !strcmp(reply,"DISK ERROR this core has no disk control"));
   cmd.structured=false;
   cmd.error=false;
   /* and the commands they run, by name, still answer as themselves */
   reply[0]=0;
   CHECK(!command_save_state_slot(&cmd,"x")
         && !strncmp(reply,"SAVE_STATE_SLOT ERROR ",22));
   cmd.error=false; /* command_run clears it; these call the handlers */

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
   /* a trailing space, or a byte that is not one, writes nothing past
    * the bytes given rather than zeroes to the end of the region */
   CHECK(command_write_memory(&cmd,"1000 aa "));
   reply[0]=0;
   CHECK(!command_write_memory(&cmd,"1000 bb zz")
         && strstr(reply,"WRITE_CORE_MEMORY ERROR"));
   reply[0]=0;
   CHECK(!command_write_memory(&cmd,"1000 1ff")
         && strstr(reply,"WRITE_CORE_MEMORY ERROR"));
   cmd.error=false; reply[0]=0;
   CHECK(command_read_memory(&cmd,"1000 3")
         && !strcmp(reply,"READ_CORE_MEMORY 1000 AA 0F 10\n"));
   /* a second space after the command's name is still taken */
   reply[0]=0;
   CHECK(command_read_memory(&cmd," 1001 1")
         && !strcmp(reply,"READ_CORE_MEMORY 1001 0F\n"));
   /* an address or count past 32 bits is refused, not wrapped */
   reply[0]=0;
   CHECK(!command_read_memory(&cmd,"100001000 1")
         && strstr(reply,"READ_CORE_MEMORY ERROR"));
   reply[0]=0;
   CHECK(!command_read_memory(&cmd,"1000 4294967297")
         && strstr(reply,"READ_CORE_MEMORY ERROR"));
   reply[0]=0;
   CHECK(!command_write_memory(&cmd,"100001000 cc")
         && strstr(reply,"WRITE_CORE_MEMORY ERROR"));
   cmd.error=false;
   CHECK(command_write_memory(&cmd,"1000 00"));
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

   /* GET_INPUT reads what a player holds without changing it, which
    * SET_INPUT cannot do and RELEASE_INPUT undoes before answering */
   CHECK(command_release_input(&cmd,""));
   reply[0]=0;
   CHECK(command_set_input(&cmd,"1 a 1") && !strcmp(reply,"1\ta\tnone\n"));
   reply[0]=0;
   CHECK(command_get_input(&cmd,"") && !strcmp(reply,"1\ta\tnone\n"));
   reply[0]=0;
   CHECK(command_get_input(&cmd,"1") && !strcmp(reply,"1\ta\tnone\n"));
   CHECK(command_release_input(&cmd,""));
   reply[0]=0;
   CHECK(command_get_input(&cmd,"") && !strcmp(reply,"1\tnone\tnone\n"));
   /* a second space before a count is still taken */
   reply[0]=0;
   CHECK(command_run_frames(&cmd," 1"));
   runloop_iterate();
   CHECK(!strcmp(reply,"RUN_FRAMES 1\n"));
   reply[0]=0;
   CHECK(!command_get_input(&cmd,"99") && strstr(reply,"GET_INPUT ERROR"));

   /* A core path that is not a core is found out before anything is
    * torn down for it: the cfg this test wrote is a real file and not a
    * library, and the content running stays running. */
   reply[0]=0;
   CHECK(!command_load_core(&cmd,"/no/such/core.so")
         && strstr(reply,"no core at"));
   reply[0]=0;
   CHECK(!command_load_core(&cmd,cfg_path)
         && strstr(reply,"is not a libretro core"));
   reply[0]=0;
   CHECK(!command_load_core(&cmd,"") && strstr(reply,"a core path"));
   {
      char req[1024];
      snprintf(req,sizeof(req),"%s|%s",cfg_path,cfg_path);
      reply[0]=0;
      CHECK(!command_load_content(&cmd,req)
            && strstr(reply,"is not a libretro core"));
   }
   CHECK(loop->flags & RUNLOOP_FLAG_CORE_RUNNING);
   CHECK(loop->current_core_type != CORE_TYPE_DUMMY);
   /* asking after the core already loaded opens the same library,
    * whose retro_set_environment must not be called mid-session */
   command_load_core(&cmd,argv[1]);
   cmd.error=false; reply[0]=0;
   CHECK(command_read_memory(&cmd,"1009 1")
         && !strcmp(reply,"READ_CORE_MEMORY 1009 00\n"));

   /* PRESS lets go only of what it pressed itself: a button a client
    * holds with SET_INPUT is still held once the press ends. GET_INPUT
    * is what makes this checkable. */
   CHECK(command_release_input(&cmd,""));
   CHECK(command_set_input(&cmd,"1 a 1"));
   CHECK(command_press(&cmd,"1 a 2"));
   runloop_iterate(); runloop_iterate();
   reply[0]=0;
   CHECK(command_get_input(&cmd,"") && !strcmp(reply,"1\ta\tnone\n"));
   reply[0]=0;
   CHECK(command_press(&cmd,"1 a+b 2"));
   runloop_iterate(); runloop_iterate();
   reply[0]=0;
   CHECK(command_get_input(&cmd,"") && !strcmp(reply,"1\ta\tnone\n"));
   CHECK(command_release_input(&cmd,""));
   reply[0]=0;
   CHECK(command_press(&cmd,"1 b 1"));
   runloop_iterate();
   reply[0]=0;
   CHECK(command_get_input(&cmd,"") && !strcmp(reply,"1\tnone\tnone\n"));

   /* a family of commands as one: the word picks the command, and only
    * the ones that take a number take what follows */
   reply[0]=0; cmd.error=false;
   CHECK(!command_menu(&cmd,"toggle") && strstr(reply,"MENU ERROR one of up|"));
   reply[0]=0;
   CHECK(!command_menu(&cmd,"up 5") && strstr(reply,"nothing after up"));
   reply[0]=0;
   CHECK(!command_replay(&cmd,"record 3") && strstr(reply,"nothing after record"));
   reply[0]=0;
   CHECK(!command_state(&cmd,"keep 1") && strstr(reply,"one of save|load"));
   /* a slot past 32 bits is refused, not taken as slot 1 */
   reply[0]=0;
   CHECK(!command_state(&cmd,"load 4294967297") && strstr(reply,"ERROR"));
   reply[0]=0;
   CHECK(!command_state(&cmd,"save -1") && strstr(reply,"ERROR"));
   cmd.error=false;

   /* WATCH carries a look along with every step. Known bytes first:
    * the memory tests above wrote over what retro_init left. */
   CHECK(command_write_memory(&cmd,"1001 11 22 33"));
   reply[0]=0;
   CHECK(command_watch(&cmd,"") && !strcmp(reply,"WATCH none\n"));
   reply[0]=0;
   CHECK(command_watch(&cmd,"one 1001") && !strcmp(reply,"one\t1001\t17\n"));
   CHECK(command_watch(&cmd,"two 1002"));
   /* several pairs in one request, taken or refused together */
   reply[0]=0;
   CHECK(command_watch(&cmd,"two off  one 1002 two 1003")
         && !strcmp(reply,"one\t1002\t34\ntwo\t1003\t51\n"));
   reply[0]=0;
   CHECK(!command_watch(&cmd,"one 1001 x 99000") && strstr(reply,"x: "));
   reply[0]=0;
   CHECK(command_watch(&cmd,"")
         && !strcmp(reply,"one\t1002\t34\ntwo\t1003\t51\n"));
   reply[0]=0;
   CHECK(!command_watch(&cmd,"one 1001 two") && strstr(reply,"WATCH ERROR"));
   reply[0]=0;
   CHECK(command_watch(&cmd,"one 1001 two 1002")
         && !strcmp(reply,"one\t1001\t17\ntwo\t1002\t34\n"));
   reply[0]=0;
   CHECK(command_run_frames(&cmd,"1"));
   runloop_iterate();
   CHECK(!strcmp(reply,"RUN_FRAMES 1\none\t1001\t17\ntwo\t1002\t34\n"));
   reply[0]=0;
   CHECK(command_press(&cmd,"1 b 1"));
   runloop_iterate();
   CHECK(!strcmp(reply,"PRESS 1 b 1\none\t1001\t17\ntwo\t1002\t34\n"));
   /* a failure is its reason, not a reading */
   reply[0]=0;
   CHECK(command_run_frames(&cmd,"1"));
   command_run_frames_cancel("tidying up");
   CHECK(!strcmp(reply,"RUN_FRAMES ERROR tidying up"));
   /* a label keeps its place when its address changes */
   reply[0]=0;
   CHECK(command_watch(&cmd,"one 1003")
         && !strcmp(reply,"one\t1003\t51\ntwo\t1002\t34\n"));
   /* RUN_UNTIL opens with how it ended and closes with its own byte */
   reply[0]=0;
   CHECK(command_run_until(&cmd,"1002 == 34") && !strcmp(reply,
         "MET 0\none\t1003\t51\ntwo\t1002\t34\nVALUE 34\n"));
   /* RUN_UNTIL and the watches look the byte up every frame, so a
    * core that moves its memory is followed, and one that stops mapping
    * it is told apart from a byte that holds a value */
   {
      uint8_t alt[16];
      rarch_memory_descriptor_t *d = &loop->system.mmaps.descriptors[0];
      void *was = d->core.ptr;
      memcpy(alt,was,sizeof(alt));
      alt[2]=99;
      reply[0]=0;
      CHECK(command_run_until(&cmd,"1002 == 99") && !reply[0]);
      d->core.ptr=alt;
      runloop_iterate();
      CHECK(!strcmp(reply,"MET 1\none\t1003\t51\ntwo\t1002\t99\nVALUE 99\n"));
      reply[0]=0;
      CHECK(command_run_until(&cmd,"1002 == 1") && !reply[0]);
      d->core.ptr=NULL;
      runloop_iterate();
      CHECK(!strcmp(reply,"RUN_UNTIL ERROR nothing maps the address waited on now")
            && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      reply[0]=0;
      CHECK(command_watch(&cmd,"")
            && !strcmp(reply,"one\t1003\tnone\ntwo\t1002\tnone\n"));
      d->core.ptr=was;
      /* a later RUN_FRAMES carries no VALUE of its own */
      reply[0]=0;
      CHECK(command_run_frames(&cmd,"1"));
      runloop_iterate();
      CHECK(!strcmp(reply,"RUN_FRAMES 1\none\t1003\t51\ntwo\t1002\t34\n"));
   }
   reply[0]=0;
   CHECK(command_watch(&cmd,"one off") && !strcmp(reply,"two\t1002\t34\n"));
   /* and what it refuses */
   reply[0]=0;
   CHECK(!command_watch(&cmd,"x 99000") && strstr(reply,"WATCH ERROR"));
   CHECK(!command_watch(&cmd,"x zz") && strstr(reply,"hexadecimal"));
   CHECK(!command_watch(&cmd,"nospace")
         && !strcmp(reply,"WATCH ERROR nospace needs an address or \"off\" after it"));
   CHECK(!command_watch(&cmd,"one 1001 odd ") && strstr(reply,"odd needs an address"));
   /* past 32 bits is refused, not wrapped onto 1001 */
   reply[0]=0;
   CHECK(!command_watch(&cmd,"x 100001001") && strstr(reply,"WATCH ERROR"));
   CHECK(!command_watch(&cmd,"x -1") && strstr(reply,"hexadecimal"));
   /* a tab in a label would split its row */
   reply[0]=0;
   CHECK(!command_watch(&cmd,"x\ty 1001") && strstr(reply,"WATCH ERROR"));
   reply[0]=0;
   CHECK(command_watch(&cmd,"two 1002 ") && !strcmp(reply,"two\t1002\t34\n"));
   {
      unsigned w;
      char req[32];
      /* room for eight, counted from empty */
      CHECK(command_watch(&cmd,"two off"));
      for (w = 0; w < 8; w++)
      {
         snprintf(req,sizeof(req),"w%u 100%u",w,w);
         CHECK(command_watch(&cmd,req));
      }
      reply[0]=0;
      CHECK(!command_watch(&cmd,"w9 1009")
            && strstr(reply,"at most 8 bytes watched"));
      /* room made earlier in the same request counts */
      CHECK(command_watch(&cmd,"w7 off w9 1009 w9 off w7 1007"));
      /* but replacing one of the eight still works */
      CHECK(command_watch(&cmd,"w0 1005"));
      for (w = 0; w < 8; w++)
      {
         snprintf(req,sizeof(req),"w%u off",w);
         CHECK(command_watch(&cmd,req));
      }
      reply[0]=0;
      CHECK(command_watch(&cmd,"") && !strcmp(reply,"WATCH none\n"));
   }

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

   /* RUN_UNTIL waits on a byte the core maps, answered the frame it
    * meets what was asked. ram[7] follows the frame counter. */
   {
      char req[64];
      unsigned now;
      /* what it already holds: no frame need run */
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"1007 == %u",now);
      CHECK(command_run_until(&cmd,req));
      CHECK(!strncmp(reply,"MET 0\nVALUE ",STRLEN_CONST("MET 0\nVALUE "))
            && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      /* three frames away: answered on the third, not before */
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"1007 == %u",(now + 3) & 0xff);
      CHECK(command_run_until(&cmd,req) && !reply[0]);
      runloop_iterate();
      CHECK(!reply[0]);
      runloop_iterate();
      CHECK(!reply[0]);
      runloop_iterate();
      {
         char want[64];
         snprintf(want,sizeof(want),"MET 3\nVALUE %u\n",(now + 3) & 0xff);
         CHECK(!strcmp(reply,want));
      }
      /* the frames run out first: how far it got, to be asked again */
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"1007 == %u 2",(now + 90) & 0xff);
      CHECK(command_run_until(&cmd,req) && !reply[0]);
      runloop_iterate();
      runloop_iterate();
      CHECK(!strncmp(reply,"WAITING 2\nVALUE ",STRLEN_CONST("WAITING 2\nVALUE ")));
      /* left to itself it waits nine seconds of the content's frames,
       * 540 at this core's 60fps, not the 600 a request may ask for */
      {
         unsigned n=0;
         reply[0]=0;
         CHECK(command_run_until(&cmd,"1007 > 255") && !reply[0]);
         while (!reply[0] && n < 700) { runloop_iterate(); n++; }
         CHECK(n == 540 && !strncmp(reply,"WAITING 540\nVALUE ",
                  STRLEN_CONST("WAITING 540\nVALUE ")));
      }
      /* every condition it takes */
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"1007 != %u",(now + 1) & 0xff);
      CHECK(command_run_until(&cmd,req) && !strncmp(reply,"MET 0\nVALUE ",STRLEN_CONST("MET 0\nVALUE ")));
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"1007 >= %u",now);
      CHECK(command_run_until(&cmd,req) && !strncmp(reply,"MET 0\nVALUE ",STRLEN_CONST("MET 0\nVALUE ")));
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"1007 <= %u",now);
      CHECK(command_run_until(&cmd,req) && !strncmp(reply,"MET 0\nVALUE ",STRLEN_CONST("MET 0\nVALUE ")));
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"1007 < %u",(now + 1) & 0xff);
      CHECK(now == 0xff || (command_run_until(&cmd,req) && !strncmp(reply,"MET 0\nVALUE ",STRLEN_CONST("MET 0\nVALUE "))));
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"1007 > %u",(now + 0xff) & 0xff);
      CHECK(now == 0 || (command_run_until(&cmd,req) && !strncmp(reply,"MET 0\nVALUE ",STRLEN_CONST("MET 0\nVALUE "))));
      /* an address past 32 bits, not wrapped onto 1007 */
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"100001007 == 0")
            && strstr(reply,"hexadecimal address"));
      /* a value past what a byte holds, however far past */
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"1007 == 4294967296")
            && strstr(reply,"0 to 255"));
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"1007 == 0 4294967297")
            && strstr(reply,"at most 600"));
      /* and what it refuses */
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"1007 ~= 5")
            && strstr(reply,"RUN_UNTIL ERROR"));
      CHECK(!command_run_until(&cmd,"1007 == 300")
            && strstr(reply,"0 to 255"));
      CHECK(!command_run_until(&cmd,"99000 == 0")
            && strstr(reply,"RUN_UNTIL ERROR"));
      CHECK(!command_run_until(&cmd,"1007 == 0 601")
            && strstr(reply,"at most 600"));
      CHECK(!command_run_until(&cmd,"1007 == 0 5 junk")
            && strstr(reply,"RUN_UNTIL ERROR"));
      /* one frame job at a time, as RUN_FRAMES and PRESS are */
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"1007 == %u",(now + 90) & 0xff);
      CHECK(command_run_until(&cmd,req));
      CHECK(command_run_frames(&cmd,"1")
            && strstr(reply,"RUN_UNTIL ERROR replaced by another RUN_FRAMES"));
      runloop_iterate();
      CHECK(!strcmp(reply,"RUN_FRAMES 1\n"));
   }

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
   /* closing the content drops the watches: the next content's
    * addresses mean something else */
   CHECK(command_watch(&cmd,"two 1002"));
   command_event(CMD_EVENT_UNLOAD_CORE,NULL);
   for (i=0;i<8;i++) { runloop_iterate(); task_queue_check(); }
   reply[0]=0;
   CHECK(command_watch(&cmd,"") && !strcmp(reply,"WATCH none\n"));
   main_exit(NULL); remove(cfg_path);
   printf("command_frames: %s (%u failures)\n",failures?"FAIL":"PASS",failures);
   return failures ? 1 : 0;
}
