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
   /* Resizes windowed scale. Will reinitialize video driver. */
   CMD_EVENT_RESIZE_WINDOWED_SCALE,
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
   CMD_INFO_DESTRUCTIVE = (1 << 1)
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
/* A command with its own handler; @arg_desc names the argument, or
 * "No argument". */
struct cmd_action_map
{
   const char *str;
   bool (*action)(command_t* cmd, const char *arg);
   const char *arg_desc;
   const char *desc;
   unsigned flags;       /* enum cmd_info_flags */
};

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
bool command_load_core(command_t *cmd, const char* arg);
bool command_start_core(command_t *cmd, const char* arg);
bool command_load_content(command_t *cmd, const char* arg);
bool command_close_content(command_t *cmd, const char* arg);
bool command_unload_core(command_t *cmd, const char* arg);
bool command_video_reinit(command_t *cmd, const char* arg);
bool command_audio_reinit(command_t *cmd, const char* arg);
bool command_drivers_reinit(command_t *cmd, const char* arg);

static const struct cmd_action_map action_map[] = {
   { "HELP",             command_help,             "[command]", "List the commands, or describe one.", CMD_INFO_READ_ONLY },
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   { "SET_SHADER",       command_set_shader,       "<shader path>", "Load the shader preset at the given path.", 0 },
#endif
   { "VERSION",          command_version,          "No argument", "Report the RetroArch version.", CMD_INFO_READ_ONLY },
   { "GET_STATUS",       command_get_status,       "No argument", "Report whether content is playing or paused, with its system, name and CRC32, or that none is loaded.", CMD_INFO_READ_ONLY },
   { "GET_CONFIG_PARAM", command_get_config_param, "<param name>", "Report the value of a configuration setting.", CMD_INFO_READ_ONLY },
   { "LIST_CORES",       command_list_cores,       "No argument", "List the installed cores: name, a tab, and the path LOAD_CONTENT takes.", CMD_INFO_READ_ONLY },
   { "LIST_PLAYLISTS",   command_list_playlists,   "No argument", "List the playlists, as GET_PLAYLIST takes them.", CMD_INFO_READ_ONLY },
   { "GET_PLAYLIST",     command_get_playlist,     "<playlist> [first entry]", "List a playlist's entries: index, label, content path and core path, tab separated, 200 at a time; MORE <next> when there are more.", CMD_INFO_READ_ONLY },
   { "SHOW_MSG",         command_show_osd_msg,     "<message>", "Show a message on screen.", 0 },
#if defined(HAVE_CHEEVOS)
   /* These functions use achievement addresses and only work if a game with achievements is
    * loaded. READ_CORE_MEMORY and WRITE_CORE_MEMORY are preferred and use system addresses. */
   { "READ_CORE_RAM",    command_read_ram,         "<address> <number of bytes>", "Read bytes from core memory at an achievement address (needs a game with achievements).", CMD_INFO_READ_ONLY },
   { "WRITE_CORE_RAM",   command_write_ram,        "<address> <byte1> <byte2> ...", "Write bytes to core memory at an achievement address (needs a game with achievements).", CMD_INFO_DESTRUCTIVE },
#endif
   { "READ_CORE_MEMORY", command_read_memory,      "<address> <number of bytes>", "Read bytes from core memory at a system address.", CMD_INFO_READ_ONLY },
   { "WRITE_CORE_MEMORY",command_write_memory,     "<address> <byte1> <byte2> ...", "Write bytes to core memory at a system address.", CMD_INFO_DESTRUCTIVE },

   { "LOAD_STATE_SLOT",command_load_state_slot, "<slot number>", "Load the save state in the given slot, replacing the current game state.", CMD_INFO_DESTRUCTIVE },
   { "SAVE_STATE_SLOT",command_save_state_slot, "<slot number>", "Save a state to the given slot, overwriting what it held.", CMD_INFO_DESTRUCTIVE },
   { "PLAY_REPLAY_SLOT",command_play_replay_slot, "<slot number>", "Play back the replay in the given slot.", CMD_INFO_DESTRUCTIVE },
   { "SEEK_REPLAY",command_seek_replay, "<frame number>", "Seek the playing replay to the given frame.", CMD_INFO_DESTRUCTIVE },

   { "SAVE_FILES", command_save_savefiles, "No argument", "Write the core's save files (SRAM) to disk.", CMD_INFO_DESTRUCTIVE },
   { "LOAD_FILES", command_load_savefiles, "No argument", "Reload the core's save files (SRAM) from disk.", CMD_INFO_DESTRUCTIVE },

   { "LOAD_CORE", command_load_core, "<core path>", "Load the core library at the given path.", CMD_INFO_DESTRUCTIVE },
   { "START_CORE", command_start_core, "No argument", "Start the loaded core without content.", CMD_INFO_DESTRUCTIVE },
   { "LOAD_CONTENT", command_load_content, "<core path>|<content path>", "Load the content at the given path with the core at the given path.", CMD_INFO_DESTRUCTIVE },
   { "CLOSE_CONTENT", command_close_content, "No argument", "Close the running content.", CMD_INFO_DESTRUCTIVE },
   { "UNLOAD_CORE", command_unload_core, "No argument", "Close the content and unload the core.", CMD_INFO_DESTRUCTIVE },
   { "VIDEO_REINIT", command_video_reinit, "No argument", "Restart the video driver.", 0 },
   { "AUDIO_REINIT", command_audio_reinit, "No argument", "Restart the audio driver.", 0 },
   { "DRIVERS_REINIT", command_drivers_reinit, "No argument", "Restart all drivers.", 0 },
};

static const struct cmd_map map[] = {
   { "MENU_TOGGLE", RARCH_MENU_TOGGLE, "Open or close the menu.", 0 },
   { "QUIT", RARCH_QUIT_KEY, "Quit RetroArch.", CMD_INFO_DESTRUCTIVE },
   { "RESET", RARCH_RESET, "Reset the running content.", CMD_INFO_DESTRUCTIVE },

   { "FAST_FORWARD", RARCH_FAST_FORWARD_KEY, "Toggle fast-forward.", 0 },
   { "FAST_FORWARD_HOLD", RARCH_FAST_FORWARD_HOLD_KEY, "Fast-forward while held (one frame when sent as a command).", 0 },
   { "SLOWMOTION", RARCH_SLOWMOTION_KEY, "Toggle slow motion.", 0 },
   { "SLOWMOTION_HOLD", RARCH_SLOWMOTION_HOLD_KEY, "Slow motion while held (one frame when sent as a command).", 0 },
   { "REWIND", RARCH_REWIND, "Rewind while held (one frame when sent as a command).", 0 },
   { "PAUSE_TOGGLE", RARCH_PAUSE_TOGGLE, "Pause or resume the content.", 0 },
   { "FRAMEADVANCE", RARCH_FRAMEADVANCE, "Advance one frame while paused.", 0 },

   { "MUTE", RARCH_MUTE, "Mute or unmute audio.", 0 },
   { "VOLUME_UP", RARCH_VOLUME_UP, "Raise the volume.", 0 },
   { "VOLUME_DOWN", RARCH_VOLUME_DOWN, "Lower the volume.", 0 },

   { "LOAD_STATE", RARCH_LOAD_STATE_KEY, "Load the save state in the current slot.", CMD_INFO_DESTRUCTIVE },
   { "SAVE_STATE", RARCH_SAVE_STATE_KEY, "Save a state to the current slot.", CMD_INFO_DESTRUCTIVE },
   { "STATE_SLOT_PLUS", RARCH_STATE_SLOT_PLUS, "Select the next save state slot.", 0 },
   { "STATE_SLOT_MINUS", RARCH_STATE_SLOT_MINUS, "Select the previous save state slot.", 0 },

   { "PLAY_REPLAY", RARCH_PLAY_REPLAY_KEY, "Play back the replay in the current slot.", 0 },
   { "RECORD_REPLAY", RARCH_RECORD_REPLAY_KEY, "Start recording a replay to the current slot.", CMD_INFO_DESTRUCTIVE },
   { "HALT_REPLAY", RARCH_HALT_REPLAY_KEY, "Stop playing or recording the replay.", 0 },
   { "SAVE_REPLAY_CHECKPOINT", RARCH_SAVE_REPLAY_CHECKPOINT_KEY, "Add a checkpoint to the replay being recorded.", 0 },
   { "PREV_REPLAY_CHECKPOINT", RARCH_PREV_REPLAY_CHECKPOINT_KEY, "Seek the replay back to the previous checkpoint.", CMD_INFO_DESTRUCTIVE },
   { "NEXT_REPLAY_CHECKPOINT", RARCH_NEXT_REPLAY_CHECKPOINT_KEY, "Seek the replay forward to the next checkpoint.", CMD_INFO_DESTRUCTIVE },
   { "REPLAY_SLOT_PLUS", RARCH_REPLAY_SLOT_PLUS, "Select the next replay slot.", 0 },
   { "REPLAY_SLOT_MINUS", RARCH_REPLAY_SLOT_MINUS, "Select the previous replay slot.", 0 },

   { "DISK_EJECT_TOGGLE", RARCH_DISK_EJECT_TOGGLE, "Eject or insert the virtual disc.", 0 },
   { "DISK_NEXT", RARCH_DISK_NEXT, "Select the next disc image.", 0 },
   { "DISK_PREV", RARCH_DISK_PREV, "Select the previous disc image.", 0 },

   { "SHADER_TOGGLE", RARCH_SHADER_TOGGLE, "Turn the shader on or off.", 0 },
   { "SHADER_HOLD", RARCH_SHADER_HOLD, "Turn the shader off while held (one frame when sent as a command).", 0 },
   { "SHADER_NEXT", RARCH_SHADER_NEXT, "Load the next shader preset in its directory.", 0 },
   { "SHADER_PREV", RARCH_SHADER_PREV, "Load the previous shader preset in its directory.", 0 },

   { "CHEAT_TOGGLE", RARCH_CHEAT_TOGGLE, "Turn the selected cheat on or off.", 0 },
   { "CHEAT_INDEX_PLUS", RARCH_CHEAT_INDEX_PLUS, "Select the next cheat.", 0 },
   { "CHEAT_INDEX_MINUS", RARCH_CHEAT_INDEX_MINUS, "Select the previous cheat.", 0 },

   { "SCREENSHOT", RARCH_SCREENSHOT, "Take a screenshot.", 0 },
   { "RECORDING_TOGGLE", RARCH_RECORDING_TOGGLE, "Start or stop recording video.", 0 },
   { "STREAMING_TOGGLE", RARCH_STREAMING_TOGGLE, "Start or stop streaming.", 0 },

   { "TURBO_FIRE_TOGGLE", RARCH_TURBO_FIRE_TOGGLE, "Turn turbo fire on or off.", 0 },
   { "GRAB_MOUSE_TOGGLE", RARCH_GRAB_MOUSE_TOGGLE, "Grab or release the mouse.", 0 },
   { "GAME_FOCUS_TOGGLE", RARCH_GAME_FOCUS_TOGGLE, "Give the game all keyboard input, or hand it back to hotkeys.", 0 },
   { "FULLSCREEN_TOGGLE", RARCH_FULLSCREEN_TOGGLE_KEY, "Switch between fullscreen and windowed.", 0 },
   { "UI_COMPANION_TOGGLE", RARCH_UI_COMPANION_TOGGLE, "Show or hide the desktop companion window.", 0 },

   { "VRR_RUNLOOP_TOGGLE", RARCH_VRR_RUNLOOP_TOGGLE, "Turn sync to exact content framerate on or off.", 0 },
   { "RUNAHEAD_TOGGLE", RARCH_RUNAHEAD_TOGGLE, "Turn run-ahead on or off.", 0 },
   { "PREEMPT_TOGGLE", RARCH_PREEMPT_TOGGLE, "Turn preemptive frames on or off.", 0 },
   { "VIDEO_FILTER_TOGGLE", RARCH_VIDEO_FILTER_TOGGLE, "Turn the video filter on or off.", 0 },
   { "HEADSET_RECENTER", RARCH_HEADSET_RECENTER, "Recenter the headset's screens in front of where it looks.", 0 },
   { "LASER_POINTER_TOGGLE", RARCH_LASER_POINTER_TOGGLE, "Show or hide the headset's laser pointer.", 0 },
   { "FPS_TOGGLE", RARCH_FPS_TOGGLE, "Show or hide the framerate.", 0 },
   { "STATISTICS_TOGGLE", RARCH_STATISTICS_TOGGLE, "Show or hide the technical statistics.", 0 },
   { "AI_SERVICE", RARCH_AI_SERVICE, "Run the AI service (translate or narrate the screen).", 0 },

   { "NETPLAY_PING_TOGGLE", RARCH_NETPLAY_PING_TOGGLE, "Show or hide the netplay ping.", 0 },
   { "NETPLAY_HOST_TOGGLE", RARCH_NETPLAY_HOST_TOGGLE, "Start or stop hosting netplay.", 0 },
   { "NETPLAY_GAME_WATCH", RARCH_NETPLAY_GAME_WATCH, "Switch netplay between playing and spectating.", 0 },
   { "NETPLAY_PLAYER_CHAT", RARCH_NETPLAY_PLAYER_CHAT, "Send a netplay chat message.", 0 },
   { "NETPLAY_FADE_CHAT_TOGGLE", RARCH_NETPLAY_FADE_CHAT_TOGGLE, "Turn fading of netplay chat on or off.", 0 },

   { "MENU_UP", RETRO_DEVICE_ID_JOYPAD_UP, "Menu: move up.", 0 },
   { "MENU_DOWN", RETRO_DEVICE_ID_JOYPAD_DOWN, "Menu: move down.", 0 },
   { "MENU_LEFT", RETRO_DEVICE_ID_JOYPAD_LEFT, "Menu: move left.", 0 },
   { "MENU_RIGHT", RETRO_DEVICE_ID_JOYPAD_RIGHT, "Menu: move right.", 0 },
   { "MENU_A", RETRO_DEVICE_ID_JOYPAD_A, "Menu: confirm (A).", 0 },
   { "MENU_B", RETRO_DEVICE_ID_JOYPAD_B, "Menu: back (B).", 0 },

   { "OVERLAY_NEXT", RARCH_OVERLAY_NEXT, "Switch to the next overlay.", 0 },
   { "OSK", RARCH_OSK, "Show or hide the on-screen keyboard.", 0 },

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

void command_event_reinit(const int flags);

bool command_event_main_state(unsigned cmd);

RETRO_END_DECLS

#endif
