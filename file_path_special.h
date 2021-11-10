/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef _FILE_PATH_SPECIAL_H
#define _FILE_PATH_SPECIAL_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_environment.h>

RETRO_BEGIN_DECLS

#define FILE_PATH_UNKNOWN          "null"
#define FILE_PATH_CONTENT_BASENAME "content.png"
#define FILE_PATH_BUILTIN          "builtin"
#define FILE_PATH_DETECT           "DETECT"
#define FILE_PATH_LUTRO_PLAYLIST   "Lutro.lpl"
#define FILE_PATH_NUL              "nul"
#define FILE_PATH_CGP_EXTENSION ".cgp"
#define FILE_PATH_GLSLP_EXTENSION ".glslp"
#define FILE_PATH_SLANGP_EXTENSION ".slangp"
#define FILE_PATH_AUTO_EXTENSION ".auto"
#define FILE_PATH_BSV_EXTENSION ".bsv"
#define FILE_PATH_OPT_EXTENSION ".opt"
#define FILE_PATH_CORE_INFO_EXTENSION ".info"
#define FILE_PATH_CONFIG_EXTENSION ".cfg"
#define FILE_PATH_REMAP_EXTENSION ".rmp"
#define FILE_PATH_RTC_EXTENSION ".rtc"
#define FILE_PATH_CHT_EXTENSION ".cht"
#define FILE_PATH_SRM_EXTENSION ".srm"
#define FILE_PATH_STATE_EXTENSION ".state"
#define FILE_PATH_LPL_EXTENSION ".lpl"
#define FILE_PATH_LPL_EXTENSION_NO_DOT "lpl"
#define FILE_PATH_PNG_EXTENSION ".png"
#define FILE_PATH_MP3_EXTENSION ".mp3"
#define FILE_PATH_FLAC_EXTENSION ".flac"
#define FILE_PATH_OGG_EXTENSION ".ogg"
#define FILE_PATH_WAV_EXTENSION ".wav"
#define FILE_PATH_MOD_EXTENSION ".mod"
#define FILE_PATH_S3M_EXTENSION ".s3m"
#define FILE_PATH_XM_EXTENSION ".xm"
#define FILE_PATH_JPEG_EXTENSION ".jpeg"
#define FILE_PATH_BMP_EXTENSION ".bmp"
#define FILE_PATH_TGA_EXTENSION ".tga"
#define FILE_PATH_JPG_EXTENSION ".jpg"
#define FILE_PATH_UPS_EXTENSION ".ups"
#define FILE_PATH_IPS_EXTENSION ".ips"
#define FILE_PATH_BPS_EXTENSION ".bps"
#define FILE_PATH_RDB_EXTENSION ".rdb"
#define FILE_PATH_RDB_EXTENSION_NO_DOT "rdb"
#define FILE_PATH_ZIP_EXTENSION ".zip"
#define FILE_PATH_7Z_EXTENSION ".7z"
#define FILE_PATH_INDEX_URL ".index"
#define FILE_PATH_INDEX_DIRS_URL ".index-dirs"
#define FILE_PATH_INDEX_EXTENDED_URL ".index-extended"
#define FILE_PATH_NETPLAY_ROOM_LIST_URL "registry.lpl"
#define FILE_PATH_RETROACHIEVEMENTS_URL "http://i.retroachievements.org"
#define FILE_PATH_LOBBY_LIBRETRO_URL "http://lobby.libretro.com/"
#define FILE_PATH_CORE_THUMBNAILS_URL "http://thumbnails.libretro.com"
#define FILE_PATH_CORE_THUMBNAILPACKS_URL "http://thumbnailpacks.libretro.com"
#ifdef HAVE_LAKKA_NIGHTLY
#define FILE_PATH_LAKKA_URL "http://nightly.builds.lakka.tv/.updater"
#else
#define FILE_PATH_LAKKA_URL "http://le.builds.lakka.tv"
#endif
#define FILE_PATH_SHADERS_GLSL_ZIP "shaders_glsl.zip"
#define FILE_PATH_SHADERS_SLANG_ZIP "shaders_slang.zip"
#define FILE_PATH_SHADERS_CG_ZIP "shaders_cg.zip"
#define FILE_PATH_DATABASE_RDB_ZIP "database-rdb.zip"
#define FILE_PATH_OVERLAYS_ZIP "overlays.zip"
#define FILE_PATH_CORE_INFO_ZIP "info.zip"
#define FILE_PATH_CHEATS_ZIP "cheats.zip"
#define FILE_PATH_ASSETS_ZIP "assets.zip"
#define FILE_PATH_AUTOCONFIG_ZIP "autoconfig.zip"
#define FILE_PATH_CONTENT_FAVORITES "content_favorites.lpl"
#define FILE_PATH_CONTENT_HISTORY "content_history.lpl"
#define FILE_PATH_CONTENT_IMAGE_HISTORY "content_image_history.lpl"
#define FILE_PATH_CONTENT_MUSIC_HISTORY "content_music_history.lpl"
#define FILE_PATH_CONTENT_VIDEO_HISTORY "content_video_history.lpl"
#define FILE_PATH_CORE_OPTIONS_CONFIG "retroarch-core-options.cfg"
#define FILE_PATH_MAIN_CONFIG "retroarch.cfg"
#define FILE_PATH_SALAMANDER_CONFIG "retroarch-salamander.cfg"
#define FILE_PATH_BACKGROUND_IMAGE "bg.png"
#define FILE_PATH_TTF_FONT "font.ttf"
#define FILE_PATH_RUNTIME_EXTENSION ".lrtl"
#define FILE_PATH_DEFAULT_EVENT_LOG "retroarch.log"
#define FILE_PATH_EVENT_LOG_EXTENSION ".log"
#define FILE_PATH_DISK_CONTROL_INDEX_EXTENSION ".ldci"
#define FILE_PATH_CORE_BACKUP_EXTENSION ".lcbk"
#define FILE_PATH_CORE_BACKUP_EXTENSION_NO_DOT "lcbk"
#define FILE_PATH_LOCK_EXTENSION ".lck"
#define FILE_PATH_LOCK_EXTENSION_NO_DOT "lck"
#define FILE_PATH_BACKUP_EXTENSION ".bak"
#if defined(RARCH_MOBILE)
#define FILE_PATH_DEFAULT_OVERLAY "gamepads/neo-retropad/neo-retropad.cfg"
#endif
#define FILE_PATH_CORE_INFO_CACHE "core_info.cache"
#define FILE_PATH_CORE_INFO_CACHE_REFRESH "core_info.refresh"

enum application_special_type
{
   APPLICATION_SPECIAL_NONE = 0,
   APPLICATION_SPECIAL_DIRECTORY_AUTOCONFIG,
   APPLICATION_SPECIAL_DIRECTORY_CONFIG,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG_AR_FONT,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_PKG_CJK_FONT,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_ICONS,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE_ICONS,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_RGUI_FONT,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_SOUNDS,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_SYSICONS,
   APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES,
   APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS
};

bool fill_pathname_application_data(char *s, size_t len);

void fill_pathname_application_special(char *s, size_t len, enum application_special_type type);

RETRO_END_DECLS

#endif
