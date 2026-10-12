/* Real option handlers and runloop setters, with driver events counted. */
#include <stdio.h>
#include <string.h>
#include <string/stdstring.h>

#include "../../../configuration.h"
#include "../../../command.h"
#include "../../../core_info.h"
#include "../../../driver.h"
#include "../../../runloop.h"
#include "../../../paths.h"
#include "../../../audio/audio_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../input/input_driver.h"
#include "../../../network/netplay/netplay.h"
#include "../../../record/record_driver.h"
#include "../../../cheevos/cheevos.h"

static settings_t settings;
static input_driver_state_t input_st;
static audio_driver_state_t audio_st;
static video_driver_state_t video_st;
static recording_state_t rec_st;
static bool hardcore;
static bool allow_timeskip = true;
static unsigned starts;
static unsigned stops;
static unsigned nonblock_updates;
static unsigned frame_limit_updates;
static unsigned failures;
static char reply[1024];

#define CHECK(test) do { if (!(test)) { \
   fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #test); \
   failures++; } } while (0)

settings_t *config_get_ptr(void) { return &settings; }
input_driver_state_t *input_state_get_ptr(void) { return &input_st; }
void input_driver_command_hold(unsigned port, unsigned control,
      int16_t value)
{
   if (control >= 16)
      input_st.cmd_analog[port][control - 16] = value;
   else if (value)
      input_st.cmd_buttons[port] |=  (uint16_t)(1u << control);
   else
      input_st.cmd_buttons[port] &= ~(uint16_t)(1u << control);
   input_st.cmd_input = input_st.cmd_buttons[port]
      || input_st.cmd_analog[port][0] || input_st.cmd_analog[port][1]
      || input_st.cmd_analog[port][2] || input_st.cmd_analog[port][3];
}
void input_driver_command_release(unsigned port)
{
   if (port < MAX_USERS)
   {
      input_st.cmd_buttons[port] = 0;
      memset(input_st.cmd_analog[port], 0, sizeof(input_st.cmd_analog[port]));
   }
   else
   {
      memset(input_st.cmd_buttons, 0, sizeof(input_st.cmd_buttons));
      memset(input_st.cmd_analog,  0, sizeof(input_st.cmd_analog));
   }
   input_st.cmd_input = false;
}
uint16_t input_driver_command_held(unsigned port, int16_t *analog)
{
   memcpy(analog, input_st.cmd_analog[port], 4 * sizeof(*analog));
   return input_st.cmd_buttons[port];
}
audio_driver_state_t *audio_state_get_ptr(void) { return &audio_st; }
video_driver_state_t *video_state_get_ptr(void) { return &video_st; }
recording_state_t *recording_state_get_ptr(void) { return &rec_st; }
bool rcheevos_hardcore_active(void) { return hardcore; }
bool input_driver_mouse_grabbed(void) { return false; }
unsigned input_driver_command_generation(void) { return 0; }
bool input_driver_game_focus_enabled(void) { return false; }
uint64_t video_driver_get_frame_count(void) { return 0; }
bool core_info_get_current_core(core_info_t **core)
{ *core = NULL; return false; }
char *dir_get_ptr(enum rarch_dir_type type)
{ (void)type; return settings.paths.directory_system; }
const char *path_get(enum rarch_path_type type)
{ (void)type; return ""; }
void driver_set_nonblock_state(void) { nonblock_updates++; }
bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{ (void)data; CHECK(state == RARCH_NETPLAY_CTL_ALLOW_TIMESKIP); return allow_timeskip; }

bool command_event(enum event_command event, void *data)
{
   (void)data;
   switch (event)
   {
      case CMD_EVENT_RECORD_INIT:
         starts++;
         rec_st.enable = true;
         break;
      case CMD_EVENT_RECORD_DEINIT:
         stops++;
         rec_st.enable = false;
         rec_st.streaming_enable = false;
         break;
      case CMD_EVENT_STREAMING_TOGGLE:
         if (rec_st.streaming_enable)
            return command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         rec_st.streaming_enable = true;
         return command_event(CMD_EVENT_RECORD_INIT, NULL);
      case CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE:
         if (settings.bools.cheevos_hardcore_mode_enable)
            hardcore = !hardcore;
         break;
      case CMD_EVENT_SET_FRAME_LIMIT:
         frame_limit_updates++;
         break;
      default:
         return false;
   }
   return true;
}

static unsigned core_option_sets;

bool core_option_manager_get_idx(core_option_manager_t *opt,
      const char *key, size_t *idx)
{
   size_t i;
   for (i = 0; i < opt->size; i++)
      if (string_is_equal(opt->opts[i].key, key))
      {
         *idx = i;
         return true;
      }
   return false;
}

bool core_option_manager_get_val_idx(core_option_manager_t *opt,
      size_t idx, const char *val, size_t *val_idx)
{
   size_t i;
   for (i = 0; i < opt->opts[idx].vals->size; i++)
      if (string_is_equal(opt->opts[idx].vals->elems[i].data, val))
      {
         *val_idx = i;
         return true;
      }
   return false;
}

void core_option_manager_set_val(core_option_manager_t *opt,
      size_t idx, size_t val_idx, bool refresh_menu)
{
   (void)refresh_menu;
   opt->opts[idx].index = val_idx;
   core_option_sets++;
}

static void capture(command_t *cmd, const char *data, size_t len)
{
   (void)cmd;
   CHECK(len < sizeof(reply));
   if (len >= sizeof(reply))
      len = sizeof(reply) - 1;
   memcpy(reply, data, len);
   reply[len] = '\0';
}

static bool set(command_t *cmd, const char *arg)
{
   cmd->error = false;
   reply[0] = '\0';
   return command_set_option(cmd, arg);
}

int main(void)
{
   command_t cmd;
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   memset(&cmd, 0, sizeof(cmd));
   cmd.replier = capture;

   CHECK(set(&cmd, "recording on"));
   CHECK(starts == 1);
   CHECK(set(&cmd, "recording on"));
   CHECK(starts == 1);
   CHECK(!set(&cmd, "streaming on"));
   CHECK(cmd.error && strstr(reply, "stop the active"));
   CHECK(starts == 1 && stops == 0 && !rec_st.streaming_enable);
   CHECK(set(&cmd, "streaming off"));
   CHECK(stops == 0 && rec_st.enable);
   CHECK(set(&cmd, "recording off"));
   CHECK(set(&cmd, "streaming on"));
   CHECK(starts == 2 && stops == 1);
   CHECK(!set(&cmd, "recording on"));
   CHECK(cmd.error && strstr(reply, "stop the active"));
   CHECK(starts == 2 && stops == 1 && rec_st.streaming_enable);
   CHECK(set(&cmd, "recording off"));
   CHECK(stops == 1 && rec_st.enable);
   CHECK(set(&cmd, "streaming off"));
   CHECK(stops == 2);

   settings.bools.cheevos_hardcore_mode_enable = true;
   hardcore = true;
   CHECK(set(&cmd, "cheevos_hardcore off"));
   CHECK(!hardcore && settings.bools.cheevos_hardcore_mode_enable);
   CHECK(command_get_option(&cmd, "cheevos_hardcore"));
   CHECK(!strcmp(reply, "cheevos_hardcore\toff\n"));
   CHECK(set(&cmd, "cheevos_hardcore off"));
   CHECK(!hardcore);
   CHECK(set(&cmd, "cheevos_hardcore on"));
   CHECK(hardcore);
   CHECK(set(&cmd, "cheevos_hardcore on"));
   CHECK(hardcore);
   settings.bools.cheevos_hardcore_mode_enable = false;
   hardcore = false;
   CHECK(!set(&cmd, "cheevos_hardcore on"));
   CHECK(cmd.error && !hardcore);

   CHECK(!set(&cmd, "fast_forward on"));
   CHECK(cmd.error && !cmd.state[RARCH_FAST_FORWARD_KEY]);
   runloop_st->flags = RUNLOOP_FLAG_CORE_RUNNING | RUNLOOP_FLAG_PAUSED;
   CHECK(!set(&cmd, "fast_forward on"));
   CHECK(!set(&cmd, "slow_motion on"));
   runloop_st->flags = RUNLOOP_FLAG_CORE_RUNNING;
   allow_timeskip = false;
   CHECK(!set(&cmd, "fast_forward on"));
   CHECK(!set(&cmd, "slow_motion on"));
   allow_timeskip = true;
   runloop_st->fastmotion_override.current.inhibit_toggle = true;
   CHECK(!set(&cmd, "fast_forward on"));
   runloop_st->fastmotion_override.current.inhibit_toggle = false;
   runloop_st->fastmotion_override.pending = true;
   CHECK(!set(&cmd, "fast_forward on"));
   runloop_st->fastmotion_override.pending = false;
   CHECK(nonblock_updates == 0 && frame_limit_updates == 0);

   settings.bools.audio_fastforward_mute = true;
   settings.bools.frame_time_counter_auto_reset = true;
   CHECK(set(&cmd, "fast_forward on"));
   CHECK(!strcmp(reply, "fast_forward\ton\n"));
   CHECK(runloop_st->flags & RUNLOOP_FLAG_FASTMOTION);
   CHECK(input_st.flags & INP_FLAG_NONBLOCKING);
   CHECK(AUDIO_FLAGS_GET(&audio_st) & AUDIO_FLAG_MUTED);
   CHECK(set(&cmd, "fast_forward on"));
   CHECK(nonblock_updates == 1 && frame_limit_updates == 1);
   video_st.frame_time_count = 10;
   CHECK(set(&cmd, "fast_forward off"));
   CHECK(!(runloop_st->flags & RUNLOOP_FLAG_FASTMOTION));
   CHECK(!(input_st.flags & INP_FLAG_NONBLOCKING));
   CHECK(!(AUDIO_FLAGS_GET(&audio_st) & AUDIO_FLAG_MUTED));
   CHECK(video_st.frame_time_count == 0);
   CHECK(runloop_st->fastforward_after_frames == 1);
   CHECK(nonblock_updates == 2 && frame_limit_updates == 1);
   CHECK(!cmd.state[RARCH_FAST_FORWARD_KEY]);
   CHECK(set(&cmd, "fast_forward off"));
   CHECK(nonblock_updates == 2);

   hardcore = true;
   CHECK(!set(&cmd, "slow_motion on"));
   CHECK(cmd.error && !(runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION));
   hardcore = false;
   CHECK(set(&cmd, "slow_motion on"));
   CHECK(runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION);
   CHECK(set(&cmd, "slow_motion on"));
   CHECK(set(&cmd, "slow_motion off"));
   CHECK(!(runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION));
   CHECK(!cmd.state[RARCH_SLOWMOTION_KEY]);

   {
      static struct string_list_elem res_vals[3];
      static struct string_list_elem fps_vals[2];
      static struct string_list res_list;
      static struct string_list fps_list;
      static struct core_option opts[2];
      static core_option_manager_t mgr;

      CHECK(!command_get_core_option(&cmd, ""));
      CHECK(cmd.error && strstr(reply, "has no options"));

      res_vals[0].data = (char*)"320x240";
      res_vals[1].data = (char*)"640x480";
      res_vals[2].data = (char*)"1280x960 (slow)";
      res_list.elems   = res_vals;
      res_list.size    = 3;
      fps_vals[0].data = (char*)"disabled";
      fps_vals[1].data = (char*)"enabled";
      fps_list.elems   = fps_vals;
      fps_list.size    = 2;
      opts[0].key      = (char*)"core_resolution";
      opts[0].desc     = (char*)"Internal resolution";
      opts[0].vals     = &res_list;
      opts[1].key      = (char*)"core_fps";
      opts[1].desc     = (char*)"Show FPS";
      opts[1].vals     = &fps_list;
      opts[1].index    = 1;
      mgr.opts         = opts;
      mgr.size         = 2;
      runloop_st->core_options = &mgr;

      cmd.error = false;
      CHECK(command_get_core_option(&cmd, ""));
      CHECK(!strcmp(reply, "core_resolution\t320x240\tInternal resolution\n"
               "core_fps\tenabled\tShow FPS\n"));
      CHECK(command_get_core_option(&cmd, "core_resolution"));
      CHECK(!strcmp(reply, "core_resolution\t320x240\tInternal resolution"
               "\t320x240|640x480|1280x960 (slow)\n"));
      CHECK(!command_get_core_option(&cmd, "core_nope"));
      CHECK(cmd.error && strstr(reply, "no such core option core_nope"));

      cmd.error = false;
      CHECK(command_set_core_option(&cmd, "core_resolution 1280x960 (slow)"));
      CHECK(opts[0].index == 2 && core_option_sets == 1);
      CHECK(!strcmp(reply,
               "core_resolution\t1280x960 (slow)\tInternal resolution\n"));
      CHECK(command_set_core_option(&cmd, "core_resolution 1280x960 (slow)"));
      CHECK(core_option_sets == 1);
      CHECK(!command_set_core_option(&cmd, "core_resolution 99x99"));
      CHECK(cmd.error && strstr(reply, "does not take 99x99"));
      CHECK(opts[0].index == 2);
      cmd.error = false;
      CHECK(!command_set_core_option(&cmd, "core_fps"));
      CHECK(cmd.error && core_option_sets == 1);
      runloop_st->core_options = NULL;
   }

   {
      static uint8_t wram[16];
      static rarch_memory_descriptor_t descs[2];

      cmd.error = false;
      CHECK(!command_get_memory_map(&cmd, ""));
      CHECK(cmd.error && strstr(reply, "no memory map"));

      descs[0].core.flags     = RETRO_MEMDESC_SYSTEM_RAM;
      descs[0].core.ptr       = wram;
      descs[0].core.start     = 0x7e0000;
      descs[0].core.len       = sizeof(wram);
      descs[0].core.addrspace = "WRAM";
      descs[1].core.flags     = RETRO_MEMDESC_CONST | RETRO_MEMDESC_BIGENDIAN;
      descs[1].core.start     = 0x8000;
      descs[1].core.select    = 0xff8000;
      descs[1].core.disconnect= 0x7f0000;
      descs[1].core.len       = 0x400000;
      runloop_st->system.mmaps.descriptors     = descs;
      runloop_st->system.mmaps.num_descriptors = 2;

      cmd.error = false;
      CHECK(command_get_memory_map(&cmd, ""));
      CHECK(!strcmp(reply, "7e0000\t10\t0\t0\tsystem\trw\tlittle\tWRAM\n"
               "8000\t400000\tff8000\t7f0000\tother\tnone\tbig\t\n"));
      runloop_st->system.mmaps.num_descriptors = 0;
   }

   cmd.error = false;
   runloop_st->flags = 0;
   CHECK(!command_run_frames(&cmd, "3"));
   CHECK(cmd.error && strstr(reply, "no content"));
   runloop_st->flags = RUNLOOP_FLAG_CORE_RUNNING;
   CHECK(!command_run_frames(&cmd, "0"));
   cmd.error = false;
   reply[0] = '\0';
   CHECK(command_run_frames(&cmd, "3"));
   CHECK(!reply[0] && (runloop_st->frame_work & RUNLOOP_WORK_RUN_FRAMES));
   command_run_frames_tick();
   command_run_frames_tick();
   CHECK(!reply[0]);
   command_run_frames_tick();
   CHECK(!strcmp(reply, "RUN_FRAMES 3\n") && !cmd.error);
   CHECK(!(runloop_st->frame_work & RUNLOOP_WORK_RUN_FRAMES));
   CHECK(command_run_frames(&cmd, "5"));
   command_run_frames_tick();
   CHECK(command_run_frames(&cmd, "2"));
   CHECK(strstr(reply, "RUN_FRAMES ERROR replaced"));
   cmd.error = false;
   command_run_frames_tick();
   command_run_frames_tick();
   CHECK(!strcmp(reply, "RUN_FRAMES 2\n") && !cmd.error);
   CHECK(command_run_frames(&cmd, "4"));
   command_run_frames_cancel("the content was closed");
   CHECK(cmd.error && strstr(reply, "content was closed"));
   CHECK(!(runloop_st->frame_work & RUNLOOP_WORK_RUN_FRAMES));
   runloop_st->flags = 0;

   cmd.error = false;
   CHECK(command_set_input(&cmd, "2 a 1"));
   CHECK(!strcmp(reply, "2\ta\tnone\n"));
   CHECK(input_st.cmd_input && input_st.cmd_buttons[1] == (1u << 8));
   CHECK(command_set_input(&cmd, "2 right 1"));
   CHECK(command_set_input(&cmd, "2 left_x -32768"));
   CHECK(!strcmp(reply, "2\tright a\tleft_x=-32768\n"));
   CHECK(command_set_input(&cmd, "2 a 0"));
   CHECK(!strcmp(reply, "2\tright\tleft_x=-32768\n"));
   CHECK(!command_set_input(&cmd, "2 a 2"));
   CHECK(cmd.error && strstr(reply, "a takes 1"));
   cmd.error = false;
   CHECK(!command_set_input(&cmd, "2 left_x 40000"));
   CHECK(!command_set_input(&cmd, "2 turbo 1"));
   CHECK(!command_set_input(&cmd, "0 a 1"));
   cmd.error = false;
   CHECK(command_release_input(&cmd, "2"));
   CHECK(!strcmp(reply, "2\tnone\tnone\n"));
   CHECK(!input_st.cmd_input && !input_st.cmd_buttons[1]
         && !input_st.cmd_analog[1][0]);
   CHECK(command_set_input(&cmd, "1 start 1"));
   CHECK(command_release_input(&cmd, ""));
   CHECK(!input_st.cmd_input);
   hardcore = true;
   CHECK(!command_set_input(&cmd, "1 start 1"));
   CHECK(cmd.error && !input_st.cmd_input);
   hardcore = false;

   printf("command_options: %s\n", failures ? "FAIL" : "PASS");
   return failures ? 1 : 0;
}
