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
audio_driver_state_t *audio_state_get_ptr(void) { return &audio_st; }
video_driver_state_t *video_state_get_ptr(void) { return &video_st; }
recording_state_t *recording_state_get_ptr(void) { return &rec_st; }
bool rcheevos_hardcore_active(void) { return hardcore; }
bool input_driver_mouse_grabbed(void) { return false; }
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

   printf("command_options: %s\n", failures ? "FAIL" : "PASS");
   return failures ? 1 : 0;
}
