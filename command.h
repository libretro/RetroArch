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

#include "retroarch_types.h"
#include "input/input_defines.h"

#include "configuration.h"

RETRO_BEGIN_DECLS

#define MAX_CMD_DRIVERS              3
#define DEFAULT_NETWORK_CMD_PORT 55355

struct cmd_map
{
   const char *str;
   unsigned id;
};

struct command_handler;

typedef void (*command_poller_t)(struct command_handler *cmd);
typedef void (*command_replier_t)(struct command_handler *cmd, const char * data, size_t len);
typedef void (*command_destructor_t)(struct command_handler *cmd);

struct command_handler
{
   /* Interface to poll the driver */
   command_poller_t poll;
   /* Interface to reply */
   command_replier_t replier;
   /* Interface to delete the underlying command */
   command_destructor_t destroy;
   /* Underlying command storage */
   void *userptr;
   /* State received */
   bool state[RARCH_BIND_LIST_END];
};

typedef struct command_handler command_t;

enum event_command
{
   CMD_EVENT_NONE = 0,
   /* Resets RetroArch. */
   CMD_EVENT_RESET,
   CMD_EVENT_SET_PER_GAME_RESOLUTION,
   CMD_EVENT_SET_FRAME_LIMIT,
   CMD_EVENT_VRR_RUNLOOP_TOGGLE,
   /* Loads core. */
   CMD_EVENT_LOAD_CORE,
   CMD_EVENT_LOAD_CORE_PERSIST,
#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   CMD_EVENT_LOAD_SECOND_CORE,
#endif
   CMD_EVENT_UNLOAD_CORE,
   CMD_EVENT_CLOSE_CONTENT,
   CMD_EVENT_LOAD_STATE,
   CMD_EVENT_LOAD_STATE_FROM_RAM,
   /* Swaps the current state with what's on the undo load buffer */
   CMD_EVENT_UNDO_LOAD_STATE,
   /* Rewrites a savestate on disk */
   CMD_EVENT_UNDO_SAVE_STATE,
   CMD_EVENT_SAVE_STATE,
   CMD_EVENT_SAVE_STATE_TO_RAM,
   CMD_EVENT_SAVE_STATE_DECREMENT,
   CMD_EVENT_SAVE_STATE_INCREMENT,
   CMD_EVENT_RAM_STATE_TO_FILE,
   /* Takes screenshot. */
   CMD_EVENT_TAKE_SCREENSHOT,
   /* Quits RetroArch. */
   CMD_EVENT_QUIT,
   /* Reinitialize all drivers. */
   CMD_EVENT_REINIT_FROM_TOGGLE,
   /* Reinitialize all drivers. */
   CMD_EVENT_REINIT,
   /* Toggles cheevos hardcore mode. */
   CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE,
   /* Deinitialize rewind. */
   CMD_EVENT_REWIND_DEINIT,
   /* Initializes rewind. */
   CMD_EVENT_REWIND_INIT,
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
   /* Toggles FPS counter. */
   CMD_EVENT_FPS_TOGGLE,
   /* Toggles statistics display. */
   CMD_EVENT_STATISTICS_TOGGLE,
   /* Toggle ping counter. */
   CMD_EVENT_NETPLAY_PING_TOGGLE,
   /* Gathers diagnostic info about the system and RetroArch configuration, then sends it to our servers. */
   CMD_EVENT_SEND_DEBUG_INFO,
   /* Toggles netplay hosting. */
   CMD_EVENT_NETPLAY_HOST_TOGGLE,
   /* Initializes overlay. */
   CMD_EVENT_OVERLAY_INIT,
   /* Deinitializes overlay. */
   CMD_EVENT_OVERLAY_DEINIT,
   /* Sets current scale factor for overlay. */
   CMD_EVENT_OVERLAY_SET_SCALE_FACTOR,
   /* Sets current alpha modulation for overlay. */
   CMD_EVENT_OVERLAY_SET_ALPHA_MOD,
   /* Cycle to next overlay. */
   CMD_EVENT_OVERLAY_NEXT,
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
   CMD_EVENT_UNPAUSE,
   /* Unpauses retroArch. */
   CMD_EVENT_PAUSE,
   CMD_EVENT_MENU_RESET_TO_DEFAULT_CONFIG,
   CMD_EVENT_MENU_SAVE_CURRENT_CONFIG,
   CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   CMD_EVENT_MENU_SAVE_CONFIG,
   CMD_EVENT_MENU_PAUSE_LIBRETRO,
   /* Toggles menu on/off. */
   CMD_EVENT_MENU_TOGGLE,
   /* Applies shader changes. */
   CMD_EVENT_SHADERS_APPLY_CHANGES,
   /* A new shader preset has been loaded */
   CMD_EVENT_SHADER_PRESET_LOADED,
   /* Apply cheats. */
   CMD_EVENT_CHEATS_APPLY,
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
   /* Toggles mouse grab. */
   CMD_EVENT_GRAB_MOUSE_TOGGLE,
   /* Toggles game focus. */
   CMD_EVENT_GAME_FOCUS_TOGGLE,
   /* Toggles desktop menu. */
   CMD_EVENT_UI_COMPANION_TOGGLE,
   /* Toggles fullscreen mode. */
   CMD_EVENT_FULLSCREEN_TOGGLE,
   CMD_EVENT_VOLUME_UP,
   CMD_EVENT_VOLUME_DOWN,
   CMD_EVENT_MIXER_VOLUME_UP,
   CMD_EVENT_MIXER_VOLUME_DOWN,
   CMD_EVENT_DISCORD_INIT,
   CMD_EVENT_PRESENCE_UPDATE,
   CMD_EVENT_OSK_TOGGLE,
   CMD_EVENT_RECORDING_TOGGLE,
   CMD_EVENT_STREAMING_TOGGLE,
   CMD_EVENT_RUNAHEAD_TOGGLE,
   CMD_EVENT_AI_SERVICE_TOGGLE,
   CMD_EVENT_BSV_RECORDING_TOGGLE,
   CMD_EVENT_SHADER_NEXT,
   CMD_EVENT_SHADER_PREV,
   CMD_EVENT_CHEAT_INDEX_PLUS,
   CMD_EVENT_CHEAT_INDEX_MINUS,
   CMD_EVENT_CHEAT_TOGGLE,
   CMD_EVENT_AI_SERVICE_CALL,
   CMD_EVENT_SAVE_FILES,
   CMD_EVENT_CONTROLLER_INIT
};

typedef struct command_handle
{
   command_t *handle;
   unsigned id;
} command_handle_t;

enum cmd_source_t
{
   CMD_NONE = 0,
   CMD_STDIN,
   CMD_NETWORK
};

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
command_t* command_network_new(uint16_t port);
command_t* command_stdin_new(void);
command_t* command_uds_new(void);

bool command_network_send(const char *cmd_);

#ifdef HAVE_BSV_MOVIE
struct bsv_state
{
   /* Movie playback/recording support. */
   char movie_path[PATH_MAX_LENGTH];
   /* Immediate playback/recording. */
   char movie_start_path[PATH_MAX_LENGTH];

   bool movie_start_recording;
   bool movie_start_playback;
   bool movie_playback;
   bool eof_exit;
   bool movie_end;

};

struct bsv_movie
{
   intfstream_t *file;
   uint8_t *state;
   /* A ring buffer keeping track of positions
    * in the file for each frame. */
   size_t *frame_pos;
   size_t frame_mask;
   size_t frame_ptr;
   size_t min_file_pos;
   size_t state_size;

   bool playback;
   bool first_rewind;
   bool did_rewind;
};

typedef struct bsv_movie bsv_movie_t;
#endif

#ifdef HAVE_CONFIGFILE
bool command_event_save_config(
      const char *config_path,
      char *s, size_t len);
#endif

void command_event_undo_save_state(char *s, size_t len);

void command_event_undo_load_state(char *s, size_t len);

void command_event_set_mixer_volume(
      settings_t *settings,
      float gain);

bool command_event_resize_windowed_scale(settings_t *settings,
      unsigned window_scale);

bool command_event_save_auto_state(
      bool savestate_auto_save,
      const enum rarch_core_type current_core_type);

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

bool command_event_load_entry_state(void);

void command_event_load_auto_state(void);

void command_event_set_savestate_auto_index(
      settings_t *settings);

void command_event_set_savestate_garbage_collect(
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
struct cmd_action_map
{
   const char *str;
   bool (*action)(command_t* cmd, const char *arg);
   const char *arg_desc;
};

bool command_version(command_t *cmd, const char* arg);
bool command_get_status(command_t *cmd, const char* arg);
bool command_get_config_param(command_t *cmd, const char* arg);
bool command_show_osd_msg(command_t *cmd, const char* arg);
#ifdef HAVE_CHEEVOS
bool command_read_ram(command_t *cmd, const char *arg);
bool command_write_ram(command_t *cmd, const char *arg);
#endif
bool command_read_memory(command_t *cmd, const char *arg);
bool command_write_memory(command_t *cmd, const char *arg);
uint8_t *command_memory_get_pointer(
      const rarch_system_info_t* system,
      unsigned address,
      unsigned int* max_bytes,
      int for_write,
      char *reply_at,
      size_t len);

static const struct cmd_action_map action_map[] = {
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   { "SET_SHADER",       command_set_shader,       "<shader path>" },
#endif
   { "VERSION",          command_version,          "No argument"},
   { "GET_STATUS",       command_get_status,       "No argument" },
   { "GET_CONFIG_PARAM", command_get_config_param, "<param name>" },
   { "SHOW_MSG",         command_show_osd_msg,     "No argument" },
#if defined(HAVE_CHEEVOS)
   /* These functions use achievement addresses and only work if a game with achievements is
    * loaded. READ_CORE_MEMORY and WRITE_CORE_MEMORY are preferred and use system addresses. */
   { "READ_CORE_RAM",    command_read_ram,         "<address> <number of bytes>" },
   { "WRITE_CORE_RAM",   command_write_ram,        "<address> <byte1> <byte2> ..." },
#endif
   { "READ_CORE_MEMORY", command_read_memory,      "<address> <number of bytes>" },
   { "WRITE_CORE_MEMORY",command_write_memory,     "<address> <byte1> <byte2> ..." },
};

static const struct cmd_map map[] = {
   { "FAST_FORWARD",           RARCH_FAST_FORWARD_KEY },
   { "FAST_FORWARD_HOLD",      RARCH_FAST_FORWARD_HOLD_KEY },
   { "SLOWMOTION",             RARCH_SLOWMOTION_KEY },
   { "SLOWMOTION_HOLD",        RARCH_SLOWMOTION_HOLD_KEY },
   { "VRR_RUNLOOP_TOGGLE",     RARCH_VRR_RUNLOOP_TOGGLE },
   { "LOAD_STATE",             RARCH_LOAD_STATE_KEY },
   { "SAVE_STATE",             RARCH_SAVE_STATE_KEY },
   { "FULLSCREEN_TOGGLE",      RARCH_FULLSCREEN_TOGGLE_KEY },
   { "CLOSE_CONTENT",          RARCH_CLOSE_CONTENT_KEY },
   { "QUIT",                   RARCH_QUIT_KEY },
   { "STATE_SLOT_PLUS",        RARCH_STATE_SLOT_PLUS },
   { "STATE_SLOT_MINUS",       RARCH_STATE_SLOT_MINUS },
   { "REWIND",                 RARCH_REWIND },
   { "BSV_RECORD_TOGGLE",      RARCH_BSV_RECORD_TOGGLE },
   { "PAUSE_TOGGLE",           RARCH_PAUSE_TOGGLE },
   { "FRAMEADVANCE",           RARCH_FRAMEADVANCE },
   { "RESET",                  RARCH_RESET },
   { "SHADER_NEXT",            RARCH_SHADER_NEXT },
   { "SHADER_PREV",            RARCH_SHADER_PREV },
   { "CHEAT_INDEX_PLUS",       RARCH_CHEAT_INDEX_PLUS },
   { "CHEAT_INDEX_MINUS",      RARCH_CHEAT_INDEX_MINUS },
   { "CHEAT_TOGGLE",           RARCH_CHEAT_TOGGLE },
   { "SCREENSHOT",             RARCH_SCREENSHOT },
   { "MUTE",                   RARCH_MUTE },
   { "OSK",                    RARCH_OSK },
   { "FPS_TOGGLE",             RARCH_FPS_TOGGLE },
   { "STATISTICS_TOGGLE",      RARCH_STATISTICS_TOGGLE },
   { "SEND_DEBUG_INFO",        RARCH_SEND_DEBUG_INFO },
   { "NETPLAY_HOST_TOGGLE",    RARCH_NETPLAY_HOST_TOGGLE },
   { "NETPLAY_GAME_WATCH",     RARCH_NETPLAY_GAME_WATCH },
   { "VOLUME_UP",              RARCH_VOLUME_UP },
   { "VOLUME_DOWN",            RARCH_VOLUME_DOWN },
   { "OVERLAY_NEXT",           RARCH_OVERLAY_NEXT },
   { "DISK_EJECT_TOGGLE",      RARCH_DISK_EJECT_TOGGLE },
   { "DISK_NEXT",              RARCH_DISK_NEXT },
   { "DISK_PREV",              RARCH_DISK_PREV },
   { "GRAB_MOUSE_TOGGLE",      RARCH_GRAB_MOUSE_TOGGLE },
   { "UI_COMPANION_TOGGLE",    RARCH_UI_COMPANION_TOGGLE },
   { "GAME_FOCUS_TOGGLE",      RARCH_GAME_FOCUS_TOGGLE },
   { "MENU_TOGGLE",            RARCH_MENU_TOGGLE },
   { "RECORDING_TOGGLE",       RARCH_RECORDING_TOGGLE },
   { "STREAMING_TOGGLE",       RARCH_STREAMING_TOGGLE },
   { "RUNAHEAD_TOGGLE",        RARCH_RUNAHEAD_TOGGLE },
   { "MENU_UP",                RETRO_DEVICE_ID_JOYPAD_UP },
   { "MENU_DOWN",              RETRO_DEVICE_ID_JOYPAD_DOWN },
   { "MENU_LEFT",              RETRO_DEVICE_ID_JOYPAD_LEFT },
   { "MENU_RIGHT",             RETRO_DEVICE_ID_JOYPAD_RIGHT },
   { "MENU_A",                 RETRO_DEVICE_ID_JOYPAD_A },
   { "MENU_B",                 RETRO_DEVICE_ID_JOYPAD_B },
   { "AI_SERVICE",             RARCH_AI_SERVICE },
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
