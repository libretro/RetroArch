
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <time/rtime.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <retro_timers.h>
#include "../../../runloop.h"
#include "../../../retroarch.h"
#include "../../../configuration.h"
#include "../../../command.h"
#include "../../../input/input_driver.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../runahead.h"
#include "../../../gfx/video_driver.h"
#include "../../../network/netplay/netplay.h"
#include "../../../menu/menu_driver.h"
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #c); failures++; } } while (0)
static bool blocked;
static unsigned calls, gates;
static void (*loaded_run)(void);
static char reply[1024];
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
static size_t zero_size(void) { return 0; }
static void receive(command_t *cmd, const char *data, size_t n)
{
   (void)cmd;
   if (n >= sizeof(reply)) n = sizeof(reply)-1;
   memcpy(reply,data,n); reply[n]=0;
}
/* @line sent to the network command interface as a client would, read
 * through its own parser; the reply, if any, in @out. */
#define UDP_PORT 55437
static void udp_line(const char *line, char *out, size_t cap)
{
   static command_t *net;
   static int        fd = -1;
   static struct addrinfo *to;
   int  i;
   long n = -1;
   out[0] = '\0';
   if (!net)
   {
      network_init();
      net = command_network_new(UDP_PORT, "127.0.0.1");
      fd  = socket_init((void**)&to, UDP_PORT, "127.0.0.1",
            SOCKET_TYPE_DATAGRAM, AF_INET);
      if (fd >= 0)
         socket_nonblock(fd);
   }
   if (!net || fd < 0)
      return;
   sendto(fd, line, strlen(line), 0, to->ai_addr, to->ai_addrlen);
   for (i = 0; i < 200 && n <= 0; i++)
   {
      net->poll(net);
      n = (long)recvfrom(fd, out, cap - 1, 0, NULL, NULL);
      if (n <= 0)
         retro_sleep(1);
   }
   out[n > 0 ? n : 0] = '\0';
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
   CHECK(!command_state(&cmd,"lo") && !strcmp(reply,"STATE ERROR one of save|load|checkpoint|rollback"));
   reply[0]=0;
   CHECK(!command_state(&cmd,"loads 1") && !strcmp(reply,"STATE ERROR one of save|load|checkpoint|rollback"));
   reply[0]=0;
   CHECK(!command_state(&cmd,"load") && !strcmp(reply,"STATE ERROR a slot number, not nothing"));
   strlcpy(loop->name.savestate,"command_frames_test.state",
         sizeof(loop->name.savestate));
   reply[0]=0;
   CHECK(command_state(&cmd," save  1"));
   for (i=0;i<8 && !reply[0];i++) { runloop_iterate(); task_queue_check(); }
   CHECK(!strcmp(reply,"STATE save 1"));
   remove("command_frames_test.state1");
   /* a checkpoint in memory, and the rollback to it */
   reply[0]=0;
   CHECK(command_state(&cmd,"rollback"));
   for (i=0;i<8 && !reply[0];i++) { runloop_iterate(); task_queue_check(); }
   CHECK(!strncmp(reply,"STATE ERROR no checkpoint",25));
   cmd.error=false;
   {
      unsigned at;
      char want[32];
      reply[0]=0;
      CHECK(command_state(&cmd,"checkpoint"));
      for (i=0;i<8 && !reply[0];i++) { task_queue_check(); }
      CHECK(!strcmp(reply,"STATE checkpoint"));
      at=ram7(&cmd);
      CHECK(command_run_frames(&cmd,"5"));
      for (i=0;i<5;i++) runloop_iterate();
      reply[0]=0;
      CHECK(command_state(&cmd,"rollback"));
      for (i=0;i<8 && !reply[0];i++) { task_queue_check(); }
      CHECK(!strcmp(reply,"STATE rollback"));
      /* the frame counter is the harness core's whole state */
      reply[0]=0;
      CHECK(command_run_frames(&cmd,"1"));
      runloop_iterate();
      snprintf(want,sizeof(want),"%u",(at + 1) & 0xff);
      CHECK(ram7(&cmd) == ((at + 1) & 0xff));
      (void)want;
   }
#ifdef HAVE_BSV_MOVIE
   /* a recording would go on from a state it never saw: refused, as a
    * load from a slot is */
   reply[0]=0;
   CHECK(command_replay(&cmd,"record"));
   for (i=0;i<8 && !reply[0];i++) { runloop_iterate(); task_queue_check(); }
   if (input_state_get_ptr()->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING)
   {
      reply[0]=0;
      CHECK(command_state(&cmd,"rollback"));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      CHECK(!strncmp(reply,"STATE ERROR a replay is recording",33));
      reply[0]=0;
      command_replay(&cmd,"halt");
      for (i=0;i<4;i++) { runloop_iterate(); task_queue_check(); }
   }
   else
      printf("replay recording did not start here: rollback-while-recording not checked\n");
   cmd.error=false;
#endif
   reply[0]=0;
   CHECK(!command_state(&cmd,"checkpoint 3") && strstr(reply,"nothing after"));
   cmd.error=false;
   /* a reply longer than any fixed buffer still answers as the tool */
   {
      char long_path[700];
      memset(long_path,'d',600);
      strlcpy(long_path+600,"/missing.state",sizeof(long_path)-600);
      strlcpy(loop->name.savestate,long_path,sizeof(loop->name.savestate));
      size_t (*size)(void)=loop->current_core.retro_serialize_size;
      /* nothing to serialize: refused at once, naming the path */
      loop->current_core.retro_serialize_size=zero_size;
      reply[0]=0;
      command_state(&cmd,"save 1");
      for (i=0;i<8 && !reply[0];i++) { runloop_iterate(); task_queue_check(); }
      CHECK(strlen(reply) > 600 && !strncmp(reply,"STATE ERROR ",12));
      loop->current_core.retro_serialize_size=size;
      strlcpy(loop->name.savestate,"command_frames_test.state",
            sizeof(loop->name.savestate));
      cmd.error=false;
   }

   /* the other families answer under their own names too */
   reply[0]=0;
   CHECK(!command_replay(&cmd,"seek 600") && !strncmp(reply,"REPLAY ERROR ",13));
   cmd.structured=true;
   reply[0]=0;
   CHECK(command_menu(&cmd,"show") && !strcmp(reply,"CLOSED\n"));
   /* a command that answers nothing of its own answers as the tool */
   reply[0]=0;
   CHECK(command_reinit(&cmd,"audio") && !strcmp(reply,"REINIT audio\n"));
   /* RESET is a hotkey and an action: HELP describes it once */
   {
      char *nl;
      unsigned lines = 0;
      reply[0]=0;
      CHECK(command_help(&cmd,"RESET"));
      for (nl = reply; (nl = strchr(nl, '\n')); nl++)
         lines++;
      CHECK(lines == 1 && strstr(reply,"RESET"));
   }
   /* CORE stands for LOAD_CORE, START_CORE and UNLOAD_CORE */
   reply[0]=0;
   CHECK(!command_core(&cmd,"fly") && !strcmp(reply,"CORE ERROR one of load|start|unload"));
   reply[0]=0;
   CHECK(!command_core(&cmd,"start now") && !strcmp(reply,"CORE ERROR nothing after start"));
   reply[0]=0;
   CHECK(!command_core(&cmd,"load /no/such/core.so")
         && !strncmp(reply,"CORE ERROR no core at",21));
   cmd.error=false;
   /* this core has no disks: the hotkeys would do nothing */
   reply[0]=0;
   CHECK(!command_disk(&cmd,"next")
         && !strcmp(reply,"DISK ERROR this core has no disk control"));
   cmd.structured=false;
   cmd.error=false;
   /* and the commands they run, by name, still answer as themselves */
   reply[0]=0;
   {
      char net_reply[256];
      udp_line("SAVE_STATE_SLOT x\n", net_reply, sizeof(net_reply));
      CHECK(!strncmp(net_reply,"SAVE_STATE_SLOT ERROR ",22));
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
   /* a line client ending its lines in CRLF writes as one ending in LF */
   {
      char net_reply[256];
      udp_line("WRITE_CORE_MEMORY 1000 ab\r\n", net_reply, sizeof(net_reply));
      CHECK(!strcmp(net_reply,"WRITE_CORE_MEMORY 1000 1\n"));
      reply[0]=0;
      CHECK(command_read_memory(&cmd,"1000 1")
            && !strcmp(reply,"READ_CORE_MEMORY 1000 AB\n"));
      CHECK(command_write_memory(&cmd,"1000 aa"));
      /* only the \r before a line's end goes: one elsewhere is part of
       * the command, not a second one */
      udp_line("WRITE_CORE_MEMORY 1000 bb\rWRITE_CORE_MEMORY 1001 cc\n",
            net_reply, sizeof(net_reply));
      CHECK(!strncmp(net_reply,"WRITE_CORE_MEMORY ERROR",23));
      reply[0]=0;
      CHECK(command_read_memory(&cmd,"1000 2")
            && !strcmp(reply,"READ_CORE_MEMORY 1000 AA 0F\n"));
   }
   /* a log cursor is not a slot: past INT_MAX it is still taken */
   reply[0]=0;
   CHECK(command_get_log(&cmd,"3000000000") && !strstr(reply,"ERROR"));
   cmd.error=false;
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
   /* a read past the region it is in runs on into the next */
   reply[0]=0;
   CHECK(command_read_memory(&cmd,"100e 8")
         && !strcmp(reply,"READ_CORE_MEMORY 100e 0E 0F 10 11 12 13 14 15\n"));
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
            "1000\t10\t1ff0\t0\tsystem\trw\tlittle\tram\n"
            "1010\t10\t1ff0\t0\tsystem\trw\tlittle\tram\n"));

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
   /* b was let go on this boundary: a frame let go, then the tap */
   runloop_iterate(); runloop_iterate();
   reply[0]=0;
   CHECK(command_get_input(&cmd,"") && !strcmp(reply,"1\tnone\tnone\n"));

   /* a family of commands as one: the word picks the command, and only
    * the ones that take a number take what follows */
   reply[0]=0; cmd.error=false;
#ifdef HAVE_SCREENSHOTS
   /* GET_FRAME answers with an image, which a line interface cannot carry */
   reply[0]=0; cmd.error=false;
   CHECK(!command_get_frame(&cmd,"") && strstr(reply,"carries no image"));
   reply[0]=0; cmd.error=false;
   CHECK(!command_get_frame(&cmd,"1000") && strstr(reply,"64 to 640"));
#endif
   reply[0]=0; cmd.error=false;
   CHECK(!command_menu(&cmd,"toggle") && strstr(reply,"MENU ERROR one of show|"));
   reply[0]=0;
   CHECK(!command_menu(&cmd,"up 5") && strstr(reply,"up takes no index"));
   reply[0]=0;
   CHECK(!command_menu(&cmd,"down") && strstr(reply,"the menu is closed"));
   /* MENU answers once the menu has run its action, with what it shows */
   reply[0]=0; cmd.error=false;
   CHECK(command_menu(&cmd,"open") && !reply[0]);
   for (i=0;i<8 && !reply[0];i++) runloop_iterate();
   CHECK(!strncmp(reply,"MENU ",5) && strstr(reply,"\nSELECTED 0\nCOUNT "));
   reply[0]=0;
   CHECK(command_menu(&cmd,"down") && !reply[0]);
   /* a second request replaces the first, which is told */
   CHECK(command_menu(&cmd,"down") && strstr(reply,"replaced by another"));
   reply[0]=0;
   for (i=0;i<8 && !reply[0];i++) runloop_iterate();
   CHECK(strstr(reply,"\nSELECTED 1\n") != NULL);
   reply[0]=0;
   CHECK(command_menu(&cmd,"select 0") && !reply[0]);
   for (i=0;i<8 && !reply[0];i++) runloop_iterate();
   CHECK(strstr(reply,"\nSELECTED 0\n") != NULL);
   reply[0]=0;
   CHECK(!command_menu(&cmd,"select 9999") && strstr(reply,"no entry 9999"));
   /* a list another hand changes before the frame: the entry asked for
    * is not reached, and the request after it is not held to that */
   {
      menu_entry_t *entry = (menu_entry_t*)malloc(sizeof(*entry));
      reply[0]=0; cmd.error=false;
      CHECK(entry && command_menu(&cmd,"select 0") && !reply[0]);
      if (entry)
      {
         MENU_ENTRY_INITIALIZE((*entry));
         menu_entry_get(entry, 0, 0, NULL, false);
         menu_entry_action(entry, 0, MENU_ACTION_OK);
         free(entry);
      }
      for (i=0;i<8 && !reply[0];i++) runloop_iterate();
      CHECK(strstr(reply,"the list changed") != NULL);
      reply[0]=0; cmd.error=false;
      CHECK(command_menu(&cmd,"b") && !reply[0]);
      for (i=0;i<8 && !reply[0];i++) runloop_iterate();
      CHECK(!strncmp(reply,"MENU ",5) && !cmd.error);
   }
   /* one whose menu closes first is told so, not left waiting */
   reply[0]=0; cmd.error=false;
   CHECK(command_menu(&cmd,"down") && !reply[0]);
   command_event(CMD_EVENT_MENU_TOGGLE,NULL);
   for (i=0;i<8 && !reply[0];i++) runloop_iterate();
   CHECK(!strcmp(reply,"CLOSED\n"));
   reply[0]=0;
   CHECK(command_menu(&cmd,"open"));
   for (i=0;i<8 && !reply[0];i++) runloop_iterate();
   reply[0]=0; cmd.error=false;
   CHECK(command_menu(&cmd,"close") && !strcmp(reply,"CLOSED\n"));
   /* INPUT, OPTION and CORE_OPTION: their rows as they are, their
    * errors under their own names */
   reply[0]=0; cmd.error=false;
   CHECK(command_input(&cmd,"show 1") && !strcmp(reply,"1\tnone\tnone\n"));
   reply[0]=0;
   CHECK(!command_input(&cmd,"set 1 a") && !strncmp(reply,"INPUT ERROR ",12));
   reply[0]=0; cmd.error=false;
   CHECK(command_option(&cmd,"show pause") && !strncmp(reply,"pause\t",6));
   reply[0]=0;
   CHECK(!command_option(&cmd,"set nope on") && strstr(reply,
         "OPTION ERROR no such option nope; OPTION show lists them"));
   reply[0]=0; cmd.error=false;
   CHECK(!command_option(&cmd,"toggle") && strstr(reply,
         "OPTION ERROR one of show|set"));
   reply[0]=0; cmd.error=false;
   CHECK(!command_core_option(&cmd,"show nope") && strstr(reply,
         "CORE_OPTION ERROR "));
   cmd.error=false;
   reply[0]=0;
   CHECK(!command_replay(&cmd,"record 3") && strstr(reply,"nothing after record"));
   reply[0]=0;
   CHECK(!command_state(&cmd,"keep 1") && strstr(reply,"one of save|load"));
   /* a slot past 32 bits is refused, not taken as slot 1 */
   reply[0]=0;
   CHECK(!command_state(&cmd,"load 4294967297") && strstr(reply,"ERROR"));
   reply[0]=0;
   CHECK(!command_state(&cmd,"save -1") && strstr(reply,"ERROR"));
   /* nor one past INT_MAX, which would be the auto slot's file */
   reply[0]=0;
   CHECK(!command_state(&cmd,"save 2147483648")
         && !strcmp(reply,"STATE ERROR a slot number, not 2147483648"));
   reply[0]=0;
   CHECK(!command_state(&cmd,"save 4294967295") && strstr(reply,"STATE ERROR"));
   reply[0]=0;
   CHECK(!command_replay(&cmd,"play 2147483648")
         && !strcmp(reply,"REPLAY ERROR a slot number, not 2147483648"));
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
      /* room for sixteen, counted from empty */
      CHECK(command_watch(&cmd,"two off"));
      for (w = 0; w < 16; w++)
      {
         snprintf(req,sizeof(req),"w%u %x",w,0x1000 + w);
         CHECK(command_watch(&cmd,req));
      }
      reply[0]=0;
      CHECK(!command_watch(&cmd,"w16 1009")
            && strstr(reply,"at most 16 bytes watched"));
      /* room made earlier in the same request counts */
      CHECK(command_watch(&cmd,"w15 off w16 1009 w16 off w15 100f"));
      /* but replacing one of the sixteen still works */
      CHECK(command_watch(&cmd,"w0 1005"));
      for (w = 0; w < 16; w++)
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
   /* a was let go on this boundary: a frame let go, then the press */
   runloop_iterate();
   CHECK(!reply[0] && input_driver_command_held(0,axes) == 0x0100);
   runloop_iterate();
   CHECK(!strcmp(reply,"PRESS 1 a+a 1\n"));
   /* an axis is held at a value, so it is SET_INPUT's */
   reply[0]=0;
   CHECK(!command_press(&cmd,"1 left_x 2") && strstr(reply,"PRESS ERROR"));
   CHECK(!command_press(&cmd,"1 a 0") && strstr(reply,"frame count"));
   /* a press holds input for every frame it runs, so it is bounded by
    * what a client waits for, not by RUN_FRAMES' ten minutes */
   reply[0]=0;
   CHECK(!command_press(&cmd,"1 a 601") && strstr(reply,"INPUT set"));
   /* a player past 32 bits is refused, not taken as player 1 */
   reply[0]=0;
   CHECK(!command_press(&cmd,"4294967297 a 1") && strstr(reply,"a player from 1"));
   CHECK(!command_set_input(&cmd,"4294967297 a 1") && strstr(reply,"a player from 1"));
   CHECK(!command_set_input(&cmd,"-18446744073709551615 a 1")
         && strstr(reply,"a player from 1"));
   cmd.error=false;
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
   /* nine seconds of frames, inside the ten a reply is waited for */
   CHECK(!command_run_frames(&cmd,"36000")
         && strstr(reply,"a frame count from 1 to 540"));
   CHECK(!command_run_frames(&cmd,"541"));
   CHECK(command_run_frames(&cmd,"540"));
   command_run_frames_cancel("tidying up");
   reply[0]=0;
   CHECK(!command_press(&cmd,"1 a 541") && strstr(reply,"1 to 540"));
   CHECK(!command_run_until(&cmd,"1000 == 0 541") && strstr(reply,"at most 540"));
   /* and at 50fps, where 600 frames is twelve seconds */
   video_state_get_ptr()->av_info.timing.fps=50.0;
   reply[0]=0;
   CHECK(!command_run_frames(&cmd,"451") && strstr(reply,"a frame count from 1 to 450"));
   CHECK(!command_press(&cmd,"1 a 451") && strstr(reply,"1 to 450"));
   CHECK(command_run_frames(&cmd,"450"));
   command_run_frames_cancel("tidying up");
   video_state_get_ptr()->av_info.timing.fps=60.0;
   cmd.structured=false;
   cmd.error=false;
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
   /* a press over a button SET_INPUT holds leaves that hold in place;
    * a frame first, so B's press is not the one just let go */
   CHECK(command_run_frames(&cmd,"1"));
   runloop_iterate();
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
      char req[256];
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

      /* two bytes as one value, high:low: ram[6] holds 6 */
      {
         char want[64];
         now=ram7(&cmd); reply[0]=0;
         snprintf(req,sizeof(req),"1006:1007 == %u",0x600 + ((now + 2) & 0xff));
         CHECK(command_run_until(&cmd,req) && !reply[0]);
         runloop_iterate(); runloop_iterate();
         snprintf(want,sizeof(want),"MET 2\nVALUE %u\n",0x600 + ((now + 2) & 0xff));
         CHECK(!strcmp(reply,want));
      }
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"1006:1007 == 65536") && strstr(reply,"0 to 65535"));
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"1006: == 1") && strstr(reply,"second hexadecimal"));
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"99000:1007 == 1") && strstr(reply,"RUN_UNTIL ERROR"));
      cmd.error=false;

      /* PRESS with until: the press, then on until it holds - a jump
       * and its landing in one request */
      {
         char want[64];
         now=ram7(&cmd); reply[0]=0;
         snprintf(req,sizeof(req),"1 a 2 until 1007 == %u",(now + 5) & 0xff);
         CHECK(command_press(&cmd,req) && !reply[0]);
         runloop_iterate();
         CHECK((input_driver_command_held(0,axes) & (1u << RETRO_DEVICE_ID_JOYPAD_A)) != 0);
         runloop_iterate();
         /* let go once its frames have run, while the wait goes on */
         CHECK(input_driver_command_held(0,axes) == 0 && !reply[0]);
         runloop_iterate(); runloop_iterate(); runloop_iterate();
         snprintf(want,sizeof(want),"MET 5\nVALUE %u\n",(now + 5) & 0xff);
         CHECK(!strcmp(reply,want));
      }
      reply[0]=0;
      CHECK(command_press(&cmd,"1 a 2 until 1006 == 6"));
      runloop_iterate(); runloop_iterate();
      CHECK(!strcmp(reply,"MET 2\nVALUE 6\n"));
      /* without until it answers as it always has */
      reply[0]=0;
      CHECK(command_press(&cmd,"1 a 1"));
      /* a tap after the last one let go: its own frame, after a gap */
      runloop_iterate();
      CHECK(!reply[0]);
      runloop_iterate();
      CHECK(!strcmp(reply,"PRESS 1 a 1\n"));
      reply[0]=0;
      CHECK(!command_press(&cmd,"1 a 2 until 99000 == 0") && strstr(reply,"PRESS ERROR"));
      reply[0]=0;
      CHECK(!command_press(&cmd,"1 a 2 until 1007 ~ 1") && strstr(reply,"after until"));
      cmd.error=false;

      /* STEPS: a stretch of play as one request */
      {
         char want[64];
         now=ram7(&cmd); reply[0]=0;
         CHECK(command_watch(&cmd,"one 1003"));
         snprintf(req,sizeof(req),"run 2; press 1 b 1; until 1007 == %u",(now + 6) & 0xff);
         reply[0]=0;
         CHECK(command_steps(&cmd,req) && !reply[0]);
         for (i=0;i<5;i++) runloop_iterate();
         CHECK(!reply[0]);
         runloop_iterate();
         CHECK(!strcmp(reply,"DONE 6\none\t1003\t51\nSTEP 3\n"));
         CHECK(command_watch(&cmd,"one off"));
         /* stop ends it after the frame where it holds */
         now=ram7(&cmd); reply[0]=0;
         snprintf(req,sizeof(req),"run 10; stop 1007 == %u",(now + 3) & 0xff);
         CHECK(command_steps(&cmd,req));
         for (i=0;i<3;i++) runloop_iterate();
         CHECK(!strcmp(reply,"STOPPED 3\nSTOP 1\nSTEP 1\n")
               && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
         /* what already holds runs no frame */
         reply[0]=0;
         CHECK(command_steps(&cmd,"until 1006 == 6") && !strcmp(reply,"DONE 0\nSTEP 1\n"));
         /* past nine seconds it says how far it got and where */
         {
            unsigned n=0;
            reply[0]=0;
            CHECK(command_steps(&cmd,"run 1; until 1007 > 255"));
            while (!reply[0] && n < 700) { runloop_iterate(); n++; }
            CHECK(n == 540 && !strcmp(reply,"WAITING 540\nSTEP 2\n"));
         }
         (void)want;
      }
      /* a watch of two bytes as one value, high:low */
      {
         char want[64];
         CHECK(command_watch(&cmd,"pos 1006:1007"));
         now=ram7(&cmd); reply[0]=0;
         CHECK(command_run_frames(&cmd,"1"));
         runloop_iterate();
         snprintf(want,sizeof(want),"RUN_FRAMES 1\npos\t1006:1007\t%u\n",0x600 + ((now + 1) & 0xff));
         CHECK(!strcmp(reply,want));
         CHECK(command_watch(&cmd,"pos off"));
      }
      /* only the bits a mask names: ram[6] holds 6, 110 in binary */
      reply[0]=0;
      CHECK(command_run_until(&cmd,"1006 & 0x4 == 4") && !strcmp(reply,"MET 0\nVALUE 4\n"));
      reply[0]=0;
      CHECK(command_run_until(&cmd,"1006 & 1 == 0") && !strcmp(reply,"MET 0\nVALUE 0\n"));
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"1006 & zz == 0") && strstr(reply,"a mask"));
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"1006 & 0x 4 == 4") && strstr(reply,"a mask"));
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"1006 & 0x+4 == 4") && strstr(reply,"a mask"));
      cmd.error=false;
      /* any of several stops ends the list */
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"run 20; stop 1006 == 7; stop 1007 == %u",(now + 2) & 0xff);
      CHECK(command_steps(&cmd,req));
      runloop_iterate(); runloop_iterate();
      /* and says which */
      CHECK(!strcmp(reply,"STOPPED 2\nSTOP 2\nSTEP 1\n"));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"run 541") && strstr(reply,"within the 540 frames"));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"run 4294967295; run 2") && strstr(reply,"within the 540 frames"));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"run 300; press 1 a 300") && strstr(reply,"step 2"));
      /* two presses of one button are two taps: a frame let go between */
      {
         unsigned want[5] = {1,1,0,1,1};
         unsigned k;
         bool     ok = true;
         reply[0]=0;
         cmd.error=false;
         CHECK(command_steps(&cmd,"press 1 a 2; press 1 a 2"));
         for (k=0;k<5;k++)
         {
            /* held during frame k is what the frame before left */
            if (((input_driver_command_held(0,axes) >> RETRO_DEVICE_ID_JOYPAD_A) & 1) != want[k])
               ok = false;
            runloop_iterate();
         }
         CHECK(ok && !strcmp(reply,"DONE 5\nSTEP 2\n"));
      }
      /* an until that already holds between them is no frame: still
       * two taps */
      {
         unsigned want[5] = {1,1,0,1,1};
         unsigned k;
         bool     ok = true;
         CHECK(command_run_frames(&cmd,"1"));
         runloop_iterate();
         reply[0]=0;
         CHECK(command_steps(&cmd,"press 1 a 2; until 1006 == 6; press 1 a 2"));
         for (k=0;k<5;k++)
         {
            if (((input_driver_command_held(0,axes) >> RETRO_DEVICE_ID_JOYPAD_A) & 1) != want[k])
               ok = false;
            runloop_iterate();
         }
         CHECK(ok && !strcmp(reply,"DONE 5\nSTEP 3\n"));
      }
      /* and so are a press ending one request and the same press
       * starting the next */
      {
         unsigned want[5] = {1,1,0,1,1};
         unsigned k;
         bool     ok = true;
         CHECK(command_run_frames(&cmd,"1"));
         runloop_iterate();
         reply[0]=0;
         CHECK(command_steps(&cmd,"press 1 a 2"));
         for (k=0;k<2;k++)
         {
            if (((input_driver_command_held(0,axes) >> RETRO_DEVICE_ID_JOYPAD_A) & 1) != want[k])
               ok = false;
            runloop_iterate();
         }
         CHECK(!strcmp(reply,"DONE 2\nSTEP 1\n"));
         reply[0]=0;
         CHECK(command_steps(&cmd,"press 1 a 2"));
         for (k=2;k<5;k++)
         {
            if (((input_driver_command_held(0,axes) >> RETRO_DEVICE_ID_JOYPAD_A) & 1) != want[k])
               ok = false;
            runloop_iterate();
         }
         CHECK(ok && !strcmp(reply,"DONE 3\nSTEP 1\n"));
      }
      /* that frame let go counts against the frames a request runs */
      reply[0]=0;
      CHECK(!command_steps(&cmd,"press 1 a 270; press 1 a 270") && strstr(reply,"step 2"));
      if (loop->frame_work & RUNLOOP_WORK_RUN_FRAMES)
         command_run_frames_cancel("tidying up");
      {
         unsigned n=0;
         CHECK(command_run_frames(&cmd,"1"));
         runloop_iterate();
         reply[0]=0;
         CHECK(command_steps(&cmd,"press 1 a 270; press 1 a 269"));
         while (!reply[0] && n < 700) { runloop_iterate(); n++; }
         CHECK(n == 540 && !strcmp(reply,"DONE 540\nSTEP 2\n"));
      }
      reply[0]=0;
      {
         char longarg[400];
         memset(longarg,'0',sizeof(longarg));
         memcpy(longarg,"1 a ",4);
         longarg[sizeof(longarg)-2]='5'; longarg[sizeof(longarg)-1]='\0';
         CHECK(!command_press(&cmd,longarg) && strstr(reply,"at most 255 characters"));
      }
      reply[0]=0;
      CHECK(!command_steps(&cmd,"jump 3") && strstr(reply,"run, press, until, checkpoint or stop"));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"") && strstr(reply,"STEPS ERROR"));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"stop 1007 == 1; stop 1007 == 2; stop 1007 == 3;"
            " stop 1007 == 4; stop 1007 == 5; run 1") && strstr(reply,"at most 4 stops"));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"press 1 left_x 3") && strstr(reply,"axis"));
      reply[0]=0;
      for (i=0, req[0]=0; i<33; i++)
         strlcat(req,"run 1;",sizeof(req));
      CHECK(!command_steps(&cmd,req) && strstr(reply,"at most 32 steps"));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"until 99000 == 0") && strstr(reply,"STEPS ERROR"));
      cmd.error=false;

      /* a checkpoint as a step: where the list got to, not where it
       * started */
      now=ram7(&cmd); reply[0]=0;
      CHECK(command_steps(&cmd,"run 2; checkpoint; run 3"));
      for (i=0;i<5;i++) runloop_iterate();
      CHECK(!strcmp(reply,"DONE 5\nSTEP 3\n"));
      reply[0]=0;
      CHECK(command_state(&cmd,"rollback"));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      CHECK(command_run_frames(&cmd,"1"));
      runloop_iterate();
      CHECK(ram7(&cmd) == ((now + 3) & 0xff));
      /* first in the list, it is kept before the first frame runs */
      now=ram7(&cmd); reply[0]=0;
      CHECK(command_steps(&cmd,"checkpoint; run 2") && !reply[0]);
      runloop_iterate(); runloop_iterate();
      CHECK(!strcmp(reply,"DONE 2\nSTEP 2\n"));
      reply[0]=0;
      CHECK(command_state(&cmd,"rollback"));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      CHECK(command_run_frames(&cmd,"1"));
      runloop_iterate();
      CHECK(ram7(&cmd) == ((now + 1) & 0xff));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"checkpoint") && strstr(reply,"a step at least"));
      cmd.error=false;

      /* queued: answered at once, played on through what is added, and
       * paused once it has all run, answering no one */
      now=ram7(&cmd); reply[0]=0;
      CHECK(command_steps(&cmd,"run 3|on") && !strcmp(reply,"QUEUED 1\n")
            && (loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 2 |on") && !strcmp(reply,"QUEUED 2\n"));
      reply[0]=0;
      for (i=0;i<4;i++) runloop_iterate();
      CHECK(!reply[0] && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES)
            && ram7(&cmd) == ((now + 5) & 0xff));
      runloop_iterate();
      CHECK(ram7(&cmd) == ((now + 5) & 0xff));
      /* run dry, it says how long it has sat paused waiting for more,
       * and the steps that start it again say so too */
      retro_sleep(30);
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strncmp(reply,"QUEUED 0\nIDLE ",14)
            && atol(reply + 14) >= 20);
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 1|on") && !strncmp(reply,"QUEUED 1\nIDLE ",14)
            && atol(reply + 14) >= 20);
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strcmp(reply,"QUEUED 1\n"));
      runloop_iterate();
      /* a stop is kept for the next queued STEPS, whose steps are not
       * played on from it */
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"run 10; stop 1007 == %u|on",(now + 2) & 0xff);
      CHECK(command_steps(&cmd,req) && !strncmp(reply,"QUEUED 1\nIDLE ",14));
      reply[0]=0;
      runloop_iterate(); runloop_iterate();
      CHECK(!reply[0] && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      CHECK(command_steps(&cmd,"run 1|on")
            && !strcmp(reply,"DISCARDED 1\nPREVIOUS STOPPED 2 STOP 1 STEP 1\n")
            && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strcmp(reply,"QUEUED 0\n"));
      /* each queued list runs with its own stops, none where it has
       * none, and counts its own frames */
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"run 2; stop 1007 == %u|on",(now + 4) & 0xff);
      CHECK(command_steps(&cmd,req));
      CHECK(command_steps(&cmd,"run 5|on"));
      for (i=0;i<7;i++) runloop_iterate();
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strncmp(reply,"QUEUED 0\nIDLE ",14)
            && ram7(&cmd) == ((now + 7) & 0xff));
      now=ram7(&cmd); reply[0]=0;
      CHECK(command_steps(&cmd,"run 3|on"));
      snprintf(req,sizeof(req),"run 9; stop 1007 == %u|on",(now + 5) & 0xff);
      CHECK(command_steps(&cmd,req));
      for (i=0;i<5;i++) runloop_iterate();
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on")
            && !strcmp(reply,"QUEUED 0\nPREVIOUS STOPPED 2 STOP 1 STEP 1\n"));
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"run 2; stop 1007 == %u|on",(now + 4) & 0xff);
      CHECK(command_steps(&cmd,req));
      CHECK(command_steps(&cmd,"run 5; stop 1006 == 7|on"));
      for (i=0;i<7;i++) runloop_iterate();
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strncmp(reply,"QUEUED 0\nIDLE ",14)
            && ram7(&cmd) == ((now + 7) & 0xff));
      /* a rollback drops a stop no one has heard about */
      now=ram7(&cmd); reply[0]=0;
      snprintf(req,sizeof(req),"run 5; stop 1007 == %u|on",(now + 1) & 0xff);
      CHECK(command_steps(&cmd,req));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_state(&cmd,"rollback"));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strcmp(reply,"QUEUED 0\n"));
      /* and ends a queue planned from the state it left */
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 10|on"));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_state(&cmd,"rollback"));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      CHECK(!(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strcmp(reply,"QUEUED 0\n"));
      /* so do a state loaded from a slot and a reset */
      reply[0]=0;
      CHECK(command_state(&cmd,"save 1"));
      for (i=0;i<8 && !reply[0];i++) { runloop_iterate(); task_queue_check(); }
      CHECK(!strcmp(reply,"STATE save 1"));
      CHECK(command_steps(&cmd,"run 100|on"));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_state(&cmd,"load 1"));
      for (i=0;i<8 && !reply[0];i++) { runloop_iterate(); task_queue_check(); }
      CHECK(!strcmp(reply,"STATE load 1, queue ended")
            && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      remove("command_frames_test.state1");
      remove("command_frames_test.state1.png");
      CHECK(command_steps(&cmd,"run 100|on"));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_reset(&cmd,""));
      for (i=0;i<8 && !reply[0];i++) { runloop_iterate(); task_queue_check(); }
      CHECK(!strncmp(reply,"RESET",5)
            && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strcmp(reply,"QUEUED 0\n"));
      /* a request that waits replaces the queue */
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 10|on"));
      reply[0]=0;
      CHECK(command_run_frames(&cmd,"1") && !reply[0]);
      runloop_iterate();
      CHECK(!strcmp(reply,"RUN_FRAMES 1\nPREVIOUS REPLACED 0 STEP 1\n")
            && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      /* and is not queued behind */
      reply[0]=0;
      CHECK(command_run_frames(&cmd,"5"));
      CHECK(!command_steps(&cmd,"run 1|on") && strstr(reply,"a request waits"));
      command_run_frames_cancel("tidying up");
      cmd.error=false;
      /* 64 queued and running at most */
      for (i=0, req[0]=0; i<32; i++)
         strlcat(req,"run 1;",sizeof(req));
      strlcat(req,"|on",sizeof(req));
      reply[0]=0;
      CHECK(command_steps(&cmd,req) && !strcmp(reply,"QUEUED 32\n"));
      reply[0]=0;
      CHECK(command_steps(&cmd,req) && !strcmp(reply,"QUEUED 64\n"));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"run 1|on") && strstr(reply,"the queue is full"));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 1|on") && !strcmp(reply,"QUEUED 64\n"));
      command_run_frames_cancel("tidying up");
      /* what the tidying above left to report */
      command_run_frames_forget();
      /* a queued wait that never ends gives up as a request's does */
      {
         unsigned n=0;
         reply[0]=0;
         CHECK(command_steps(&cmd,"until 1007 > 255|on"));
         while (n < 700 && (loop->frame_work & RUNLOOP_WORK_RUN_FRAMES))
         { runloop_iterate(); n++; }
         reply[0]=0;
         CHECK(n == 540 && command_steps(&cmd,"|on")
               && !strcmp(reply,"QUEUED 0\nPREVIOUS WAITING 540 STEP 1\n"));
      }
      /* a reset, by hotkey as much as by command, ends the queue */
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 100|on"));
      runloop_iterate();
      command_event(CMD_EVENT_RESET,NULL);
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strcmp(reply,"QUEUED 0\n")
            && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      /* so does achievements' hardcore mode taking effect, which lets
       * go of held input too, and says why the queue ended */
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 100|on"));
      runloop_iterate();
      CHECK(command_set_input(&cmd,"1 a 1")
            && input_driver_command_held(0,axes) != 0);
      command_hardcore_entered();
      CHECK(input_driver_command_held(0,axes) == 0
            && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strcmp(reply,
            "QUEUED 0\nPREVIOUS ERROR achievements hardcore mode is on\n"));
      /* idle is time the content sat paused: a frame it runs ends it */
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 1|on"));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strncmp(reply,"QUEUED 0\nIDLE ",14));
      command_event(CMD_EVENT_UNPAUSE,NULL);
      runloop_iterate();
      command_event(CMD_EVENT_PAUSE,NULL);
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strcmp(reply,"QUEUED 0\n"));
      /* and a press after such a frame is a tap with no gap before it */
      reply[0]=0;
      CHECK(command_press(&cmd,"1 b 1"));
      runloop_iterate();
      CHECK(!strcmp(reply,"PRESS 1 b 1\n"));
      command_event(CMD_EVENT_UNPAUSE,NULL);
      runloop_iterate();
      command_event(CMD_EVENT_PAUSE,NULL);
      reply[0]=0;
      CHECK(command_press(&cmd,"1 b 1"));
      runloop_iterate();
      CHECK(!strcmp(reply,"PRESS 1 b 1\n"));
      reply[0]=0;
      CHECK(!command_steps(&cmd,"run 1|maybe") && strstr(reply,"on or off"));
      cmd.error=false;

      /* how a queued list ended reaches whichever call comes next,
       * labelled PREVIOUS, with whether that call's own steps ran */
      command_run_frames_forget();
      /* a: stopped, then a call that is not queued: its steps run */
      CHECK(command_steps(&cmd,"run 5; stop 1006 == 6|on"));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 1"));
      runloop_iterate();
      CHECK(!strcmp(reply,"DONE 1\nPREVIOUS STOPPED 1 STOP 1 STEP 1\nSTEP 1\n"));
      /* b: stopped, then a queued call: its steps are dropped, and it
       * says so */
      CHECK(command_steps(&cmd,"run 5; stop 1006 == 6|on"));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 2; run 3|on")
            && !strcmp(reply,"DISCARDED 2\nPREVIOUS STOPPED 1 STOP 1 STEP 1\n")
            && !(loop->frame_work & RUNLOOP_WORK_RUN_FRAMES));
      reply[0]=0;
      CHECK(command_steps(&cmd,"|on") && !strcmp(reply,"QUEUED 0\n"));
      /* c: a queued wait gave up, then a call that is not queued */
      {
         unsigned n=0;
         CHECK(command_steps(&cmd,"until 1007 > 255|on"));
         while (n < 700 && (loop->frame_work & RUNLOOP_WORK_RUN_FRAMES))
         { runloop_iterate(); n++; }
         reply[0]=0;
         CHECK(command_run_frames(&cmd,"1"));
         runloop_iterate();
         CHECK(n == 540 && !strcmp(reply,"RUN_FRAMES 1\nPREVIOUS WAITING 540 STEP 1\n"));
      }
      /* d: a queue still running, replaced by a call that is not */
      CHECK(command_steps(&cmd,"run 100|on"));
      runloop_iterate(); runloop_iterate(); runloop_iterate();
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 1"));
      runloop_iterate();
      CHECK(!strcmp(reply,"DONE 1\nPREVIOUS REPLACED 3 STEP 1\nSTEP 1\n"));
      /* and with nothing before it, none */
      reply[0]=0;
      CHECK(command_steps(&cmd,"run 1"));
      runloop_iterate();
      CHECK(!strcmp(reply,"DONE 1\nSTEP 1\n"));

      /* values in hexadecimal, as masks are; ram[6] holds 6 */
      reply[0]=0;
      CHECK(command_run_until(&cmd,"1006 == 0x6") && !strcmp(reply,"MET 0\nVALUE 6\n"));
      reply[0]=0;
      CHECK(command_run_until(&cmd,"1006 & 0xF0 == 0x0") && !strcmp(reply,"MET 0\nVALUE 0\n"));
      reply[0]=0;
      CHECK(command_steps(&cmd,"until 1006 == 0x06") && !strcmp(reply,"DONE 0\nSTEP 1\n"));
      reply[0]=0;
      CHECK(!command_run_until(&cmd,"1006 == 0x1ff") && strstr(reply,"0 to 255"));
      cmd.error=false; reply[0]=0;
      CHECK(!command_run_until(&cmd,"1006 == 0x") && strstr(reply,"0 to 255"));
      cmd.error=false;

      /* a read runs on into the region that follows; where nothing
       * maps the rest, it says how much it read */
      reply[0]=0;
      CHECK(command_read_memory(&cmd,"100e 4")
            && !strcmp(reply,"READ_CORE_MEMORY 100e 0E 0F 10 11\n"));
      reply[0]=0;
      CHECK(command_read_memory(&cmd,"101e 4")
            && !strcmp(reply,"READ_CORE_MEMORY 101e 1E 1F\nTRUNCATED 2\n"));

      /* over MCP, one range: the one it takes */
      cmd.structured=true;
      reply[0]=0;
      CHECK(!command_run_frames(&cmd,"-5") && strstr(reply,"a frame count from 1 to 540"));
      reply[0]=0;
      CHECK(!command_run_frames(&cmd,"541") && strstr(reply,"a frame count from 1 to 540"));
      cmd.structured=false;
      cmd.error=false;

      /* a rollback or a reset that ends a queue says so */
      reply[0]=0;
      CHECK(command_state(&cmd,"checkpoint"));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      CHECK(command_steps(&cmd,"run 100|on"));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_state(&cmd,"rollback"));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      CHECK(!strcmp(reply,"STATE rollback, queue ended"));
      CHECK(command_steps(&cmd,"run 100|on"));
      runloop_iterate();
      reply[0]=0;
      CHECK(command_reset(&cmd,""));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      CHECK(!strcmp(reply,"RESET, queue ended"));
      /* and with no queue, nothing to say */
      reply[0]=0;
      CHECK(command_reset(&cmd,""));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      CHECK(!strcmp(reply,"RESET"));

      /* RESET paused runs no frame after the reset */
      before=calls; reply[0]=0;
      CHECK(command_reset(&cmd,"on"));
      for (i=0;i<8 && !reply[0];i++) task_queue_check();
      runloop_iterate(); runloop_iterate();
      CHECK(!strcmp(reply,"RESET") && calls == before
            && (loop->flags & RUNLOOP_FLAG_PAUSED));
      cmd.error=false;
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
   /* --command: a STEPS list's "; " is not where the host starts */
   {
      char out[256];
      CHECK(command_network_send("STEPS run 2; run 1|on;127.0.0.1;55437"));
      udp_line("STEPS |on", out, sizeof(out));
      CHECK(!strncmp(out, "QUEUED 2\n", 9));
      command_run_frames_cancel("tidying up");
      /* ";;port": the empty host is the default one */
      udp_line("STEPS |on", out, sizeof(out));
      CHECK(command_network_send("STEPS run 2|on;;55437"));
      udp_line("STEPS |on", out, sizeof(out));
      CHECK(!strncmp(out, "QUEUED 1\n", 9));
      command_run_frames_cancel("tidying up");
   }
   /* closing the content drops the watches: the next content's
    * addresses mean something else */
   CHECK(command_watch(&cmd,"two 1002"));
   CHECK(command_set_input(&cmd,"1 start 1"));
   {
      char req[64];
      unsigned now=ram7(&cmd);
      snprintf(req,sizeof(req),"run 5; stop 1007 == %u|on",(now + 1) & 0xff);
      CHECK(command_steps(&cmd,req));
      runloop_iterate();
   }
   command_event(CMD_EVENT_UNLOAD_CORE,NULL);
   for (i=0;i<8;i++) { runloop_iterate(); task_queue_check(); }
   reply[0]=0;
   CHECK(command_watch(&cmd,"") && !strcmp(reply,"WATCH none\n"));
   /* and what SET_INPUT held: the next content starts with nothing held */
   CHECK(input_driver_command_held(0,axes) == 0);
   /* and a stop no one heard about */
   reply[0]=0;
   CHECK(command_steps(&cmd,"|on") && !strcmp(reply,"QUEUED 0\n"));
   /* LOAD_CONTENT paused from no running content: how many frames the
    * core has run once it answers - 1007 counts them */
   {
      char line[1024];
      FILE *f = fopen("command_frames_test.bin", "wb");
      if (f) { fputs("x", f); fclose(f); }
      snprintf(line, sizeof(line), "%s|command_frames_test.bin|on", argv[1]);
      cmd.structured=true;
      reply[0]=0;
      CHECK(command_load_content(&cmd,line));
      for (i=0;i<60 && !reply[0];i++) { runloop_iterate(); task_queue_check(); }
      CHECK(!strncmp(reply,"LOAD_CONTENT ",13));
      reply[0]=0;
      CHECK(command_read_memory(&cmd,"1007 1")
            && !strcmp(reply,"READ_CORE_MEMORY 1007 01\n"));
      for (i=0;i<5;i++) runloop_iterate();
      reply[0]=0;
      /* and one more, the frame the pause waits for, as a core is
       * only marked running once it has run one */
      CHECK(command_read_memory(&cmd,"1007 1")
            && !strcmp(reply,"READ_CORE_MEMORY 1007 02\n"));
      CHECK(loop->flags & RUNLOOP_FLAG_PAUSED);
      cmd.structured=false;
      remove("command_frames_test.bin");
   }
   main_exit(NULL); remove(cfg_path);
   printf("command_frames: %s (%u failures)\n",failures?"FAIL":"PASS",failures);
   return failures ? 1 : 0;
}
