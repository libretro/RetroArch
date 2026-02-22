/*  RetroArch - A frontend for libretro.
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

#ifndef MCP_ADAPTER_TOOLS_H__
#define MCP_ADAPTER_TOOLS_H__

#include "mcp_adapter_utils.h"

#include <file/file_path.h>

/*
 * Tool definitions.
 *
 * Each tool has two parts:
 *   1. A JSON fragment describing the tool
 *   2. The handler function implementing the tool
 *
 * To add a new tool, add both parts here and then register it
 * in the mcp_tools[] array in mcp_adapter_tool_list.c.
 */

/* ================================================================
 * get_content_info
 * ================================================================ */

static const char tool_get_content_info_json[] =
   "{"
      "\"name\":\"get_content_info\","
      "\"title\":\"Get Content Info\","
      "\"description\":\"Get information about the currently "
         "running content in RetroArch (game name, core, system, "
         "path, CRC32). Returns content status and details "
         "if content is loaded.\","
      "\"inputSchema\":{"
         "\"type\":\"object\","
         "\"properties\":{},"
         "\"additionalProperties\":false"
      "}"
   "}";

static void mcp_tool_get_content_info(const char *args_json, size_t args_len,
      char *buf, size_t buf_size)
{
   char result[4096] = "{";
   uint8_t flags = content_get_flags();

   if (flags & CONTENT_ST_FLAG_IS_INITED)
   {
      core_info_t *core_info      = NULL;
      runloop_state_t *runloop_st = runloop_state_get_ptr();
      const char *content_path    = path_get(RARCH_PATH_CONTENT);
      const char *basename_path   = path_get(RARCH_PATH_BASENAME);
      const char *core_path       = path_get(RARCH_PATH_CORE);
      uint32_t crc                = content_get_crc();

      core_info_get_current_core(&core_info);

      mcp_json_add(result, sizeof(result), "status",
            (runloop_st && (runloop_st->flags & RUNLOOP_FLAG_PAUSED))
            ? "paused" : "playing");

      if (core_info)
      {
         if (core_info->core_name)
            mcp_json_add(result, sizeof(result), "core_name", core_info->core_name);
         if (core_info->systemname)
            mcp_json_add(result, sizeof(result), "system_name", core_info->systemname);
         if (core_info->system_id)
            mcp_json_add(result, sizeof(result), "system_id", core_info->system_id);
         if (core_info->display_name)
            mcp_json_add(result, sizeof(result), "core_display_name", core_info->display_name);
      }
      else if (runloop_st && runloop_st->system.info.library_name)
      {
         mcp_json_add(result, sizeof(result), "core_name", runloop_st->system.info.library_name);
      }

      if (content_path)
         mcp_json_add(result, sizeof(result), "content_path", content_path);

      if (basename_path)
      {
         const char *basename = path_basename(basename_path);
         if (basename)
            mcp_json_add(result, sizeof(result), "content_name", basename);
      }

      if (core_path)
         mcp_json_add(result, sizeof(result), "core_path", core_path);

      if (crc != 0)
      {
         char crc_str[16];
         snprintf(crc_str, sizeof(crc_str), "%08lx", (unsigned long)crc);
         mcp_json_add(result, sizeof(result), "crc32", crc_str);
      }
   }
   else
      mcp_json_add(result, sizeof(result), "status", "no_content");

   strlcat(result, "}", sizeof(result));

   strlcpy(buf, result, buf_size);
}

/* ================================================================
 * get_status
 * ================================================================ */

static const char tool_get_status_json[] =
   "{"
      "\"name\":\"get_status\","
      "\"title\":\"Get Status\","
      "\"description\":\"Get the current RetroArch status including "
         "version, content state (playing, paused, or no content), "
         "and frame count.\","
      "\"inputSchema\":{"
         "\"type\":\"object\","
         "\"properties\":{},"
         "\"additionalProperties\":false"
      "}"
   "}";

static void mcp_tool_get_status(const char *args_json, size_t args_len,
      char *buf, size_t buf_size)
{
   char result[2048] = "{";
   uint8_t flags     = content_get_flags();
   video_driver_state_t *video_st = video_state_get_ptr();

   mcp_json_add(result, sizeof(result), "retroarch_version", PACKAGE_VERSION);

   if (flags & CONTENT_ST_FLAG_IS_INITED)
   {
      runloop_state_t *runloop_st = runloop_state_get_ptr();

      if (runloop_st && (runloop_st->flags & RUNLOOP_FLAG_PAUSED))
         mcp_json_add(result, sizeof(result), "status", "paused");
      else if (runloop_st && (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION))
         mcp_json_add(result, sizeof(result), "status", "fast_forward");
      else if (runloop_st && (runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION))
         mcp_json_add(result, sizeof(result), "status", "slow_motion");
      else
         mcp_json_add(result, sizeof(result), "status", "playing");

      if (video_st)
         mcp_json_add_int(result, sizeof(result), "frame_count", (int64_t)video_st->frame_count);
   }
   else
      mcp_json_add(result, sizeof(result), "status", "no_content");

   strlcat(result, "}", sizeof(result));
   strlcpy(buf, result, buf_size);
}

/* ================================================================
 * pause_resume
 * ================================================================ */

static const char tool_pause_resume_json[] =
   "{"
      "\"name\":\"pause_resume\","
      "\"title\":\"Pause or Resume\","
      "\"description\":\"Pause, resume, or toggle the currently "
         "running content in RetroArch.\","
      "\"inputSchema\":{"
         "\"type\":\"object\","
         "\"properties\":{"
            "\"action\":{\"type\":\"string\","
               "\"enum\":[\"pause\",\"resume\"],"
               "\"description\":\"The action to perform: pause or "
                  "resume the current pause state.\"}"
         "},"
         "\"required\":[\"action\"],"
         "\"additionalProperties\":false"
      "}"
   "}";

static void mcp_tool_pause_resume(const char *args_json, size_t args_len,
      char *buf, size_t buf_size)
{
   char action[32] = "";
   uint8_t flags   = content_get_flags();

   if (!(flags & CONTENT_ST_FLAG_IS_INITED))
   {
      strlcpy(buf, "{\"error\":\"no content is currently running\"}", buf_size);
      return;
   }

   if (  !args_json
       || !mcp_json_extract_string(args_json, args_len,
            "action", action, sizeof(action))
       || action[0] == '\0')
   {
      strlcpy(buf, "{\"error\":\"missing required parameter: action\"}", buf_size);
      return;
   }

   if (string_is_equal(action, "pause"))
      command_event(CMD_EVENT_PAUSE, NULL);
   else if (string_is_equal(action, "resume"))
      command_event(CMD_EVENT_UNPAUSE, NULL);
   else
   {
      strlcpy(buf, "{\"error\":\"invalid action, use: pause or resume\"}", buf_size);
      return;
   }

   {
      char result[256] = "{";
      runloop_state_t *runloop_st = runloop_state_get_ptr();
      mcp_json_add(result, sizeof(result), "result", "ok");
      mcp_json_add(result, sizeof(result), "status",
            (runloop_st && (runloop_st->flags & RUNLOOP_FLAG_PAUSED))
            ? "paused" : "playing");
      strlcat(result, "}", sizeof(result));
      strlcpy(buf, result, buf_size);
   }
}

/* ================================================================
 * reset
 * ================================================================ */

static const char tool_reset_json[] =
   "{"
      "\"name\":\"reset\","
      "\"title\":\"Reset\","
      "\"description\":\"Reset the currently running content "
         "(equivalent to a soft reset of the emulated system).\","
      "\"inputSchema\":{"
         "\"type\":\"object\","
         "\"properties\":{},"
         "\"additionalProperties\":false"
      "}"
   "}";

static void mcp_tool_reset(const char *args_json, size_t args_len,
      char *buf, size_t buf_size)
{
   uint8_t flags = content_get_flags();

   if (!(flags & CONTENT_ST_FLAG_IS_INITED))
   {
      strlcpy(buf, "{\"error\":\"no content is currently running\"}", buf_size);
      return;
   }

   command_event(CMD_EVENT_RESET, NULL);

   strlcpy(buf, "{\"result\":\"ok\"}", buf_size);
}

/* ================================================================
 * save_state
 * ================================================================ */

static const char tool_save_state_json[] =
   "{"
      "\"name\":\"save_state\","
      "\"title\":\"Save State\","
      "\"description\":\"Save the current emulation state to "
         "a slot. If no slot is specified, the currently selected "
         "slot is used.\","
      "\"inputSchema\":{"
         "\"type\":\"object\","
         "\"properties\":{"
            "\"slot\":{\"type\":\"integer\","
               "\"description\":\"The save state slot number "
                  "(0-999). If omitted, the current slot is used.\"}"
         "},"
         "\"additionalProperties\":false"
      "}"
   "}";

static void mcp_tool_save_state(const char *args_json, size_t args_len,
      char *buf, size_t buf_size)
{
   char result[512]     = "{";
   settings_t *settings = config_get_ptr();
   uint8_t flags        = content_get_flags();
   int slot;

   if (!(flags & CONTENT_ST_FLAG_IS_INITED))
   {
      strlcpy(buf, "{\"error\":\"no content is currently running\"}", buf_size);
      return;
   }

   if (!core_info_current_supports_savestate())
   {
      strlcpy(buf, "{\"error\":\"current core does not support save states\"}", buf_size);
      return;
   }

   if (args_json && mcp_json_extract_int(args_json, args_len, "slot", &slot))
      configuration_set_int(settings, settings->ints.state_slot, slot);

   command_event(CMD_EVENT_SAVE_STATE, NULL);

   mcp_json_add(result, sizeof(result), "result", "ok");
   mcp_json_add_int(result, sizeof(result), "slot", settings->ints.state_slot);
   strlcat(result, "}", sizeof(result));
   strlcpy(buf, result, buf_size);
}

/* ================================================================
 * load_state
 * ================================================================ */

static const char tool_load_state_json[] =
   "{"
      "\"name\":\"load_state\","
      "\"title\":\"Load State\","
      "\"description\":\"Load an emulation state from a slot. "
         "If no slot is specified, the currently selected slot "
         "is used.\","
      "\"inputSchema\":{"
         "\"type\":\"object\","
         "\"properties\":{"
            "\"slot\":{\"type\":\"integer\","
               "\"description\":\"The save state slot number "
                  "(0-999). If omitted, the current slot is used.\"}"
         "},"
         "\"additionalProperties\":false"
      "}"
   "}";

static void mcp_tool_load_state(
      const char *args_json, size_t args_len,
      char *buf, size_t buf_size)
{
   char result[512]     = "{";
   settings_t *settings = config_get_ptr();
   uint8_t flags        = content_get_flags();
   int slot;

   if (!(flags & CONTENT_ST_FLAG_IS_INITED))
   {
      strlcpy(buf, "{\"error\":\"no content is currently running\"}", buf_size);
      return;
   }

   if (!core_info_current_supports_savestate())
   {
      strlcpy(buf, "{\"error\":\"current core does not support save states\"}", buf_size);
      return;
   }

   if (args_json && mcp_json_extract_int(args_json, args_len, "slot", &slot))
      configuration_set_int(settings, settings->ints.state_slot, slot);

   {
      char state_path[PATH_MAX_LENGTH];
      if (runloop_get_savestate_path(state_path, sizeof(state_path), settings->ints.state_slot))
      {
         if (!path_is_valid(state_path))
         {
            char err[512];
            snprintf(err, sizeof(err),
                  "{\"error\":\"no save state file found in slot %d\"}",
                  settings->ints.state_slot);
            strlcpy(buf, err, buf_size);
            return;
         }
      }
   }

   command_event(CMD_EVENT_LOAD_STATE, NULL);

   mcp_json_add(result, sizeof(result), "result", "ok");
   mcp_json_add_int(result, sizeof(result), "slot", settings->ints.state_slot);
   strlcat(result, "}", sizeof(result));
   strlcpy(buf, result, buf_size);
}

#endif /* MCP_ADAPTER_TOOLS_H__ */
