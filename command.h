/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2021      - David G.F.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef COMMAND_H__
#define COMMAND_H__

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <streams/interface_stream.h>
#include "tasks/task_notify.h"

#include "retroarch_types.h"
#include "input/input_defines.h"

#include "configuration.h"

#define MAX_CMD_DRIVERS              4
#define DEFAULT_NETWORK_CMD_PORT 55355

RETRO_BEGIN_DECLS

enum event_command
{
   CMD_SPECIAL = -1,
   CMD_EVENT_NONE = 0,
   /* Resets RetroArch. */
   CMD_EVENT_RESET,
   CMD_EVENT_SET_PER_GAME_RESOLUTION,
   CMD_EVENT_SET_FRAME_LIMIT,
   /* Loads core. */
   CMD_EVENT_LOAD_CORE,
   CMD_EVENT_LOAD_CORE_PERSIST,
#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   CMD_EVENT_LOAD_SECOND_CORE,
#endif
   CMD_EVENT_UNLOAD_CORE,
   /* Closes content. */
   CMD_EVENT_CLOSE_CONTENT,
   /* Swaps the current state with what's on the undo load buffer. */
   CMD_EVENT_UNDO_LOAD_STATE,
   /* Rewrites a savestate on disk. */
   CMD_EVENT_UNDO_SAVE_STATE,
   /* Save state hotkeys. */
   CMD_EVENT_LOAD_STATE,
   CMD_EVENT_SAVE_STATE,
   CMD_EVENT_SAVE_STATE_DECREMENT,
   CMD_EVENT_SAVE_STATE_INCREMENT,
   /* Replay hotkeys. RECORD_REPLAY and the checkpoint events take a
    * task_notify_t, told when their work is through, or NULL. */
   CMD_EVENT_PLAY_REPLAY,
   CMD_EVENT_RECORD_REPLAY,
   CMD_EVENT_HALT_REPLAY,
   CMD_EVENT_SAVE_REPLAY_CHECKPOINT,
   CMD_EVENT_PREV_REPLAY_CHECKPOINT,
   CMD_EVENT_NEXT_REPLAY_CHECKPOINT,
   CMD_EVENT_REPLAY_DECREMENT,
   CMD_EVENT_REPLAY_INCREMENT,
   /* Save state actions. */
   CMD_EVENT_SAVE_STATE_TO_RAM,
   CMD_EVENT_LOAD_STATE_FROM_RAM,
   CMD_EVENT_RAM_STATE_TO_FILE,
   /* Takes screenshot. */
   CMD_EVENT_TAKE_SCREENSHOT,
   /* Quits RetroArch. */
   CMD_EVENT_QUIT,
   /* Reinitialize all drivers. */
   CMD_EVENT_REINIT_FROM_TOGGLE,
   /* Re-evaluate the on-screen notification (gfx_widgets) system to
    * match current settings, without a full driver reinit. */
   CMD_EVENT_OSD_NOTIFICATION_TOGGLE,
   /* Reinitialize all drivers. */
   CMD_EVENT_REINIT,
   /* Toggles cheevos hardcore mode. */
   CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE,
   /* Deinitialize rewind. */
   CMD_EVENT_REWIND_DEINIT,
   /* Initializes rewind. */
   CMD_EVENT_REWIND_INIT,
   /* Reinitializes rewind (primarily if the state size changes). */
   CMD_EVENT_REWIND_REINIT,
   /* Toggles rewind. */
   CMD_EVENT_REWIND_TOGGLE,
   /* Initializes autosave. */
   CMD_EVENT_AUTOSAVE_INIT,
   /* Stops audio. */
   CMD_EVENT_AUDIO_STOP,
   /* Starts audio. */
   CMD_EVENT_AUDIO_START,
   /* Mutes audio. */
   CMD_EVENT_AUDIO_MUTE_TOGGLE,
   /* Volume adjustments. */
   CMD_EVENT_VOLUME_UP,
   CMD_EVENT_VOLUME_DOWN,
   CMD_EVENT_MIXER_VOLUME_UP,
   CMD_EVENT_MIXER_VOLUME_DOWN,
   /* Toggles Video Filter*/
   CMD_VIDEO_FILTER_TOGGLE,
   /* Toggles FPS counter. */
   CMD_EVENT_FPS_TOGGLE,
   /* Toggles statistics display. */
   CMD_EVENT_STATISTICS_TOGGLE,
   /* Initializes video filter. */
   CMD_EVENT_VIDEO_FILTER_INIT,
   /* Initializes overlay. */
   CMD_EVENT_OVERLAY_INIT,
   /* Frees or caches overlay. */
   CMD_EVENT_OVERLAY_UNLOAD,
   /* Sets current scale factor for overlay. */
   CMD_EVENT_OVERLAY_SET_SCALE_FACTOR,
   /* Sets current alpha modulation for overlay. */
   CMD_EVENT_OVERLAY_SET_ALPHA_MOD,
   /* Sets diagonal sensitivities of overlay eightway areas. */
   CMD_EVENT_OVERLAY_SET_EIGHTWAY_DIAGONAL_SENSITIVITY,
   /* Deinitializes overlay. */
   CMD_EVENT_DSP_FILTER_INIT,
   /* Initializes recording system. */
   CMD_EVENT_RECORD_INIT,
   /* Deinitializes recording system. */
   CMD_EVENT_RECORD_DEINIT,
   /* Deinitializes history playlist. */
   CMD_EVENT_HISTORY_DEINIT,
   /* Initializes history playlist. */
   CMD_EVENT_HISTORY_INIT,
   /* Deinitializes core information. */
   CMD_EVENT_CORE_INFO_DEINIT,
   /* Initializes core information. */
   CMD_EVENT_CORE_INFO_INIT,
   /* Deinitializes core. */
   CMD_EVENT_CORE_DEINIT,
   /* Initializes core. */
   CMD_EVENT_CORE_INIT,
   /* Apply video state changes. */
   CMD_EVENT_VIDEO_APPLY_STATE_CHANGES,
   /* Set video blocking state. */
   CMD_EVENT_VIDEO_SET_BLOCKING_STATE,
   /* Sets current aspect ratio index. */
   CMD_EVENT_VIDEO_SET_ASPECT_RATIO,
   /* Restarts RetroArch. */
   CMD_EVENT_RESTART_RETROARCH,
#ifdef HAVE_CLOUDSYNC
   /* Trigger cloud sync */
   CMD_EVENT_CLOUD_SYNC,
   /* Resolve cloud sync conflicts by keeping local files */
   CMD_EVENT_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   /* Resolve cloud sync conflicts by keeping server files */
   CMD_EVENT_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
#endif
   /* Shutdown the OS */
   CMD_EVENT_SHUTDOWN,
   /* Reboot the OS */
   CMD_EVENT_REBOOT,
   /* Resume RetroArch when in menu. */
   CMD_EVENT_RESUME,
   /* Add a playlist entry to favorites. */
   CMD_EVENT_ADD_TO_FAVORITES,
   /* Reset playlist entry associated core to DETECT */
   CMD_EVENT_RESET_CORE_ASSOCIATION,
   /* Toggles pause. */
   CMD_EVENT_PAUSE_TOGGLE,
   /* Pauses RetroArch. */
   CMD_EVENT_MENU_PAUSE_LIBRETRO,
   CMD_EVENT_PAUSE,
   /* Unpauses RetroArch. */
   CMD_EVENT_UNPAUSE,
   /* Toggles menu on/off. */
   CMD_EVENT_MENU_TOGGLE,
   /* Configuration saving. */
   CMD_EVENT_MENU_RESET_TO_DEFAULT_CONFIG,
   CMD_EVENT_MENU_SAVE_CONFIG,
   CMD_EVENT_MENU_SAVE_AS_CONFIG,
   CMD_EVENT_MENU_SAVE_MAIN_CONFIG,
   CMD_EVENT_MENU_SAVE_CURRENT_CONFIG,
   CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   CMD_EVENT_MENU_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   CMD_EVENT_MENU_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   CMD_EVENT_MENU_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   /* Applies shader changes. */
   CMD_EVENT_SHADERS_APPLY_CHANGES,
   /* A new shader preset has been loaded */
   CMD_EVENT_SHADER_PRESET_LOADED,
   /* Shader hotkeys. */
   CMD_EVENT_SHADER_NEXT,
   CMD_EVENT_SHADER_PREV,
   CMD_EVENT_SHADER_TOGGLE,
   /* Apply cheats. */
   CMD_EVENT_CHEATS_APPLY,
   /* The codec compressed saves are written with, from the setting. */
   CMD_EVENT_SAVE_COMPRESSION_CODEC_APPLY,
   /* Cheat hotkeys. */
   CMD_EVENT_CHEAT_TOGGLE,
   CMD_EVENT_CHEAT_INDEX_PLUS,
   CMD_EVENT_CHEAT_INDEX_MINUS,
   /* Initializes network system. */
   CMD_EVENT_NETWORK_INIT,
   /* Initializes netplay system with a string or no host specified. */
   CMD_EVENT_NETPLAY_INIT,
   /* Initializes netplay system with a direct host specified. */
   CMD_EVENT_NETPLAY_INIT_DIRECT,
   /* Initializes netplay system with a direct host specified after loading content. */
   CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED,
   /* Deinitializes netplay system. */
   CMD_EVENT_NETPLAY_DEINIT,
   /* Switch between netplay gaming and watching. */
   CMD_EVENT_NETPLAY_GAME_WATCH,
   /* Open a netplay chat input menu. */
   CMD_EVENT_NETPLAY_PLAYER_CHAT,
   /* Toggle chat fading. */
   CMD_EVENT_NETPLAY_FADE_CHAT_TOGGLE,
   /* Start hosting netplay. */
   CMD_EVENT_NETPLAY_ENABLE_HOST,
   /* Disconnect from the netplay host. */
   CMD_EVENT_NETPLAY_DISCONNECT,
   /* Toggle ping counter. */
   CMD_EVENT_NETPLAY_PING_TOGGLE,
   /* Toggles netplay hosting. */
   CMD_EVENT_NETPLAY_HOST_TOGGLE,
   /* Reinitializes audio driver. */
   CMD_EVENT_AUDIO_REINIT,
   /* Resizes the window to the queued scale. */
   CMD_EVENT_RESIZE_WINDOWED_SCALE,
   /* Applies the configured windowed scale. */
   CMD_EVENT_APPLY_WINDOWED_SCALE,
   /* Toggles disk eject. */
   CMD_EVENT_DISK_EJECT_TOGGLE,
   /* Cycle to next disk. */
   CMD_EVENT_DISK_NEXT,
   /* Cycle to previous disk. */
   CMD_EVENT_DISK_PREV,
   /* Switch to specified disk index */
   CMD_EVENT_DISK_INDEX,
   /* Appends disk image to disk image list. */
   CMD_EVENT_DISK_APPEND_IMAGE,
   /* Stops rumbling. */
   CMD_EVENT_RUMBLE_STOP,
   /* Toggles turbo fire. */
   CMD_EVENT_TURBO_FIRE_TOGGLE,
   /* Toggles mouse grab. */
   CMD_EVENT_GRAB_MOUSE_TOGGLE,
   /* Toggles game focus. */
   CMD_EVENT_GAME_FOCUS_TOGGLE,
   /* Toggles desktop menu. */
   CMD_EVENT_UI_COMPANION_TOGGLE,
   /* Toggles fullscreen mode. */
   CMD_EVENT_FULLSCREEN_TOGGLE,
   CMD_EVENT_FULLSCREEN_APPLY,
   /* Toggle recording. */
   CMD_EVENT_RECORDING_TOGGLE,
   /* Toggle streaming. */
   CMD_EVENT_STREAMING_TOGGLE,
   /* Toggle Run-Ahead. */
   CMD_EVENT_RUNAHEAD_TOGGLE,
   /* Toggle Preemtive Frames. */
   CMD_EVENT_PREEMPT_TOGGLE,
   /* Deinitialize or Reinitialize Preemptive Frames. */
   CMD_EVENT_PREEMPT_UPDATE,
   /* Force Preemptive Frames to refill its state buffer. */
   CMD_EVENT_PREEMPT_RESET_BUFFER,
   /* Toggle VRR runloop. */
   CMD_EVENT_VRR_RUNLOOP_TOGGLE,
   /* Place the headset's screens in front of where it looks now. */
   CMD_EVENT_HEADSET_RECENTER,
   /* Switch the headset's Laser Pointer between Off and its last mode. */
   CMD_EVENT_LASER_POINTER_TOGGLE,
   /* Cycle what a core is told the headset displays: immersive, stereoscopic, flat. */
   CMD_EVENT_HEADSET_VIEW_TOGGLE,
   /* Start or stop moving the headset's screens with its controllers. */
   CMD_EVENT_HEADSET_EDIT_SCREENS,
   /* AI service. */
   CMD_EVENT_AI_SERVICE_TOGGLE,
   CMD_EVENT_AI_SERVICE_CALL,
   /* Misc. */
   CMD_EVENT_SAVE_FILES,
   CMD_EVENT_LOAD_FILES,
   CMD_EVENT_CONTROLLER_INIT,
   CMD_EVENT_DISCORD_INIT,
   CMD_EVENT_PRESENCE_UPDATE,
   /* camera_allow changed: a running camera the user just disallowed
    * is stopped here, so the frame path tests only its frame_work bit */
   CMD_EVENT_CAMERA_ALLOW_APPLY,
   CMD_EVENT_OVERLAY_NEXT,
   CMD_EVENT_OSK_TOGGLE,
   CMD_EVENT_RELOAD_CONFIG,
#ifdef HAVE_MICROPHONE
   /* Stops all enabled microphones. */
   CMD_EVENT_MICROPHONE_STOP,
   /* Starts all enabled microphones */
   CMD_EVENT_MICROPHONE_START,
   /* Reinitializes microphone driver. */
   CMD_EVENT_MICROPHONE_REINIT,
#endif
   /* Add a playlist entry to another playlist. */
   CMD_EVENT_ADD_TO_PLAYLIST
};

enum cmd_source_t
{
   CMD_NONE = 0,
   CMD_STDIN,
   CMD_NETWORK
};

/* What a command does, for interfaces that describe the commands to a
 * client: HELP, and tool annotations for clients that ask before they
 * run anything. */
enum cmd_info_flags
{
   /* Only reports; changes nothing. */
   CMD_INFO_READ_ONLY   = (1 << 0),
   /* May lose unsaved progress or data, or loads code from a path:
    * quits, resets, loads or overwrites states and saves, writes core
    * memory, loads or unloads cores and content. */
   CMD_INFO_DESTRUCTIVE = (1 << 1),
   /* A line command that is not offered as a tool: another command
    * covers it or takes its value outright, or it steps a selection
    * nothing reports. It stays a command on every line interface. */
   CMD_INFO_NO_TOOL     = (1 << 2)
};

/* A hotkey command: sending it presses the hotkey for one frame. */
struct cmd_map
{
   const char *str;
   unsigned id;
   const char *desc;
   unsigned flags;       /* enum cmd_info_flags */
};

struct command_handler;

typedef void (*command_poller_t)(struct command_handler *cmd);
typedef void (*command_replier_t)(struct command_handler *cmd, const char * data, size_t len);
typedef void (*command_destructor_t)(struct command_handler *cmd);
typedef void *(*command_reply_dest_t)(struct command_handler *cmd);
typedef void (*command_reply_to_t)(struct command_handler *cmd,
      void *dest, const char *data, size_t len);
/* As reply_to, with an image alongside the text: @image is base64 of
 * @mime */
typedef void (*command_reply_image_to_t)(struct command_handler *cmd,
      void *dest, const char *text, size_t len, const char *mime,
      const char *image, size_t image_len);

struct command_handler
{
   /* Interface to poll the driver */
   command_poller_t poll;
   /* Interface to reply */
   command_replier_t replier;
   /* Where the reply to the command being handled goes, for a reply
    * sent after the handler has returned: a heap copy, released with
    * free().  NULL on an interface whose replies all go one way. */
   command_reply_dest_t reply_dest;
   /* Sends to a destination reply_dest returned. */
   command_reply_to_t reply_to;
   /* Sends a reply carrying an image, where the interface can; NULL on
    * the others, which are sent the text alone */
   command_reply_image_to_t reply_image_to;
   /* Interface to delete the underlying command */
   command_destructor_t destroy;
   /* Underlying command storage */
   void *userptr;
   /* State received */
   bool state[RARCH_BIND_LIST_END];
   /* Requests are structured (MCP), not lines: given the replies a
    * line-based client never had */
   bool structured;
   /* The reply being sent reports a failure, for the interface sending
    * it to read: what a line client reads as text a structured one is
    * told outright. Set beside the reply by whatever answers - a
    * command, a deferred reply, command_reply_error() - and cleared
    * when a command starts and once the reply is away. */
   bool error;
};

typedef struct command_handler command_t;

typedef struct command_handle
{
   command_t *handle;
   unsigned id;
} command_handle_t;

struct rarch_state;

/**
 * command_event:
 * @cmd                  : Command index.
 *
 * Performs RetroArch command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool command_event(enum event_command action, void *data);

/* @action, with @arg (or NULL) as its int argument, carried out on the
 * main thread when it next retires finished tasks: for a thread that
 * is not the main one - the video thread - asking for what only the
 * main thread may do, such as starting the drivers again. Callable
 * from any thread. False if it could not be asked for. */
bool command_event_later(enum event_command action, const int *arg);

/* Constructors for the supported drivers */
#ifdef HAVE_NETWORK_CMD
/* bind_address may be NULL or empty to listen on every interface, or an
 * IPv4 address such as "127.0.0.1" to restrict the interface to it. */
command_t* command_network_new(uint16_t port, const char *bind_address);
bool command_network_send(const char *cmd_);
#endif
#ifdef HAVE_STDIN_CMD
command_t* command_stdin_new(void);
#endif
#ifdef HAVE_LAKKA
command_t* command_uds_new(void);
#endif
#ifdef __EMSCRIPTEN__
command_t* command_emscripten_new(void);
#endif


void command_event_set_mixer_volume(
      settings_t *settings,
      float gain);

bool command_event_resize_windowed_scale(settings_t *settings,
      unsigned window_scale);

size_t command_event_save_auto_state(void);

/**
 * event_set_volume:
 * @gain      : amount of gain to be applied to current volume level.
 *
 * Adjusts the current audio volume level.
 *
 **/
void command_event_set_volume(
      settings_t *settings,
      float gain,
      bool widgets_active,
      bool audio_driver_mute_enable);

/**
 * command_event_init_controllers:
 *
 * Initialize libretro controllers.
 **/
void command_event_init_controllers(rarch_system_info_t *info,
      settings_t *settings, unsigned num_active_users);

bool command_event_load_entry_state(settings_t *settings);

bool command_event_load_auto_state(void);

void command_event_set_savestate_auto_index(
      settings_t *settings);

int command_event_get_next_savestate_auto_index(
      settings_t *settings);

void command_event_set_replay_auto_index(
      settings_t *settings);

void command_event_set_replay_garbage_collect(
      unsigned max_to_keep,
      bool show_hidden_files
      );

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
bool command_set_shader(command_t *cmd, const char *arg);
#endif

#ifdef HAVE_CHEATS
void command_event_init_cheats(
      bool apply_cheats_after_load,
      const char *path_cheat_db,
      void *bsv_movie_data);
#endif

#if defined(HAVE_COMMAND)
enum cmd_param_type
{
   /* text, as the command takes it */
   CMD_PARAM_STRING = 0,
   /* a whole number; a structured client sends a JSON number */
   CMD_PARAM_INT
};

/* One of a command's parameters, for interfaces that take a request
 * apart rather than a line: a tool schema is generated from these, and
 * what a client sends by name is put back together into the line the
 * handlers read.
 *
 * A space joins them in that line, and nothing precedes the first, so
 * @lead is only for a command whose own form uses something else -
 * LOAD_CONTENT's "|" - and a command's grammar is described here
 * instead of being parsed by hand at both ends. @values, where set, is the "|"-separated set the
 * parameter accepts, which the schema offers as an enum.
 *
 * @def stands in for an optional parameter the client left out, so the
 * line form says it rather than leaving the handler to guess: what
 * GET_PLAYLIST reads as a trailing entry number cannot be told from a
 * playlist whose name ends in one, and a default settles it.
 *
 * An array of these ends with a row whose @name is NULL. */
struct cmd_param
{
   const char *name;
   const char *desc;
   enum cmd_param_type type;
   bool        required;
   const char *lead;
   const char *values;
   const char *def;
};

/* One column of a reply that is rows: the name a field on the line is
 * given by an interface that answers in fields rather than in text. */
struct cmd_column
{
   const char *name;
   const char *desc;
};

/* A line among a reply's rows that is not one of them: a word, and a
 * number ending the line. "MORE 200" says a list was cut short and where to
 * resume, "NEXT 48" where GET_LOG reached, "SKIPPED 12" how many log
 * lines were gone before the first it could give.
 *
 * @flag is a field the word alone sets true, for a word that can come
 * without a number - a bare "MORE", from a reply with no resume point.
 * @name is the field its number is answered as. Either may be NULL.
 * @first is a word that opens the reply instead of closing it. @text
 * makes the rest of the line, on whichever line it comes, the field's
 * text: "MESSAGE Press again to quit" rather than a number. */
struct cmd_trailer
{
   const char *word;
   const char *flag;
   const char *name;
   const char *desc;
   bool        first;
   bool        text;
};

/* The shape of a reply that is rows of named columns - the lines of
 * tab-separated fields the lists and GET_PLAYLIST answer with - so an
 * interface can hand a client the same reply taken apart rather than
 * leaving every client to split the text.
 *
 * @slack is the one column whose field may hold a tab itself, a game's
 * name among them: the columns before it are read from the start of the
 * line and those after it from the end, so that tab takes the slack
 * instead of shifting the row.
 *
 * A line the command opens with its own name - "LIST_CORES none",
 * "GET_PLAYLIST ERROR ..." - is not a row, and neither is one of
 * @trailers, which may be that same line: "RUN_FRAMES 3".
 *
 * @required is how many of the columns every row has, where a reply
 * ends short of them: GET_CORE_OPTION gives an option's key, value and
 * description, and the values it takes only when asked for that option
 * by key. Zero means every column, which is what the rest answer.
 *
 * @columns ends with a row whose @name is NULL, @trailers - when there
 * are any - with one whose @word is. @columns itself is NULL for a
 * reply whose lines are prose and whose trailers are all a client
 * wants: GET_LOG's answer is the log, and the line it reached. */
struct cmd_rows
{
   const struct cmd_column  *columns;
   const struct cmd_trailer *trailers;
   unsigned                  slack;
   unsigned                  required;
   /* Set, the reply is one record rather than rows: the line the
    * command opens with its own name, split into @columns by these
    * characters in turn - the last column takes what is left - and
    * answered as fields of their own. */
   const char               *seps;
   /* The reply may carry a "PREVIOUS ..." line: how a queued STEPS no
    * reply has told of ended. */
   bool                      previous;
};

/* A command with its own handler; @arg_desc names the argument, or
 * "No argument", and @params describes it for a structured client, or
 * is NULL on one that takes none. @rows is the shape of its reply where
 * that reply is rows, and NULL where it is prose. */
struct cmd_action_map
{
   const char *str;
   bool (*action)(command_t* cmd, const char *arg);
   const char *arg_desc;
   const char *desc;
   unsigned flags;       /* enum cmd_info_flags */
   const struct cmd_param *params;
   const struct cmd_rows *rows;
};

#if defined(HAVE_SCREENSHOTS) && defined(HAVE_RPNG)
/* GET_FRAME [size]: the frame on screen, as image content where the
 * interface carries one; a PNG, built in memory */
#define COMMAND_FRAME_SIZE     320
#define COMMAND_FRAME_SIZE_MIN 64
#define COMMAND_FRAME_SIZE_MAX 640
bool command_get_frame(command_t *cmd, const char *arg);
#endif

/* GET_OPTION [option] / SET_OPTION <option> on|off */
bool command_get_option(command_t *cmd, const char *arg);
bool command_set_option(command_t *cmd, const char *arg);

#ifdef HAVE_MCP
/* GET_LOG [since] */
bool command_get_log(command_t *cmd, const char *arg);
#endif

/* RUN_FRAMES <count>. The runloop calls: tick after a frame a job ran;
 * free_frame after one with no job; between before a frame, with the
 * core off the stack, for a STEPS checkpoint; cancel when the content
 * closes. forget ends a queued STEPS once the state it was planned
 * from is gone: a state loaded, a reset, a close. */
#define COMMAND_RUN_FRAMES_MAX 36000
bool command_run_frames(command_t *cmd, const char *arg);
void command_run_frames_tick(void);
void command_run_frames_between(void);

#ifdef HAVE_MENU
/* MENU: the runloop hands action the menu's own action for a frame,
 * before the menu runs it, and calls iterated once it has. */
unsigned command_menu_action(unsigned action);
void command_menu_iterated(void);
void command_menu_closed(void);
#endif
void command_run_frames_cancel(const char *why);
void command_run_frames_forget(void);

/* Achievements in hardcore mode are the player's own, as they forbid
 * rewind and cheats: tools_refused is why an assistant's tool calls are
 * refused now, or NULL; hardcore_entered stops what one left running -
 * a frame job, a queue, held input - as hardcore mode takes effect. */
const char *command_tools_refused(void);
void command_hardcore_entered(void);
/* On the main thread each frame, before commands are read: what other
 * threads left for it, and a MENU request whose menu closed. */
void command_frame_start(void);
void command_run_frames_free_frame(void);

/* SET_INPUT <player> <control> <value> / RELEASE_INPUT [player], and
 * GET_INPUT [player] to look without changing anything. */
bool command_set_input(command_t *cmd, const char *arg);
bool command_release_input(command_t *cmd, const char *arg);
bool command_get_input(command_t *cmd, const char *arg);

/* The most frames a request runs, which a structured interface lowers
 * to nine seconds of the content's frames to answer inside its
 * ten-second reply deadline. PRESS keeps to it everywhere, since it
 * holds input for every frame it runs. */
#define COMMAND_FRAMES_REPLY_MAX 600

/* PRESS <player> <control> <frames> [until <condition>]: hold, run and
 * let go, then run on until the condition holds, in one. */
bool command_press(command_t *cmd, const char *arg);

/* RUN_UNTIL <address>[:<address>] <condition> <value> [max frames]: run
 * the content until a byte, or two read as one value, says to stop. The
 * conditions it takes are command.c's, which is all that compares them. */
bool command_run_until(command_t *cmd, const char *arg);

/* STEPS <step>[; <step>...][|on]: run, press, until and checkpoint
 * steps, and a stop, as one request; with on, queued behind those
 * still running, answered at once. */
bool command_steps(command_t *cmd, const char *arg);

/* WATCH [<label> <address|off>]...: the bytes RUN_FRAMES, PRESS and
 * RUN_UNTIL carry along with their own answer, looked up afresh each
 * time. Cleared when content closes: the next content's addresses mean
 * something else. */
bool command_watch(command_t *cmd, const char *arg);
void command_watch_clear(void);

/* GET_CORE_OPTION [key] / SET_CORE_OPTION <key> <value> */
bool command_get_core_option(command_t *cmd, const char *arg);
bool command_set_core_option(command_t *cmd, const char *arg);

/* Replies with a failure and why, and marks the reply as one. The text
 * opens with @name and ERROR, as a deferred reply's does. */
void command_reply_error(command_t *cmd, const char *name,
      const char *fmt, ...);

/* HELP [command]: the commands, their arguments and what they do. */
bool command_help(command_t *cmd, const char* arg);
/* Queries for clients that browse and launch content. */
bool command_list_cores(command_t *cmd, const char* arg);
bool command_list_playlists(command_t *cmd, const char* arg);
bool command_get_playlist(command_t *cmd, const char* arg);

/* Runs one command by name; see command.c. */
bool command_run(command_t *handle, const char *name, const char *arg);

/* True once a command has started a content load: requests an
 * interface has yet to take wait for the next poll, which comes once
 * the load is through. */
bool command_interfaces_held(void);

/* Every command, for interfaces that describe them to a client. */
const struct cmd_action_map *command_action_list(size_t *count);
const struct cmd_map *command_hotkey_list(size_t *count);

bool command_version(command_t *cmd, const char* arg);
bool command_get_status(command_t *cmd, const char* arg);
bool command_get_config_param(command_t *cmd, const char* arg);
bool command_show_osd_msg(command_t *cmd, const char* arg);
bool command_load_state_slot(command_t *cmd, const char* arg);
bool command_save_state_slot(command_t* cmd, const char* arg);
bool command_play_replay_slot(command_t *cmd, const char* arg);
bool command_seek_replay(command_t *cmd, const char *arg);
bool command_save_savefiles(command_t *cmd, const char* arg);
bool command_load_savefiles(command_t *cmd, const char* arg);
#ifdef HAVE_CHEEVOS
bool command_read_ram(command_t *cmd, const char *arg);
bool command_write_ram(command_t *cmd, const char *arg);
#endif
bool command_read_memory(command_t *cmd, const char *arg);
bool command_write_memory(command_t *cmd, const char *arg);
bool command_get_memory_map(command_t *cmd, const char *arg);
bool command_load_core(command_t *cmd, const char* arg);
bool command_start_core(command_t *cmd, const char* arg);
bool command_load_content(command_t *cmd, const char* arg);
bool command_close_content(command_t *cmd, const char* arg);
bool command_unload_core(command_t *cmd, const char* arg);
bool command_video_reinit(command_t *cmd, const char* arg);
bool command_audio_reinit(command_t *cmd, const char* arg);
bool command_drivers_reinit(command_t *cmd, const char* arg);

/* MENU <button>, DISK <action>, REINIT <driver>, STATE <action> <slot>,
 * REPLAY <action> [slot|frame]: the hotkeys and commands of one family
 * behind one name, as one tool. */
#define COMMAND_MENU_ACTIONS   "show|open|close|select|up|down|left|right|a|b|type"
/* the entries MENU answers with at once */
#define COMMAND_MENU_PAGE      50
#define COMMAND_DISK_ACTIONS   "eject|next|prev"
#define COMMAND_REINIT_DRIVERS "video|audio|all"
#define COMMAND_STATE_ACTIONS  "save|load|checkpoint|rollback"
#define COMMAND_REPLAY_ACTIONS "play|seek|record|halt|checkpoint"
#define COMMAND_CORE_ACTIONS   "load|start|unload"
#define COMMAND_INPUT_ACTIONS  "set|release|show"
#define COMMAND_OPTION_ACTIONS "show|set"
bool command_menu(command_t *cmd, const char *arg);
bool command_disk(command_t *cmd, const char *arg);
bool command_reinit(command_t *cmd, const char *arg);
bool command_state(command_t *cmd, const char *arg);
/* CHECKPOINT_STATE and ROLLBACK_STATE: one state kept in memory for
 * the content's life, which command_checkpoint_clear() drops. */
bool command_checkpoint_save(command_t *cmd, const char *arg);
/* RESET [on|off]: the hotkey's reset, paused after it with on. */
bool command_reset(command_t *cmd, const char *arg);
bool command_checkpoint_rollback(command_t *cmd, const char *arg);
void command_checkpoint_clear(void);
bool command_core(command_t *cmd, const char *arg);
bool command_replay(command_t *cmd, const char *arg);
/* INPUT, OPTION and CORE_OPTION: SET_INPUT, RELEASE_INPUT, GET_INPUT,
 * GET_OPTION, SET_OPTION, GET_CORE_OPTION and SET_CORE_OPTION */
bool command_input(command_t *cmd, const char *arg);
bool command_option(command_t *cmd, const char *arg);
bool command_core_option(command_t *cmd, const char *arg);

/* The parameters of the commands that take any. Several commands take
 * the same ones; what tells READ_CORE_RAM from READ_CORE_MEMORY is the
 * command's own description, not its parameters'. */
static const struct cmd_param cmd_params_help[] = {
   { "command", "A command name. Left out, every command.",
      CMD_PARAM_STRING, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_preset[] = {
   { "preset", "Path to a shader preset, absolute or under the shader "
      "directory. Empty turns the shader off.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_option[] = {
   { "option", "An option name, as GET_OPTION with no argument lists.",
      CMD_PARAM_STRING, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_option_set[] = {
   { "option", "An option name, as GET_OPTION with no argument lists.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "state", "The state to put it in.",
      CMD_PARAM_STRING, true, NULL, "on|off", NULL },
   { NULL }
};
#ifdef HAVE_MCP
static const struct cmd_param cmd_params_log[] = {
   { "since", "The first line wanted, as next gave it; 0 when left out.",
      CMD_PARAM_INT, false, NULL, NULL, "0" },
   { NULL }
};
#endif
static const struct cmd_param cmd_params_run_frames[] = {
   { "count", "How many frames to run; ask again to run more.", CMD_PARAM_INT, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_input[] = {
   { "player", "The player, from 1.", CMD_PARAM_INT, true, NULL, NULL, NULL },
   /* No enum: a combination is not one of the names, and a client that
    * held the schema to one would refuse to send it. */
   { "control", "A RetroPad button or stick axis, or several joined by "
      "\"+\" to hold together: b, y, select, start, up, down, left, "
      "right, a, x, l, r, l2, r2, l3, r3, left_x, left_y, right_x, "
      "right_y. \"right+a\" runs and jumps in one request.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "value", "1 holds a button and 0 lets it go; an axis takes -32768 "
      "to 32767, 0 letting it go.", CMD_PARAM_INT, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_input_family[] = {
   { "action", "set holds or lets go of controls; release lets go of all "
      "a player holds; show reads it.", CMD_PARAM_STRING, true, NULL,
      COMMAND_INPUT_ACTIONS, NULL },
   { "player", "From 1. Left out, release frees every player and show "
      "reads the first.", CMD_PARAM_INT, false, NULL, NULL, NULL },
   /* No enum: a combination is not one of the names, and a client that
    * held the schema to one would refuse to send it. */
   { "control", "For set: a button or stick axis, or several joined by "
      "\"+\": b, y, select, start, up, down, left, right, a, x, l, r, l2, "
      "r2, l3, r3, left_x, left_y, right_x, right_y.",
      CMD_PARAM_STRING, false, NULL, NULL, NULL },
   { "value", "For set: 1 holds a button, 0 lets go; an axis takes "
      "-32768 to 32767.", CMD_PARAM_INT, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_option_family[] = {
   { "action", "show one option, or all; set one.", CMD_PARAM_STRING,
      true, NULL, COMMAND_OPTION_ACTIONS, NULL },
   { "option", "An option name, as show lists it.", CMD_PARAM_STRING,
      false, NULL, NULL, NULL },
   { "state", "For set.", CMD_PARAM_STRING, false, NULL, "on|off", NULL },
   { NULL }
};
static const struct cmd_param cmd_params_core_option_family[] = {
   { "action", "show the core's options, or one with the values it "
      "takes; set one.", CMD_PARAM_STRING, true, NULL,
      COMMAND_OPTION_ACTIONS, NULL },
   { "option", "A key, as show lists it.", CMD_PARAM_STRING, false, NULL,
      NULL, NULL },
   { "value", "For set: one of the values show lists for the key.",
      CMD_PARAM_STRING, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_watch[] = {
   { "watches", "Label and hexadecimal address pairs, as GET_MEMORY_MAP "
      "gives them: \"x 86 mode 770\"; \"x 6d:86\" reads two bytes as one "
      "value. A label watched again is replaced; off as its address stops "
      "it. 16 at most.",
      CMD_PARAM_STRING, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_until[] = {
   { "address", "Hexadecimal, as GET_MEMORY_MAP gives it; high:low "
      "(6d:86) reads two bytes as one value; & 0x40 after it compares "
      "only those bits.", CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "condition", "How it compares with the value.",
      CMD_PARAM_STRING, true, NULL, "==|!=|<|>|<=|>=", NULL },
   { "value", "0 to 255, or 65535 for high:low.",
      CMD_PARAM_INT, true, NULL, NULL, NULL },
   { "frames", "Frames to wait at most; a request's nine seconds when "
      "left out.", CMD_PARAM_INT, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_press[] = {
   { "player", "From 1.", CMD_PARAM_INT, true, NULL, NULL, NULL },
   { "control", "Buttons as INPUT names them, joined by \"+\": "
      "\"right+a\" runs and jumps.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "frames", "Frames to hold them: a tap is 2 to 4. Right after a "
      "press it lets go for a frame first.",
      CMD_PARAM_INT, true, NULL, NULL, NULL },
   { "until", "Then run until this holds, as RUN_UNTIL's: \"1d == 0\" "
      "lands a jump.",
      CMD_PARAM_STRING, false, " until ", NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_steps[] = {
   { "steps", "In order, by \";\": run <frames>, press <player> <controls> "
      "<frames>, until <condition> as RUN_UNTIL's, checkpoint, and up to "
      "four stop <condition> that end it: \"until 86 >= 200; "
      "press 1 a 20; until 1d == 0; stop e == 11\". 32 at most. A press "
      "after a press lets go for a frame first.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "queue", "on: answer now, run after those queued, pause when all "
      "have (idle: ms since). After a queued list ends the next reply says "
      "how (previous), and a queued call drops its steps (discarded). "
      "Empty steps asks; a call not queued ends the queue.",
      CMD_PARAM_STRING, false, "|", "on|off", NULL },
   { NULL }
};
static const struct cmd_param cmd_params_input_release[] = {
   { "player", "The player, from 1; every player when left out.",
      CMD_PARAM_INT, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_input_get[] = {
   { "player", "The player, from 1; the first when left out.",
      CMD_PARAM_INT, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_core_option[] = {
   { "option", "A core option key, as GET_CORE_OPTION with no argument "
      "lists it.", CMD_PARAM_STRING, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_core_option_set[] = {
   { "option", "A core option key, as GET_CORE_OPTION with no argument "
      "lists it.", CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "value", "One of the values GET_CORE_OPTION with the key lists.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_config[] = {
   { "parameter", "A configuration setting's name.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_playlist[] = {
   { "playlist", "A playlist name as LIST_PLAYLISTS gives it, \".lpl\" "
      "and all.", CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "first", "The entry to start at; the first when left out.",
      CMD_PARAM_INT, false, NULL, NULL, "0" },
   { NULL }
};
#if defined(HAVE_SCREENSHOTS) && defined(HAVE_RPNG)
static const struct cmd_param cmd_params_get_frame[] = {
   { "size", "The longest side at most, 64 to 640; 320 when left out.",
      CMD_PARAM_INT, false, NULL, NULL, "320" },
   { NULL }
};
#endif
static const struct cmd_param cmd_params_message[] = {
   { "message", "The text.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_read[] = {
   { "address", "The address to read from, in hexadecimal.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "bytes", "How many.",
      CMD_PARAM_INT, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_write[] = {
   { "address", "The address to write to, in hexadecimal.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "bytes", "The bytes to write, in hexadecimal, separated by spaces.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_slot[] = {
   { "slot", "The slot number.", CMD_PARAM_INT, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_menu[] = {
   { "action", "select moves to index; left and right change a value, a "
      "confirms, b goes back, type answers a keyboard.",
      CMD_PARAM_STRING, true, NULL, COMMAND_MENU_ACTIONS, NULL },
   { "text", "For type: the text, typed and confirmed.",
      CMD_PARAM_STRING, false, NULL, NULL, NULL },
   { "index", "An entry: select, left, right and a act on it.",
      CMD_PARAM_INT, false, NULL, NULL, NULL },
   { "from", "The first entry listed; else the page with the selection.",
      CMD_PARAM_INT, false, " from ", NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_disk[] = {
   { "action", "eject ejects or inserts it.", CMD_PARAM_STRING, true,
      NULL, COMMAND_DISK_ACTIONS, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_reinit[] = {
   { "driver", "Which driver, or all of them.", CMD_PARAM_STRING, true,
      NULL, COMMAND_REINIT_DRIVERS, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_state[] = {
   { "action", "checkpoint keeps a state in memory until the content "
      "closes; rollback returns to it, the quick retry after a mistake.",
      CMD_PARAM_STRING, true, NULL,
      COMMAND_STATE_ACTIONS, NULL },
   { "slot", "For save and load.", CMD_PARAM_INT, false,
      NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_reset[] = {
   { "paused", "on: pause once it has reset, before a frame runs; the "
      "game has not set up its RAM yet.", CMD_PARAM_STRING, false, NULL,
      "on|off", NULL },
   { NULL }
};
static const struct cmd_param cmd_params_core_family[] = {
   { "action", "unload closes the content too.",
      CMD_PARAM_STRING, true, NULL, COMMAND_CORE_ACTIONS, NULL },
   { "core", "load: the path, as LIST_CORES gives it.", CMD_PARAM_STRING,
      false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_replay[] = {
   { "action", "seek moves the playing replay to a frame; record uses "
      "the current slot; checkpoint marks the one recording.",
      CMD_PARAM_STRING, true,
      NULL, COMMAND_REPLAY_ACTIONS, NULL },
   { "number", "The slot to play, or the frame to seek to.",
      CMD_PARAM_INT, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_frame[] = {
   { "frame", "The frame to seek to.", CMD_PARAM_INT, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_cores[] = {
   { "content", "Only the cores that support this content path.",
      CMD_PARAM_STRING, false, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_core[] = {
   { "core", "Path to a core library, as LIST_CORES gives it.",
      CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param cmd_params_content[] = {
   { "core", "A core's path, as LIST_CORES and GET_PLAYLIST give it; "
      "DETECT picks one that supports the content.",
      CMD_PARAM_STRING, false, NULL, NULL, "DETECT" },
   { "content", "Path to the content, as GET_PLAYLIST gives it.",
      CMD_PARAM_STRING, true, "|", NULL, NULL },
   { "paused", "on: pause once it has loaded. The core has run a frame "
      "or two; the game may not have set up its RAM yet.",
      CMD_PARAM_STRING, false,
      "|", "on|off", NULL },
   { NULL }
};

/* The replies that are rows of named columns, in the order the line
 * gives the fields. */
static const struct cmd_column cmd_cols_option[] = {
   { "option", "The option's name, as OPTION set takes it." },
   { "value", "Its state - on or off - or, for one that only reports, "
      "what it reports." },
   { NULL }
};
static const struct cmd_column cmd_cols_core[] = {
   { "name", "The core's display name." },
   { "path", "The core's path, as LOAD_CONTENT takes it." },
   { NULL }
};
static const struct cmd_column cmd_cols_playlist[] = {
   { "playlist", "The playlist's name, as GET_PLAYLIST takes it." },
   { NULL }
};
static const struct cmd_column cmd_cols_entry[] = {
   { "index", "The entry's number in the playlist." },
   { "label", "The entry's name." },
   { "content", "The content's path, as LOAD_CONTENT takes it." },
   { "core", "The core's path, as LOAD_CONTENT takes it; empty where the "
      "playlist names none." },
   { NULL }
};

static const struct cmd_column cmd_cols_core_option[] = {
   { "key", "The option's key, as CORE_OPTION set takes it." },
   { "value", "The value it has now." },
   { "description", "What the option is called in the menu." },
   { "values", "The values it takes, separated by \"|\"; given only "
      "where the option was asked for by key." },
   { NULL }
};
static const struct cmd_column cmd_cols_memory[] = {
   { "start", "The region's first address." },
   { "length", "How many bytes it covers." },
   { "select", "Which address bits name the region." },
   { "disconnect", "Which address bits it ignores." },
   { "kind", "system, save, video or other." },
   { "access", "rw, ro, or none where the core exposes none." },
   { "order", "big or little." },
   { "space", "What the core calls the address space, or empty." },
   { NULL }
};

static const struct cmd_column cmd_cols_watch[] = {
   { "label", "What the client called it." },
   { "address", "Where it is, in hexadecimal." },
   { "value", "What it holds, in decimal: 0 to 255, or to 65535 for "
      "high:low; none where nothing maps it now." },
   { NULL }
};
static const struct cmd_column cmd_cols_input[] = {
   { "player", "The player the controls are held for, from 1." },
   { "buttons", "The buttons held, separated by spaces, or none." },
   { "axes", "The axes held, as name=value separated by spaces, or none." },
   { NULL }
};
/* A list long enough to be cut short, with nowhere to resume from. */
static const struct cmd_trailer cmd_trailers_cut[] = {
   { "MORE", "more", NULL, NULL },
   { NULL }
};
/* A list cut short with the entry to resume from. */
static const struct cmd_trailer cmd_trailers_page[] = {
   { "MORE", "more", "next",
      "The entry the next page starts at, as GET_PLAYLIST's first "
      "takes it." },
   { NULL }
};
/* RUN_UNTIL opens with one of the first two, whichever happened. */
static const struct cmd_trailer cmd_trailers_until[] = {
   { "MET", NULL, "met",
      "The frames run until the condition held.", true },
   { "WAITING", NULL, "waiting",
      "The frames run without it meeting it; ask again to wait more.",
      true },
   { "VALUE", NULL, "value",
      "What the memory waited on holds now." },
   { NULL }
};
/* the step's own line: the frames it ran end it */
static const struct cmd_trailer cmd_trailers_frames[] = {
   { "RUN_FRAMES", NULL, "frames", "The frames run.", true },
   { NULL }
};
static const struct cmd_trailer cmd_trailers_press[] = {
   { "PRESS", NULL, "frames", "The frames run.", true },
   { "MET", NULL, "met", "With until: frames run until it held.", true },
   { "WAITING", NULL, "waiting", "With until: frames run without.", true },
   { "VALUE", NULL, "value", "With until: what it read." },
   { NULL }
};
static const struct cmd_trailer cmd_trailers_steps[] = {
   { "QUEUED", NULL, "queued", "Steps left.", true },
   { "DISCARDED", NULL, "discarded", "Steps dropped.", true },
   { "DONE", NULL, "done", "Frames run.", true },
   { "WAITING", NULL, "waiting", "Frames run; ask again.", true },
   { "STOPPED", NULL, "stopped", "Frames run.", true },
   { "IDLE", NULL, "idle", "Ms paused, run dry." },
   { "STOP", NULL, "stop", NULL },
   { "STEP", NULL, "step", NULL },
   { NULL }
};
static const struct cmd_trailer cmd_trailers_log[] = {
   { "NEXT", NULL, "next",
      "The line to ask for next, as GET_LOG's since takes it." },
   { "SKIPPED", NULL, "skipped",
      "Lines that were asked for but had already left the log.", true },
   { NULL }
};

static const struct cmd_rows cmd_rows_option    = { cmd_cols_option, NULL, 1, 0 };
static const struct cmd_rows cmd_rows_cores     = { cmd_cols_core, cmd_trailers_cut, 0, 0 };
static const struct cmd_rows cmd_rows_playlists = { cmd_cols_playlist, cmd_trailers_cut, 0, 0 };
/* the values, where given, are last and take whatever is left */
static const struct cmd_rows cmd_rows_core_option =
   { cmd_cols_core_option, NULL, 3, 3 };
static const struct cmd_rows cmd_rows_memory    = { cmd_cols_memory, NULL, 7, 0 };
static const struct cmd_rows cmd_rows_entries   = { cmd_cols_entry, cmd_trailers_page, 1, 0 };
static const struct cmd_rows cmd_rows_input     = { cmd_cols_input, NULL, 0, 0 };
static const struct cmd_rows cmd_rows_watch     = { cmd_cols_watch, NULL, 0, 0 };
static const struct cmd_rows cmd_rows_frames    = { cmd_cols_watch, cmd_trailers_frames, 0, 0, NULL, true };
static const struct cmd_rows cmd_rows_press     = { cmd_cols_watch, cmd_trailers_press, 0, 0, NULL, true };
static const struct cmd_rows cmd_rows_until     = { cmd_cols_watch, cmd_trailers_until, 0, 0, NULL, true };
static const struct cmd_rows cmd_rows_steps     = { cmd_cols_watch, cmd_trailers_steps, 0, 0, NULL, true };
/* No columns: the lines are the text, and splitting a 60k page into a
 * row apiece would answer with it twice over. What a client cannot read
 * off the text is how far the log got, which is what the trailers say. */
static const struct cmd_rows cmd_rows_log       = { NULL, cmd_trailers_log, 0, 0 };
/* "GET_STATUS PLAYING nes,Super Mario Bros.": a comma in the game's
 * name stays in it, as the last field takes the rest */
static const struct cmd_column cmd_cols_status[] = {
   { "state", "PLAYING, PAUSED or CONTENTLESS." },
   { "system", "The content's system, or the core's name." },
   { "content", "The content's file name." },
   { NULL }
};
static const struct cmd_rows cmd_rows_status    = { cmd_cols_status, NULL, 0, 1, " ," };
static const struct cmd_column cmd_cols_read[] = {
   { "address", "Where the bytes were read, in hexadecimal." },
   { "bytes", "The bytes, in hexadecimal, separated by spaces." },
   { NULL }
};
static const struct cmd_trailer cmd_trailers_read[] = {
   { "TRUNCATED", "truncated", "read", "Bytes read, short of those asked." },
   { NULL }
};
static const struct cmd_rows cmd_rows_read      = { cmd_cols_read, cmd_trailers_read, 0, 0, " " };
/* "MENU Main Menu", the entries, then which is selected of how many */
static const struct cmd_column cmd_cols_menu[] = {
   { "index", NULL },
   { "label", NULL },
   { "value", "\"...\" opens a list; \"checked\" is the choice in use." },
   { NULL }
};
static const struct cmd_trailer cmd_trailers_menu[] = {
   { "MENU", NULL, "title", NULL, false, true },
   { "CLOSED", "closed", NULL, "The menu is closed.", true },
   { "KEYBOARD", NULL, "keyboard", "Asks for text: type, or b.",
      false, true },
   { "MESSAGE", NULL, "message", "Over the list: a confirms, b dismisses.",
      false, true },
   { "PATH", NULL, "path", "The lists above it.", false, true },
   { "SELECTED", NULL, "selected", NULL },
   { "COUNT", NULL, "count", NULL },
   { "MORE", "more", "next", "from for the next page." },
   { NULL }
};
static const struct cmd_rows cmd_rows_menu      = { cmd_cols_menu, cmd_trailers_menu, 1, 0, NULL, false };

static const struct cmd_action_map action_map[] = {
   /* HELP and VERSION are not tools: a structured interface lists the
    * commands itself and names its version in every answer. */
   { "HELP",             command_help,             "[command]", "List the commands, or describe one.", CMD_INFO_READ_ONLY | CMD_INFO_NO_TOOL, cmd_params_help },
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   { "SET_SHADER",       command_set_shader,       "<shader path>", "Load the shader preset at the given path.", 0, cmd_params_preset },
#endif
   { "VERSION",          command_version,          "No argument", "Report the RetroArch version.", CMD_INFO_READ_ONLY | CMD_INFO_NO_TOOL, NULL },
   { "GET_STATUS",       command_get_status,       "No argument", "PLAYING, PAUSED or CONTENTLESS, with the system and the content's file name.", CMD_INFO_READ_ONLY, NULL, &cmd_rows_status },
#if defined(HAVE_SCREENSHOTS) && defined(HAVE_RPNG)
   { "GET_FRAME",        command_get_frame,        "[size]", "The content's frame on screen now, as an image scaled down to fit size; nothing is written. For what memory cannot say - a game whose RAM is not charted, or checking it.", CMD_INFO_READ_ONLY, cmd_params_get_frame },
#endif
   { "GET_OPTION",       command_get_option,       "[option]", "Report an option's value, or list every option with its value.", CMD_INFO_READ_ONLY | CMD_INFO_NO_TOOL, cmd_params_option, &cmd_rows_option },
   { "SET_OPTION",       command_set_option,       "<option> on|off", "Turn an option on or off. Asking for the state it is already in changes nothing.", CMD_INFO_NO_TOOL, cmd_params_option_set, &cmd_rows_option },
   /* GET_OPTION reports these and more; this stays for the clients that
    * have always asked it. */
   { "GET_CONFIG_PARAM", command_get_config_param, "<param name>", "Report the value of a configuration setting.", CMD_INFO_READ_ONLY | CMD_INFO_NO_TOOL, cmd_params_config },
#ifdef HAVE_MCP
   { "GET_LOG",          command_get_log,          "[since]", "The lines RetroArch has logged since a line, where to ask from next (next), and how many asked for were gone (skipped).", CMD_INFO_READ_ONLY, cmd_params_log, &cmd_rows_log },
#endif
   { "RUN_FRAMES",       command_run_frames,       "<count>", "Pause and advance the content some frames; it finishes paused. One request runs nine seconds of frames at most (540 at 60fps), as PRESS, RUN_UNTIL and STEPS do.", 0, cmd_params_run_frames, &cmd_rows_frames },
   { "SET_INPUT",        command_set_input,        "<player> <control>[+<control>...] <value>", "Hold RetroPad buttons or stick axes for the core, over the player's own input, until set again or released. The core is given exactly what is held: opposing directions are not cleaned and the D-Pad is not turned with the picture. Under netplay the player is this machine's, and what is held is sent like a press. Answers with the player, the buttons held and the axes held, none for either where nothing is.", CMD_INFO_NO_TOOL, cmd_params_input, &cmd_rows_input },
   { "PRESS",            command_press,            "<player> <control>[+<control>...] <frames> [until <condition>]", "Press buttons for some frames and let go; with until, run on until a condition holds. What INPUT holds stays held. Answers with what WATCH watches; finishes paused.", 0, cmd_params_press, &cmd_rows_press },
   { "RUN_UNTIL",        command_run_until,        "<address>[:<address>] <condition> <value> [max frames]", "Run until memory meets a condition. Answers met or waiting (ask again) with the frames run, what WATCH watches and the value.", 0, cmd_params_until, &cmd_rows_until },
   { "STEPS",            command_steps,            "<step>[; <step>...][|on]", "Play a stretch in one request, or queued to play on unpaused. Answers with how it ended and what WATCH watches.", 0, cmd_params_steps, &cmd_rows_steps },
   { "WATCH",            command_watch,            "[<label> <address|off>]...", "Watch bytes under labels: every step answers with them, so needs no read after. With none, answers each value, in decimal. Cleared when content closes.", 0, cmd_params_watch, &cmd_rows_watch },
   { "RELEASE_INPUT",    command_release_input,    "[player]", "Let go of everything SET_INPUT holds, for one player, or every player when given none. Answers as GET_INPUT does, for that player or the first.", CMD_INFO_NO_TOOL, cmd_params_input_release, &cmd_rows_input },
   { "GET_INPUT",        command_get_input,        "[player]", "Report what a player holds, without changing it: SET_INPUT has to set something to answer and RELEASE_INPUT lets go before it does.", CMD_INFO_READ_ONLY | CMD_INFO_NO_TOOL, cmd_params_input_get, &cmd_rows_input },
   { "GET_CORE_OPTION",  command_get_core_option,  "[key]", "List the running core's options: key, value and description; with a key, that option with the values it takes.", CMD_INFO_READ_ONLY | CMD_INFO_NO_TOOL, cmd_params_core_option, &cmd_rows_core_option },
   { "SET_CORE_OPTION",  command_set_core_option,  "<key> <value>", "Set a core option to one of its values. Asking for the value it already has changes nothing.", CMD_INFO_NO_TOOL, cmd_params_core_option_set },
   { "LIST_CORES",       command_list_cores,       "[content path]", "The installed cores: name, and the path LOAD_CONTENT takes.", CMD_INFO_READ_ONLY, cmd_params_cores, &cmd_rows_cores },
   { "LIST_PLAYLISTS",   command_list_playlists,   "No argument", "List the playlists, as GET_PLAYLIST takes them.", CMD_INFO_READ_ONLY, NULL, &cmd_rows_playlists },
   { "GET_PLAYLIST",     command_get_playlist,     "<playlist> [first entry]", "A playlist's entries, 200 at a time: index, label, content and core paths, and next where there are more.", CMD_INFO_READ_ONLY, cmd_params_playlist, &cmd_rows_entries },
   { "SHOW_MSG",         command_show_osd_msg,     "<message>", "Show a message on screen.", 0, cmd_params_message },
#if defined(HAVE_CHEEVOS)
   /* These functions use achievement addresses and only work if a game with achievements is
    * loaded. READ_CORE_MEMORY and WRITE_CORE_MEMORY are preferred and use system addresses. */
   { "READ_CORE_RAM",    command_read_ram,         "<address> <number of bytes>", "Read bytes from core memory at an achievement address (needs a game with achievements).", CMD_INFO_READ_ONLY | CMD_INFO_NO_TOOL, cmd_params_read },
   { "WRITE_CORE_RAM",   command_write_ram,        "<address> <byte1> <byte2> ...", "Write bytes to core memory at an achievement address (needs a game with achievements).", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, cmd_params_write },
#endif
   { "READ_CORE_MEMORY", command_read_memory,      "<address> <number of bytes>", "Read bytes from core memory at a system address. Answers with the address and the bytes, in hexadecimal.", CMD_INFO_READ_ONLY, cmd_params_read, &cmd_rows_read },
   { "GET_MEMORY_MAP",   command_get_memory_map,   "No argument", "The core's memory map: start, length, select and disconnect in hexadecimal, as READ_CORE_MEMORY takes them. A core with none lists the RAM it hands over, in space core: system RAM from 0, save RAM after.", CMD_INFO_READ_ONLY, NULL, &cmd_rows_memory },
   { "WRITE_CORE_MEMORY",command_write_memory,     "<address> <byte1> <byte2> ...", "Write bytes to core memory at a system address. Answers with the address and how many bytes were written.", CMD_INFO_DESTRUCTIVE, cmd_params_write },

   { "LOAD_STATE_SLOT",command_load_state_slot, "<slot number>", "Load the save state in the given slot, replacing the current game state.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, cmd_params_slot },
   { "SAVE_STATE_SLOT",command_save_state_slot, "<slot number>", "Save a state to the given slot, overwriting what it held.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, cmd_params_slot },
   { "CHECKPOINT_STATE", command_checkpoint_save, "No argument", "Keep a state in memory, for ROLLBACK_STATE, until the content closes.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, NULL },
   { "ROLLBACK_STATE", command_checkpoint_rollback, "No argument", "Restore the state CHECKPOINT_STATE kept.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, NULL },
   { "STATE",         command_state,         "<save|load|checkpoint|rollback> [slot]", "Save a state to a slot, or load the one in it; or checkpoint one in memory and roll back to it.", CMD_INFO_DESTRUCTIVE, cmd_params_state },
   { "PLAY_REPLAY_SLOT",command_play_replay_slot, "<slot number>", "Play back the replay in the given slot.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, cmd_params_slot },
   { "SEEK_REPLAY",command_seek_replay, "<frame number>", "Seek the playing replay to the given frame.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, cmd_params_frame },
   { "REPLAY",        command_replay,        "<play|seek|record|halt|checkpoint> [slot|frame]", "Play, seek, record, halt or checkpoint a replay.", CMD_INFO_DESTRUCTIVE, cmd_params_replay },

   { "SAVE_FILES", command_save_savefiles, "No argument", "Write the core's save files (SRAM) to disk.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, NULL },
   { "LOAD_FILES", command_load_savefiles, "No argument", "Reload the core's save files (SRAM) from disk.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, NULL },

   { "LOAD_CORE", command_load_core, "<core path>", "Load the core library at the given path.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, cmd_params_core },
   { "START_CORE", command_start_core, "No argument", "Start the loaded core without content.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, NULL },
   { "INPUT", command_input, "<set|release|show> [player] [control] [value]", "Hold RetroPad buttons or stick axes over a player's own input until set again or released, or show what is held. The core gets exactly what is held: opposites are not cleaned nor the D-Pad turned; under netplay the player is this machine's. Answers player, buttons and axes, tab separated, none where nothing is held.", 0, cmd_params_input_family, &cmd_rows_input },
   { "OPTION", command_option, "<show|set> [option] [on|off]", "Show RetroArch's options - pause, fast-forward, the frame count, the slots and more - or turn one on or off.", 0, cmd_params_option_family, &cmd_rows_option },
   { "CORE_OPTION", command_core_option, "<show|set> [key] [value]", "Show the running core's options - key, value and description - or set one to one of its values.", 0, cmd_params_core_option_family, &cmd_rows_core_option },
   { "CORE", command_core, "<load|start|unload> [core path]", "Load a core library, start it without content, or unload it.", CMD_INFO_DESTRUCTIVE, cmd_params_core_family },
   { "LOAD_CONTENT", command_load_content, "<core path>|<content path>[|on]", "Load content with a core, or with one that supports it where core is DETECT or empty.", CMD_INFO_DESTRUCTIVE, cmd_params_content },
   { "RESET", command_reset, "[on|off]", "Reset the running content; with paused on, pause once it has.", CMD_INFO_DESTRUCTIVE, cmd_params_reset },
   { "CLOSE_CONTENT", command_close_content, "No argument", "Close the running content.", CMD_INFO_DESTRUCTIVE, NULL },
   { "UNLOAD_CORE", command_unload_core, "No argument", "Close the content and unload the core.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL, NULL },
   { "VIDEO_REINIT", command_video_reinit, "No argument", "Restart the video driver.", CMD_INFO_NO_TOOL, NULL },
   { "AUDIO_REINIT", command_audio_reinit, "No argument", "Restart the audio driver.", CMD_INFO_NO_TOOL, NULL },
   { "DRIVERS_REINIT", command_drivers_reinit, "No argument", "Restart all drivers.", CMD_INFO_NO_TOOL, NULL },
   { "REINIT",       command_reinit,       "<video|audio|all>", "Restart the video or audio driver, or all drivers.", 0, cmd_params_reinit },
   { "MENU",         command_menu,         "<action> [index] [from <entry>]", "RetroArch's own menu, not a game's. Answers with what it shows after the action: title, a page of entries, the selection. Ozone: left reaches the tabs, up and down pick one, right returns. XMB: left and right switch tabs.", CMD_INFO_DESTRUCTIVE, cmd_params_menu, &cmd_rows_menu },
   { "DISK",         command_disk,         "<eject|next|prev>", "Eject or insert the virtual disc, or select the next or previous disc image.", 0, cmd_params_disk },
};

static const struct cmd_map map[] = {
   { "MENU_TOGGLE", RARCH_MENU_TOGGLE, "Open or close the menu.", CMD_INFO_NO_TOOL },
   { "QUIT", RARCH_QUIT_KEY, "Quit RetroArch.", CMD_INFO_DESTRUCTIVE },
   /* the tool is the action RESET, which takes paused */
   { "RESET", RARCH_RESET, "Reset the running content.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL },

   { "FAST_FORWARD", RARCH_FAST_FORWARD_KEY, "Toggle fast-forward.", CMD_INFO_NO_TOOL },
   { "FAST_FORWARD_HOLD", RARCH_FAST_FORWARD_HOLD_KEY, "Fast-forward while held (one frame when sent as a command).", CMD_INFO_NO_TOOL },
   { "SLOWMOTION", RARCH_SLOWMOTION_KEY, "Toggle slow motion.", CMD_INFO_NO_TOOL },
   { "SLOWMOTION_HOLD", RARCH_SLOWMOTION_HOLD_KEY, "Slow motion while held (one frame when sent as a command).", CMD_INFO_NO_TOOL },
   { "REWIND", RARCH_REWIND, "Rewind while held (one frame when sent as a command).", CMD_INFO_NO_TOOL },
   { "PAUSE_TOGGLE", RARCH_PAUSE_TOGGLE, "Pause or resume the content.", CMD_INFO_NO_TOOL },
   { "FRAMEADVANCE", RARCH_FRAMEADVANCE, "Advance one frame while paused.", CMD_INFO_NO_TOOL },

   { "MUTE", RARCH_MUTE, "Mute or unmute audio.", CMD_INFO_NO_TOOL },
   { "VOLUME_UP", RARCH_VOLUME_UP, "Raise the volume.", CMD_INFO_NO_TOOL },
   { "VOLUME_DOWN", RARCH_VOLUME_DOWN, "Lower the volume.", CMD_INFO_NO_TOOL },

   { "LOAD_STATE", RARCH_LOAD_STATE_KEY, "Load the save state in the current slot.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL },
   { "SAVE_STATE", RARCH_SAVE_STATE_KEY, "Save a state to the current slot.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL },
   { "STATE_SLOT_PLUS", RARCH_STATE_SLOT_PLUS, "Select the next save state slot.", CMD_INFO_NO_TOOL },
   { "STATE_SLOT_MINUS", RARCH_STATE_SLOT_MINUS, "Select the previous save state slot.", CMD_INFO_NO_TOOL },

   { "PLAY_REPLAY", RARCH_PLAY_REPLAY_KEY, "Play back the replay in the current slot.", CMD_INFO_NO_TOOL },
   { "RECORD_REPLAY", RARCH_RECORD_REPLAY_KEY, "Start recording a replay to the current slot.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL },
   { "HALT_REPLAY", RARCH_HALT_REPLAY_KEY, "Stop playing or recording the replay.", CMD_INFO_NO_TOOL },
   { "SAVE_REPLAY_CHECKPOINT", RARCH_SAVE_REPLAY_CHECKPOINT_KEY, "Add a checkpoint to the replay being recorded.", CMD_INFO_NO_TOOL },
   { "PREV_REPLAY_CHECKPOINT", RARCH_PREV_REPLAY_CHECKPOINT_KEY, "Seek the replay back to the previous checkpoint.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL },
   { "NEXT_REPLAY_CHECKPOINT", RARCH_NEXT_REPLAY_CHECKPOINT_KEY, "Seek the replay forward to the next checkpoint.", CMD_INFO_DESTRUCTIVE | CMD_INFO_NO_TOOL },
   { "REPLAY_SLOT_PLUS", RARCH_REPLAY_SLOT_PLUS, "Select the next replay slot.", CMD_INFO_NO_TOOL },
   { "REPLAY_SLOT_MINUS", RARCH_REPLAY_SLOT_MINUS, "Select the previous replay slot.", CMD_INFO_NO_TOOL },

   { "DISK_EJECT_TOGGLE", RARCH_DISK_EJECT_TOGGLE, "Eject or insert the virtual disc.", CMD_INFO_NO_TOOL },
   { "DISK_NEXT", RARCH_DISK_NEXT, "Select the next disc image.", CMD_INFO_NO_TOOL },
   { "DISK_PREV", RARCH_DISK_PREV, "Select the previous disc image.", CMD_INFO_NO_TOOL },

   { "SHADER_TOGGLE", RARCH_SHADER_TOGGLE, "Turn the shader on or off.", CMD_INFO_NO_TOOL },
   { "SHADER_HOLD", RARCH_SHADER_HOLD, "Turn the shader off while held (one frame when sent as a command).", CMD_INFO_NO_TOOL },
   { "SHADER_NEXT", RARCH_SHADER_NEXT, "Load the next shader preset in its directory.", CMD_INFO_NO_TOOL },
   { "SHADER_PREV", RARCH_SHADER_PREV, "Load the previous shader preset in its directory.", CMD_INFO_NO_TOOL },

   { "CHEAT_TOGGLE", RARCH_CHEAT_TOGGLE, "Turn the selected cheat on or off.", CMD_INFO_NO_TOOL },
   { "CHEAT_INDEX_PLUS", RARCH_CHEAT_INDEX_PLUS, "Select the next cheat.", CMD_INFO_NO_TOOL },
   { "CHEAT_INDEX_MINUS", RARCH_CHEAT_INDEX_MINUS, "Select the previous cheat.", CMD_INFO_NO_TOOL },

   { "SCREENSHOT", RARCH_SCREENSHOT, "Take a screenshot.", 0 },
   { "RECORDING_TOGGLE", RARCH_RECORDING_TOGGLE, "Start or stop recording video.", CMD_INFO_NO_TOOL },
   { "STREAMING_TOGGLE", RARCH_STREAMING_TOGGLE, "Start or stop streaming.", CMD_INFO_NO_TOOL },

   { "TURBO_FIRE_TOGGLE", RARCH_TURBO_FIRE_TOGGLE, "Turn turbo fire on or off.", CMD_INFO_NO_TOOL },
   { "GRAB_MOUSE_TOGGLE", RARCH_GRAB_MOUSE_TOGGLE, "Grab or release the mouse.", CMD_INFO_NO_TOOL },
   { "GAME_FOCUS_TOGGLE", RARCH_GAME_FOCUS_TOGGLE, "Give the game all keyboard input, or hand it back to hotkeys.", CMD_INFO_NO_TOOL },
   { "FULLSCREEN_TOGGLE", RARCH_FULLSCREEN_TOGGLE_KEY, "Switch between fullscreen and windowed.", CMD_INFO_NO_TOOL },
   { "UI_COMPANION_TOGGLE", RARCH_UI_COMPANION_TOGGLE, "Show or hide the desktop companion window.", CMD_INFO_NO_TOOL },

   { "VRR_RUNLOOP_TOGGLE", RARCH_VRR_RUNLOOP_TOGGLE, "Turn sync to exact content framerate on or off.", CMD_INFO_NO_TOOL },
   { "RUNAHEAD_TOGGLE", RARCH_RUNAHEAD_TOGGLE, "Turn run-ahead on or off.", CMD_INFO_NO_TOOL },
   { "PREEMPT_TOGGLE", RARCH_PREEMPT_TOGGLE, "Turn preemptive frames on or off.", CMD_INFO_NO_TOOL },
   { "VIDEO_FILTER_TOGGLE", RARCH_VIDEO_FILTER_TOGGLE, "Turn the video filter on or off.", CMD_INFO_NO_TOOL },
   { "HEADSET_RECENTER", RARCH_HEADSET_RECENTER, "Recenter the headset's screens in front of where it looks.", CMD_INFO_NO_TOOL },
   { "LASER_POINTER_TOGGLE", RARCH_LASER_POINTER_TOGGLE, "Show or hide the headset's laser pointer.", CMD_INFO_NO_TOOL },
   { "HEADSET_VIEW_TOGGLE", RARCH_HEADSET_VIEW_TOGGLE, "Cycle the headset view between immersive, stereoscopic and flat.", CMD_INFO_NO_TOOL },
   { "HEADSET_EDIT_SCREENS", RARCH_HEADSET_EDIT_SCREENS, "Start or stop moving the headset's screens and menu with its controllers.", CMD_INFO_NO_TOOL },
   { "FPS_TOGGLE", RARCH_FPS_TOGGLE, "Show or hide the framerate.", CMD_INFO_NO_TOOL },
   { "STATISTICS_TOGGLE", RARCH_STATISTICS_TOGGLE, "Show or hide the technical statistics.", CMD_INFO_NO_TOOL },
   { "AI_SERVICE", RARCH_AI_SERVICE, "Run the AI service (translate or narrate the screen).", CMD_INFO_NO_TOOL },

   { "NETPLAY_PING_TOGGLE", RARCH_NETPLAY_PING_TOGGLE, "Show or hide the netplay ping.", CMD_INFO_NO_TOOL },
   { "NETPLAY_HOST_TOGGLE", RARCH_NETPLAY_HOST_TOGGLE, "Start or stop hosting netplay.", CMD_INFO_NO_TOOL },
   { "NETPLAY_GAME_WATCH", RARCH_NETPLAY_GAME_WATCH, "Switch netplay between playing and spectating.", CMD_INFO_NO_TOOL },
   { "NETPLAY_PLAYER_CHAT", RARCH_NETPLAY_PLAYER_CHAT, "Send a netplay chat message.", CMD_INFO_NO_TOOL },
   { "NETPLAY_FADE_CHAT_TOGGLE", RARCH_NETPLAY_FADE_CHAT_TOGGLE, "Turn fading of netplay chat on or off.", CMD_INFO_NO_TOOL },

   { "MENU_UP", RETRO_DEVICE_ID_JOYPAD_UP, "Menu: move up.", CMD_INFO_NO_TOOL },
   { "MENU_DOWN", RETRO_DEVICE_ID_JOYPAD_DOWN, "Menu: move down.", CMD_INFO_NO_TOOL },
   { "MENU_LEFT", RETRO_DEVICE_ID_JOYPAD_LEFT, "Menu: move left.", CMD_INFO_NO_TOOL },
   { "MENU_RIGHT", RETRO_DEVICE_ID_JOYPAD_RIGHT, "Menu: move right.", CMD_INFO_NO_TOOL },
   { "MENU_A", RETRO_DEVICE_ID_JOYPAD_A, "Menu: confirm (A).", CMD_INFO_NO_TOOL },
   { "MENU_B", RETRO_DEVICE_ID_JOYPAD_B, "Menu: back (B).", CMD_INFO_NO_TOOL },

   { "OVERLAY_NEXT", RARCH_OVERLAY_NEXT, "Switch to the next overlay.", CMD_INFO_NO_TOOL },
   { "OSK", RARCH_OSK, "Show or hide the on-screen keyboard.", CMD_INFO_NO_TOOL },

#if 0
   /* Deprecated */
   { "SEND_DEBUG_INFO", RARCH_SEND_DEBUG_INFO, "Deprecated.", 0 },
#endif
};
#endif

#ifdef HAVE_CONFIGFILE
/**
 * command_event_save_core_config:
 *
 * Saves a new (core) configuration to a file. Filename is based
 * on heuristics to avoid typing.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool command_event_save_core_config(
      const char *dir_menu_config,
      const char *rarch_path_config);

/**
 * command_event_save_current_config:
 *
 * Saves current configuration file to disk, and (optionally)
 * autosave state.
 **/
void command_event_save_current_config(enum override_type type);

/**
 * command_event_remove_current_config:
 *
 * Removes current configuration file from disk.
 **/
void command_event_remove_current_config(enum override_type type);
#endif

/**
 * command_event_disk_control_append_image:
 * @path                 : Path to disk image.
 *
 * Appends disk image to disk image list.
 **/
bool command_event_disk_control_append_image(const char *path);

/* Whether the drivers were started over: not while a load is under
 * way that starts them anyway. */
bool command_event_reinit(const int flags);

bool command_event_main_state(unsigned cmd);

RETRO_END_DECLS

#endif
