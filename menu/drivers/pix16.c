/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Evan Manning
 *
 *  PIX16: a low-resolution, 16-bit console styled menu driver.
 *
 *  Draws into a native 320x240 (4:3) or 480x270 (16:9) software
 *  framebuffer with an 8x8 bitmap font, so it is pixel-exact on 15 kHz
 *  CRTs driven by CRT Emudriver / CRT SwitchRes. Boots straight into
 *  the playlist ("systems") list.
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

#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string/stdstring.h>
#include <lists/file_list.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_inline.h>
#include <formats/image.h>
#include <streams/image_texture_file.h>
#include <file/config_file.h>
#include <lists/dir_list.h>
#include <streams/file_stream.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"
#include "../menu_input.h"
#include "../../gfx/gfx_animation.h"
#include "../../gfx/gfx_display.h"
#include "../../msg_hash_lbl_str.h"
#include "../../configuration.h"
#include "../../input/input_osk.h"
#include "../../runloop.h"
#include "../../retroarch.h"
#include "../../defaults.h"
#include "../../playlist.h"
#include "../../core_info.h"
#include "../../paths.h"
#include "../../version.h"
#include "../../msg_hash.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include <features/features_cpu.h>
#include <memory/mem_stats.h>
#include "../../manual_content_scan.h"
#include "../../tasks/tasks_internal.h"
#include "../menu_cbs.h"
#ifdef HAVE_NETWORKING
#include "../../core_updater_list.h"
#endif

#ifdef HAVE_AUDIOMIXER
#include "../../audio/audio_driver.h"
#endif

#include "pix16_font.h"

/* ------------------------------------------------------------------
 * Layout (native framebuffer pixels)
 * ------------------------------------------------------------------ */

/* Framebuffer: 320x240 for 4:3; 480x270 for 16:9 (an exact 4x of
 * 1080p) with the Widescreen Layout setting when the picture is wide */
#define PIX16_FB_MAX_W        480
#define PIX16_FB_MAX_H        270
static int pix16_fb_w      = 320;
static int pix16_fb_h      = 240;
/* The grid is justified: rows are full except the last, whose tiles
 * stretch to the full width, so a page never leaves a hole. */
static int pix16_grid_cols = 4;    /* tiles in a full row */
static int pix16_grid_rows = 3;
static int pix16_grid_last = 4;    /* tiles in the last row */
static int pix16_grid_w    = 296;  /* width of the grid area */
static int pix16_full_w    = 71;   /* width of a tile in a full row */
static int pix16_last_w    = 71;   /* width of a tile in the last row */
static int pix16_tile_h    = 54;
static int pix16_icon_h    = 40;
static int pix16_row_stride= 56;
static int pix16_tile_gap  = 4;
#define PIX16_FB_WIDTH        pix16_fb_w
#define PIX16_FB_HEIGHT       pix16_fb_h
/* the systems grid sits lower on the taller widescreen frame */
#define PIX16_GRID_DY         ((PIX16_FB_HEIGHT - 240) / 2)
#define PIX16_SAFE            12   /* overscan margin */
#define PIX16_BAR_H           18   /* tab bar / title bar */
#define PIX16_ROW_H           15
#define PIX16_GAP             6
#define PIX16_DESC_H          24
#define PIX16_FOOTER_H        10
#define PIX16_SHADOW          2
/* The systems grid is laid out for the number of systems: filled in by
 * pix16_grid_layout() so the tiles fill the grid area. */
#define PIX16_GRID_COLS       pix16_grid_cols
#define PIX16_GRID_ROWS       pix16_grid_rows
#define PIX16_TILE_H          pix16_tile_h
#define PIX16_TILE_GAP_X      pix16_tile_gap
#define PIX16_ROW_STRIDE      pix16_row_stride
#define PIX16_ICON_H          pix16_icon_h
/* width of the tile at (row, col): the last row is justified */
#define PIX16_TILE_W_AT(r)    (((r) == PIX16_GRID_ROWS - 1) \
      ? pix16_last_w : pix16_full_w)
/* Native art size: the dot-art icons are scaled from this box, so they
 * grow with the tile (1:1 at the standard tile size). */
#define PIX16_ART_W           64
#define PIX16_ART_H           40
/* Inside a tile: the name sits clear of the bottom edge, and the icon
 * box stops a little above it, so neither one touches the frame. */
#define PIX16_LABEL_PAD       4    /* clear space under the name */
#define PIX16_LABEL_GAP       2    /* clear space above the name */

#define PIX16_QUICK_W         176
#define PIX16_TICKER_SPACER   "   "
#define PIX16_MSG_MAX         1024
#define PIX16_SLOT_NONE       (-2)  /* no save state */

/* ------------------------------------------------------------------
 * Palette (ARGB8888, converted to the platform format at init)
 * ------------------------------------------------------------------ */

enum pix16_color_id
{
   PIX16_C_BG_A = 0,
   PIX16_C_BG_B,
   PIX16_C_PANEL,
   PIX16_C_SHADOW,
   PIX16_C_TEXT,
   PIX16_C_TEXT_DIM,
   PIX16_C_VALUE,
   PIX16_C_LAVENDER,
   PIX16_C_YELLOW,
   PIX16_C_RED,
   PIX16_C_GREEN,
   PIX16_C_BLUE,
   PIX16_C_DARK_TEXT,
   PIX16_C_PAPER,
   PIX16_C_SCRIM,
   /* JRPG theme */
   PIX16_C_J_BG,
   PIX16_C_J_EDGE,
   PIX16_C_J_BORDER,
   PIX16_C_J_BORDER_LO,
   PIX16_C_J_TEXT,
   PIX16_C_J_SHADOW,
   PIX16_C_J_DIM,
   PIX16_C_J_VALUE,
   PIX16_C_J_GOLD,
   PIX16_C_J_GLOVE_SHADE,
   PIX16_C_LAST
};

static const uint32_t pix16_palette_argb[PIX16_C_LAST] = {
   0xFF1D1640, /* BG_A      */
   0xFF231A4C, /* BG_B      */
   0xFF2A1F5C, /* PANEL     */
   0xFF0D0A24, /* SHADOW    */
   0xFFF0F0F0, /* TEXT      (no pure white: avoids CRT bloom) */
   0xFFE0DBFF, /* TEXT_DIM  */
   0xFFA99FF0, /* VALUE     */
   0xFF8A7FD1, /* LAVENDER  */
   0xFFF0C419, /* YELLOW    */
   0xFFD83A3A, /* RED       (softened: pure red smears on a tube) */
   0xFF22B35A, /* GREEN     */
   0xFF3A5CE0, /* BLUE      */
   0xFF1D1640, /* DARK_TEXT */
   0xFFF0F0F0, /* PAPER     */
   0xB00D0A24, /* SCRIM     (darkens running content behind quick menu) */
   0xFF05050C, /* J_BG         */
   0xFF0A0A16, /* J_EDGE       outer outline of a window */
   0xFFDCDCE8, /* J_BORDER     bright bevel */
   0xFF8088A8, /* J_BORDER_LO  inner bevel line */
   0xFFF0F0F0, /* J_TEXT       */
   0xFF0A0A18, /* J_SHADOW     text drop shadow */
   0xFF8C94B4, /* J_DIM        unselected tabs / disabled */
   0xFFA8D4FF, /* J_VALUE      setting values */
   0xFFF4D060, /* J_GOLD       headings */
   0xFFA4A8C0  /* J_GLOVE_SHADE */
};

/* JRPG window gradient, top -> bottom */
#define PIX16_J_WIN_TOP 0xFF3454BC
#define PIX16_J_WIN_BOT 0xFF0C1450

/* ------------------------------------------------------------------
 * Tabs
 * ------------------------------------------------------------------ */

enum pix16_tab
{
   PIX16_TAB_MAIN = 0,
   PIX16_TAB_SETUP,
   PIX16_TAB_LISTS,
#ifdef HAVE_NETWORKING
   PIX16_TAB_NET,
#endif
   PIX16_TAB_COUNT
};

typedef struct
{
   uint32_t color;
   uint32_t fg;
   enum msg_hash_enums name;
} pix16_tab_info_t;

/* A window or tile drawn this frame, for the open/close transitions */
#define PIX16_MAX_RECTS 48
enum { PIX16_RECT_WIN = 0, PIX16_RECT_TILE };
typedef struct
{
   int16_t x, y, w, h;
   uint8_t kind;
} pix16_rect_t;

#define PIX16_TR_CLOSE_MS 120   /* old windows shrink shut   */
#define PIX16_TR_OPEN_MS  180   /* new windows grow open     */
#define PIX16_POP_MS      120   /* popup windows open/close  */

static const pix16_tab_info_t pix16_tabs[PIX16_TAB_COUNT] = {
   { 0xFFD83A3A, 0xFFF0F0F0, MSG_PIX16_TAB_MAIN  },
   { 0xFFF0C419, 0xFF1D1640, MSG_PIX16_TAB_SETUP },
   { 0xFF22B35A, 0xFF10123A, MSG_PIX16_TAB_LISTS },
#ifdef HAVE_NETWORKING
   { 0xFF3A5CE0, 0xFFF0F0F0, MSG_PIX16_TAB_NET   },
#endif
};

/* ------------------------------------------------------------------
 * Known systems: playlist name -> short name, title, colour
 * (anything not listed falls back to the part after the last " - ")
 * ------------------------------------------------------------------ */

typedef struct
{
   const char *playlist;
   const char *short_name;   /* grid label, 8 characters max */
   const char *title;        /* game list title bar */
   uint32_t color;
   uint32_t fg;
} pix16_system_t;

/* Known systems, in grid order: arcade first, then by maker; within a
 * maker consoles first, then handhelds; each oldest to newest */
static const pix16_system_t pix16_systems[] = {
   /* Arcade (shown as one ARCADE tile) */
   { "FBNeo - Arcade Games",                           "ARCADE",   "FBNeo Arcade",         0xFFF08A2A, 0xFF1D1640 },
   { "MAME",                                           "MAME",     "MAME",                 0xFFF08A2A, 0xFF1D1640 },
   { "Capcom - CP System I",                           "CPS-1",    "Capcom CPS-1",         0xFFF08A2A, 0xFF1D1640 },
   { "Capcom - CP System II",                          "CPS-2",    "Capcom CPS-2",         0xFFF08A2A, 0xFF1D1640 },
   { "Capcom - CP System III",                         "CPS-3",    "Capcom CPS-3",         0xFFF08A2A, 0xFF1D1640 },
   /* Nintendo consoles */
   { "Nintendo - Nintendo Entertainment System",       "NES",      "NES",                  0xFFD83A3A, 0xFFF0F0F0 },
   { "Nintendo - Family Computer Disk System",         "FDS",      "Famicom Disk System",  0xFFD83A3A, 0xFFF0F0F0 },
   { "Nintendo - Super Nintendo Entertainment System", "SNES",     "Super Nintendo",       0xFF7B4FD8, 0xFFF0F0F0 },
   { "Nintendo - Satellaview",                         "BS-X",     "Satellaview",          0xFF7B4FD8, 0xFFF0F0F0 },
   { "Nintendo - Nintendo 64",                         "N64",      "Nintendo 64",          0xFFF0C419, 0xFF1D1640 },
   /* Nintendo handhelds */
   { "Nintendo - Game Boy",                            "GAME BOY", "Game Boy",             0xFF22B35A, 0xFF10123A },
   { "Nintendo - Virtual Boy",                         "V-BOY",    "Virtual Boy",          0xFFD83A3A, 0xFFF0F0F0 },
   { "Nintendo - Game Boy Color",                      "GB COLOR", "Game Boy Color",       0xFF1FB0B0, 0xFF10123A },
   { "Nintendo - Game Boy Advance",                    "GBA",      "Game Boy Advance",     0xFFE0589A, 0xFF1D1640 },
   /* Sega consoles */
   { "Sega - Master System - Mark III",                "MASTER",   "Master System",        0xFF3A5CE0, 0xFFF0F0F0 },
   { "Sega - Mega Drive - Genesis",                    "GENESIS",  "Sega Genesis",         0xFF3A5CE0, 0xFFF0F0F0 },
   { "Sega - Mega-CD - Sega CD",                       "SEGA CD",  "Sega CD",              0xFF3A5CE0, 0xFFF0F0F0 },
   { "Sega - 32X",                                     "32X",      "Sega 32X",             0xFF3A5CE0, 0xFFF0F0F0 },
   { "Sega - Saturn",                                  "SATURN",   "Sega Saturn",          0xFF5A6A80, 0xFFF0F0F0 },
   { "Sega - Dreamcast",                               "DCAST",    "Dreamcast",            0xFFF08A2A, 0xFF1D1640 },
   /* Sega handhelds */
   { "Sega - Game Gear",                               "GAMEGEAR", "Game Gear",            0xFF1FB0B0, 0xFF10123A },
   /* Sony */
   { "Sony - PlayStation",                             "PS1",      "PlayStation",          0xFF1FB0B0, 0xFF10123A },
   /* NEC */
   { "NEC - PC Engine - TurboGrafx 16",                "PCE",      "PC Engine",            0xFFF08A2A, 0xFF1D1640 },
   { "NEC - PC Engine CD - TurboGrafx-CD",             "PCE CD",   "PC Engine CD",         0xFFF08A2A, 0xFF1D1640 },
   { "NEC - PC-FX",                                    "PC-FX",    "PC-FX",                0xFFF08A2A, 0xFF1D1640 },
   /* SNK consoles */
   { "SNK - Neo Geo",                                  "NEO GEO",  "Neo Geo",              0xFFF0C419, 0xFF1D1640 },
   { "SNK - Neo Geo CD",                               "NEO CD",   "Neo Geo CD",           0xFFF0C419, 0xFF1D1640 },
   /* SNK handhelds */
   { "SNK - Neo Geo Pocket Color",                     "NGPC",     "Neo Geo Pocket Color", 0xFF22B35A, 0xFF10123A },
   /* Atari consoles */
   { "Atari - 2600",                                   "2600",     "Atari 2600",           0xFFD83A3A, 0xFFF0F0F0 },
   { "Atari - 7800",                                   "7800",     "Atari 7800",           0xFFD83A3A, 0xFFF0F0F0 },
   /* Atari handhelds */
   { "Atari - Lynx",                                   "LYNX",     "Atari Lynx",           0xFFF0C419, 0xFF1D1640 },
   /* Bandai handhelds */
   { "Bandai - WonderSwan",                            "WSWAN",    "WonderSwan",           0xFF3A5CE0, 0xFFF0F0F0 },
   { "Bandai - WonderSwan Color",                      "WSC",      "WonderSwan Color",     0xFF3A5CE0, 0xFFF0F0F0 },
   /* Others */
   { "Coleco - ColecoVision",                          "COLECO",   "ColecoVision",         0xFF5A6A80, 0xFFF0F0F0 },
   { "The 3DO Company - 3DO",                          "3DO",      "3DO",                  0xFFF0C419, 0xFF1D1640 },
   { NULL, NULL, NULL, 0, 0 }
};

/* Colours handed out to unknown playlists, by name hash */
static const uint32_t pix16_fallback_colors[][2] = {
   { 0xFFD83A3A, 0xFFF0F0F0 },
   { 0xFF7B4FD8, 0xFFF0F0F0 },
   { 0xFF3A5CE0, 0xFFF0F0F0 },
   { 0xFF22B35A, 0xFF10123A },
   { 0xFFE0589A, 0xFF1D1640 },
   { 0xFFF0C419, 0xFF1D1640 },
   { 0xFF1FB0B0, 0xFF10123A },
   { 0xFFF08A2A, 0xFF1D1640 }
};

/* ------------------------------------------------------------------
 * Driver state
 * ------------------------------------------------------------------ */

enum pix16_flags
{
   PIX16_FLAG_BOOT_PENDING  = (1 << 0),
   PIX16_FLAG_FORCE_REDRAW  = (1 << 1),
   PIX16_FLAG_SELECT_SYSTEM = (1 << 2),
   PIX16_FLAG_AUTO_RUN      = (1 << 3), /* A on a game: press "Run" for us */
   PIX16_FLAG_RETURN_GEAR   = (1 << 4), /* back from options: focus the gear */
   PIX16_FLAG_LISTS_CHANGED = (1 << 5)  /* a scan made/changed playlists */
};

struct pix16_art
{
   char *name;
   uint16_t *px;              /* converted pixels */
   uint8_t *mask;             /* 1 = opaque */
   unsigned w, h;             /* 0 = no file */
};

/* "+" button: the Add System wizard */
enum pix16_wiz_step
{
   PIX16_WIZ_CORE = 0,
   PIX16_WIZ_SYSTEM,
   PIX16_WIZ_PLACE,
   PIX16_WIZ_SOURCE,
   PIX16_WIZ_BROWSE,
   PIX16_WIZ_DAT,
   PIX16_WIZ_BIOS,
   PIX16_WIZ_CONFIRM
};

/* what a row of the wizard does when chosen */
enum pix16_wiz_row
{
   PIX16_WR_INFO = 0,         /* text only, the cursor skips it */
   PIX16_WR_CORE,
   PIX16_WR_DOWNLOAD,
   PIX16_WR_SYS,
   PIX16_WR_OTHER,
   PIX16_WR_HOME,
   PIX16_WR_ARCADE,
   PIX16_WR_HOST,
   PIX16_WR_SRC_DIR,
   PIX16_WR_SRC_ONE,
   PIX16_WR_USE_DIR,
   PIX16_WR_UP,
   PIX16_WR_DIR,
   PIX16_WR_FILE,
   PIX16_WR_NO_DAT,
   PIX16_WR_PICK_DAT,
   PIX16_WR_CONTINUE,
   PIX16_WR_RECHECK,
   PIX16_WR_ADD
};

enum { PIX16_BROWSE_DIR = 0, PIX16_BROWSE_GAME, PIX16_BROWSE_DAT };

#define PIX16_WIZ_HIST 16
#define PIX16_PICK_MAX 256
#define PIX16_WIZ_ROWS ((size_t)((PIX16_FB_HEIGHT - 52 - 70) / 14))

typedef struct
{
   struct string_list *rows;  /* label; attr.i = PIX16_WR_* */
   struct string_list *vals;  /* path / name behind each row */
   dir_list_iter_t *walk;     /* folder being read, a slice per frame */
   struct string_list *walk_list;
   size_t sel, top;
   int hist[PIX16_WIZ_HIST];          /* steps to go back to */
   int hist_n;
   int step;                          /* PIX16_WIZ_* */
   int source;                        /* 0 = folder, 1 = one game */
   int browse_for;                    /* PIX16_BROWSE_* */
   char core_path[PATH_MAX_LENGTH];
   char core_name[NAME_MAX_LENGTH];
   char system[NAME_MAX_LENGTH];
   char place[NAME_MAX_LENGTH];       /* "" home, "ARCADE", or a playlist */
   char place_title[NAME_MAX_LENGTH];
   char dir[PATH_MAX_LENGTH];         /* folder shown ("" = drives) */
   char pick[PATH_MAX_LENGTH];        /* the games folder, or the game */
   char dat[PATH_MAX_LENGTH];         /* arcade name file, or "" */
   char hint[PATH_MAX_LENGTH];        /* folder to put the cursor on */
   bool open;
   bool resume;                       /* reopen after the core downloader */
} pix16_wiz_t;

typedef struct
{
   /* long labels: still for a second, then they scroll left */
   struct { retro_time_t t0, seen; uint32_t key; } tick[4];
   retro_time_t grid_start;
   retro_time_t glove_start;
   retro_time_t qm_status_time;
   retro_time_t slide_t0;
   retro_time_t tr_t0;
   retro_time_t pop_t0;
   retro_time_t popc_t0;
   retro_time_t boot_t0;      /* 0 = not started yet */
   retro_time_t logo_t0;
   retro_time_t boot_go_t0;   /* button pressed on the logo: flash, then go */

   uint16_t *fb;              /* PIX16_FB_WIDTH * PIX16_FB_HEIGHT */
   uint16_t *slide_old;       /* the tab being left */
   uint16_t *upload;          /* packed view when it is narrower */
   uint16_t (*conv)(uint32_t argb);
   uint32_t win_top, win_bot; /* themed JRPG window gradient (ARGB) */
   uint32_t sel_top, sel_bot; /* themed selected-window gradient (ARGB) */
   uint16_t bg_dark, bg_light; /* themed dither of the background */
   unsigned palette;          /* PIX16_PALETTE_* */
   unsigned transition;       /* PIX16_TRANSITION_* */
   unsigned background;       /* PIX16_BACKGROUND_* */
   uint16_t bg_ramp[8];       /* bg_dark .. bg_light .. near white */
   struct pix16_art *arts;    /* system art cache */
   size_t art_count;
   size_t *grid_map;          /* systems grid: display order -> list index */
   size_t grid_count;
   size_t grid_cap;
   size_t *sub_idx;           /* playlists inside another tile */
   size_t *sub_host;
   size_t sub_count;
   size_t sub_cap;
   size_t *arcade_idx;        /* arcade playlists behind one ARCADE tile */
   size_t arcade_count;
   size_t arcade_cap;
   struct string_list *cnt_files; /* the boot game count, a slice per frame */
   RFILE *cnt_fp;
   char *cnt_buf;
   size_t cnt_idx;
   size_t cnt_tail;
   menu_entry_t entry;        /* large: kept off the stack */
   pix16_wiz_t wiz;           /* the Add System wizard */
   pix16_rect_t empty_rect;  /* the no systems window, for clicks */
   bool empty_show;           /* the window is on screen now */

   /* list index of History / Favorites, or (size_t)-1 when hidden */
   size_t top_idx[2];
   size_t picker_host;        /* tile whose picker is open */
   size_t pick[PIX16_PICK_MAX];  /* the picker's rows (list indices) */
   size_t arcade_rep;         /* list index standing in for the group */
   size_t pce_cart, pce_cd;   /* PC Engine playlists, or PIX16_TOP_NONE */
   size_t arcade_sel;         /* row in the picker */
   size_t game_menu_idx;      /* list index of the game */
   size_t grid_page;
   size_t grid_pages;
   size_t auto_run_depth;

   uint32_t grid_ctx;
   uint32_t glove_ctx;
   uint32_t scr_key;
   unsigned flags;
   unsigned buttons;          /* PIX16_BUTTONS_* */
   unsigned game_menu_sel;    /* 0 = RUN, 1 = LOAD SAVE */
   unsigned theme;            /* PIX16_THEME_* */
   unsigned launch_state;     /* PIX16_LAUNCH_* */
   retro_time_t swirl_t0;     /* start of the battle-swirl launch effect */
   unsigned launch_kind;      /* PIX16_TRANSITION_* resolved for this launch */
   bool swirl_active;         /* the launch effect is playing */
   retro_time_t handover_t0;  /* swirl done: hold the screen until the game */
   bool handover;
   bool fmt_565;              /* framebuffer format is RGB565, not xxxx4444 */
   unsigned qm_sel;           /* PIX16_QM_* */
   unsigned quit_sel;         /* 0 = YES, 1 = NO */
   int sys_glove_x, sys_glove_y;
   int game_menu_slot;        /* newest save state slot, -1 auto, -2 none */
   int pending_slot;          /* slot to load when "Run" is pressed for us */
   int grid_from_px, grid_to_px;
   int clip_x0, clip_y0, clip_x1, clip_y1; /* exclusive right/bottom */
   int glove_from_x, glove_from_y, glove_to_x, glove_to_y;
   int qm_slot;               /* newest save state, PIX16_SLOT_NONE if none */
   /* part of the framebuffer sent to the screen: all of it, except in a
    * game whose CRT mode has fewer lines/columns than the menu */
   int view_x, view_y, view_w, view_h;
   int slide_dir;             /* +1: new tab comes in from the right */
   int prev_tab;
   int rec_n;
   int last_n;
   int tr_old_n;
   int pop_prev;
   int boot_phase;            /* 0 = BIOS, 1 = logo */
   int boot_systems, boot_games;

   /* window open/close transitions (JRPG theme) */
   pix16_rect_t rec[PIX16_MAX_RECTS];   /* windows/tiles drawn this frame */
   pix16_rect_t last[PIX16_MAX_RECTS];  /* ...and last frame */
   pix16_rect_t tr_old[PIX16_MAX_RECTS];/* the screen being closed */
   pix16_rect_t pop_rect;     /* popup windows (pickers, RUN window) */
   pix16_rect_t popc_rect;
   uint16_t colors[PIX16_C_LAST];
   uint16_t tab_color[PIX16_TAB_COUNT];
   uint16_t tab_fg[PIX16_TAB_COUNT];

   char msg[PIX16_MSG_MAX];
   char title[NAME_MAX_LENGTH];
   char shown_title[NAME_MAX_LENGTH]; /* title as displayed */
   char sel_full[NAME_MAX_LENGTH];    /* full name of a cut-off row */
   char qm_status[32];        /* "STATE SAVED", shown for a moment */

   bool sys_glove_on;
   bool gear_focus;           /* the options gear (not a list entry) */
   bool power_focus;          /* the power button, top left (ditto) */
   bool add_focus;            /* the + (Add System) button */
   bool order_focus;          /* the order (arrange) button */
   bool sel_cut;              /* the selected row did not fit */
   bool arcade_open;          /* picker window showing (ARCADE or PCE) */
   bool order_open;           /* system order window showing */
   bool mg_open;              /* actions window for one system */
   bool mg_pick;              /* choosing a system to merge into */
   bool mg_icon;              /* the picker is choosing an icon */
   bool mg_icon_more;         /* showing the icon categories */
   int  mg_icon_cat;          /* >= 0: showing that category's icons */
   int mg_sel;                /* highlighted row */
   size_t mg_n;               /* rows in the merge picker */
   size_t mg_map[64];         /* grid indices, for the picker */
   char mg_label[64][40];     /* their labels */
   char mg_name[NAME_MAX_LENGTH];   /* system the actions apply to */
   char mg_member[NAME_MAX_LENGTH]; /* its playlist name */

   int order_sel;             /* highlighted row */
   size_t order_n;            /* rows in it */
   size_t order_map[64];      /* list indices, top to bottom */
   char order_label[64][40];  /* their names, copied when it opens */
   unsigned char order_state[64]; /* 0 tile, 1 merged away, 2 hidden */
   bool picker_pce;           /* ...and it is the PC Engine one */
   bool from_gear;            /* options were opened with the gear */
   bool game_menu_open;       /* RUN / LOAD SAVE window for a game */
   bool grid_valid;
   bool glove_valid;
   bool text_shadow;          /* draw 1px drop shadow under text */
   bool qm_full;              /* Start pressed: RetroArch's full Quick Menu */
   bool qm_slot_valid;
   bool quit_confirm;         /* "Are you sure you want to quit?" showing */
   bool confirm_exit;         /* ...it is the EXIT (close RetroArch) one */
   bool opt_back;             /* options screen: glove is on the BACK button */
   bool slide_active;
   bool prev_opt;
   bool rec_on;
   bool scr_key_valid;
   bool tr_active;
   bool popc_active;
   bool boot_enabled;         /* the Boot Screen setting */
   bool start_enabled;        /* the Start Screen setting */
   bool screen_asked;         /* the 4:3 / widescreen question was answered */
   bool screen_prompt;        /* the question is on screen now */
   unsigned screen_sel;       /* 0 = 4:3, 1 = widescreen */
   bool wide_cfg;             /* the Widescreen Layout setting */
   bool reduced_motion;       /* the Reduced Motion setting */
   bool boot_active;
   bool boot_counted;
   bool cnt_done;
   bool keys;                 /* no pad connected: show keyboard keys */
} pix16_t;

enum pix16_sfx
{
   PIX16_SFX_UP = 0,
   PIX16_SFX_DOWN,
   PIX16_SFX_OK,
   PIX16_SFX_BACK
};
static void pix16_sfx(enum pix16_sfx which);

enum pix16_qm_row
{
   PIX16_QM_RESUME = 0,
   PIX16_QM_QUIT,
   PIX16_QM_SAVE,
   PIX16_QM_LOAD,
   PIX16_QM_ROWS
};

/* Tracks a game started with A, to come back to its list */
enum pix16_launch
{
   PIX16_LAUNCH_NONE = 0,
   PIX16_LAUNCH_STARTING,     /* "Run" pressed, content not up yet */
   PIX16_LAUNCH_RUNNING       /* content seen running */
};

enum pix16_theme
{
   PIX16_THEME_JRPG = 0,
   PIX16_THEME_CLASSIC
};

/* Button guide along the bottom */
static int pix16_draw_guide(pix16_t *pix, int x, int right, int y, bool jrpg);

/* ------------------------------------------------------------------
 * Pixel format conversion (same targets RGUI uses)
 * ------------------------------------------------------------------ */

static uint16_t pix16_argb_to_rgba4444(uint32_t c)
{
   return (uint16_t)((((c >> 20) & 0xF) << 12) | (((c >> 12) & 0xF) << 8)
         | (((c >> 4) & 0xF) << 4) | ((c >> 28) & 0xF));
}

static uint16_t pix16_argb_to_argb4444(uint32_t c)
{
   return (uint16_t)((((c >> 28) & 0xF) << 12) | (((c >> 20) & 0xF) << 8)
         | (((c >> 12) & 0xF) << 4) | ((c >> 4) & 0xF));
}

static uint16_t pix16_argb_to_bgra4444(uint32_t c)
{
   return (uint16_t)((((c >> 4) & 0xF) << 12) | (((c >> 12) & 0xF) << 8)
         | (((c >> 20) & 0xF) << 4) | ((c >> 28) & 0xF));
}

static uint16_t pix16_argb_to_abgr4444(uint32_t c)
{
   return (uint16_t)((((c >> 28) & 0xF) << 12) | (((c >> 4) & 0xF) << 8)
         | (((c >> 12) & 0xF) << 4) | ((c >> 20) & 0xF));
}

static uint16_t pix16_argb_to_rgb565(uint32_t c)
{
   unsigned a = (c >> 24) & 0xFF;
   unsigned r = (c >> 16) & 0xFF;
   unsigned g = (c >> 8)  & 0xFF;
   unsigned b =  c        & 0xFF;
   /* No alpha channel: pre-darken translucent colours */
   r = (r * a) / 255;
   g = (g * a) / 255;
   b = (b * a) / 255;
   return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void pix16_select_pixel_format(pix16_t *pix)
{
   const char *ident = video_driver_get_ident();

   pix->conv    = pix16_argb_to_rgba4444;
   pix->fmt_565 = false;

   if (string_is_empty(ident))
      return;

   if (     string_is_equal(ident, "d3d8")
         || string_is_equal(ident, "d3d9_hlsl")
         || string_is_equal(ident, "d3d9_cg")
         || string_is_equal(ident, "rsx"))
      pix->conv = pix16_argb_to_argb4444;
   else if (string_is_equal(ident, "d3d10")
         || string_is_equal(ident, "d3d11")
         || string_is_equal(ident, "d3d12")
         || string_is_equal(ident, "metal"))
      pix->conv = pix16_argb_to_bgra4444;
   else if (string_is_equal(ident, "psp1"))
      pix->conv = pix16_argb_to_abgr4444;
   else if (string_is_equal(ident, "sdl_dingux")
         || string_is_equal(ident, "sdl_rs90"))
   {
      pix->conv    = pix16_argb_to_rgb565;
      pix->fmt_565 = true;
   }
}

typedef struct
{
   uint32_t bg_a, bg_b, panel, shadow, j_bg, j_edge, win_top, win_bot;
   uint32_t gold, accent, bg_dark, bg_light, sel_top, sel_bot;
} pix16_palette_t;

static const pix16_palette_t pix16_palettes[PIX16_PALETTE_LAST] = {
   /* 16-bit: the stock colours, already in pix16_palette_argb */
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0xFF060A20, 0xFF16205C, 0xFF86A4F6, 0xFF4060D0 },
   /* Amber CRT */
   { 0xFF1A1206, 0xFF241A08, 0xFF2E210B, 0xFF0C0904, 0xFF0A0703,
     0xFF1A1206, 0xFF6B4A12, 0xFF241505, 0xFFF0B030, 0xFFF0D8A0,
     0xFF0C0802, 0xFF2A1E08, 0xFFF0C070, 0xFF8A5A10 },
   /* Game Boy */
   { 0xFF0F1B0F, 0xFF16260F, 0xFF1E3315, 0xFF0A1006, 0xFF0A1208,
     0xFF121C0D, 0xFF4E7B34, 0xFF1E3315, 0xFF9BBC0F, 0xFFC6DE8C,
     0xFF0A1206, 0xFF1E3315, 0xFFC6DE8C, 0xFF5E8A34 },
   /* Ice */
   { 0xFF0A1620, 0xFF0E1E2C, 0xFF142838, 0xFF060C14, 0xFF050A10,
     0xFF0C1A26, 0xFF2A6FA8, 0xFF0E2A44, 0xFFC8ECFF, 0xFFA8D4FF,
     0xFF050A12, 0xFF14283C, 0xFFD8F4FF, 0xFF3A7AB0 },
   /* Vaporwave */
   { 0xFF1A0A2A, 0xFF26123E, 0xFF341A52, 0xFF100520, 0xFF100520,
     0xFF2A1042, 0xFFA02AA8, 0xFF3A0A5A, 0xFF6EF0FF, 0xFFFF9AE8,
     0xFF0E0418, 0xFF2A0E44, 0xFF9AE8FF, 0xFF7A1A9A },
   /* Sunset */
   { 0xFF1E0E12, 0xFF2E1418, 0xFF3A1A1E, 0xFF120608, 0xFF120608,
     0xFF2A0E10, 0xFFC0502A, 0xFF4A1408, 0xFFF2B060, 0xFFFFD0A0,
     0xFF120608, 0xFF3A1410, 0xFFFFD890, 0xFFA04010 },
   /* Mono */
   { 0xFF141414, 0xFF1E1E1E, 0xFF262626, 0xFF0C0C0C, 0xFF0C0C0C,
     0xFF1A1A1A, 0xFF8A8A8A, 0xFF2A2A2A, 0xFFF0F0F0, 0xFFD0D0D0,
     0xFF0A0A0A, 0xFF2A2A2A, 0xFFFFFFFF, 0xFF909090 },
   /* Crimson */
   { 0xFF1A0A0C, 0xFF261014, 0xFF321418, 0xFF0E0507, 0xFF0E0507,
     0xFF240A0E, 0xFFA02830, 0xFF3A0A10, 0xFFF2A0A0, 0xFFFFB0B0,
     0xFF0E0508, 0xFF2E0C12, 0xFFFF8A8A, 0xFFA02030 },
   /* Cobalt */
   { 0xFF0A1224, 0xFF101C38, 0xFF16243F, 0xFF060A16, 0xFF080E1C,
     0xFF14203A, 0xFF24509E, 0xFF0C1C40, 0xFFF0C870, 0xFFA8CEFF,
     0xFF060C18, 0xFF1A2E56, 0xFF86B4F0, 0xFF2A4E92 },
   /* Forest */
   { 0xFF08160C, 0xFF0E2012, 0xFF142A1A, 0xFF040C06, 0xFF07120A,
     0xFF12241A, 0xFF2E7038, 0xFF0A2814, 0xFFE8D478, 0xFFA8E0A8,
     0xFF050E07, 0xFF1E4026, 0xFF90D898, 0xFF2E6836 },
   /* Plum */
   { 0xFF150A1E, 0xFF1E0E2A, 0xFF2A1438, 0xFF0A0410, 0xFF120818,
     0xFF22102E, 0xFF7030A0, 0xFF2A0C40, 0xFFF0B870, 0xFFE0A8FF,
     0xFF0C0512, 0xFF341A4C, 0xFFC890F0, 0xFF7030A0 },
   /* Sakura */
   { 0xFF1A0C14, 0xFF24121C, 0xFF341A26, 0xFF0E060A, 0xFF180A12,
     0xFF2C1420, 0xFFA04870, 0xFF3A1428, 0xFFFFD8B8, 0xFFFFB8D8,
     0xFF12060E, 0xFF502436, 0xFFFFC0DC, 0xFFA04870 },
   /* Ocean */
   { 0xFF20207A, 0xFF2A2A8E, 0xFF3838A8, 0xFF10104A, 0xFF24247E,
     0xFF4646C0, 0xFF6060D8, 0xFF2A2A82, 0xFFF0E488, 0xFFD0D0FF,
     0xFF1A1A6E, 0xFF4A4AC4, 0xFF8A8AE8, 0xFF4040A8 },
   /* Copper */
   { 0xFF1C1208, 0xFF26180C, 0xFF321E10, 0xFF0C0702, 0xFF180E06,
     0xFF2C1A0C, 0xFF90501E, 0xFF3C1C08, 0xFFF0C088, 0xFFFFD0A0,
     0xFF120A04, 0xFF402612, 0xFFC07838, 0xFF7A3E12 },
   /* Midnight */
   { 0xFF05050C, 0xFF090A14, 0xFF121220, 0xFF020204, 0xFF05050A,
     0xFF0E0E1A, 0xFF1C1C3E, 0xFF07070E, 0xFFE8C860, 0xFFB4C4E4,
     0xFF030306, 0xFF16162A, 0xFF7078A8, 0xFF1E2042 }
};

/* Rough perceived brightness, 0..25500 */
static unsigned pix16_lum(uint32_t c)
{
   return ((c >> 16) & 0xFF) * 30 + ((c >> 8) & 0xFF) * 59 + (c & 0xFF) * 11;
}

static uint32_t pix16_lighten(uint32_t c, unsigned f)
{
   unsigned r = (c >> 16) & 0xFF;
   unsigned g = (c >>  8) & 0xFF;
   unsigned b =  c        & 0xFF;
   r += (255 - r) * f / 255;
   g += (255 - g) * f / 255;
   b += (255 - b) * f / 255;
   return 0xFF000000u | (r << 16) | (g << 8) | b;
}

/* a..b by f/256, per channel; alpha opaque */
static uint32_t pix16_argb_mix(uint32_t a, uint32_t b, unsigned f)
{
   uint32_t r = 0xFF000000;
   unsigned i;
   for (i = 0; i < 24; i += 8)
   {
      int ca = (int)((a >> i) & 0xFF);
      int cb = (int)((b >> i) & 0xFF);
      r |= (uint32_t)(ca + (cb - ca) * (int)f / 256) << i;
   }
   return r;
}

static void pix16_prepare_colors(pix16_t *pix)
{
   uint32_t pal[PIX16_C_LAST];
   const pix16_palette_t *t = &pix16_palettes[pix->palette % PIX16_PALETTE_LAST];
   uint32_t bg_dk = 0xFF060A20;
   uint32_t bg_lt = 0xFF16205C;
   unsigned i;

   memcpy(pal, pix16_palette_argb, sizeof(pal));
   pix->win_top    = PIX16_J_WIN_TOP;
   pix->win_bot    = PIX16_J_WIN_BOT;
   pix->bg_dark    = pix->conv(0xFF060A20);
   pix->bg_light   = pix->conv(0xFF16205C);
   pix->sel_top    = 0xFF86A4F6;
   pix->sel_bot    = 0xFF4060D0;

   if (pix->palette)
   {
      pal[PIX16_C_BG_A]    = t->bg_a;
      pal[PIX16_C_BG_B]    = t->bg_b;
      pal[PIX16_C_PANEL]   = t->panel;
      pal[PIX16_C_SHADOW]  = t->shadow;
      pal[PIX16_C_J_BG]    = t->j_bg;
      pal[PIX16_C_J_EDGE]  = t->j_edge;
      pal[PIX16_C_J_GOLD]  = t->gold;
      pal[PIX16_C_J_VALUE] = t->accent;
      pix->win_top         = t->win_top;
      pix->win_bot         = t->win_bot;
      pix->sel_top         = t->sel_top;
      pix->sel_bot         = t->sel_bot;
      pix->bg_dark         = pix->conv(t->bg_dark);
      pix->bg_light        = pix->conv(t->bg_light);
      bg_dk                = t->bg_dark;
      bg_lt                = t->bg_light;
   }

   for (i = 0; i < 6; i++)
      pix->bg_ramp[i] = pix->conv(pix16_argb_mix(bg_dk, bg_lt, i * 256 / 5));
   pix->bg_ramp[6] = pix->conv(pix16_argb_mix(bg_lt, 0xFFFFFFFF, 90));
   pix->bg_ramp[7] = pix->conv(pix16_argb_mix(bg_lt, 0xFFFFFFFF, 190));

   /* pale themes can put the heading at the window's brightness: lift
    * it until the two separate */
   {
      unsigned lg = pix16_lum(pal[PIX16_C_J_GOLD]);
      unsigned lb = pix16_lum(pix->win_top);
      unsigned d  = lg > lb ? lg - lb : lb - lg;
      if (d < 4500)
         pal[PIX16_C_J_GOLD] = pix16_lighten(pal[PIX16_C_J_GOLD],
               4500 - d > 120 ? 120 : 4500 - d);
   }

   for (i = 0; i < PIX16_C_LAST; i++)
      pix->colors[i] = pix->conv(pal[i]);
   for (i = 0; i < PIX16_TAB_COUNT; i++)
   {
      pix->tab_color[i] = pix->conv(pix16_tabs[i].color);
      pix->tab_fg[i]    = pix->conv(pix16_tabs[i].fg);
   }
}

/* ------------------------------------------------------------------
 * Drawing primitives
 * ------------------------------------------------------------------ */

static void pix16_clip_reset(pix16_t *pix)
{
   pix->clip_x0 = 0;
   pix->clip_y0 = 0;
   pix->clip_x1 = PIX16_FB_WIDTH;
   pix->clip_y1 = PIX16_FB_HEIGHT;
}

static void pix16_clip_set(pix16_t *pix, int x0, int y0, int x1, int y1)
{
   pix->clip_x0 = x0 < 0 ? 0 : x0;
   pix->clip_y0 = y0 < 0 ? 0 : y0;
   pix->clip_x1 = x1 > PIX16_FB_WIDTH  ? PIX16_FB_WIDTH  : x1;
   pix->clip_y1 = y1 > PIX16_FB_HEIGHT ? PIX16_FB_HEIGHT : y1;
}

static void pix16_fill(pix16_t *pix, int x, int y, int w, int h,
      uint16_t color)
{
   int i, j;
   int x0 = x < pix->clip_x0 ? pix->clip_x0 : x;
   int y0 = y < pix->clip_y0 ? pix->clip_y0 : y;
   int x1 = x + w > pix->clip_x1 ? pix->clip_x1 : x + w;
   int y1 = y + h > pix->clip_y1 ? pix->clip_y1 : y + h;

   for (j = y0; j < y1; j++)
   {
      uint16_t *row = pix->fb + j * PIX16_FB_WIDTH;
      for (i = x0; i < x1; i++)
         row[i] = color;
   }
}

/* Panel with a hard drop shadow, 16-bit style */
static void pix16_panel(pix16_t *pix, int x, int y, int w, int h,
      uint16_t color)
{
   pix16_fill(pix, x + PIX16_SHADOW, y + PIX16_SHADOW, w, h,
         pix->colors[PIX16_C_SHADOW]);
   pix16_fill(pix, x, y, w, h, color);
}

static void pix16_glyph(pix16_t *pix, int x, int y, unsigned char c,
      uint16_t color, int scale)
{
   int gx, gy;
   const uint8_t *bits;

   if (c < 0x20 || c > 0x7E)
      c = '?';
   bits = pix16_font8x8[c - 0x20];

   for (gy = 0; gy < PIX16_GLYPH_H; gy++)
   {
      uint8_t line = bits[gy];
      if (!line)
         continue;
      for (gx = 0; gx < PIX16_GLYPH_W; gx++)
         if (line & (1 << gx))
            pix16_fill(pix, x + gx * scale, y + gy * scale,
                  scale, scale, color);
   }
}

/* Draws at most max_chars characters (0 = unlimited). Non-ASCII
 * bytes (UTF-8) are shown as '?', one per code point. */
static void pix16_text(pix16_t *pix, int x, int y, const char *s,
      uint16_t color, int scale, size_t max_chars)
{
   size_t n = 0;
   if (!s)
      return;
   while (*s && (!max_chars || n < max_chars))
   {
      unsigned char c = (unsigned char)*s++;
      if (c >= 0x80)
      {
         /* skip UTF-8 continuation bytes */
         while ((((unsigned char)*s) & 0xC0) == 0x80)
            s++;
         c = '?';
      }
      if (pix->text_shadow)
         pix16_glyph(pix, x + scale, y + scale, c,
               pix->colors[PIX16_C_J_SHADOW], scale);
      pix16_glyph(pix, x, y, c, color, scale);
      x += PIX16_GLYPH_W * scale;
      n++;
   }
}

static size_t pix16_strlen_glyphs(const char *s)
{
   size_t n = 0;
   if (!s)
      return 0;
   for (; *s; s++)
      if ((((unsigned char)*s) & 0xC0) != 0x80)
         n++;
   return n;
}

/* Text that fits in max_chars, adding ".." when truncated */
static void pix16_text_fit(pix16_t *pix, int x, int y, const char *s,
      uint16_t color, size_t max_chars)
{
   size_t len = pix16_strlen_glyphs(s);
   if (max_chars == 0)
      return;
   if (len <= max_chars)
      pix16_text(pix, x, y, s, color, 1, 0);
   else if (max_chars > 2)
   {
      pix16_text(pix, x, y, s, color, 1, max_chars - 2);
      pix16_text(pix, x + (int)(max_chars - 2) * PIX16_GLYPH_W, y,
            "..", color, 1, 0);
   }
}

/* Selected rows: a label too long for its space sits still for a
 * second, then scrolls to the left, loops round and rests again */
#define PIX16_TICK_WAIT_MS 1000
#define PIX16_TICK_PX_S    40
#define PIX16_TICK_GAP     4      /* spaces between the end and the start */
static void pix16_text_ticker(pix16_t *pix, int x, int y, const char *s,
      uint16_t color, size_t max_chars, bool selected)
{
   retro_time_t now;
   uint32_t key = 2166136261u;
   const char *p;
   size_t len = pix16_strlen_glyphs(s);
   int k, slot = -1, oldest = 0, off = 0;
   int cx0, cy0, cx1, cy1;

   if (!selected || len <= max_chars || max_chars < 3)
   {
      pix16_text_fit(pix, x, y, s, color, max_chars);
      return;
   }
   if (pix->reduced_motion)
   {
      /* resting only: no scrolling */
      pix16_text_fit(pix, x, y, s, color, max_chars);
      return;
   }

   /* which label this is (text and place), and since when */
   for (p = s; *p; p++)
      key = (key ^ (unsigned char)*p) * 16777619u;
   key ^= (uint32_t)(x * 7919 + y * 104729);
   now = menu_driver_get_current_time();
   for (k = 0; k < 4; k++)
   {
      if (pix->tick[k].key == key && now - pix->tick[k].seen < 200000)
         slot = k;
      if (pix->tick[k].seen < pix->tick[oldest].seen)
         oldest = k;
   }
   if (slot < 0)
   {
      slot = oldest;
      pix->tick[slot].key = key;
      pix->tick[slot].t0  = now;
   }
   pix->tick[slot].seen = now;
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;

   {
      retro_time_t ms = (now - pix->tick[slot].t0) / 1000;
      int period      = (int)(len + PIX16_TICK_GAP) * PIX16_GLYPH_W;
      int scroll_ms   = period * 1000 / PIX16_TICK_PX_S;
      if (ms >= PIX16_TICK_WAIT_MS)
      {
         int m = (int)((ms - PIX16_TICK_WAIT_MS)
                       % (scroll_ms + PIX16_TICK_WAIT_MS));
         if (m < scroll_ms)
            off = m * PIX16_TICK_PX_S / 1000;
      }
      if (off == 0)
      {
         /* resting: the start of the name, cut at the edge */
         pix16_text_fit(pix, x, y, s, color, max_chars);
         return;
      }
      cx0 = pix->clip_x0; cy0 = pix->clip_y0;
      cx1 = pix->clip_x1; cy1 = pix->clip_y1;
      pix16_clip_set(pix, x > cx0 ? x : cx0, cy0,
            x + (int)max_chars * PIX16_GLYPH_W + 1 < cx1
               ? x + (int)max_chars * PIX16_GLYPH_W + 1 : cx1, cy1);
      pix16_text(pix, x - off, y, s, color, 1, 0);
      pix16_text(pix, x - off + period, y, s, color, 1, 0);
      pix->clip_x0 = cx0; pix->clip_y0 = cy0;
      pix->clip_x1 = cx1; pix->clip_y1 = cy1;
   }
}

/* Face buttons by position: RetroPad A is the right one, B the bottom */
enum pix16_face { PIX16_FACE_RIGHT = 0, PIX16_FACE_BOTTOM };

/* Dark round button with the symbol or letter of the chosen style */
static void pix16_face_icon(pix16_t *pix, int x, int y, enum pix16_face f)
{
   static const uint8_t sym[4][6] = {
      { 0x1E, 0x21, 0x21, 0x21, 0x21, 0x1E },   /* circle */
      { 0x21, 0x12, 0x0C, 0x0C, 0x12, 0x21 },   /* cross  */
      { 0x0C, 0x12, 0x12, 0x1E, 0x12, 0x12 },   /* A      */
      { 0x1C, 0x12, 0x1C, 0x12, 0x12, 0x1C }    /* B      */
   };
   /* [style][face]: symbol, colour */
   static const uint8_t face_sym[PIX16_BUTTONS_LAST][2] = {
      { 0, 1 }, { 2, 3 }, { 3, 2 }
   };
   static const uint32_t face_col[PIX16_BUTTONS_LAST][2] = {
      { 0xFFF07070, 0xFF8AB4F8 },   /* red circle, blue cross */
      { 0xFFF07070, 0xFFF0D050 },   /* red A, yellow B */
      { 0xFFF07070, 0xFF6CD86C }    /* red B, green A */
   };
   unsigned st   = pix->buttons < PIX16_BUTTONS_LAST
         ? pix->buttons : PIX16_BUTTONS_PLAYSTATION;
   const uint8_t *g = sym[face_sym[st][f]];
   uint16_t edge = pix->conv(0xFF0A0A14);
   uint16_t body = pix->conv(0xFF3A3A48);
   uint16_t c    = pix->conv(face_col[st][f]);
   int r, k;

   pix16_fill(pix, x + 2, y,     6, 10, edge);
   pix16_fill(pix, x + 1, y + 1, 8,  8, edge);
   pix16_fill(pix, x,     y + 2, 10, 6, edge);
   pix16_fill(pix, x + 3, y + 1, 4,  8, body);
   pix16_fill(pix, x + 2, y + 2, 6,  6, body);
   pix16_fill(pix, x + 1, y + 3, 8,  4, body);
   for (r = 0; r < 6; r++)
      for (k = 0; k < 6; k++)
         if (g[r] & (0x20 >> k))
            pix16_fill(pix, x + 2 + k, y + 2 + r, 1, 1, c);
}

/* RetroArch's menu OK is RetroPad A and cancel is B, unless
 * "Swap OK/Cancel Buttons" is on */
static enum pix16_face pix16_face_ok(void)
{
   settings_t *settings = config_get_ptr();
   return (settings && settings->bools.input_menu_swap_ok_cancel_buttons)
         ? PIX16_FACE_BOTTOM : PIX16_FACE_RIGHT;
}

static enum pix16_face pix16_face_back(void)
{
   return pix16_face_ok() == PIX16_FACE_BOTTOM
         ? PIX16_FACE_RIGHT : PIX16_FACE_BOTTOM;
}

/* Shoulder buttons and Start, named for the chosen style */
static void pix16_button_names(const pix16_t *pix, const char **l,
      const char **r, const char **start)
{
   switch (pix->buttons)
   {
      case PIX16_BUTTONS_NINTENDO:
         *l = "L";
         *r = "R";
         *start = "START";
         break;
      case PIX16_BUTTONS_XBOX:
         *l = "LB";
         *r = "RB";
         *start = "MENU";
         break;
      default:
         *l = "L1";
         *r = "R1";
         *start = "START";
         break;
   }
   if (pix->keys)
   {
      bool narrow = pix16_fb_w < PIX16_FB_MAX_W;
      *l     = narrow ? "PGU" : "PGUP";
      *r     = narrow ? "PGD" : "PGDN";
      *start = narrow ? "SPC" : "SPACE";
   }
}

static int pix16_pill(pix16_t *pix, int x, int y, const char *text,
      uint16_t bg, uint16_t fg);
/* The actions button: Select on a pad, Right Shift on a keyboard */
static const char *pix16_select_name(const pix16_t *pix)
{
   bool narrow = pix16_fb_w < PIX16_FB_MAX_W;
   if (pix->keys)
      return narrow ? "RSFT" : "RSHIFT";
   switch (pix->buttons)
   {
      case PIX16_BUTTONS_NINTENDO:
         return "-";
      case PIX16_BUTTONS_XBOX:
         return "VIEW";
      default:
         return narrow ? "SEL" : "SELECT";
   }
}

/* Key names in the 4:3 layout sit in slimmer pills with less space
 * between items, so a full guide still fits */
static bool pix16_guide_tight(const pix16_t *pix)
{
   return pix->keys && pix16_fb_w < PIX16_FB_MAX_W;
}

#define PIX16_PILL_PAD(pix) (pix16_guide_tight(pix) ? 4 : 6)
#define PIX16_GUIDE_GAP(pix) (pix16_guide_tight(pix) ? 6 : 10)
#define PIX16_PILL_GAP(pix)  (pix16_guide_tight(pix) ? 2 : 3)
#define PIX16_PAIR_GAP(pix)  (pix16_guide_tight(pix) ? 1 : 2)

/* The menu's own keys for OK and back, used when no pad is
 * connected; shortened in the 4:3 layout */
static const char *pix16_okback_key(bool ok)
{
   if (pix16_fb_w < PIX16_FB_MAX_W)
      return ok ? "ENTR" : "BK";
   return ok ? "ENTER" : "BKSP";
}

static int pix16_okback_width(const pix16_t *pix, bool ok)
{
   if (pix->keys)
      return (int)strlen(pix16_okback_key(ok)) * PIX16_GLYPH_W
            + PIX16_PILL_PAD(pix) + PIX16_PILL_GAP(pix);
   return 13;
}

/* OK or back: the pad's face button, or its key without a pad */
static int pix16_okback(pix16_t *pix, int x, int y, bool ok, bool jrpg)
{
   if (pix->keys)
      return pix16_pill(pix, x, y, pix16_okback_key(ok),
            jrpg ? pix->colors[PIX16_C_J_BORDER] : pix->colors[PIX16_C_LAVENDER],
            jrpg ? pix->colors[PIX16_C_J_EDGE]   : pix->colors[PIX16_C_DARK_TEXT])
            + PIX16_PILL_GAP(pix);
   pix16_face_icon(pix, x, y, ok ? pix16_face_ok() : pix16_face_back());
   return 13;
}

/* Pixel-art cursor: a 4x7 right-pointing triangle */
static void pix16_cursor(pix16_t *pix, int x, int y, uint16_t color)
{
   pix16_fill(pix, x,     y,     1, 7, color);
   pix16_fill(pix, x + 1, y + 1, 1, 5, color);
   pix16_fill(pix, x + 2, y + 2, 1, 3, color);
   pix16_fill(pix, x + 3, y + 3, 1, 1, color);
}

/* ------------------------------------------------------------------
 * JRPG theme primitives
 * ------------------------------------------------------------------ */

static const uint8_t pix16_bayer4[4][4] = {
   {  0,  8,  2, 10 },
   { 12,  4, 14,  6 },
   {  3, 11,  1,  9 },
   { 15,  7, 13,  5 }
};

/* Rect with its corners rounded off by r pixels (r = 0..2) */
static void pix16_round_fill(pix16_t *pix, int x, int y, int w, int h,
      int r, uint16_t color)
{
   if (r >= 2 && h > 4)
   {
      pix16_fill(pix, x + 2, y,         w - 4, 1,     color);
      pix16_fill(pix, x + 1, y + 1,     w - 2, 1,     color);
      pix16_fill(pix, x,     y + 2,     w,     h - 4, color);
      pix16_fill(pix, x + 1, y + h - 2, w - 2, 1,     color);
      pix16_fill(pix, x + 2, y + h - 1, w - 4, 1,     color);
   }
   else if (r == 1 && h > 2)
   {
      pix16_fill(pix, x + 1, y,         w - 2, 1,     color);
      pix16_fill(pix, x,     y + 1,     w,     h - 2, color);
      pix16_fill(pix, x + 1, y + h - 1, w - 2, 1,     color);
   }
   else
      pix16_fill(pix, x, y, w, h, color);
}

/* Vertical gradient with 4x4 ordered dithering, the way SNES-era
 * menus faked smooth fills with few colours */
static void pix16_vgradient(pix16_t *pix, int x, int y, int w, int h,
      uint32_t top, uint32_t bot)
{
   int i, j;
   int tr = (top >> 16) & 0xFF, tg = (top >> 8) & 0xFF, tb = top & 0xFF;
   int br = (bot >> 16) & 0xFF, bg = (bot >> 8) & 0xFF, bb = bot & 0xFF;

   for (j = 0; j < h; j++)
   {
      int py = y + j;
      int r8, g8, b8;
      if (py < pix->clip_y0 || py >= pix->clip_y1)
         continue;
      r8 = tr + (br - tr) * j / (h > 1 ? h - 1 : 1);
      g8 = tg + (bg - tg) * j / (h > 1 ? h - 1 : 1);
      b8 = tb + (bb - tb) * j / (h > 1 ? h - 1 : 1);
      for (i = 0; i < w; i++)
      {
         int px = x + i;
         int d, r, g, b;
         if (px < pix->clip_x0 || px >= pix->clip_x1)
            continue;
         /* one 4-bit step is 17 in 8-bit space */
         d = ((int)pix16_bayer4[py & 3][px & 3] - 8) * 17 / 16;
         r = r8 + d; g = g8 + d; b = b8 + d;
         r = r < 0 ? 0 : r > 255 ? 255 : r;
         g = g < 0 ? 0 : g > 255 ? 255 : g;
         b = b < 0 ? 0 : b > 255 ? 255 : b;
         pix->fb[py * PIX16_FB_WIDTH + px] = pix->conv(
               0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
      }
   }
}

/* A JRPG menu window: dark outline, bright bevel, inner line,
 * dithered blue gradient */
static void pix16_record(pix16_t *pix, int x, int y, int w, int h, int kind)
{
   if (pix->rec_on && pix->rec_n < PIX16_MAX_RECTS)
   {
      pix16_rect_t *r = &pix->rec[pix->rec_n++];
      r->x = (int16_t)x; r->y = (int16_t)y;
      r->w = (int16_t)w; r->h = (int16_t)h;
      r->kind = (uint8_t)kind;
   }
}

static void pix16_window(pix16_t *pix, int x, int y, int w, int h)
{
   pix16_record(pix, x, y, w, h, PIX16_RECT_WIN);
   pix16_round_fill(pix, x,     y,     w,     h,     2, pix->colors[PIX16_C_J_EDGE]);
   pix16_round_fill(pix, x + 1, y + 1, w - 2, h - 2, 2, pix->colors[PIX16_C_J_BORDER]);
   pix16_round_fill(pix, x + 3, y + 3, w - 6, h - 6, 1, pix->colors[PIX16_C_J_BORDER_LO]);
   pix16_vgradient(pix, x + 4, y + 4, w - 8, h - 8,
         pix->win_top, pix->win_bot);
}

/* White glove pointer, 13x9 */
static const char *pix16_glove[9] = {
   "....XXXX.....",
   "...XWWWWXXXX.",
   "XXXWWWWWWWWWX",
   "XWWWWWWXXXXX.",
   "XWWWWWWWWX...",
   "XWWWWWWXX....",
   "XSWWWWWWX....",
   ".XSSWWWX.....",
   "..XXXXX......"
};

/* Idle bounce: the glove nudges back and forth like an FF cursor */
static int pix16_glove_bounce(const pix16_t *pix)
{
   static const int8_t seq[8] = { 0, 1, 2, 2, 1, 0, 0, 0 };
   retro_time_t ms = menu_driver_get_current_time() / 1000;
   if (pix->reduced_motion)
      return 0;
   return seq[(ms / 90) % 8];
}

static void pix16_glove_cursor(pix16_t *pix, int x, int y)
{
   int gx, gy;
   x -= pix16_glove_bounce(pix);
   for (gy = 0; gy < 9; gy++)
      for (gx = 0; gx < 13; gx++)
      {
         char c = pix16_glove[gy][gx];
         if (c == 'X')
            pix16_fill(pix, x + gx, y + gy, 1, 1, pix->colors[PIX16_C_J_EDGE]);
         else if (c == 'W')
            pix16_fill(pix, x + gx, y + gy, 1, 1, pix->colors[PIX16_C_J_TEXT]);
         else if (c == 'S')
            pix16_fill(pix, x + gx, y + gy, 1, 1, pix->colors[PIX16_C_J_GLOVE_SHADE]);
      }
}

static size_t pix16_stack_depth(void);

#ifndef PIX16_GLOVE_MS
#define PIX16_GLOVE_MS 110   /* slide time between positions */
#endif

/* Which screen the glove is on: a slide only happens within one
 * screen, switching screens puts the glove straight in place */
static uint32_t pix16_glove_context(const pix16_t *pix, unsigned extra)
{
   uint32_t h = 2166136261u;
   const char *p;
   for (p = pix->title; *p; p++)
      h = (h ^ (unsigned char)*p) * 16777619u;
   return h ^ (extra * 2654435761u) ^ (uint32_t)pix16_stack_depth();
}

/* Advances the glove's slide from where it was to (x, y) and returns the
 * position it is at now */
static void pix16_glove_update(pix16_t *pix, int x, int y, uint32_t ctx,
      int *out_x, int *out_y)
{
   retro_time_t now = menu_driver_get_current_time();
   int cx = x, cy = y;

   if (!pix->glove_valid || ctx != pix->glove_ctx)
   {
      pix->glove_valid  = true;
      pix->glove_ctx    = ctx;
      pix->glove_from_x = pix->glove_to_x = x;
      pix->glove_from_y = pix->glove_to_y = y;
      pix->glove_start  = now - (retro_time_t)PIX16_GLOVE_MS * 1000;
   }
   else if (pix->reduced_motion)
   {
      /* no slide: the glove is already where it was asked to be */
      pix->glove_to_x   = pix->glove_from_x = x;
      pix->glove_to_y   = pix->glove_from_y = y;
      pix->glove_start  = now - (retro_time_t)PIX16_GLOVE_MS * 1000;
   }
   else
   {
      float t = (float)(now - pix->glove_start) / (PIX16_GLOVE_MS * 1000.0f);
      float e;
      if (t < 0.0f)
         t = 0.0f;
      if (t > 1.0f)
         t = 1.0f;
      e  = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); /* ease-out */
      cx = pix->glove_from_x + (int)((pix->glove_to_x - pix->glove_from_x) * e + (pix->glove_to_x >= pix->glove_from_x ? 0.5f : -0.5f));
      cy = pix->glove_from_y + (int)((pix->glove_to_y - pix->glove_from_y) * e + (pix->glove_to_y >= pix->glove_from_y ? 0.5f : -0.5f));

      if (x != pix->glove_to_x || y != pix->glove_to_y)
      {
         /* new target: start from wherever the glove is right now */
         pix->glove_from_x = cx;
         pix->glove_from_y = cy;
         pix->glove_to_x   = x;
         pix->glove_to_y   = y;
         pix->glove_start  = now;
      }
   }

   *out_x = cx;
   *out_y = cy;
}

/* Draws the glove, sliding it from where it was to (x, y) */
static void pix16_glove_at(pix16_t *pix, int x, int y, uint32_t ctx)
{
   int cx, cy;
   pix16_glove_update(pix, x, y, ctx, &cx, &cy);
   pix16_glove_cursor(pix, cx, cy);
}

/* assets/pix16[/sub]/file[ext], on the heap (path buffers are too big
 * for the stack). Falls back to the assets an AppImage bundles when the
 * user has none; NULL when nothing is found. Free it. */
static char *pix16_asset_path(const char *sub, const char *file,
      const char *ext)
{
   settings_t *settings = config_get_ptr();
   const char *appdir;
   char *path, *tmp;
   if (!(path = (char*)malloc(2 * PATH_MAX_LENGTH)))
      return NULL;
   tmp = path + PATH_MAX_LENGTH;
   if (settings && !string_is_empty(settings->paths.directory_assets))
   {
      fill_pathname_join_special(tmp, settings->paths.directory_assets,
            "pix16", PATH_MAX_LENGTH);
      if (sub)
      {
         fill_pathname_join_special(path, tmp, sub, PATH_MAX_LENGTH);
         strlcpy(tmp, path, PATH_MAX_LENGTH);
      }
      fill_pathname_join_special(path, tmp, file, PATH_MAX_LENGTH);
      if (ext)
         strlcat(path, ext, PATH_MAX_LENGTH);
      if (path_is_valid(path))
         return path;
   }
   /* an AppImage mounts its payload at $APPDIR */
   appdir = getenv("APPDIR");
   if (appdir && *appdir)
   {
      fill_pathname_join_special(tmp, appdir,
            "usr" PATH_DEFAULT_SLASH() "share" PATH_DEFAULT_SLASH()
            "retroarch" PATH_DEFAULT_SLASH() "assets" PATH_DEFAULT_SLASH()
            "pix16", PATH_MAX_LENGTH);
      if (sub)
      {
         fill_pathname_join_special(path, tmp, sub, PATH_MAX_LENGTH);
         strlcpy(tmp, path, PATH_MAX_LENGTH);
      }
      fill_pathname_join_special(path, tmp, file, PATH_MAX_LENGTH);
      if (ext)
         strlcat(path, ext, PATH_MAX_LENGTH);
      if (path_is_valid(path))
         return path;
   }
   free(path);
   return NULL;
}

/* Layout and look follow the settings, which can change at any time */
static void pix16_load_settings(pix16_t *pix)
{
   settings_t *settings = config_get_ptr();
   unsigned i;
   pix->wide_cfg = settings && settings->bools.menu_pix16_widescreen;
   pix->reduced_motion = settings && settings->bools.menu_pix16_reduced_motion;
   pix->start_enabled = settings && settings->bools.menu_pix16_start_screen;
   pix->screen_asked  = settings && settings->bools.menu_pix16_screen_asked;
   pix->buttons  = settings ? settings->uints.menu_pix16_button_style
         : PIX16_BUTTONS_PLAYSTATION;
   {
      unsigned pal = settings ? settings->uints.menu_pix16_palette
            : PIX16_PALETTE_SIXTEEN;
      if (pal != pix->palette)
      {
         pix->palette = pal;
         pix16_prepare_colors(pix);
         pix->scr_key_valid = false;
      }
      pix->transition = settings ? settings->uints.menu_pix16_transition
            : PIX16_TRANSITION_SWIRL;
      pix->background = settings ? settings->uints.menu_pix16_background
            : PIX16_BACKGROUND_DIAMONDS;
   }
   pix->keys     = true;
   for (i = 0; i < MAX_USERS; i++)
   {
      if (input_config_get_device_name(i))
      {
         pix->keys = false;
         break;
      }
   }
}

static void pix16_checkerboard(pix16_t *pix)
{
   int x, y;
   uint16_t a = pix->colors[PIX16_C_BG_A];
   uint16_t b = pix->colors[PIX16_C_BG_B];
   for (y = 0; y < PIX16_FB_HEIGHT; y++)
   {
      uint16_t *row = pix->fb + y * PIX16_FB_WIDTH;
      for (x = 0; x < PIX16_FB_WIDTH; x++)
         row[x] = (((x >> 4) ^ (y >> 4)) & 1) ? b : a;
   }
}

/* ------------------------------------------------------------------
 * Menu state helpers
 * ------------------------------------------------------------------ */

static file_list_t *pix16_menu_stack(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET(menu_list, 0) : NULL;
}

static file_list_t *pix16_selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

static size_t pix16_stack_depth(void)
{
   file_list_t *stack = pix16_menu_stack();
   return stack ? stack->size : 0;
}

/* Which tab the bottom of the menu stack belongs to */
static int pix16_root_tab(void)
{
   file_list_t *stack = pix16_menu_stack();
   const char *label;

   if (!stack || stack->size < 1)
      return PIX16_TAB_MAIN;

   label = stack->list[0].label;
   if (string_is_empty(label))
      return PIX16_TAB_MAIN;
   if (string_is_equal(label, MENU_ENUM_LABEL_SETTINGS_TAB_STR))
      return PIX16_TAB_SETUP;
   if (string_is_equal(label, MENU_ENUM_LABEL_PLAYLISTS_TAB_STR))
      return PIX16_TAB_LISTS;
#ifdef HAVE_NETWORKING
   if (string_is_equal(label, MENU_ENUM_LABEL_NETPLAY_TAB_STR))
      return PIX16_TAB_NET;
#endif
   return PIX16_TAB_MAIN;
}

static bool pix16_content_running(void)
{
   return    !retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)
         && (runloop_state_get_ptr()->flags & RUNLOOP_FLAG_CORE_RUNNING);
}

/* Same approach as MaterialUI/XMB: relabel the root of the stack,
 * then push the new top-level list in place. */
static int pix16_switch_tab(int target)
{
   file_list_t *stack         = pix16_menu_stack();
   file_list_t *selection_buf = pix16_selection_buf();
   size_t size;

   if (!stack || !selection_buf)
      return -1;

   if (stack->size > 1)
   {
      menu_entries_flush_stack(MENU_ENUM_LABEL_MAIN_MENU_STR, 0);
      filebrowser_clear_type();
      stack = pix16_menu_stack();
      if (!stack)
         return -1;
   }

   size = stack->size;
   if (size < 1)
      return -1;

   file_list_free_label(stack, size - 1);

   switch (target)
   {
      case PIX16_TAB_SETUP:
         stack->list[size - 1].label = strdup(MENU_ENUM_LABEL_SETTINGS_TAB_STR);
         stack->list[size - 1].type  = MENU_SETTINGS_TAB;
         break;
      case PIX16_TAB_LISTS:
         stack->list[size - 1].label = strdup(MENU_ENUM_LABEL_PLAYLISTS_TAB_STR);
         stack->list[size - 1].type  = MENU_PLAYLISTS_TAB;
         break;
#ifdef HAVE_NETWORKING
      case PIX16_TAB_NET:
         stack->list[size - 1].label = strdup(MENU_ENUM_LABEL_NETPLAY_TAB_STR);
         stack->list[size - 1].type  = MENU_NETPLAY_TAB;
         break;
#endif
      case PIX16_TAB_MAIN:
      default:
         stack->list[size - 1].label = strdup(MENU_ENUM_LABEL_MAIN_MENU_STR);
         stack->list[size - 1].type  = MENU_SETTINGS;
         break;
   }

   return menu_driver_deferred_push_content_list(selection_buf);
}

/* The playlists tab lists History/Favorites/Import... before the
 * systems: put the cursor on the first system instead */
static void pix16_grid_build(pix16_t *pix);

/* Does a History entry's db_name ("NES.lpl", "Capcom.lpl", or the full
 * database name) refer to this playlist file? Manual scans store the
 * name the playlist had when it was scanned, which is often shorter
 * than the file's name now. */
static void pix16_canon_name(char *base, size_t len);
static bool pix16_same_system(const char *file, const char *db)
{
   char a[NAME_MAX_LENGTH], b[NAME_MAX_LENGTH];
   const char *tail;
   if (string_is_empty(file) || string_is_empty(db))
      return false;
   if (string_is_equal(file, db))
      return true;
   strlcpy(a, file, sizeof(a));
   strlcpy(b, db,   sizeof(b));
   path_remove_extension(a);
   path_remove_extension(b);
   /* "Arcade - Capcom" vs "Capcom" */
   tail = strstr(a, " - ");
   while (tail && strstr(tail + 3, " - "))
      tail = strstr(tail + 3, " - ");
   if (tail && string_is_equal_noncase(tail + 3, b))
      return true;
   pix16_canon_name(a, sizeof(a));
   pix16_canon_name(b, sizeof(b));
   return string_is_equal(a, b);
}

static bool pix16_select_first_system(pix16_t *pix)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *list          = pix16_selection_buf();
   size_t i;

   if (!list)
      return false;

   /* Prefer the system of the last game played (from History) */
   if (g_defaults.content_history && playlist_size(g_defaults.content_history) > 0)
   {
      const struct playlist_entry *last = NULL;
      playlist_get_index(g_defaults.content_history, 0, &last);
      if (last && !string_is_empty(last->db_name))
      {
         for (i = 0; i < list->size; i++)
         {
            const char *p = list->list[i].path;
            if (     list->list[i].type == FILE_TYPE_PLAYLIST_COLLECTION
                  && p && pix16_same_system(path_basename(p), last->db_name))
            {
               menu_st->selection_ptr = i;
               menu_st->entries.begin = 0;
               return true;
            }
         }
      }
   }

   /* otherwise the first tile of the grid (NES when present) */
   pix16_grid_build(pix);
   if (pix->grid_count > 0)
   {
      menu_st->selection_ptr = pix->grid_map[0];
      menu_st->entries.begin = 0;
      return true;
   }
   return false;
}

/* Other names people give playlists (manual scans, renamed files),
 * mapped to the database name the systems table uses. Matched without
 * regard to case. */
static const char *pix16_aliases[][2] = {
   { "Nintendo - NES",                 "Nintendo - Nintendo Entertainment System" },
   { "NES",                            "Nintendo - Nintendo Entertainment System" },
   { "Nintendo Entertainment System",  "Nintendo - Nintendo Entertainment System" },
   { "Nintendo - Famicom",             "Nintendo - Nintendo Entertainment System" },
   { "Nintendo - FDS",                 "Nintendo - Family Computer Disk System" },
   { "Nintendo - SNES",                "Nintendo - Super Nintendo Entertainment System" },
   { "SNES",                           "Nintendo - Super Nintendo Entertainment System" },
   { "Super Nintendo",                 "Nintendo - Super Nintendo Entertainment System" },
   { "Nintendo - Super Famicom",       "Nintendo - Super Nintendo Entertainment System" },
   { "Nintendo - N64",                 "Nintendo - Nintendo 64" },
   { "N64",                            "Nintendo - Nintendo 64" },
   { "Nintendo 64",                    "Nintendo - Nintendo 64" },
   { "Nintendo - GB",                  "Nintendo - Game Boy" },
   { "Game Boy",                       "Nintendo - Game Boy" },
   { "Nintendo - GBC",                 "Nintendo - Game Boy Color" },
   { "Game Boy Color",                 "Nintendo - Game Boy Color" },
   { "Nintendo - GBA",                 "Nintendo - Game Boy Advance" },
   { "Game Boy Advance",               "Nintendo - Game Boy Advance" },
   { "Sega - Master System",           "Sega - Master System - Mark III" },
   { "Master System",                  "Sega - Master System - Mark III" },
   { "Sega - Genesis",                 "Sega - Mega Drive - Genesis" },
   { "Sega - Mega Drive",              "Sega - Mega Drive - Genesis" },
   { "Genesis",                        "Sega - Mega Drive - Genesis" },
   { "Mega Drive",                     "Sega - Mega Drive - Genesis" },
   { "Sega - CD",                      "Sega - Mega-CD - Sega CD" },
   { "Sega - Sega CD",                 "Sega - Mega-CD - Sega CD" },
   { "Sega - Mega-CD",                 "Sega - Mega-CD - Sega CD" },
   { "Sega CD",                        "Sega - Mega-CD - Sega CD" },
   { "Sega 32X",                       "Sega - 32X" },
   { "Saturn",                         "Sega - Saturn" },
   { "Dreamcast",                      "Sega - Dreamcast" },
   { "Game Gear",                      "Sega - Game Gear" },
   { "PlayStation",                    "Sony - PlayStation" },
   { "Sony - PSX",                     "Sony - PlayStation" },
   { "Sony - PS1",                     "Sony - PlayStation" },
   { "NEC - PC Engine",                "NEC - PC Engine - TurboGrafx 16" },
   { "NEC - TurboGrafx-16",            "NEC - PC Engine - TurboGrafx 16" },
   { "PC Engine",                      "NEC - PC Engine - TurboGrafx 16" },
   { "TurboGrafx-16",                  "NEC - PC Engine - TurboGrafx 16" },
   { "NEC - PC Engine CD",             "NEC - PC Engine CD - TurboGrafx-CD" },
   { "NEC - TurboGrafx-CD",            "NEC - PC Engine CD - TurboGrafx-CD" },
   { "PC Engine CD",                   "NEC - PC Engine CD - TurboGrafx-CD" },
   { "Neo Geo CD",                     "SNK - Neo Geo CD" },
   { "3DO - Interactive Multiplayer",  "The 3DO Company - 3DO" },
   { "3DO",                            "The 3DO Company - 3DO" },
   { "Arcade - FBNeo",                 "FBNeo - Arcade Games" },
   { "FinalBurn Neo",                  "FBNeo - Arcade Games" },
   { NULL, NULL }
};

/* Turns a playlist base name (no folder, no .lpl) into the name the
 * systems table and the icon files use */
static void pix16_canon_name(char *base, size_t len)
{
   size_t i;
   for (i = 0; pix16_aliases[i][0]; i++)
      if (string_is_equal_noncase(base, pix16_aliases[i][0]))
      {
         strlcpy(base, pix16_aliases[i][1], len);
         return;
      }
}

/* Playlist display name -> short name/colours */
static void pix16_system_lookup(pix16_t *pix, const char *name,
      char *short_out, size_t short_len, uint16_t *bg, uint16_t *fg);

static void pix16_system_lookup_raw(pix16_t *pix, const char *name,
      char *short_out, size_t short_len, uint16_t *bg, uint16_t *fg)
{
   size_t i;
   char base[NAME_MAX_LENGTH];
   const char *after;
   unsigned hash = 5381;

   strlcpy(base, name ? name : "", sizeof(base));
   path_remove_extension(base);
   pix16_canon_name(base, sizeof(base));

   for (i = 0; pix16_systems[i].playlist; i++)
   {
      if (string_is_equal(base, pix16_systems[i].playlist))
      {
         strlcpy(short_out, pix16_systems[i].short_name, short_len);
         *bg = pix->conv(pix16_systems[i].color);
         *fg = pix->conv(pix16_systems[i].fg);
         return;
      }
   }

   after = strrchr(base, '-');
   after = (after && after[1] == ' ') ? after + 2 : base;
   strlcpy(short_out, after, short_len);
   string_to_upper(short_out);

   for (i = 0; base[i]; i++)
      hash = ((hash << 5) + hash) + (unsigned char)base[i];
   i   = hash % (sizeof(pix16_fallback_colors) / sizeof(pix16_fallback_colors[0]));
   *bg = pix->conv(pix16_fallback_colors[i][0]);
   *fg = pix->conv(pix16_fallback_colors[i][1]);
}

/* Clean title for a playlist: the table's title, else the part after
 * the last " - " ("Capcom - CP System II" -> "CP System II") */
static void pix16_system_title(const char *playlist, char *out, size_t len)
{
   char base[NAME_MAX_LENGTH];
   const char *after;
   size_t i;

   strlcpy(base, playlist ? playlist : "", sizeof(base));
   path_remove_extension(base);
   pix16_canon_name(base, sizeof(base));
   for (i = 0; pix16_systems[i].playlist; i++)
      if (string_is_equal(base, pix16_systems[i].playlist))
      {
         strlcpy(out, pix16_systems[i].title, len);
         return;
      }
   after = strstr(base, " - ");
   while (after && strstr(after + 3, " - "))
      after = strstr(after + 3, " - ");
   strlcpy(out, after ? after + 3 : base, len);
}

static bool pix16_is_playlist_entry(const menu_entry_t *entry)
{
   return entry->type == FILE_TYPE_PLAYLIST_COLLECTION;
}

static void pix16_get_entry(pix16_t *pix, size_t i, bool want_sublabel)
{
   MENU_ENTRY_INITIALIZE(pix->entry);
   /* LABEL too: RetroArch's OK actions take it (a game's label is its
    * file path, which tells RetroArch which game is running) */
   pix->entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                    |  MENU_ENTRY_FLAG_LABEL_ENABLED
                    |  MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                    |  MENU_ENTRY_FLAG_VALUE_ENABLED;
   if (want_sublabel)
      pix->entry.flags |= MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
   menu_entry_get(&pix->entry, 0, (unsigned)i, NULL, true);
}

static const char *pix16_entry_label(const menu_entry_t *entry)
{
   return *entry->rich_label ? entry->rich_label : entry->path;
}

static const char *pix16_entry_value(const menu_entry_t *entry)
{
   if (entry->enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
      return entry->password_value;
   return entry->value;
}

/* Keep the selection on screen */
static size_t pix16_scroll(size_t selection, size_t count, size_t rows)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   size_t begin               = menu_st->entries.begin;

   if (rows == 0)
      return 0;
   if (selection < begin)
      begin = selection;
   else if (selection >= begin + rows)
      begin = selection - rows + 1;
   if (count <= rows)
      begin = 0;
   else if (begin > count - rows)
      begin = count - rows;

   menu_st->entries.begin = begin;
   return begin;
}

/* ------------------------------------------------------------------
 * Screen sections
 * ------------------------------------------------------------------ */

static void pix16_draw_clock(pix16_t *pix, int right_x, int y,
      uint16_t color)
{
   char buf[16];
   time_t now   = time(NULL);
   struct tm *t = localtime(&now);
   if (!t)
      return;
   strftime(buf, sizeof(buf), "%H:%M", t);
   pix16_text(pix, right_x - (int)strlen(buf) * PIX16_GLYPH_W, y,
         buf, color, 1, 0);
}

/* The options screen's top bar: every tab but LISTS (that is the
 * systems screen), then a BACK button */
static int pix16_opt_tabs(int *out)
{
   int i, n = 0;
   for (i = 0; i < PIX16_TAB_COUNT; i++)
      if (i != PIX16_TAB_LISTS)
         out[n++] = i;
   return n;
}

static int pix16_opt_pos(int tab)
{
   int order[PIX16_TAB_COUNT];
   int i, n = pix16_opt_tabs(order);
   for (i = 0; i < n; i++)
      if (order[i] == tab)
         return i;
   return 0;
}

static void pix16_draw_tab_bar(pix16_t *pix, int active)
{
   int i;
   int gap = 4;
   int w   = (PIX16_FB_WIDTH - 2 * PIX16_SAFE - gap * (PIX16_TAB_COUNT - 1))
         / PIX16_TAB_COUNT;

   for (i = 0; i < PIX16_TAB_COUNT; i++)
   {
      int x          = PIX16_SAFE + i * (w + gap);
      int y          = PIX16_SAFE;
      const char *tn = msg_hash_to_str(pix16_tabs[i].name);
      size_t len     = strlen(tn);
      int text_x;

      if (i == active)
      {
         pix16_panel(pix, x, y, w, PIX16_BAR_H, pix->tab_color[i]);
         text_x = x + (w - (int)len * PIX16_GLYPH_W) / 2;
         pix16_text(pix, text_x, y + 5, tn, pix->tab_fg[i], 1, 0);
      }
      else
      {
         pix16_panel(pix, x, y, w, PIX16_BAR_H, pix->colors[PIX16_C_PANEL]);
         /* colour "gem" so each tab keeps its identity */
         pix16_fill(pix, x + 4, y + 6, 6, 6, pix->tab_color[i]);
         text_x = x + 4 + 6 + (w - 10 - (int)len * PIX16_GLYPH_W) / 2;
         pix16_text(pix, text_x, y + 5, tn, pix->colors[PIX16_C_TEXT_DIM], 1, 0);
      }
   }
}

static void pix16_draw_title_bar(pix16_t *pix, const char *title,
      uint16_t bg, uint16_t fg, bool clock)
{
   int x         = PIX16_SAFE;
   int y         = PIX16_SAFE;
   int w         = PIX16_FB_WIDTH - 2 * PIX16_SAFE;
   size_t max_ch = (size_t)((w - 12 - (clock ? 48 : 0)) / PIX16_GLYPH_W);
   char buf[NAME_MAX_LENGTH];

   strlcpy(buf, title ? title : "", sizeof(buf));
   string_to_upper(buf);

   pix16_panel(pix, x, y, w, PIX16_BAR_H, bg);
   pix16_text_fit(pix, x + 6, y + 5, buf, fg, max_ch);
   if (clock)
      pix16_draw_clock(pix, x + w - 6, y + 5, fg);
}

static void pix16_draw_footer(pix16_t *pix, bool at_root)
{
   int y = PIX16_FB_HEIGHT - PIX16_SAFE - PIX16_FOOTER_H;
   (void)at_root;
   pix16_draw_guide(pix, PIX16_SAFE,
         PIX16_FB_WIDTH - PIX16_SAFE - 6 * PIX16_GLYPH_W - 4, y, false);
   pix16_draw_clock(pix, PIX16_FB_WIDTH - PIX16_SAFE, y + 1,
         pix->colors[PIX16_C_TEXT_DIM]);
}

/* Generic list: rows with a highlighted, tab-coloured selection */
static void pix16_draw_list(pix16_t *pix, int x, int y, int w, int h,
      uint16_t hl_bg, uint16_t hl_fg, bool systems_view)
{
   size_t i, begin, rows, count;
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *list          = pix16_selection_buf();
   size_t selection           = menu_st->selection_ptr;
   int inner_x                = x + 4;
   int inner_w                = w - 8;
   int row_y;

   pix16_panel(pix, x, y, w, h, pix->colors[PIX16_C_PANEL]);

   count = list ? list->size : 0;
   rows  = (size_t)((h - 8) / PIX16_ROW_H);

   if (count == 0)
   {
      pix16_text(pix, inner_x + 12, y + 4 + 4, msg_hash_to_str(MSG_PIX16_NO_ITEMS),
            pix->colors[PIX16_C_VALUE], 1, 0);
      return;
   }

   begin = pix16_scroll(selection, count, rows);
   row_y = y + 4;

   for (i = begin; i < count && i < begin + rows; i++, row_y += PIX16_ROW_H)
   {
      bool selected    = (i == selection);
      const char *label;
      const char *value;
      size_t value_len = 0;
      size_t max_label;
      uint16_t fg, val_fg, row_bg = hl_bg, row_fg = hl_fg;
      char short_name[64];

      pix16_get_entry(pix, i, false);
      label = pix16_entry_label(&pix->entry);
      value = pix16_entry_value(&pix->entry);

      if (systems_view && pix16_is_playlist_entry(&pix->entry))
      {
         pix16_system_lookup(pix, label, short_name, sizeof(short_name),
               &row_bg, &row_fg);
         label = short_name;
      }

      if (selected)
      {
         pix16_fill(pix, inner_x, row_y, inner_w, PIX16_ROW_H - 1, row_bg);
         pix16_cursor(pix, inner_x + 3, row_y + 4, row_fg);
         fg     = row_fg;
         val_fg = row_fg;
      }
      else
      {
         fg     = pix->colors[PIX16_C_TEXT_DIM];
         val_fg = pix->colors[PIX16_C_VALUE];
      }

      if (!string_is_empty(value) && !systems_view)
      {
         value_len = pix16_strlen_glyphs(value);
         if (value_len > 12)
            value_len = 12;
         pix16_text_fit(pix,
               inner_x + inner_w - 4 - (int)value_len * PIX16_GLYPH_W,
               row_y + 4, value, val_fg, value_len);
      }

      if (pix->entry.flags & MENU_ENTRY_FLAG_CHECKED)
         pix16_glyph(pix,
               inner_x + inner_w - 4 - (int)(value_len + 2) * PIX16_GLYPH_W,
               row_y + 4, '*', val_fg, 1);

      max_label = (size_t)((inner_w - 12 - 4
               - (value_len ? (int)(value_len + 1) * PIX16_GLYPH_W : 0))
               / PIX16_GLYPH_W);
      pix16_text_ticker(pix, inner_x + 12, row_y + 4, label, fg,
            max_label, selected);
   }

   /* scroll markers */
   if (begin > 0)
      pix16_text(pix, x + w - 10, y + 1, "^",
            pix->colors[PIX16_C_LAVENDER], 1, 0);
   if (begin + rows < count)
      pix16_text(pix, x + w - 10, y + h - 9, "v",
            pix->colors[PIX16_C_LAVENDER], 1, 0);
}

/* Word-wrapped text block; returns number of lines drawn */
static int pix16_wrap(pix16_t *pix, int x, int y, const char *s,
      uint16_t color, size_t cols, int max_lines, int line_h)
{
   char line[128];
   int lines = 0;

   if (cols >= sizeof(line))
      cols = sizeof(line) - 1;
   if (cols == 0)
      return 0;

   while (s && *s && lines < max_lines)
   {
      size_t n = 0, cut = 0;
      while (s[n] && s[n] != '\n' && n < cols)
      {
         if (s[n] == ' ')
            cut = n;
         n++;
      }
      if (s[n] && s[n] != '\n' && n == cols && cut > 0)
         n = cut;

      memcpy(line, s, n);
      line[n] = '\0';

      if (!pix)
         ; /* counting only */
      else if (lines == max_lines - 1 && s[n] && s[n] != '\n'
            && strlen(s + n) > 0)
         pix16_text_fit(pix, x, y, s, color, cols); /* last line: ".." */
      else
         pix16_text(pix, x, y, line, color, 1, 0);

      s += n;
      /* one newline ends the line; a second one is a blank line */
      while (*s == ' ')
         s++;
      if (*s == '\n')
         s++;
      y += line_h;
      lines++;
   }
   return lines;
}

static void pix16_draw_desc(pix16_t *pix, int x, int y, int w,
      const char *text)
{
   pix16_panel(pix, x, y, w, PIX16_DESC_H, pix->colors[PIX16_C_PAPER]);
   if (!string_is_empty(text))
      pix16_wrap(pix, x + 5, y + 3, text,
            pix->colors[PIX16_C_DARK_TEXT],
            (size_t)((w - 10) / PIX16_GLYPH_W), 2, 10);
}

static void pix16_art_free_all(pix16_t *pix)
{
   size_t i;
   for (i = 0; i < pix->art_count; i++)
   {
      free(pix->arts[i].name);
      free(pix->arts[i].px);
      free(pix->arts[i].mask);
   }
   free(pix->arts);
   pix->arts      = NULL;
   pix->art_count = 0;
}

/* Returns the (cached) art for a playlist, or NULL when there's no
 * file. Each file is tried once; results are kept until the video
 * driver changes. */
/* A Dot-Art icon (pixel art drawn at 32x32 and scaled up to 256x256)
 * back to its own pixels, trimmed to what is drawn and scaled by the
 * largest whole number that fits the tile's icon area */
#define PIX16_DOT 32
static bool pix16_art_from_dot_art(pix16_t *pix, struct pix16_art *art,
      const struct texture_image *ti)
{
   uint32_t *icon32;
   unsigned bw = ti->width / PIX16_DOT, bh = ti->height / PIX16_DOT;
   unsigned x, y, x0 = PIX16_DOT, y0 = PIX16_DOT, x1 = 0, y1 = 0, k, w, h;

   if (!bw || !bh)
      return false;
   if (!(icon32 = (uint32_t*)malloc(PIX16_DOT * PIX16_DOT * sizeof(uint32_t))))
      return false;
   for (y = 0; y < PIX16_DOT; y++)
      for (x = 0; x < PIX16_DOT; x++)
      {
         /* the block's average, alpha-weighted so edges keep their colour */
         uint32_t r = 0, g = 0, b = 0, a = 0, c;
         unsigned i, j;
         for (j = 0; j < bh; j++)
            for (i = 0; i < bw; i++)
            {
               c  = ti->pixels[(y * bh + j) * ti->width + x * bw + i];
               a += (c >> 24) & 0xFF;
               r += ((c >> 16) & 0xFF) * ((c >> 24) & 0xFF);
               g += ((c >>  8) & 0xFF) * ((c >> 24) & 0xFF);
               b += ( c        & 0xFF) * ((c >> 24) & 0xFF);
            }
         if (a / (bw * bh) < 110)
         {
            icon32[y * PIX16_DOT + x] = 0;
            continue;
         }
         icon32[y * PIX16_DOT + x] = 0xFF000000u
               | ((r / a) << 16) | ((g / a) << 8) | (b / a);
         if (x < x0)
            x0 = x;
         if (y < y0)
            y0 = y;
         if (x > x1)
            x1 = x;
         if (y > y1)
            y1 = y;
      }
   if (x1 < x0)
   {
      free(icon32);
      return false;
   }
   w = x1 - x0 + 1;
   h = y1 - y0 + 1;
   k = PIX16_ART_W / w < PIX16_ART_H / h ? PIX16_ART_W / w : PIX16_ART_H / h;
   if (!k)
      k = 1;
   art->w    = w * k;
   art->h    = h * k;
   art->px   = (uint16_t*)malloc(art->w * art->h * sizeof(uint16_t));
   art->mask = (uint8_t*)malloc(art->w * art->h);
   if (!art->px || !art->mask)
   {
      free(icon32);
      return false;
   }
   for (y = 0; y < art->h; y++)
      for (x = 0; x < art->w; x++)
      {
         uint32_t c = icon32[(y0 + y / k) * PIX16_DOT + x0 + x / k];
         art->mask[y * art->w + x] = c != 0;
         art->px[y * art->w + x]   = pix->conv(c | 0xFF000000u);
      }
   free(icon32);
   return true;
}

static const char *pix16_name_of(const char *playlist);
static void pix16_order_refresh(pix16_t *pix);
static const char *pix16_icon_of(const char *playlist);

static const struct pix16_art *pix16_art_get(pix16_t *pix,
      const char *playlist_base)
{
   char *path;
   const char *icon;
   struct texture_image ti;
   struct pix16_art *art, *grown;
   size_t i, n;
   bool ok;

   icon = pix16_icon_of(playlist_base);
   if (icon && icon[0])
      playlist_base = icon;

   for (i = 0; i < pix->art_count; i++)
      if (string_is_equal(pix->arts[i].name, playlist_base))
         return pix->arts[i].px ? &pix->arts[i] : NULL;

   grown = (struct pix16_art*)realloc(pix->arts,
         (pix->art_count + 1) * sizeof(*pix->arts));
   if (!grown)
      return NULL;
   pix->arts = grown;
   art       = &pix->arts[pix->art_count++];
   memset(art, 0, sizeof(*art));
   art->name = strdup(playlist_base);

   /* "@boot_logo": assets/pix16/boot_logo.png; a system: its icon in
    * the XMB Dot-Art theme */
   if (playlist_base[0] == '@')
      path = pix16_asset_path(NULL, playlist_base + 1, ".png");
   else
   {
      const char *appdir;
      settings_t *settings = config_get_ptr();
      if (!(path = (char*)malloc(PATH_MAX_LENGTH)))
         return NULL;
      path[0] = '\0';
      if (settings && !string_is_empty(settings->paths.directory_assets))
      {
         fill_pathname_join_special(path, settings->paths.directory_assets,
               "xmb" PATH_DEFAULT_SLASH() "dot-art" PATH_DEFAULT_SLASH() "png",
               PATH_MAX_LENGTH);
         fill_pathname_slash(path, PATH_MAX_LENGTH);
         strlcat(path, playlist_base, PATH_MAX_LENGTH);
         strlcat(path, ".png", PATH_MAX_LENGTH);
      }
      /* the icons an AppImage bundles, when the user has none */
      appdir = getenv("APPDIR");
      if (!path_is_valid(path) && appdir && *appdir)
      {
         fill_pathname_join_special(path, appdir,
               "usr" PATH_DEFAULT_SLASH() "share" PATH_DEFAULT_SLASH()
               "retroarch" PATH_DEFAULT_SLASH() "assets" PATH_DEFAULT_SLASH()
               "xmb" PATH_DEFAULT_SLASH() "dot-art" PATH_DEFAULT_SLASH()
               "png", PATH_MAX_LENGTH);
         fill_pathname_slash(path, PATH_MAX_LENGTH);
         strlcat(path, playlist_base, PATH_MAX_LENGTH);
         strlcat(path, ".png", PATH_MAX_LENGTH);
      }
   }
   if (!path)
      return NULL;

   memset(&ti, 0, sizeof(ti));
   ti.supports_rgba = false; /* ask for ARGB8888 */
   ok = !string_is_empty(path) && path_is_valid(path)
      && image_texture_load(&ti, path);
   free(path);
   if (!ok || !ti.pixels || ti.pix10 || ti.fp16)
   {
      image_texture_free(&ti);
      return NULL;
   }

   if (playlist_base[0] != '@')
      ok = pix16_art_from_dot_art(pix, art, &ti);
   else
   {
      n         = (size_t)ti.width * ti.height;
      art->px   = (uint16_t*)malloc(n * sizeof(uint16_t));
      art->mask = (uint8_t*)malloc(n);
      ok        = art->px && art->mask;
      if (ok)
      {
         for (i = 0; i < n; i++)
         {
            uint32_t c   = ti.pixels[i];
            art->mask[i] = ((c >> 24) & 0xFF) >= 0x80;
            art->px[i]   = pix->conv(c | 0xFF000000);
         }
         art->w = ti.width;
         art->h = ti.height;
      }
   }
   image_texture_free(&ti);
   if (!ok)
   {
      free(art->px);
      free(art->mask);
      art->px   = NULL;
      art->mask = NULL;
      return NULL;
   }
   return art;
}

/* Draws art centred in a box, nearest-neighbour. The art is stored at
 * the native 64x40 size, so a box of that size shows it 1:1; a bigger
 * tile box zooms it up, a smaller one scales it down. */
static void pix16_art_draw(pix16_t *pix, const struct pix16_art *art,
      int bx, int by, int bw, int bh)
{
   unsigned dw = art->w, dh = art->h;
   int zoom_w = (bw * 100) / PIX16_ART_W;
   int zoom_h = (bh * 100) / PIX16_ART_H;
   int zoom   = zoom_w < zoom_h ? zoom_w : zoom_h;
   int x, y, ox, oy;

   if (zoom < 1)
      zoom = 1;
   dw = (art->w * (unsigned)zoom + 50) / 100;
   dh = (art->h * (unsigned)zoom + 50) / 100;
   if ((int)dw > bw)
      dw = (unsigned)bw;
   if ((int)dh > bh)
      dh = (unsigned)bh;
   if (!dw || !dh)
      return;
   ox = bx + (bw - (int)dw) / 2;
   oy = by + (bh - (int)dh) / 2;

   for (y = 0; y < (int)dh; y++)
   {
      unsigned sy = (unsigned)y * art->h / dh;
      int py      = oy + y;
      if (py < pix->clip_y0 || py >= pix->clip_y1)
         continue;
      for (x = 0; x < (int)dw; x++)
      {
         unsigned sx = (unsigned)x * art->w / dw;
         size_t   si = (size_t)sy * art->w + sx;
         int px      = ox + x;
         if (px < pix->clip_x0 || px >= pix->clip_x1 || !art->mask[si])
            continue;
         pix->fb[py * PIX16_FB_WIDTH + px] = art->px[si];
      }
   }
}

/* ------------------------------------------------------------------
 * Systems grid
 * ------------------------------------------------------------------ */

/* Display order: systems (playlists) first, then History, Favorites
 * and the rest */
#define PIX16_TOP_NONE ((size_t)-1)
#define PIX16_BAR_FOCUS(p) ((p)->gear_focus || (p)->power_focus || (p)->add_focus || (p)->order_focus)

/* A rename from the actions window takes the place of the table name */
static void pix16_system_lookup(pix16_t *pix, const char *name,
      char *short_out, size_t short_len, uint16_t *bg, uint16_t *fg)
{
   char base[NAME_MAX_LENGTH];
   const char *renamed;

   pix16_system_lookup_raw(pix, name, short_out, short_len, bg, fg);
   strlcpy(base, name ? name : "", sizeof(base));
   path_remove_extension(base);
   pix16_canon_name(base, sizeof(base));
   renamed = pix16_name_of(base);
   if (renamed && renamed[0])
      strlcpy(short_out, renamed, short_len);
}

/* the first rows of pix16_systems are the arcade boards */
#define PIX16_ARCADE_ROWS 5
static int pix16_system_rank(const char *path);

/* Where each system's tile goes, as the + button set it up:
 * pix16_groups.txt in the config folder, one line per playlist,
 *    <playlist name> = HOME | ARCADE | <playlist it sits inside> */
#define PIX16_GROUPS_MAX 64
typedef struct
{
   char member[NAME_MAX_LENGTH];
   char host[NAME_MAX_LENGTH];
} pix16_group_t;
static pix16_group_t *pix16_groups;   /* PIX16_GROUPS_MAX entries */
static size_t pix16_groups_n;

static char *pix16_groups_file(void)
{
   settings_t *settings = config_get_ptr();
   const char *cfg      = path_get(RARCH_PATH_CONFIG);
   char *path           = (char*)malloc(2 * PATH_MAX_LENGTH);
   char *dir;
   if (!path)
      return NULL;
   dir = path + PATH_MAX_LENGTH;
   if (settings && !string_is_empty(settings->paths.directory_menu_config))
      strlcpy(dir, settings->paths.directory_menu_config, PATH_MAX_LENGTH);
   else if (!string_is_empty(cfg))
      fill_pathname_basedir(dir, cfg, PATH_MAX_LENGTH);
   else
   {
      free(path);
      return NULL;
   }
   fill_pathname_join_special(path, dir, "pix16_groups.txt", PATH_MAX_LENGTH);
   return path;
}

static void pix16_trim(char *s)
{
   size_t n = strlen(s);
   char *p  = s;
   while (n && (s[n - 1] == '\n' || s[n - 1] == '\r'
            || s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '"'))
      s[--n] = '\0';
   while (*p == ' ' || *p == '\t' || *p == '"')
      p++;
   if (p != s)
      memmove(s, p, strlen(p) + 1);
}

static void pix16_groups_load(void)
{
   char line[NAME_MAX_LENGTH * 2 + 8];
   char *path = pix16_groups_file();
   RFILE *f   = NULL;

   pix16_groups_n = 0;
   if (!pix16_groups && !(pix16_groups = (pix16_group_t*)
            calloc(PIX16_GROUPS_MAX, sizeof(*pix16_groups))))
   {
      free(path);
      return;
   }
   if (path && path_is_valid(path))
      f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
   free(path);
   if (!f)
      return;
   while (pix16_groups_n < PIX16_GROUPS_MAX
         && filestream_gets(f, line, sizeof(line)))
   {
      char *eq;
      pix16_trim(line);
      if (!line[0] || line[0] == '#' || !(eq = strstr(line, " = ")))
         continue;
      *eq = '\0';
      strlcpy(pix16_groups[pix16_groups_n].member, line,
            sizeof(pix16_groups[0].member));
      strlcpy(pix16_groups[pix16_groups_n].host, eq + 3,
            sizeof(pix16_groups[0].host));
      pix16_trim(pix16_groups[pix16_groups_n].member);
      pix16_trim(pix16_groups[pix16_groups_n].host);
      if (pix16_groups[pix16_groups_n].member[0])
         pix16_groups_n++;
   }
   filestream_close(f);
}

static bool pix16_groups_save(void)
{
   char *path = pix16_groups_file();
   RFILE *f;
   size_t i;

   if (!path)
      return false;
   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   free(path);
   if (!f)
      return false;
   filestream_printf(f,
         "# pix16: where each system goes on the systems screen.\n"
         "# Written by the + (Add System) button; safe to edit.\n"
         "#   <playlist name> = HOME | ARCADE | <playlist it sits inside>\n");
   for (i = 0; i < pix16_groups_n; i++)
      filestream_printf(f, "%s = %s\n",
            pix16_groups[i].member, pix16_groups[i].host);
   filestream_close(f);
   return true;
}

static void pix16_groups_set(const char *member, const char *host)
{
   size_t i;
   if (!pix16_groups)
      return;
   for (i = 0; i < pix16_groups_n; i++)
      if (string_is_equal_noncase(pix16_groups[i].member, member))
         break;
   if (i == pix16_groups_n)
   {
      if (pix16_groups_n >= PIX16_GROUPS_MAX)
         return;
      pix16_groups_n++;
   }
   strlcpy(pix16_groups[i].member, member, sizeof(pix16_groups[0].member));
   strlcpy(pix16_groups[i].host,   host,   sizeof(pix16_groups[0].host));
   pix16_groups_save();
}

/* "HOME", "ARCADE", a host playlist name, or NULL (no line for it) */
static const char *pix16_group_host_of(const char *path);


/* Renames and icon swaps set from the actions window, one line each:
 * pix16_names.txt / pix16_icons.txt,   <playlist name> = <value> */
#define PIX16_OVR_MAX 64
typedef struct
{
   char member[NAME_MAX_LENGTH];
   char value[NAME_MAX_LENGTH];
} pix16_ovr_t;

static pix16_ovr_t pix16_names[PIX16_OVR_MAX];
static size_t pix16_names_n;
static pix16_ovr_t pix16_icons[PIX16_OVR_MAX];
static size_t pix16_icons_n;
static bool pix16_ovr_loaded;

static char *pix16_cfg_file(const char *leaf)
{
   settings_t *settings = config_get_ptr();
   const char *cfg      = path_get(RARCH_PATH_CONFIG);
   char *path           = (char*)malloc(2 * PATH_MAX_LENGTH);
   char *dir;
   if (!path)
      return NULL;
   dir = path + PATH_MAX_LENGTH;
   if (settings && !string_is_empty(settings->paths.directory_menu_config))
      strlcpy(dir, settings->paths.directory_menu_config, PATH_MAX_LENGTH);
   else if (!string_is_empty(cfg))
      fill_pathname_basedir(dir, cfg, PATH_MAX_LENGTH);
   else
   {
      free(path);
      return NULL;
   }
   fill_pathname_join_special(path, dir, leaf, PATH_MAX_LENGTH);
   return path;
}

static void pix16_ovr_load(pix16_ovr_t *arr, size_t *n, const char *leaf)
{
   char line[NAME_MAX_LENGTH * 2];
   char *path = pix16_cfg_file(leaf);
   RFILE *f   = NULL;

   *n = 0;
   if (path && path_is_valid(path))
      f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
   free(path);
   if (!f)
      return;
   while (*n < PIX16_OVR_MAX && filestream_gets(f, line, sizeof(line)))
   {
      char *eq;
      pix16_trim(line);
      if (!line[0] || line[0] == '#')
         continue;
      eq = strchr(line, '=');
      if (!eq)
         continue;
      *eq = '\0';
      strlcpy(arr[*n].member, line, sizeof(arr[0].member));
      strlcpy(arr[*n].value,  eq + 1, sizeof(arr[0].value));
      pix16_trim(arr[*n].member);
      pix16_trim(arr[*n].value);
      if (arr[*n].member[0])
         (*n)++;
   }
   filestream_close(f);
}

static bool pix16_ovr_save(pix16_ovr_t *arr, size_t n, const char *leaf)
{
   char *path = pix16_cfg_file(leaf);
   RFILE *f;
   size_t i;

   if (!path)
      return false;
   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   free(path);
   if (!f)
      return false;
   filestream_printf(f, "# pix16: set from the arrange window; safe to edit.\n");
   for (i = 0; i < n; i++)
      filestream_printf(f, "%s = %s\n", arr[i].member, arr[i].value);
   filestream_close(f);
   return true;
}

static const char *pix16_ovr_get(pix16_ovr_t *arr, size_t n, const char *member)
{
   size_t i;
   if (!member || !member[0])
      return NULL;
   for (i = 0; i < n; i++)
      if (string_is_equal_noncase(arr[i].member, member))
         return arr[i].value;
   return NULL;
}

static void pix16_ovr_set(pix16_ovr_t *arr, size_t *n, const char *leaf,
      const char *member, const char *value)
{
   size_t i;
   if (!member || !member[0])
      return;
   for (i = 0; i < *n; i++)
      if (string_is_equal_noncase(arr[i].member, member))
         break;
   if (i == *n)
   {
      if (*n >= PIX16_OVR_MAX)
         return;
      (*n)++;
   }
   strlcpy(arr[i].member, member, sizeof(arr[0].member));
   strlcpy(arr[i].value, value ? value : "", sizeof(arr[0].value));
   if (!arr[i].value[0])
   {
      memmove(&arr[i], &arr[i + 1], (*n - i - 1) * sizeof(arr[0]));
      (*n)--;
   }
   pix16_ovr_save(arr, *n, leaf);
}

static void pix16_ovr_ready(void)
{
   if (pix16_ovr_loaded)
      return;
   pix16_ovr_loaded = true;
   pix16_ovr_load(pix16_names, &pix16_names_n, "pix16_names.txt");
   pix16_ovr_load(pix16_icons, &pix16_icons_n, "pix16_icons.txt");
}

/* The name a tile shows, if the user renamed it */
static const char *pix16_name_of(const char *playlist)
{
   pix16_ovr_ready();
   return pix16_ovr_get(pix16_names, pix16_names_n, playlist);
}

/* The dot-art icon a tile uses, if the user swapped it */
static const char *pix16_icon_of(const char *playlist)
{
   pix16_ovr_ready();
   return pix16_ovr_get(pix16_icons, pix16_icons_n, playlist);
}

static pix16_t *pix16_mg_owner;

/* RENAME... from the actions window */
static void pix16_mg_kb_cb(void *userdata, const char *str)
{
   pix16_t *pix     = pix16_mg_owner;
   const char *line = menu_input_dialog_get_buffer();
   char name[NAME_MAX_LENGTH];

   (void)userdata;
   (void)str;
   if (pix && line && *line)
   {
      strlcpy(name, line, sizeof(name));
      pix16_trim(name);
      if (name[0])
      {
         pix16_ovr_set(pix16_names, &pix16_names_n, "pix16_names.txt",
               pix->mg_member, name);
         pix->mg_open = false;
         pix16_order_refresh(pix);
         menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      }
   }
   menu_input_dialog_end();
}


/* Every icon the Dot-Art folder holds, sorted into groups for the
 * CHANGE ICON picker's MORE list */
enum { PIX16_CAT_CONSOLE = 0, PIX16_CAT_HANDHELD, PIX16_CAT_COMPUTER,
       PIX16_CAT_MISC, PIX16_CAT_COUNT };
#define PIX16_ICON_MAX 1200

static char pix16_icon_all[PIX16_ICON_MAX][64];
static unsigned char pix16_icon_all_cat[PIX16_ICON_MAX];
static size_t pix16_icon_all_n;
static bool pix16_icon_scanned;

static bool pix16_has_icase(const char *hay, const char *needle)
{
   char a[128], b[64];
   size_t i;
   strlcpy(a, hay ? hay : "", sizeof(a));
   strlcpy(b, needle ? needle : "", sizeof(b));
   for (i = 0; a[i]; i++)
      if (a[i] >= 'A' && a[i] <= 'Z')
         a[i] = (char)(a[i] + 32);
   for (i = 0; b[i]; i++)
      if (b[i] >= 'A' && b[i] <= 'Z')
         b[i] = (char)(b[i] + 32);
   return strstr(a, b) != NULL;
}

/* Icons that are menu furniture or derivatives rather than a system */
static bool pix16_icon_skip(const char *n)
{
   static const char *prefixes[] = {
      "input_", "menu_", "core-", "Source Code - ",
      "Non Redump - ", "Non-Redump - ", NULL };
   static const char *words[] = {
      "content", "tapes", "(beta)", "hack", "cursor", "pointer",
      "setting", "non game", NULL };
   size_t i;
   for (i = 0; prefixes[i]; i++)
      if (pix16_has_icase(n, prefixes[i]) && !strncmp(n, prefixes[i],
               strlen(prefixes[i])))
         return true;
   for (i = 0; words[i]; i++)
      if (pix16_has_icase(n, words[i]))
         return true;
   return false;
}

static void pix16_icon_scan(void)
{
   struct string_list *list;
   settings_t *settings = config_get_ptr();
   char dir[PATH_MAX_LENGTH];
   size_t i;

   if (pix16_icon_scanned)
      return;
   pix16_icon_scanned = true;
   if (!settings || string_is_empty(settings->paths.directory_assets))
      return;
   fill_pathname_join_special(dir, settings->paths.directory_assets,
         "xmb" PATH_DEFAULT_SLASH() "dot-art" PATH_DEFAULT_SLASH() "png",
         sizeof(dir));
   list = dir_list_new(dir, "png", false, false, false, false);
   if (!list)
      return;
   dir_list_sort(list, true);
   for (i = 0; i < list->size && pix16_icon_all_n < PIX16_ICON_MAX; i++)
   {
      const char *path = list->elems[i].data;
      char base[64];
      if (!path)
         continue;
      strlcpy(base, path_basename(path), sizeof(base));
      path_remove_extension(base);
      if (!base[0])
         continue;
      if (pix16_icon_skip(base))
         continue;
      /* keep "Maker - System" names only, not loose one-off entries */
      if (!strstr(base, " - "))
         continue;
      strlcpy(pix16_icon_all[pix16_icon_all_n], base, sizeof(pix16_icon_all[0]));
      pix16_icon_all_n++;
   }
   string_list_free(list);
}

/* rows and labels for whatever the picker is showing */
static size_t pix16_mg_pick_count(pix16_t *pix)
{
   if (pix->mg_icon_more)
   {
      pix16_icon_scan();
      return pix16_icon_all_n;
   }
   if (pix->mg_icon)
      return pix->mg_n + 1;              /* the popular list and MORE... */
   return pix->mg_n;
}

static const char *pix16_mg_pick_text(pix16_t *pix, size_t row)
{
   if (pix->mg_icon_more)
      return row < pix16_icon_all_n ? pix16_icon_all[row] : "";
   if (pix->mg_icon)
      return row == 0 ? "MORE..." : pix->mg_label[row - 1];
   return pix->mg_label[row];
}

/* Drop a system's line from pix16_groups.txt (its tile returns) */
static void pix16_groups_clear(const char *member)
{
   size_t i;
   if (!pix16_groups || !member || !member[0])
      return;
   for (i = 0; i < pix16_groups_n; i++)
   {
      if (!string_is_equal_noncase(pix16_groups[i].member, member))
         continue;
      memmove(&pix16_groups[i], &pix16_groups[i + 1],
            (pix16_groups_n - i - 1) * sizeof(pix16_groups[0]));
      pix16_groups_n--;
      pix16_groups_save();
      return;
   }
}

/* How many systems sit inside this one (0 means it is nobody's host) */
static size_t pix16_groups_member_count(const char *host)
{
   size_t i, n = 0;
   if (!pix16_groups || !host || !host[0])
      return 0;
   for (i = 0; i < pix16_groups_n; i++)
      if (     string_is_equal_noncase(pix16_groups[i].host, host)
            && !string_is_equal_noncase(pix16_groups[i].host, "HOME")
            && !string_is_equal_noncase(pix16_groups[i].host, "ARCADE")
            && !string_is_equal_noncase(pix16_groups[i].host, "HIDDEN"))
         n++;
   return n;
}

/* A system the manage window hid: its tile stays off the grid */
static bool pix16_is_hidden(const char *path)
{
   const char *host = pix16_group_host_of(path);
   return host && string_is_equal_noncase(host, "HIDDEN");
}

static const char *pix16_group_host_of(const char *path)
{
   char base[NAME_MAX_LENGTH];
   size_t i;
   if (!path || !pix16_groups_n)
      return NULL;
   strlcpy(base, path_basename(path), sizeof(base));
   if (string_is_equal_noncase(path_get_extension(base), "lpl"))
      path_remove_extension(base);
   for (i = 0; i < pix16_groups_n; i++)
      if (string_is_equal_noncase(pix16_groups[i].member, base))
         return pix16_groups[i].host;
   return NULL;
}

/* Arcade collections: FBNeo / MAME sets, Capcom CPS boards, Naomi,
 * Atomiswave, or anything with "Arcade" in its playlist name */
static bool pix16_is_arcade_builtin(const char *path)
{
   static const char *prefixes[] = {
      "FBNeo", "FB Alpha", "MAME", "HBMAME", "Capcom - CP System", "Atomiswave",
      "Sega - Naomi", "Sammy - Atomiswave", "Konami - ", "Cave - ",
      "Daphne", NULL
   };
   const char *base = path ? path_basename(path) : NULL;
   size_t i;
   int rank;
   if (!base)
      return false;
   /* Neo Geo (MVS/AES carts) lives in the ARCADE list */
   {
      char nb[NAME_MAX_LENGTH];
      strlcpy(nb, base, sizeof(nb));
      path_remove_extension(nb);
      if (     string_is_equal_noncase(nb, "SNK - Neo Geo")
            || string_is_equal_noncase(nb, "Neo Geo"))
         return true;
   }
   /* a playlist that names a console gets its own tile */
   rank = pix16_system_rank(path);
   if (rank >= PIX16_ARCADE_ROWS)
      return false;
   if (rank >= 0)
      return true;
   for (i = 0; prefixes[i]; i++)
      if (!strncmp(base, prefixes[i], strlen(prefixes[i])))
         return true;
   return strstr(base, "Arcade") != NULL;
}

/* pix16_groups.txt first, then the built-in rules */
static bool pix16_is_arcade_playlist(const char *path)
{
   const char *host = pix16_group_host_of(path);
   if (host)
      return string_is_equal_noncase(host, "ARCADE");
   return pix16_is_arcade_builtin(path);
}

static bool pix16_arcade_grouped(const pix16_t *pix)
{
   return pix->arcade_count >= 2;
}

/* PC Engine + PC Engine CD share one tile when both are there */
static bool pix16_pce_grouped(const pix16_t *pix)
{
   return pix->pce_cart != PIX16_TOP_NONE && pix->pce_cd != PIX16_TOP_NONE;
}

/* Rows of the picker window: list indices and their labels */
static size_t pix16_picker_items(const pix16_t *pix, size_t *idx, size_t max)
{
   size_t k, n = 0;
   if (pix->picker_pce || pix->picker_host != PIX16_TOP_NONE)
   {
      size_t host = pix->picker_pce ? pix->pce_cart : pix->picker_host;
      if (max >= 2 && pix->picker_pce)
      {
         idx[n++] = pix->pce_cart;
         idx[n++] = pix->pce_cd;
      }
      else if (max >= 1)
         idx[n++] = host;
      for (k = 0; k < pix->sub_count && n < max; k++)
         if (pix->sub_host[k] == host)
            idx[n++] = pix->sub_idx[k];
      return n;
   }
   for (k = 0; k < pix->arcade_count && n < max; k++)
      idx[n++] = pix->arcade_idx[k];
   return n;
}

/* Optional order: pix16_order.txt next to pix16_groups.txt, one playlist
 * name per line. Applied once per grid build, never in the sort. */
#define PIX16_ORDER_MAX 64
static char pix16_order[PIX16_ORDER_MAX][NAME_MAX_LENGTH];
static size_t pix16_order_n;
static bool pix16_order_tried;

static char *pix16_order_file(void)
{
   settings_t *settings = config_get_ptr();
   const char *cfg      = path_get(RARCH_PATH_CONFIG);
   char *path           = (char*)malloc(2 * PATH_MAX_LENGTH);
   char *dir;

   if (!path)
      return NULL;
   dir = path + PATH_MAX_LENGTH;
   if (settings && !string_is_empty(settings->paths.directory_menu_config))
      strlcpy(dir, settings->paths.directory_menu_config, PATH_MAX_LENGTH);
   else if (!string_is_empty(cfg))
      fill_pathname_basedir(dir, cfg, PATH_MAX_LENGTH);
   else
   {
      free(path);
      return NULL;
   }
   fill_pathname_join_special(path, dir, "pix16_order.txt", PATH_MAX_LENGTH);
   return path;
}

static void pix16_order_base(const char *path, char *out, size_t len)
{
   const char *b = path ? path_basename(path) : NULL;
   out[0] = '\0';
   if (!b)
      return;
   strlcpy(out, b, len);
   path_remove_extension(out);
}

static void pix16_order_load(void)
{
   char line[NAME_MAX_LENGTH];
   char *path;
   RFILE *f = NULL;

   pix16_order_tried = true;
   pix16_order_n     = 0;
   path              = pix16_order_file();
   if (path && path_is_valid(path))
      f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
   free(path);
   if (!f)
      return;
   while (pix16_order_n < PIX16_ORDER_MAX
         && filestream_gets(f, line, sizeof(line)))
   {
      pix16_trim(line);
      if (!line[0] || line[0] == '#')
         continue;
      strlcpy(pix16_order[pix16_order_n], line, sizeof(pix16_order[0]));
      pix16_order_n++;
   }
   filestream_close(f);
}

static bool pix16_order_save(void)
{
   char *path = pix16_order_file();
   RFILE *f;
   size_t i;

   if (!path)
      return false;
   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   free(path);
   if (!f)
      return false;
   filestream_printf(f, "# pix16: the order of the systems screen.\n");
   for (i = 0; i < pix16_order_n; i++)
      filestream_printf(f, "%s\n", pix16_order[i]);
   filestream_close(f);
   return true;
}

/* One pass over the built grid: named systems first, in file order */
static void pix16_order_apply(pix16_t *pix)
{
   file_list_t *list;
   size_t *tmp;
   size_t i, k, w = 0;

   if (!pix16_order_tried)
      pix16_order_load();
   if (!pix16_order_n || pix->grid_count < 2)
      return;
   list = pix16_selection_buf();
   if (!list)
      return;
   tmp = (size_t*)malloc(pix->grid_count * sizeof(size_t));
   if (!tmp)
      return;

   for (k = 0; k < pix16_order_n; k++)
      for (i = 0; i < pix->grid_count; i++)
      {
         char base[NAME_MAX_LENGTH];
         pix16_order_base(list->list[pix->grid_map[i]].path, base,
               sizeof(base));
         if (string_is_equal_noncase(base, pix16_order[k]))
         {
            size_t j;
            bool done = false;
            for (j = 0; j < w; j++)
               if (tmp[j] == pix->grid_map[i])
               {
                  done = true;
                  break;
               }
            if (!done)
               tmp[w++] = pix->grid_map[i];
         }
      }
   for (i = 0; i < pix->grid_count; i++)
   {
      size_t j;
      bool done = false;
      for (j = 0; j < w; j++)
         if (tmp[j] == pix->grid_map[i])
         {
            done = true;
            break;
         }
      if (!done)
         tmp[w++] = pix->grid_map[i];
   }
   memcpy(pix->grid_map, tmp, pix->grid_count * sizeof(size_t));
   free(tmp);
}

static void pix16_grid_sort(pix16_t *pix);
static bool pix16_is_grid_view(void);

/* the tile a grouped playlist sits inside, or PIX16_TOP_NONE */
static size_t pix16_sub_host(const pix16_t *pix, size_t idx)
{
   size_t k;
   for (k = 0; k < pix->sub_count; k++)
      if (pix->sub_idx[k] == idx)
         return pix->sub_host[k];
   return PIX16_TOP_NONE;
}

static bool pix16_has_subs(const pix16_t *pix, size_t host)
{
   size_t k;
   for (k = 0; k < pix->sub_count; k++)
      if (pix->sub_host[k] == host)
         return true;
   return false;
}

/* List index of the playlist whose name matches this system, or
 * PIX16_TOP_NONE */
static size_t pix16_playlist_index(const pix16_t *pix, const char *name)
{
   file_list_t *list = pix16_selection_buf();
   size_t i;
   if (!list)
      return PIX16_TOP_NONE;
   for (i = 0; i < list->size; i++)
   {
      char base[NAME_MAX_LENGTH];
      if (     list->list[i].type != FILE_TYPE_PLAYLIST_COLLECTION
            || !list->list[i].path)
         continue;
      strlcpy(base, path_basename(list->list[i].path), sizeof(base));
      path_remove_extension(base);
      pix16_canon_name(base, sizeof(base));
      if (string_is_equal_noncase(base, name))
         return i;
   }
   return PIX16_TOP_NONE;
}

static void pix16_grid_build(pix16_t *pix)
{
   file_list_t *list = pix16_selection_buf();
   size_t i, n = list ? list->size : 0;

   pix->grid_count   = 0;
   pix->arcade_count = 0;
   pix->pce_cart     = PIX16_TOP_NONE;
   pix->pce_cd       = PIX16_TOP_NONE;
   pix->top_idx[0]   = PIX16_TOP_NONE; /* History   */
   pix->top_idx[1]   = PIX16_TOP_NONE; /* Favorites */
   if (n > pix->grid_cap)
   {
      size_t *m = (size_t*)realloc(pix->grid_map, n * sizeof(size_t));
      if (!m)
         return;
      pix->grid_map = m;
      pix->grid_cap = n;
   }
   if (n > pix->arcade_cap)
   {
      size_t *m = (size_t*)realloc(pix->arcade_idx, n * sizeof(size_t));
      if (!m)
         return;
      pix->arcade_idx = m;
      pix->arcade_cap = n;
   }

   if (n > pix->sub_cap)
   {
      size_t *a = (size_t*)realloc(pix->sub_idx,  n * sizeof(size_t));
      size_t *b = a ? (size_t*)realloc(pix->sub_host, n * sizeof(size_t)) : NULL;
      if (a)
         pix->sub_idx  = a;
      if (b)
         pix->sub_host = b;
      if (!a || !b)
         return;
      pix->sub_cap = n;
   }
   pix->sub_count = 0;

   /* pix16_groups.txt: playlists shown inside another system's tile */
   for (i = 0; i < n && pix16_groups_n; i++)
   {
      const char *host;
      size_t j;
      if (list->list[i].type != FILE_TYPE_PLAYLIST_COLLECTION)
         continue;
      host = pix16_group_host_of(list->list[i].path);
      if (     !host || string_is_equal_noncase(host, "HOME")
            || string_is_equal_noncase(host, "ARCADE")
            || string_is_equal_noncase(host, "HIDDEN"))
         continue;
      for (j = 0; j < n; j++)
      {
         char base[NAME_MAX_LENGTH];
         const char *hh;
         if (j == i || list->list[j].type != FILE_TYPE_PLAYLIST_COLLECTION
               || !list->list[j].path)
            continue;
         strlcpy(base, path_basename(list->list[j].path), sizeof(base));
         path_remove_extension(base);
         if (!string_is_equal_noncase(base, host))
            continue;
         /* one level only: the host must be a tile of its own */
         hh = pix16_group_host_of(list->list[j].path);
         if (hh && !string_is_equal_noncase(hh, "HOME"))
            break;
         if (pix16_is_arcade_playlist(list->list[j].path))
            break;
         pix->sub_idx[pix->sub_count]  = i;
         pix->sub_host[pix->sub_count] = j;
         pix->sub_count++;
         break;
      }
   }
   /* ARCADE: two or more arcade playlists share one tile; the first in
    * list order stands in for the group */
   pix->arcade_rep = PIX16_TOP_NONE;
   for (i = 0; i < n; i++)
   {
      if (list->list[i].type != FILE_TYPE_PLAYLIST_COLLECTION)
         continue;
      if (pix16_sub_host(pix, i) != PIX16_TOP_NONE)
         continue;
      if (pix16_is_arcade_playlist(list->list[i].path))
         pix->arcade_idx[pix->arcade_count++] = i;
   }
   if (pix->arcade_count >= 2)
      pix->arcade_rep = pix->arcade_idx[0];
   else
      pix->arcade_count = 0;

   /* PC Engine + PC Engine CD share one tile when both are there */
   pix->pce_cart = pix16_playlist_index(pix,
         "NEC - PC Engine - TurboGrafx 16");
   pix->pce_cd   = pix16_playlist_index(pix,
         "NEC - PC Engine CD - TurboGrafx-CD");
   if (pix->pce_cart == PIX16_TOP_NONE || pix->pce_cd == PIX16_TOP_NONE)
      pix->pce_cart = pix->pce_cd = PIX16_TOP_NONE;

   for (i = 0; i < n; i++)
   {
      const char *label = list->list[i].label;
      if (list->list[i].type == FILE_TYPE_PLAYLIST_COLLECTION)
      {
         if (pix16_is_hidden(list->list[i].path))
            ;  /* hidden by the manage window */
         else if (pix16_sub_host(pix, i) != PIX16_TOP_NONE)
            ;  /* shown inside its host's tile */
         else if (     pix->arcade_rep != PIX16_TOP_NONE
                    && i != pix->arcade_rep
                    && pix16_is_arcade_playlist(list->list[i].path))
            ;  /* shown inside the ARCADE tile */
         else if (pix16_pce_grouped(pix) && i == pix->pce_cd)
            ;  /* shown inside the PC Engine tile */
         else
            pix->grid_map[pix->grid_count++] = i;
      }
      else if (label && string_is_equal(label, MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY_STR))
         pix->top_idx[0] = i;
      else if (label && string_is_equal(label, MENU_ENUM_LABEL_GOTO_FAVORITES_STR))
         pix->top_idx[1] = i;
      /* everything else (Images, Import, Explore...) is left out */
   }

   /* HISTORY / FAVORITES are always on the systems screen, even when
    * RetroArch's "show History/Favorites" settings hide them from the
    * playlists list: add the missing entries ourselves */
   if (     list
         && (pix->top_idx[0] == PIX16_TOP_NONE || pix->top_idx[1] == PIX16_TOP_NONE)
         && pix16_is_grid_view())
   {
      settings_t *settings = config_get_ptr();
      if (pix->top_idx[0] == PIX16_TOP_NONE
            && menu_entries_append(list,
               settings->paths.path_content_history,
               MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY_STR,
               MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY,
               MENU_SETTING_ACTION, 0, 0, NULL))
         pix->top_idx[0] = list->size - 1;
      if (pix->top_idx[1] == PIX16_TOP_NONE
            && menu_entries_append(list,
               settings->paths.path_content_favorites,
               MENU_ENUM_LABEL_GOTO_FAVORITES_STR,
               MENU_ENUM_LABEL_GOTO_FAVORITES,
               MENU_SETTING_ACTION, 0, 0, NULL))
         pix->top_idx[1] = list->size - 1;
   }

   pix16_grid_sort(pix);
   pix16_order_apply(pix);
}

/* Position of a playlist in the systems table (grid order), or -1 */
static int pix16_system_rank(const char *path)
{
   char base[NAME_MAX_LENGTH];
   int i;
   if (!path)
      return -1;
   strlcpy(base, path_basename(path), sizeof(base));
   path_remove_extension(base);
   pix16_canon_name(base, sizeof(base));
   for (i = 0; pix16_systems[i].playlist; i++)
      if (string_is_equal(base, pix16_systems[i].playlist))
         return i;
   return -1;
}

/* Grid order: the ARCADE tile, then known systems by maker, consoles
 * before handhelds, oldest first; then unknown playlists in RetroArch's
 * (alphabetical) order */
static void pix16_grid_sort(pix16_t *pix)
{
   file_list_t *list = pix16_selection_buf();
   size_t i, j;
   if (!list)
      return;
   for (i = 1; i < pix->grid_count; i++)
   {
      size_t v   = pix->grid_map[i];
      long   key = (pix16_arcade_grouped(pix) && v == pix->arcade_rep)
                 ? -1L
                 : (pix16_system_rank(list->list[v].path) >= 0
                    ? (long)pix16_system_rank(list->list[v].path)
                    : 200000L + (long)v);
      j = i;
      while (j > 0)
      {
         size_t u    = pix->grid_map[j - 1];
         long   ukey = (pix16_arcade_grouped(pix) && u == pix->arcade_rep)
                     ? -1L
                     : (pix16_system_rank(list->list[u].path) >= 0
                        ? (long)pix16_system_rank(list->list[u].path)
                        : 200000L + (long)u);
         if (ukey <= key)
            break;
         pix->grid_map[j] = u;
         j--;
      }
      pix->grid_map[j] = v;
   }
}

/* The grid entry a list index is shown as (any arcade playlist is
 * shown as the ARCADE tile) */
static size_t pix16_canon(const pix16_t *pix, size_t idx)
{
   size_t k;
   if (pix16_sub_host(pix, idx) != PIX16_TOP_NONE)
      idx = pix16_sub_host(pix, idx);
   if (pix16_pce_grouped(pix) && idx == pix->pce_cd)
      return pix->pce_cart;
   if (!pix16_arcade_grouped(pix))
      return idx;
   for (k = 0; k < pix->arcade_count; k++)
      if (pix->arcade_idx[k] == idx)
         return pix->arcade_rep;
   return idx;
}

/* Which top button (0/1) the selection is on, or -1 */
static int pix16_top_focus(pix16_t *pix, size_t sel)
{
   if (pix->top_idx[0] != PIX16_TOP_NONE && sel == pix->top_idx[0])
      return 0;
   if (pix->top_idx[1] != PIX16_TOP_NONE && sel == pix->top_idx[1])
      return 1;
   return -1;
}

static bool pix16_grid_has(pix16_t *pix, size_t list_idx)
{
   size_t d;
   list_idx = pix16_canon(pix, list_idx);
   for (d = 0; d < pix->grid_count; d++)
      if (pix->grid_map[d] == list_idx)
         return true;
   return false;
}

static size_t pix16_grid_pos(pix16_t *pix, size_t list_idx)
{
   size_t d;
   list_idx = pix16_canon(pix, list_idx);
   for (d = 0; d < pix->grid_count; d++)
      if (pix->grid_map[d] == list_idx)
         return d;
   return 0;
}

static bool pix16_is_grid_view(void)
{
   return    pix16_stack_depth() == 1
          && pix16_root_tab() == PIX16_TAB_LISTS
          && !menu_is_running_quick_menu();
}

#define PIX16_GRID_SLOTS    12   /* systems per page, whatever the layout */
#define PIX16_GRID_PER_PAGE PIX16_GRID_SLOTS
#define PIX16_GRID_W        pix16_grid_w
#define PIX16_PAGE_STRIDE   (PIX16_GRID_W + 24)
#ifndef PIX16_PAGE_MS
#define PIX16_PAGE_MS 220    /* page slide time */
#endif

/* Info for drawing one grid tile */
typedef struct
{
   const struct pix16_art *art;
   uint16_t sys_bg, sys_fg;
   char name[64];
   bool is_sys;
} pix16_tile_info_t;

static void pix16_tile_info(pix16_t *pix, size_t list_idx,
      pix16_tile_info_t *t)
{
   const char *label;
   char base[NAME_MAX_LENGTH];

   pix16_get_entry(pix, list_idx, false);
   label     = pix16_entry_label(&pix->entry);
   t->is_sys = pix16_is_playlist_entry(&pix->entry);
   t->art    = NULL;

   if (pix16_arcade_grouped(pix) && list_idx == pix->arcade_rep)
   {
      strlcpy(t->name, msg_hash_to_str(MSG_PIX16_ARCADE), sizeof(t->name));
      t->sys_bg = pix->conv(0xFFF08A2A);
      t->sys_fg = pix->conv(0xFF1D1640);
      if (!(t->art = pix16_art_get(pix, "Arcade")))
         t->art = pix16_art_get(pix, "FBNeo - Arcade Games");
      return;
   }

   if (t->is_sys)
   {
      pix16_system_lookup(pix, label, t->name, sizeof(t->name),
            &t->sys_bg, &t->sys_fg);
      strlcpy(base, label, sizeof(base));
      path_remove_extension(base);
      pix16_canon_name(base, sizeof(base));
      t->art = pix16_art_get(pix, base);
   }
   else
   {
      strlcpy(t->name, label, sizeof(t->name));
      string_to_upper(t->name);
      t->sys_bg = pix->colors[PIX16_C_LAVENDER];
      t->sys_fg = pix->colors[PIX16_C_DARK_TEXT];
   }
}

/* Icon area of a tile: the art, or the name / initial in big letters */
static void pix16_tile_icon(pix16_t *pix, const pix16_tile_info_t *t,
      int tx, int icon_y, int tw, uint16_t text_c, uint16_t initial_c)
{
   char initial[2];
   const char *big = t->name;
   size_t len;
   int scale;
   int icon_w = tw - 7;

   if (t->art)
   {
      pix16_art_draw(pix, t->art, tx + (tw - icon_w) / 2,
            icon_y, icon_w, PIX16_ICON_H);
      return;
   }

   initial[0] = t->name[0];
   initial[1] = '\0';
   if (!t->is_sys)
      big = initial;
   len   = pix16_strlen_glyphs(big);
   scale = t->is_sys
         ? (((int)len * PIX16_GLYPH_W * 2 <= tw - 12) ? 2 : 1)
         : 3;
   if ((int)len * PIX16_GLYPH_W * scale > tw - 10)
      len = (size_t)((tw - 10) / (PIX16_GLYPH_W * scale));
   pix16_text(pix,
         tx + (tw - (int)len * PIX16_GLYPH_W * scale) / 2,
         icon_y + (PIX16_ICON_H - PIX16_GLYPH_H * scale) / 2,
         big, t->is_sys ? text_c : initial_c, scale, len);
}

static void pix16_tile_label(pix16_t *pix, const pix16_tile_info_t *t,
      int tx, int y, int tw, uint16_t color, bool selected)
{
   size_t len = pix16_strlen_glyphs(t->name);
   if (len > (size_t)(tw / PIX16_GLYPH_W))
      len = (size_t)(tw / PIX16_GLYPH_W);
   pix16_text_ticker(pix, tx + (tw - (int)len * PIX16_GLYPH_W) / 2,
         y, t->name, color, (size_t)(tw / PIX16_GLYPH_W), selected);
}

/* Classic theme tile */
static void pix16_grid_tile(pix16_t *pix, size_t list_idx, int tx, int ty,
      int tw, bool selected)
{
   pix16_tile_info_t t;
   pix16_tile_info(pix, list_idx, &t);

   if (selected)
   {
      pix16_fill(pix, tx - 1, ty - 1, tw + 2, PIX16_TILE_H + 2,
            pix->colors[PIX16_C_TEXT]);
      pix16_panel(pix, tx, ty, tw, PIX16_TILE_H, t.sys_bg);
   }
   else
   {
      pix16_panel(pix, tx, ty, tw, PIX16_TILE_H,
            pix->colors[PIX16_C_PANEL]);
      if (!t.art)
         pix16_fill(pix, tx + 6, ty + 4, tw - 12,
               PIX16_ICON_H - 4, t.sys_bg);
   }
   pix16_tile_icon(pix, &t, tx, ty + 2, tw, t.sys_fg, t.sys_fg);
   pix16_tile_label(pix, &t, tx,
         ty + PIX16_TILE_H - PIX16_LABEL_PAD - PIX16_GLYPH_H, tw,
         selected ? t.sys_fg : pix->colors[PIX16_C_TEXT_DIM], selected);
}

/* Current horizontal scroll of the page strip, sliding towards page */
static int pix16_grid_scroll(pix16_t *pix, size_t page)
{
   retro_time_t now = menu_driver_get_current_time();
   uint32_t ctx     = pix16_glove_context(pix, 7);
   int target       = (int)page * PIX16_PAGE_STRIDE;
   int cur          = target;

   if (!pix->grid_valid || ctx != pix->grid_ctx)
   {
      pix->grid_valid   = true;
      pix->grid_ctx     = ctx;
      pix->grid_from_px = pix->grid_to_px = target;
      pix->grid_start   = now - (retro_time_t)PIX16_PAGE_MS * 1000;
      return target;
   }
   else if (pix->reduced_motion)
   {
      pix->grid_from_px = pix->grid_to_px = target;
      pix->grid_start   = now - (retro_time_t)PIX16_PAGE_MS * 1000;
      return target;
   }
   else
   {
      float t = (float)(now - pix->grid_start) / (PIX16_PAGE_MS * 1000.0f);
      float e;
      if (t < 0.0f)
         t = 0.0f;
      if (t > 1.0f)
         t = 1.0f;
      e   = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
      cur = pix->grid_from_px + (int)((pix->grid_to_px - pix->grid_from_px) * e);
      if (t >= 1.0f)
         cur = pix->grid_to_px;
   }

   if (target != pix->grid_to_px)
   {
      pix->grid_from_px = cur;
      pix->grid_to_px   = target;
      pix->grid_start   = now;
   }
   return cur;
}

static void pix16_j_grid_tile(pix16_t *pix, size_t list_idx, int tx,
      int ty, int tw, bool selected);

/* Tile floor: the standard 71x54 tile, and the aspect band the layout
 * stays inside. */
#define PIX16_TILE_MIN_W      71
#define PIX16_TILE_MIN_H      54
#define PIX16_TILE_MAX_ROWS   3
#define PIX16_TILE_VGAP       2
#define PIX16_TILE_TARGET     1.35f   /* the shape a tile wants to be */

/* Works out the rows and tile sizes for count systems so the page is
 * filled exactly: every row is full, and the last row stretches its
 * tiles across the whole width, so no slot is ever left empty. Fewer
 * systems means fewer, bigger tiles.
 *
 * The rows are [cols, cols, ..., last], so a tile's row and column are
 * still (index / cols, index % cols) - the grid's navigation maths does
 * not have to know about the justified last row. */
static void pix16_grid_layout(pix16_t *pix, int avail_w, int avail_h,
      size_t count)
{
   int maxr = (avail_h + PIX16_TILE_VGAP)
            / (PIX16_TILE_MIN_H + PIX16_TILE_VGAP);
   int best_c = 0, best_r = 0, best_last = 0, best_th = 0;
   float best = -1.0f;
   int rows;

   if (maxr > PIX16_TILE_MAX_ROWS)
      maxr = PIX16_TILE_MAX_ROWS;
   if (maxr < 1)
      maxr = 1;

   for (rows = 1; rows <= maxr && rows <= (int)count; rows++)
   {
      int c    = (int)((count + (size_t)rows - 1) / (size_t)rows);
      int n    = (int)((count + (size_t)c - 1) / (size_t)c);
      int last = (int)count - c * (n - 1);
      int th, wf, wl;
      float avg, score;

      if (n != rows)
         continue;                     /* rows would not match */
      th = (avail_h - (rows - 1) * PIX16_TILE_VGAP) / rows;
      if (th < PIX16_TILE_MIN_H)
         continue;

      wf = (avail_w - (c - 1) * PIX16_TILE_GAP_X) / c;
      wl = (avail_w - (last - 1) * PIX16_TILE_GAP_X) / last;
      if (wf < PIX16_TILE_MIN_W || wl < PIX16_TILE_MIN_W)
         continue;

      avg   = ((float)wf * (float)(rows - 1) + (float)wl)
            / (float)rows / (float)th;
      score = (avg > PIX16_TILE_TARGET)
            ? (avg / PIX16_TILE_TARGET) : (PIX16_TILE_TARGET / avg);
      if (last == c)
         score *= 0.95f;               /* uniform rows look better */

      if (best < 0.0f || score < best)
      {
         best      = score;
         best_r    = rows;
         best_c    = c;
         best_last = last;
         best_th   = th;
      }
   }

   /* more systems than one page holds: a full page of standard tiles */
   if (!best_c)
   {
      best_r    = maxr;
      best_c    = (avail_w + PIX16_TILE_GAP_X)
                / (PIX16_TILE_MIN_W + PIX16_TILE_GAP_X);
      if (best_c < 1)
         best_c = 1;
      best_last = best_c;
      best_th   = (avail_h - (maxr - 1) * PIX16_TILE_VGAP) / maxr;
      if (best_th < PIX16_TILE_MIN_H)
      {
         /* the minimum tile is taller than the space allows with this many
          * rows: drop rows until a page really fits, or the last row and the
          * page dots spill off the bottom */
         best_th = PIX16_TILE_MIN_H;
         while (     best_r > 1
               &&  best_r * best_th + (best_r - 1) * PIX16_TILE_VGAP > avail_h)
            best_r--;
      }
   }

   {
      int wf = (avail_w - (best_c - 1) * PIX16_TILE_GAP_X) / best_c;
      int wl = (avail_w - (best_last - 1) * PIX16_TILE_GAP_X) / best_last;
      if (wf < PIX16_TILE_MIN_W)
         wf = PIX16_TILE_MIN_W;
      if (wl < PIX16_TILE_MIN_W)
         wl = PIX16_TILE_MIN_W;

      pix16_grid_w = avail_w;

      if (   best_c == PIX16_GRID_COLS && best_r == PIX16_GRID_ROWS
          && wf == pix16_full_w && wl == pix16_last_w
          && best_th == PIX16_TILE_H)
         return;

      pix16_grid_cols  = best_c;
      pix16_grid_rows  = best_r;
      pix16_grid_last  = best_last;
      pix16_grid_w     = avail_w;
      pix16_full_w     = wf;
      pix16_last_w     = wl;
      pix16_tile_h     = best_th;
      pix16_row_stride = best_th + PIX16_TILE_VGAP;
      pix16_icon_h     = best_th - (3 + PIX16_LABEL_GAP + PIX16_LABEL_PAD
            + PIX16_GLYPH_H);
      pix->grid_valid  = false;   /* the page strip changed width */
   }
}

/* How many tiles a row of a page holds: the last row is drawn with wider
 * tiles and often fewer of them */
static size_t pix16_grid_row_count(size_t page, size_t row, size_t cnt)
{
   size_t per   = PIX16_GRID_PER_PAGE;
   size_t on    = cnt > page * per ? cnt - page * per : 0;
   size_t above = row * PIX16_GRID_COLS;
   size_t left  = on > above ? on - above : 0;
   if (row == (size_t)PIX16_GRID_ROWS - 1 && left > (size_t)pix16_grid_last)
      left = (size_t)pix16_grid_last;
   if (left > (size_t)PIX16_GRID_COLS)
      left = PIX16_GRID_COLS;
   return left;
}

/* The tile of that row lying under the given one: widest overlap first,
 * then nearest centre */
static size_t pix16_grid_col_for(size_t row, size_t count,
      size_t in, size_t cur_row)
{
   int tw_c    = PIX16_TILE_W_AT((int)cur_row);
   int tw_t    = PIX16_TILE_W_AT((int)row);
   int cx0     = (int)(in % PIX16_GRID_COLS) * (tw_c + PIX16_TILE_GAP_X);
   int cx1     = cx0 + tw_c;
   int ccen    = (cx0 + cx1) / 2;
   int best_ov = -1;
   int best_d  = 0x7fffffff;
   size_t best = 0;
   size_t k;

   for (k = 0; k < count; k++)
   {
      int tx0 = (int)k * (tw_t + PIX16_TILE_GAP_X);
      int tx1 = tx0 + tw_t;
      int ov  = (cx1 < tx1 ? cx1 : tx1) - (cx0 > tx0 ? cx0 : tx0);
      int d   = (tx0 + tx1) / 2 - ccen;
      if (d < 0)
         d = -d;
      if (ov > best_ov || (ov == best_ov && d < best_d))
      {
         best    = k;
         best_ov = ov;
         best_d  = d;
      }
   }
   return best;
}

/* A page's own layout: a page holding fewer systems grows its tiles to
 * fill the area, so a half-full last page looks like a small library */
static void pix16_grid_page_layout(pix16_t *pix, int avail_w, int avail_h,
      size_t page)
{
   size_t on = pix->grid_count > page * (size_t)PIX16_GRID_SLOTS
             ? pix->grid_count - page * (size_t)PIX16_GRID_SLOTS : 0;
   if (on > (size_t)PIX16_GRID_SLOTS)
      on = (size_t)PIX16_GRID_SLOTS;
   pix16_grid_layout(pix, avail_w, avail_h, on ? on : (size_t)1);
}

/* The systems grid, either theme: pages of tiles laid side by side,
 * the strip slides so the selected tile's page is in view */
static void pix16_grid_pages(pix16_t *pix, int x0, int y0,
      int avail_w, int avail_h, bool jrpg)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   size_t sel_pos, page, p, first;
   int scroll, stride, glove_x = 0, glove_y = 0;
   bool glove = false;

   pix16_grid_build(pix);
   pix16_grid_layout(pix, avail_w, avail_h,
         pix->grid_count ? pix->grid_count : (size_t)1);
   stride = PIX16_ROW_STRIDE;
   if (pix->grid_count == 0)
   {
      pix->grid_pages = 0;
      if (jrpg)
      {
         pix16_window(pix, x0, y0, PIX16_GRID_W, 40);
         pix->text_shadow = true;
         pix16_text(pix, x0 + 24, y0 + 16, msg_hash_to_str(MSG_PIX16_NO_PLAYLISTS),
               pix->colors[PIX16_C_J_TEXT], 1, 0);
         pix->text_shadow = false;
      }
      else
         pix16_text(pix, x0 + 8, y0 + 8, msg_hash_to_str(MSG_PIX16_NO_PLAYLISTS),
               pix->colors[PIX16_C_VALUE], 1, 0);
      return;
   }

   pix->grid_pages = (pix->grid_count + PIX16_GRID_PER_PAGE - 1) / PIX16_GRID_PER_PAGE;
   if (pix16_grid_has(pix, menu_st->selection_ptr))
   {
      sel_pos = pix16_grid_pos(pix, menu_st->selection_ptr);
      page    = sel_pos / PIX16_GRID_PER_PAGE;
   }
   else /* on a top button: keep showing the same page */
      page    = (pix->grid_page < pix->grid_pages) ? pix->grid_page : 0;
   pix->grid_page  = page;
   /* the page in view owns the sizes the navigation uses */
   pix16_grid_page_layout(pix, avail_w, avail_h, page);
   stride          = PIX16_ROW_STRIDE;
   scroll          = pix16_grid_scroll(pix, page);

   /* keep sliding tiles inside the grid area */
   pix16_clip_set(pix, x0 - 2, y0 - 2,
         x0 + PIX16_GRID_W + PIX16_SHADOW + 2,
         y0 + avail_h + 2);

   first = (size_t)(scroll / PIX16_PAGE_STRIDE);
   for (p = first; p <= first + 1 && p < pix->grid_pages; p++)
   {
      int px = x0 + (int)p * PIX16_PAGE_STRIDE - scroll;
      int i;
      bool here = (p == page);
      if (px >= pix->clip_x1 || px + PIX16_GRID_W + PIX16_SHADOW <= pix->clip_x0)
         continue;
      if (!here)
      {
         pix16_grid_page_layout(pix, avail_w, avail_h, p);
         stride = PIX16_ROW_STRIDE;
      }
      for (i = 0; i < PIX16_GRID_PER_PAGE; i++)
      {
         size_t d = p * PIX16_GRID_PER_PAGE + (size_t)i;
         int row  = i / PIX16_GRID_COLS;
         int tw   = PIX16_TILE_W_AT(row);
         int tx, ty;
         bool selected;
         if (d >= pix->grid_count)
            break;
         tx       = px + (i % PIX16_GRID_COLS) * (tw + PIX16_TILE_GAP_X);
         ty       = y0 + row * stride;
         selected = !PIX16_BAR_FOCUS(pix) && !pix->arcade_open
               && (pix->grid_map[d] == pix16_canon(pix, menu_st->selection_ptr));
         if (jrpg)
            pix16_j_grid_tile(pix, pix->grid_map[d], tx, ty, tw, selected);
         else
            pix16_grid_tile(pix, pix->grid_map[d], tx, ty, tw, selected);
         if (selected)
         {
            glove   = true;
            glove_x = tx - 4;
            glove_y = ty + (stride - 2 - 9) / 2;
         }
      }
      if (!here)
      {
         pix16_grid_page_layout(pix, avail_w, avail_h, page);
         stride = PIX16_ROW_STRIDE;
      }
   }

   /* the glove rests on the selected tile's left edge (JRPG theme);
    * drawn by the caller so it can slide to the top buttons too */
   if (jrpg && glove)
   {
      pix->sys_glove_on = true;
      pix->sys_glove_x  = glove_x;
      pix->sys_glove_y  = glove_y;
   }
   pix16_clip_reset(pix);
}

/* Page dots, right-aligned to right_x; returns the width used */
static int pix16_page_dots(pix16_t *pix, int right_x, int y,
      uint16_t on, uint16_t off)
{
   int n = (int)pix->grid_pages, i, w, x;
   if (n <= 1)
      return 0;
   if (n > 12)
      n = 12;
   w = n * 7 - 2;
   x = right_x - w;
   for (i = 0; i < n; i++)
      pix16_fill(pix, x + i * 7, y + 2, 5, 5,
            ((size_t)i == pix->grid_page) ? on : off);
   return w + 6;
}

static void pix16_j_tile(pix16_t *pix, int x, int y, int w, int h,
      bool selected);

/* 14x14 gear */
#define PIX16_GEAR_N 14
static const char *pix16_gear[PIX16_GEAR_N] = {
   "......##......",
   "......##......",
   "..##.####.##..",
   "..##########..",
   "...########...",
   "..####..####..",
   "#####....#####",
   "#####....#####",
   "..####..####..",
   "...########...",
   "..##########..",
   "..##.####.##..",
   "......##......",
   "......##......"
};

static void pix16_gear_icon(pix16_t *pix, int x, int y, uint16_t color)
{
   int gx, gy;
   for (gy = 0; gy < PIX16_GEAR_N; gy++)
      for (gx = 0; gx < PIX16_GEAR_N; gx++)
         if (pix16_gear[gy][gx] == '#')
         {
            if (pix->text_shadow)
               pix16_fill(pix, x + gx + 1, y + gy + 1, 1, 1,
                     pix->colors[PIX16_C_J_SHADOW]);
         }
   for (gy = 0; gy < PIX16_GEAR_N; gy++)
      for (gx = 0; gx < PIX16_GEAR_N; gx++)
         if (pix16_gear[gy][gx] == '#')
            pix16_fill(pix, x + gx, y + gy, 1, 1, color);
}

/* Power symbol, 14x13 */
static const char *pix16_power_px[13] = {
   "......##......",
   "...#..##..#...",
   "..##..##..##..",
   ".##...##...##.",
   "##....##....##",
   "##....##....##",
   "##..........##",
   "##..........##",
   "##..........##",
   ".##........##.",
   ".###......###.",
   "..##########..",
   "....######...."
};

static void pix16_power_icon(pix16_t *pix, int x, int y, uint16_t color)
{
   int gx, gy;
   for (gy = 0; gy < 13; gy++)
      for (gx = 0; gx < 14; gx++)
         if (pix16_power_px[gy][gx] == '#')
         {
            if (pix->text_shadow)
               pix16_fill(pix, x + gx + 1, y + gy + 1, 1, 1,
                     pix->colors[PIX16_C_J_SHADOW]);
         }
   for (gy = 0; gy < 13; gy++)
      for (gx = 0; gx < 14; gx++)
         if (pix16_power_px[gy][gx] == '#')
            pix16_fill(pix, x + gx, y + gy, 1, 1, color);
}

/* "+" for the Add System button, 11x11 */
static void pix16_plus_icon(pix16_t *pix, int x, int y, uint16_t color)
{
   if (pix->text_shadow)
   {
      pix16_fill(pix, x + 5, y + 1, 3, 11, pix->colors[PIX16_C_J_SHADOW]);
      pix16_fill(pix, x + 1, y + 5, 11, 3, pix->colors[PIX16_C_J_SHADOW]);
   }
   pix16_fill(pix, x + 4, y, 3, 11, color);
   pix16_fill(pix, x, y + 4, 11, 3, color);
}

/* HISTORY / FAVORITES buttons across the top of the systems screen,
 * the power button at the left end and the options gear at the right */
/* Two arrows, up and down: the arrange button */
static void pix16_order_icon(pix16_t *pix, int x, int y, uint16_t color)
{
   int i;
   for (i = 0; i < 4; i++)
   {
      pix16_fill(pix, x + 3 - i, y + i, 1 + 2 * i, 1, color);
      pix16_fill(pix, x + 3 - i, y + 10 - i, 1 + 2 * i, 1, color);
   }
}

static void pix16_draw_top_bar(pix16_t *pix, int x, int y, int w, int h,
      bool jrpg)
{
   const char *names[2];
   struct menu_state *menu_st  = menu_state_get_ptr();
   int focus                   = PIX16_BAR_FOCUS(pix) ? -1
                               : pix16_top_focus(pix, menu_st->selection_ptr);
   int shown[2], n = 0, k, gap = 4, bw;
   int gear_w                  = h;           /* square button */
   int gx                      = x + w - gear_w;

   names[0] = msg_hash_to_str(MSG_PIX16_HISTORY);
   names[1] = msg_hash_to_str(MSG_PIX16_FAVORITES);

   /* the power button */
   if (jrpg)
   {
      pix16_j_tile(pix, x, y, gear_w, h, pix->power_focus);
      pix->text_shadow = true;
      pix16_power_icon(pix, x + (gear_w - 14) / 2, y + (h - 13) / 2,
            pix->power_focus ? pix->conv(0xFFF07878)
                             : pix->colors[PIX16_C_J_TEXT]);
      pix->text_shadow = false;
      if (pix->power_focus)
      {
         pix->sys_glove_on = true;
         pix->sys_glove_x  = x - 8;         /* against its left edge */
         pix->sys_glove_y  = y + (h - 9) / 2;
      }
   }
   else
   {
      if (pix->power_focus)
         pix16_fill(pix, x - 1, y - 1, gear_w + 2, h + 2, pix->colors[PIX16_C_TEXT]);
      pix16_panel(pix, x, y, gear_w, h,
            pix->power_focus ? pix->colors[PIX16_C_RED] : pix->colors[PIX16_C_PANEL]);
      pix16_power_icon(pix, x + (gear_w - 14) / 2, y + (h - 13) / 2,
            pix->power_focus ? pix->colors[PIX16_C_TEXT] : pix->colors[PIX16_C_TEXT_DIM]);
   }
   x += gear_w + gap;
   w -= gear_w + gap;

   /* the gear */
   if (jrpg)
   {
      pix16_j_tile(pix, gx, y, gear_w, h, pix->gear_focus);
      pix->text_shadow = true;
      pix16_gear_icon(pix, gx + (gear_w - PIX16_GEAR_N) / 2, y + (h - PIX16_GEAR_N) / 2,
            pix->gear_focus ? pix->colors[PIX16_C_J_GOLD]
                            : pix->colors[PIX16_C_J_TEXT]);
      pix->text_shadow = false;
      if (pix->gear_focus)
      {
         pix->sys_glove_on = true;
         pix->sys_glove_x  = gx - 9;
         pix->sys_glove_y  = y + (h - 9) / 2;
      }
   }
   else
   {
      if (pix->gear_focus)
         pix16_fill(pix, gx - 1, y - 1, gear_w + 2, h + 2, pix->colors[PIX16_C_TEXT]);
      pix16_panel(pix, gx, y, gear_w, h,
            pix->gear_focus ? pix->tab_color[PIX16_TAB_SETUP] : pix->colors[PIX16_C_PANEL]);
      pix16_gear_icon(pix, gx + (gear_w - PIX16_GEAR_N) / 2, y + (h - PIX16_GEAR_N) / 2,
            pix->gear_focus ? pix->tab_fg[PIX16_TAB_SETUP] : pix->colors[PIX16_C_TEXT_DIM]);
   }
   w -= gear_w + gap;

   /* the + (Add System) button, left of the gear */
   {
      int ax = gx - gap - gear_w;
      if (jrpg)
      {
         pix16_j_tile(pix, ax, y, gear_w, h, pix->add_focus);
         pix->text_shadow = true;
         pix16_plus_icon(pix, ax + (gear_w - 11) / 2, y + (h - 11) / 2,
               pix->add_focus ? pix->colors[PIX16_C_J_GOLD]
                              : pix->colors[PIX16_C_J_TEXT]);
         pix->text_shadow = false;
         if (pix->add_focus)
         {
            pix->sys_glove_on = true;
            pix->sys_glove_x  = ax - 9;
            pix->sys_glove_y  = y + (h - 9) / 2;
         }
      }
      else
      {
         if (pix->add_focus)
            pix16_fill(pix, ax - 1, y - 1, gear_w + 2, h + 2, pix->colors[PIX16_C_TEXT]);
         pix16_panel(pix, ax, y, gear_w, h,
               pix->add_focus ? pix->conv(0xFF52C86A) : pix->colors[PIX16_C_PANEL]);
         pix16_plus_icon(pix, ax + (gear_w - 11) / 2, y + (h - 11) / 2,
               pix->add_focus ? pix->colors[PIX16_C_TEXT] : pix->colors[PIX16_C_TEXT_DIM]);
      }
      w -= gear_w + gap;
   }

   /* the order (arrange) button, left of the + */
   {
      int ox = gx - 2 * (gap + gear_w);
      if (jrpg)
      {
         pix16_j_tile(pix, ox, y, gear_w, h, pix->order_focus);
         pix->text_shadow = true;
         pix16_order_icon(pix, ox + (gear_w - 7) / 2, y + (h - 11) / 2,
               pix->order_focus ? pix->colors[PIX16_C_J_GOLD]
                                : pix->colors[PIX16_C_J_TEXT]);
         pix->text_shadow = false;
         if (pix->order_focus)
         {
            pix->sys_glove_on = true;
            pix->sys_glove_x  = ox - 9;
            pix->sys_glove_y  = y + (h - 9) / 2;
         }
      }
      else
      {
         if (pix->order_focus)
            pix16_fill(pix, ox - 1, y - 1, gear_w + 2, h + 2, pix->colors[PIX16_C_TEXT]);
         pix16_panel(pix, ox, y, gear_w, h,
               pix->order_focus ? pix->tab_color[PIX16_TAB_SETUP]
                                : pix->colors[PIX16_C_PANEL]);
         pix16_order_icon(pix, ox + (gear_w - 7) / 2, y + (h - 11) / 2,
               pix->order_focus ? pix->colors[PIX16_C_DARK_TEXT]
                                : pix->colors[PIX16_C_TEXT_DIM]);
      }
      w -= gear_w + gap;
   }
   for (k = 0; k < 2; k++)
      if (pix->top_idx[k] != PIX16_TOP_NONE)
         shown[n++] = k;
   if (n == 0)
      return;
   bw = (w - gap * (n - 1)) / n;

   for (k = 0; k < n; k++)
   {
      int which    = shown[k];
      int bx       = x + k * (bw + gap);
      bool sel     = (focus == which);
      size_t len   = strlen(names[which]);
      int tx       = bx + (bw - (int)len * PIX16_GLYPH_W) / 2;
      int ty       = y + (h - PIX16_GLYPH_H) / 2;

      if (jrpg)
      {
         pix16_j_tile(pix, bx, y, bw, h, sel);
         pix->text_shadow = true;
         pix16_text(pix, tx, ty, names[which],
               sel ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_J_TEXT], 1, 0);
         pix->text_shadow = false;
         if (sel)
         {
            pix->sys_glove_on = true;
            pix->sys_glove_x  = tx - 17;
            pix->sys_glove_y  = ty - 1;
         }
      }
      else
      {
         uint16_t bg = sel ? pix->tab_color[PIX16_TAB_LISTS] : pix->colors[PIX16_C_PANEL];
         uint16_t fg = sel ? pix->tab_fg[PIX16_TAB_LISTS]    : pix->colors[PIX16_C_TEXT_DIM];
         if (sel)
            pix16_fill(pix, bx - 1, y - 1, bw + 2, h + 2, pix->colors[PIX16_C_TEXT]);
         pix16_panel(pix, bx, y, bw, h, bg);
         pix16_text(pix, tx, ty, names[which], fg, 1, 0);
      }
   }
}

/* Window listing the arcade collections, over the grid */
static void pix16_draw_game_menu(pix16_t *pix, bool jrpg);

static bool pix16_popup_window(pix16_t *pix, int x, int y, int w, int h);

/* The manage actions that apply to the picked system, in draw order */
enum { PIX16_MG_MERGE = 0, PIX16_MG_UNMERGE, PIX16_MG_RENAME,
       PIX16_MG_ICON, PIX16_MG_HIDE, PIX16_MG_BACK };

static size_t pix16_mg_rows(pix16_t *pix, int *act)
{
   size_t n = 0;
   const char *host = pix16_group_host_of(pix->mg_member);
   bool hidden   = host && string_is_equal_noncase(host, "HIDDEN");
   bool merged   = host && !string_is_equal_noncase(host, "HOME")
                && !string_is_equal_noncase(host, "ARCADE") && !hidden;
   act[n++] = PIX16_MG_MERGE;
   if (merged || hidden || pix16_groups_member_count(pix->mg_member) > 0)
      act[n++] = PIX16_MG_UNMERGE;
   act[n++] = PIX16_MG_RENAME;
   act[n++] = PIX16_MG_ICON;
   if (!hidden)
      act[n++] = PIX16_MG_HIDE;
   act[n++] = PIX16_MG_BACK;
   return n;
}

/* Every playlist, tiles and members alike, so merged and hidden systems
 * can be seen and acted on from the arrange window */
static void pix16_order_fill(pix16_t *pix)
{
   file_list_t *list = pix16_selection_buf();
   size_t i;
   pix->order_n = 0;
   if (!list)
      return;
   for (i = 0; i < list->size && pix->order_n < 64; i++)
   {
      const char *lbl;
      const char *host;
      if (list->list[i].type != FILE_TYPE_PLAYLIST_COLLECTION
            || !list->list[i].path)
         continue;
      pix16_get_entry(pix, i, false);
      lbl = pix16_entry_label(&pix->entry);
      host = pix16_group_host_of(list->list[i].path);
      pix->order_map[pix->order_n] = i;
      pix->order_state[pix->order_n] =
           (host && string_is_equal_noncase(host, "HIDDEN")) ? 2
         : (host && !string_is_equal_noncase(host, "HOME")
               && !string_is_equal_noncase(host, "ARCADE")) ? 1
         : 0;
      strlcpy(pix->order_label[pix->order_n], lbl ? lbl : "",
            sizeof(pix->order_label[0]));
      pix->order_n++;
   }
}

/* What the second row says: a hidden system comes back, a member splits
 * out, a host splits everything it holds */
static const char *pix16_mg_label(const pix16_t *pix, int act)
{
   if (act == PIX16_MG_MERGE)
      return "MERGE INTO...";
   if (act == PIX16_MG_HIDE)
      return "HIDE SYSTEM";
   if (act == PIX16_MG_RENAME)
      return "RENAME...";
   if (act == PIX16_MG_ICON)
      return "CHANGE ICON...";
   if (act == PIX16_MG_UNMERGE)
   {
      const char *host = pix16_group_host_of(pix->mg_member);
      if (host && string_is_equal_noncase(host, "HIDDEN"))
         return "SHOW SYSTEM";
      if (pix16_groups_member_count(pix->mg_member) > 0)
         return "SPLIT ALL";
      return "UNMERGE";
   }
   return "BACK";
}

static void pix16_mg_open(pix16_t *pix)
{
   file_list_t *list = pix16_selection_buf();
   size_t idx;
   if (!list || (size_t)pix->order_sel >= pix->order_n)
      return;
   idx = pix->order_map[pix->order_sel];
   if (idx >= list->size || !list->list[idx].path)
      return;
   pix16_order_base(list->list[idx].path, pix->mg_member, sizeof(pix->mg_member));
   strlcpy(pix->mg_name, pix->order_label[pix->order_sel], sizeof(pix->mg_name));
   pix->mg_sel  = 0;
   pix->mg_pick = false;
   pix->mg_open = true;
   pix16_sfx(PIX16_SFX_OK);
}

/* merge targets: every other system on the grid */
static void pix16_mg_fill(pix16_t *pix)
{
   size_t k;
   pix->mg_n = 0;
   for (k = 0; k < pix->order_n && pix->mg_n < 64; k++)
   {
      if ((int)k == pix->order_sel)
         continue;
      pix->mg_map[pix->mg_n] = pix->order_map[k];
      strlcpy(pix->mg_label[pix->mg_n], pix->order_label[k],
            sizeof(pix->mg_label[0]));
      pix->mg_n++;
   }
}

/* rebuild the open order window's rows after an edit */
static void pix16_order_refresh(pix16_t *pix)
{
   if (!pix->order_open)
      return;
   pix16_order_fill(pix);
   if (pix->order_n && (size_t)pix->order_sel >= pix->order_n)
      pix->order_sel = (int)pix->order_n - 1;
}

static void pix16_mg_apply(pix16_t *pix, int action)
{
   file_list_t *list = pix16_selection_buf();
   switch (action)
   {
      case PIX16_MG_MERGE:
         pix16_mg_fill(pix);
         if (pix->mg_n)
         {
            pix->mg_pick = true;
            pix->mg_sel  = 0;
         }
         break;
      case PIX16_MG_UNMERGE:
         {
            size_t i;
            bool split = false;
            for (i = 0; list && i < list->size; i++)
            {
               char base[NAME_MAX_LENGTH];
               const char *h;
               if (list->list[i].type != FILE_TYPE_PLAYLIST_COLLECTION
                     || !list->list[i].path)
                  continue;
               h = pix16_group_host_of(list->list[i].path);
               if (!h || !string_is_equal_noncase(h, pix->mg_member))
                  continue;
               pix16_order_base(list->list[i].path, base, sizeof(base));
               pix16_groups_clear(base);
               split = true;
            }
            if (!split)
               pix16_groups_clear(pix->mg_member);
            /* the order window stays open: rebuild its rows so a system
             * that just came back loses its [H] and its dim colour */
            pix16_order_refresh(pix);
            pix->mg_open = false;
            menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
            pix16_sfx(PIX16_SFX_BACK);
         }
         break;
      case PIX16_MG_RENAME:
         {
            menu_input_ctx_line_t line;
            memset(&line, 0, sizeof(line));
            line.label         = "SYSTEM NAME";
            line.label_setting = "pix16_system_name";
            line.text_type     = MENU_INPUT_DIALOG_KB_TYPE_TEXT;
            line.cb            = pix16_mg_kb_cb;
            pix16_mg_owner     = pix;
            menu_input_dialog_start(&line);
         }
         break;
      case PIX16_MG_ICON:
         {
            size_t k;
            pix->mg_n = 0;
            for (k = 0; pix16_systems[k].playlist && pix->mg_n < 64; k++)
            {
               strlcpy(pix->mg_label[pix->mg_n], pix16_systems[k].short_name,
                     sizeof(pix->mg_label[0]));
               pix->mg_map[pix->mg_n] = k;
               pix->mg_n++;
            }
            if (pix->mg_n)
            {
               pix->mg_pick      = true;
               pix->mg_icon      = true;
               pix->mg_icon_more = false;
               pix->mg_icon_cat  = -1;
               pix->mg_sel       = 0;
            }
         }
         break;
      case PIX16_MG_HIDE:
         pix16_groups_set(pix->mg_member, "HIDDEN");
         pix->mg_open = false;
         pix16_order_refresh(pix);
         menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
         pix16_sfx(PIX16_SFX_OK);
         break;
      default:
         pix->mg_open = false;
         pix->mg_pick = false;
         pix16_sfx(PIX16_SFX_BACK);
         break;
   }
}

static void pix16_mg_pick_apply(pix16_t *pix, size_t row)
{
   file_list_t *list = pix16_selection_buf();
   size_t idx;
   char target[NAME_MAX_LENGTH];

   /* MORE... on the popular list: show the categories */
   if (pix->mg_icon && !pix->mg_icon_more && pix->mg_icon_cat < 0
         && row == 0)
   {
      pix->mg_icon_more = true;
      pix->mg_sel       = 0;
      return;
   }
   /* an icon out of the full list */
   if (pix->mg_icon_more)
   {
      const char *nm = row < pix16_icon_all_n ? pix16_icon_all[row] : NULL;
      if (nm && nm[0])
         pix16_ovr_set(pix16_icons, &pix16_icons_n, "pix16_icons.txt",
               pix->mg_member, nm);
      pix->mg_open      = false;
      pix->mg_pick      = false;
      pix->mg_icon      = false;
      pix->mg_icon_more = false;
      pix->mg_icon_cat  = -1;
      pix16_order_refresh(pix);
      menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      pix16_sfx(PIX16_SFX_OK);
      return;
   }

   if (pix->mg_icon)
   {
      /* the popular list maps to system table entries, not to playlists,
       * so it cannot go through the merge path below */
      if (row == 0)
         return;                          /* MORE... is not an icon */
      row--;
      if (row >= pix->mg_n)
         return;
      pix16_ovr_set(pix16_icons, &pix16_icons_n, "pix16_icons.txt",
            pix->mg_member, pix16_systems[pix->mg_map[row]].playlist);
   }
   else
   {
      if (row >= pix->mg_n || !list)
         return;
      idx = pix->mg_map[row];
      if (idx >= list->size || !list->list[idx].path)
         return;
      pix16_order_base(list->list[idx].path, target, sizeof(target));
      pix16_groups_set(pix->mg_member, target);
   }
   pix->mg_open    = false;
   pix->mg_pick    = false;
   pix->mg_icon    = false;
   pix16_order_refresh(pix);
   menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   pix16_sfx(PIX16_SFX_OK);
}

static void pix16_mg_input(pix16_t *pix, enum menu_action action)
{
   if (pix->mg_pick)
   {
      switch (action)
      {
         case MENU_ACTION_UP:
            if (pix->mg_sel > 0) { pix->mg_sel--; pix16_sfx(PIX16_SFX_UP); }
            break;
         case MENU_ACTION_DOWN:
            if ((size_t)(pix->mg_sel + 1) < pix16_mg_pick_count(pix))
            { pix->mg_sel++; pix16_sfx(PIX16_SFX_DOWN); }
            break;
         case MENU_ACTION_OK:
         case MENU_ACTION_START:
            pix16_mg_pick_apply(pix, (size_t)pix->mg_sel);
            break;
         case MENU_ACTION_CANCEL:
            pix->mg_pick = false;
            pix->mg_sel  = 0;
            pix16_sfx(PIX16_SFX_BACK);
            break;
         default:
            break;
      }
   }
   else
   {
      int acts[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
      size_t n = pix16_mg_rows(pix, acts);
      switch (action)
      {
         case MENU_ACTION_UP:
            if (pix->mg_sel > 0) { pix->mg_sel--; pix16_sfx(PIX16_SFX_UP); }
            break;
         case MENU_ACTION_DOWN:
            if ((size_t)(pix->mg_sel + 1) < n) { pix->mg_sel++; pix16_sfx(PIX16_SFX_DOWN); }
            break;
         case MENU_ACTION_OK:
         case MENU_ACTION_START:
            pix16_mg_apply(pix, acts[pix->mg_sel]);
            break;
         case MENU_ACTION_CANCEL:
            pix->mg_open = false;
            pix16_sfx(PIX16_SFX_BACK);
            break;
         default:
            break;
      }
   }
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
}

/* Actions for one system: merge it into another tile, split it out,
 * or hide it from the grid. Merging works like ARCADE: one tile that
 * opens a picker of the systems inside it. */
static void pix16_draw_manage(pix16_t *pix, bool jrpg)
{
   int w = PIX16_FB_WIDTH - 2 * PIX16_SAFE - 40, row_h = 14, h, x, y, i;
   uint16_t fg  = jrpg ? pix->colors[PIX16_C_J_TEXT] : pix->colors[PIX16_C_TEXT];
   uint16_t sel = jrpg ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW];
   int acts[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
   int max_rows = (PIX16_FB_HEIGHT - 2 * PIX16_SAFE - 34) / 14;
   int rows;
   int first = 0;
   size_t n;
   const char *title;

   if (pix->mg_pick)
   {
      n     = pix16_mg_pick_count(pix);
      title = pix->mg_icon
            ? (pix->mg_icon_cat >= 0
                  ? (pix->mg_icon_cat == PIX16_CAT_CONSOLE  ? "CONSOLES"
                  :  pix->mg_icon_cat == PIX16_CAT_HANDHELD ? "HANDHELDS"
                  :  pix->mg_icon_cat == PIX16_CAT_COMPUTER ? "COMPUTERS"
                  :                                           "MISC")
                  : "CHANGE ICON")
            : "MERGE INTO";
   }
   else
   {
      n = pix16_mg_rows(pix, acts);
      title = pix->mg_name;
   }

   rows = (int)n > max_rows ? max_rows : (int)n;
   if (pix->mg_sel >= rows)
      first = pix->mg_sel - rows + 1;
   h = 34 + rows * row_h;
   x = PIX16_SAFE + (PIX16_FB_WIDTH - 2 * PIX16_SAFE - w) / 2;
   y = PIX16_SAFE + (PIX16_FB_HEIGHT - 2 * PIX16_SAFE - h) / 2;
   pix16_window(pix, x, y, w, h);
   pix->text_shadow = jrpg;
   pix16_text(pix, x + 8, y + 6, title, sel, 1, (size_t)((w - 16) / PIX16_GLYPH_W));
   for (i = 0; i < rows; i++)
   {
      int row = first + i;
      const char *label = pix->mg_pick ? pix16_mg_pick_text(pix, row)
                                       : pix16_mg_label(pix, acts[row]);
      int ry = y + 20 + i * row_h;
      if (row == pix->mg_sel)
         pix16_vgradient(pix, x + 6, ry - 1, w - 12, row_h, pix->sel_top, pix->sel_bot);
      pix16_text(pix, x + 10, ry + 1, label,
            row == pix->mg_sel ? pix->colors[PIX16_C_TEXT] : fg, 1,
            (size_t)((w - 22) / PIX16_GLYPH_W));
      if (row == pix->mg_sel)
         pix16_glove_cursor(pix, x - 13, ry - 1);
   }
   pix->text_shadow = false;
}

/* System order window: the grid's systems as a list the user reorders */
static void pix16_draw_order(pix16_t *pix, bool jrpg)
{
   int w        = PIX16_FB_WIDTH - 2 * PIX16_SAFE - 16;
   int row_h    = 12;
   int max_rows = (PIX16_FB_HEIGHT - 2 * PIX16_SAFE - 40) / 12;
   int rows     = (int)pix->order_n;
   int first    = 0;
   int h, x, y, i;
   uint16_t fg  = jrpg ? pix->colors[PIX16_C_J_TEXT] : pix->colors[PIX16_C_TEXT];
   uint16_t sel = jrpg ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW];

   if (rows > max_rows)
      rows = max_rows;
   if (pix->order_sel >= rows)
      first = pix->order_sel - rows + 1;
   h = 46 + rows * row_h;
   x = PIX16_SAFE + (PIX16_FB_WIDTH - 2 * PIX16_SAFE - w) / 2;
   y = PIX16_SAFE + (PIX16_FB_HEIGHT - 2 * PIX16_SAFE - h) / 2;

   pix16_window(pix, x, y, w, h);
   pix->text_shadow = jrpg;
   pix16_text(pix, x + 8, y + 6, "SYSTEM ORDER", sel, 1, 0);
   for (i = 0; i < rows; i++)
   {
      char marked[80];
      const char *label = pix->order_label[first + i];
      int ry = y + 20 + i * row_h;
      if (pix->order_state[first + i] == 2)
      {
         snprintf(marked, sizeof(marked), "[H] %s", label);
         label = marked;
      }
      if (first + i == pix->order_sel)
         pix16_vgradient(pix, x + 6, ry - 1, w - 12, row_h, pix->sel_top, pix->sel_bot);
      {
         uint16_t col = fg;
         if (pix->order_state[first + i] == 1)
            col = pix->colors[PIX16_C_VALUE];
         else if (pix->order_state[first + i] == 2)
            col = pix->colors[PIX16_C_TEXT_DIM];
         pix16_text(pix, x + 10, ry + 1, label, col, 1,
               (size_t)((w - 22) / PIX16_GLYPH_W));
      }
      if (first + i == pix->order_sel && !pix->mg_open)
         pix16_glove_cursor(pix, x - 13, ry - 1);   /* one glove at a time */
   }
   {
      int hx = x + 8;
      int hy = y + h - 26;
      uint16_t pill_bg = pix->tab_color[PIX16_TAB_SETUP];
      uint16_t pill_fg = pix->tab_fg[PIX16_TAB_SETUP];
      pix16_text(pix, hx, hy + 2, "L/R MOVE    U/D PICK", fg, 1, 0);
      hx  = x + 8;
      hy += 13;
      hx += pix16_okback(pix, hx, hy, true, jrpg) + 4;
      pix16_text(pix, hx, hy + 2, "SAVE", fg, 1, 0);
      hx += 4 * PIX16_GLYPH_W + 10;
      hx += pix16_pill(pix, hx, hy, pix16_select_name(pix), pill_bg, pill_fg) + 4;
      pix16_text(pix, hx, hy + 2, "ACTIONS", fg, 1, 0);
   }
   pix->text_shadow = false;
   if (pix->mg_open)
      pix16_draw_manage(pix, jrpg);
}

static void pix16_draw_arcade_picker(pix16_t *pix, bool jrpg)
{
   file_list_t *list = pix16_selection_buf();
   size_t *items = pix->pick;
   size_t count  = pix16_picker_items(pix, items, PIX16_PICK_MAX);
   int row_h  = 14;
   int rows   = (int)count;
   int w      = (pix->picker_pce && count <= 2) ? 144 : 232;
   char head[NAME_MAX_LENGTH];
   int h, x, y, k, begin = 0;

   if (!list || count == 0)
      return;
   if (rows > 9)
      rows = 9;
   if ((int)pix->arcade_sel >= rows)
      begin = (int)pix->arcade_sel - rows + 1;
   h = 28 + rows * row_h + 6;
   x = (PIX16_FB_WIDTH  - w) / 2;
   y = (PIX16_FB_HEIGHT - h) / 2;

   if (jrpg)
   {
      if (!pix16_popup_window(pix, x, y, w, h))
         return;
   }
   else
   {
      pix16_panel(pix, x, y, w, h, pix->colors[PIX16_C_PANEL]);
      pix16_fill(pix, x, y, w, 2, pix->conv(0xFFF08A2A));
   }
   pix->text_shadow = jrpg;
   if (pix->picker_pce)
      strlcpy(head, msg_hash_to_str(MSG_PIX16_PC_ENGINE), sizeof(head));
   else if (pix->picker_host != PIX16_TOP_NONE && pix->picker_host < list->size)
   {
      const char *hp = list->list[pix->picker_host].path;
      pix16_system_title(hp ? path_basename(hp) : "", head, sizeof(head));
   }
   else
      strlcpy(head, msg_hash_to_str(MSG_PIX16_ARCADE), sizeof(head));
   pix16_text_fit(pix, x + 12, y + 10, head,
         jrpg ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW],
         (size_t)((w - 24) / PIX16_GLYPH_W));

   for (k = 0; k < rows; k++)
   {
      size_t a = (size_t)(begin + k);
      char name[NAME_MAX_LENGTH];
      int ty   = y + 28 + k * row_h + 3;
      bool sel = (a == pix->arcade_sel);
      const char *ap;
      if (a >= count)
         break;
      ap = list->list[items[a]].path;
      if (pix->picker_pce && a < 2)
         strlcpy(name, a == 0 ? msg_hash_to_str(MSG_PIX16_CARTRIDGE) : msg_hash_to_str(MSG_PIX16_CD_ROM), sizeof(name));
      else
         pix16_system_title(ap ? path_basename(ap) : "", name, sizeof(name));
      if (!jrpg && sel)
         pix16_fill(pix, x + 6, ty - 3, w - 12, row_h - 1, pix->conv(0xFFF08A2A));
      pix16_text_ticker(pix, x + 26, ty, name,
            jrpg ? pix->colors[PIX16_C_J_TEXT]
                 : (sel ? pix->colors[PIX16_C_DARK_TEXT] : pix->colors[PIX16_C_TEXT_DIM]),
            (size_t)((w - 38) / PIX16_GLYPH_W), sel);
      if (sel && jrpg)
      {
         pix->text_shadow = false;
         pix16_glove_at(pix, x + 9, ty - 1, pix16_glove_context(pix, 9));
         pix->text_shadow = true;
      }
      else if (sel)
         pix16_cursor(pix, x + 12, ty, pix->colors[PIX16_C_DARK_TEXT]);
   }
   if (begin > 0)
      pix16_text(pix, x + w - 14, y + 2, "^", pix->colors[PIX16_C_J_GOLD], 1, 0);
   if (begin + rows < (int)count)
      pix16_text(pix, x + w - 14, y + h - 10, "v", pix->colors[PIX16_C_J_GOLD], 1, 0);
   pix->text_shadow = false;
}

static void pix16_wiz_open(pix16_t *pix);
static void pix16_draw_wizard(pix16_t *pix, bool jrpg);

/* The whole systems screen below the background: top buttons, grid,
 * glove. Keeps the selection on something that is shown. */
static void pix16_systems_screen(pix16_t *pix, bool jrpg)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   int x       = PIX16_SAFE;
   int w       = PIX16_FB_WIDTH - 2 * PIX16_SAFE;
   int bar_h   = jrpg ? 24 : PIX16_BAR_H;
   int grid_y  = (jrpg ? PIX16_SAFE + 24 + 3 : PIX16_SAFE + PIX16_BAR_H + PIX16_GAP)
               + PIX16_GRID_DY;
   int grid_h  = PIX16_FB_HEIGHT - PIX16_SAFE - PIX16_FOOTER_H - 4 - grid_y;

   pix16_grid_build(pix);

   /* back from options: put the cursor on the gear */
   if (pix->flags & PIX16_FLAG_RETURN_GEAR)
   {
      pix->flags     &= ~PIX16_FLAG_RETURN_GEAR;
      pix->gear_focus = true;
      if (pix->top_idx[1] != PIX16_TOP_NONE)
         menu_st->selection_ptr = pix->top_idx[1];
      else if (pix->top_idx[0] != PIX16_TOP_NONE)
         menu_st->selection_ptr = pix->top_idx[0];
   }

   if (     pix16_top_focus(pix, menu_st->selection_ptr) < 0
         && !pix16_grid_has(pix, menu_st->selection_ptr))
   {
      /* selection is on something hidden (e.g. Import Content) */
      if (pix->grid_count > 0)
         menu_st->selection_ptr = pix->grid_map[0];
      else if (pix->top_idx[0] != PIX16_TOP_NONE)
         menu_st->selection_ptr = pix->top_idx[0];
      else if (pix->top_idx[1] != PIX16_TOP_NONE)
         menu_st->selection_ptr = pix->top_idx[1];
   }

   /* back from the core downloader: carry on adding the system */
   if (pix->wiz.resume)
   {
      pix16_wiz_open(pix);
      pix->add_focus = true;
   }

   pix->sys_glove_on = false;
   pix16_draw_top_bar(pix, x, PIX16_SAFE, w, bar_h, jrpg);

   /* nothing in the grid yet: say so instead of showing empty space */
   pix->empty_show = false;
   if (pix->grid_count == 0 && !pix->wiz.open)
   {
      int bw = 208, bh = 56;
      int bx = x + (w - bw) / 2;
      int by = grid_y + (grid_h - bh) / 2;
      pix->text_shadow = jrpg;
      pix16_window(pix, bx, by, bw, bh);
      pix16_text(pix, bx + (bw - 14 * PIX16_GLYPH_W) / 2, by + 12,
            "NO SYSTEMS YET", pix->colors[PIX16_C_J_TEXT], 1, 0);
      pix16_text(pix, bx + (bw - 10 * PIX16_GLYPH_W) / 2, by + 30,
            "ADD SYSTEM", pix->colors[PIX16_C_J_GOLD], 1, 0);
      pix->text_shadow = false;
      pix->empty_rect.x = (int16_t)bx;
      pix->empty_rect.y = (int16_t)by;
      pix->empty_rect.w = (int16_t)bw;
      pix->empty_rect.h = (int16_t)bh;
      pix->empty_rect.kind = 0;
      pix->empty_show    = true;
   }

   pix16_grid_pages(pix, x, grid_y, w, grid_h, jrpg);

   if (pix->sys_glove_on && !pix->arcade_open && !pix->wiz.open && !pix->order_open)
      pix16_glove_at(pix, pix->sys_glove_x, pix->sys_glove_y,
            pix16_glove_context(pix, 1));

   if (pix->order_open)
      pix16_draw_order(pix, jrpg);
   else if (pix->arcade_open)
      pix16_draw_arcade_picker(pix, jrpg);
   else if (pix->wiz.open)
      pix16_draw_wizard(pix, jrpg);
}


/* Bottom bar of the grid: button guide and page dots */
static void pix16_draw_grid_footer(pix16_t *pix)
{
   int y = PIX16_FB_HEIGHT - PIX16_SAFE - PIX16_FOOTER_H;
   pix16_draw_guide(pix, PIX16_SAFE,
         PIX16_FB_WIDTH - PIX16_SAFE - 6 * PIX16_GLYPH_W - 4, y, false);
   pix16_page_dots(pix, PIX16_FB_WIDTH - PIX16_SAFE, y,
         pix->colors[PIX16_C_YELLOW], pix->colors[PIX16_C_LAVENDER]);
}

static void pix16_draw_messagebox(pix16_t *pix, const char *msg)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   bool confirm   = menu_st->dialog_st.confirm_msg
                 && menu_st->dialog_st.confirm_cmd;
   int max_w      = PIX16_FB_WIDTH - 2 * PIX16_SAFE;
   int max_lines  = (PIX16_FB_HEIGHT - 2 * PIX16_SAFE - 12 - (confirm ? 16 : 0)) / 11;
   int longest    = 0, cur = 0, lines, w, h, x, y;
   size_t cols;
   const char *p;
   uint16_t text_c = pix->colors[PIX16_C_DARK_TEXT];

   for (p = msg; *p; p++)
   {
      if (*p == '\n')
         cur = 0;
      else if (++cur > longest)
         longest = cur;
   }

   w = longest * PIX16_GLYPH_W + 16;
   if (w > max_w)
      w = max_w;
   if (confirm && w < 22 * PIX16_GLYPH_W)
      w = 22 * PIX16_GLYPH_W;
   cols  = (size_t)((w - 16) / PIX16_GLYPH_W);
   lines = pix16_wrap(NULL, 0, 0, msg, 0, cols, max_lines, 11);
   h     = lines * 11 + 12 + (confirm ? 16 : 0);
   x     = (PIX16_FB_WIDTH - w) / 2;
   y     = (PIX16_FB_HEIGHT - h) / 2;

   if (pix->theme == PIX16_THEME_JRPG)
   {
      /* window frame needs a little more room */
      x -= 4; y -= 4; w += 8; h += 8;
      pix16_window(pix, x, y, w, h);
      x += 4; y += 4; w -= 8; h -= 8;
      text_c = pix->colors[PIX16_C_J_TEXT];
   }
   else
   {
      pix16_panel(pix, x, y, w, h, pix->colors[PIX16_C_PAPER]);
      pix16_fill(pix, x, y, w, 2, pix->colors[PIX16_C_RED]);
   }
   pix16_wrap(pix, x + 8, y + 7, msg, text_c, cols, max_lines, 11);

   if (confirm)
   {
      const char *yes = msg_hash_to_str(MSG_PIX16_YES);
      const char *no  = msg_hash_to_str(MSG_PIX16_NO);
      bool jrpg       = pix->theme == PIX16_THEME_JRPG;
      int by          = y + h - 14;
      int bw          = pix16_okback_width(pix, true)
            + (int)strlen(yes) * PIX16_GLYPH_W + 16
            + pix16_okback_width(pix, false)
            + (int)strlen(no) * PIX16_GLYPH_W;
      int bx          = x + (w - bw) / 2;
      bx += pix16_okback(pix, bx, by, true, jrpg);
      pix16_text(pix, bx, by + 1, yes, text_c, 1, 0);
      bx += (int)strlen(yes) * PIX16_GLYPH_W + 16;
      bx += pix16_okback(pix, bx, by, false, jrpg);
      pix16_text(pix, bx, by + 1, no, text_c, 1, 0);
   }
}

/* The OSK's special keys are Unicode arrows; give them ASCII labels
 * the 8x8 font can draw */
static const char *pix16_osk_key_label(const char *k)
{
   if (!k)
      return "";
   if (memcmp(k, "\xe2\x87\xa6", 3) == 0) return "DEL"; /* backspace */
   if (memcmp(k, "\xe2\x8f\x8e", 3) == 0) return "OK";  /* return */
   if (memcmp(k, "\xe2\x87\xa7", 3) == 0) return "ABC"; /* shift up */
   if (memcmp(k, "\xe2\x87\xa9", 3) == 0) return "abc"; /* shift down */
   if (memcmp(k, "\xe2\x8a\x95", 3) == 0) return "+";   /* next page */
   if (memcmp(k, "\xe2\x8c\x82", 3) == 0) return "#";   /* home/symbols */
   return k;
}

/* On-screen keyboard: label, entry field and the 11x4 key grid */
static void pix16_draw_osk(pix16_t *pix)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   const char *label          = menu_st->input_dialog_kb_label;
   const char *buffer         = menu_input_dialog_get_buffer();
   int osk_ptr                = menu_st->osk_ptr;
   char **grid                = menu_st->osk_grid;
   int key_w                  = 24, key_h = 16;
   int w                      = key_w * OSK_CHARS_PER_LINE + 12;
   int h                      = 46 + key_h * 4 + 6;
   int x                      = (PIX16_FB_WIDTH  - w) / 2;
   int y                      = (PIX16_FB_HEIGHT - h) / 2;
   size_t cols                = (size_t)((w - 12) / PIX16_GLYPH_W);
   size_t blen;
   int i;

   if (pix->theme == PIX16_THEME_JRPG)
      pix16_window(pix, x - 4, y - 4, w + 8, h + 8);
   else
   {
      pix16_panel(pix, x, y, w, h, pix->colors[PIX16_C_PANEL]);
      pix16_fill(pix, x, y, w, 2, pix->colors[PIX16_C_GREEN]);
   }
   pix16_text_fit(pix, x + 6, y + 8, label,
         pix->theme == PIX16_THEME_JRPG
         ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW], cols);

   /* entry field shows the tail of the buffer plus a cursor */
   pix16_fill(pix, x + 6, y + 20, w - 12, 14,
         pix->theme == PIX16_THEME_JRPG
         ? pix->colors[PIX16_C_J_EDGE] : pix->colors[PIX16_C_PAPER]);
   blen = pix16_strlen_glyphs(buffer);
   if (buffer)
   {
      const char *tail = buffer;
      while (blen > cols - 2 && *tail)
      {
         tail++;
         while ((((unsigned char)*tail) & 0xC0) == 0x80)
            tail++;
         blen--;
      }
      pix16_text(pix, x + 9, y + 23, tail,
            pix->theme == PIX16_THEME_JRPG
            ? pix->colors[PIX16_C_J_TEXT] : pix->colors[PIX16_C_DARK_TEXT], 1, 0);
   }
   pix16_fill(pix, x + 9 + (int)blen * PIX16_GLYPH_W, y + 23, 2, 8,
         pix->colors[PIX16_C_RED]);

   if (input_osk_native_active() || !grid || !grid[0])
      return;

   for (i = 0; i < OSK_CHARS_PER_LINE * 4; i++)
   {
      int kx         = x + 6 + (i % OSK_CHARS_PER_LINE) * key_w;
      int ky         = y + 42 + (i / OSK_CHARS_PER_LINE) * key_h;
      const char *k  = grid[i];
      bool sel       = (i == osk_ptr);
      size_t klen;

      if (!k)
         continue;
      k = pix16_osk_key_label(k);
      if (sel)
         pix16_fill(pix, kx + 1, ky + 1, key_w - 2, key_h - 2,
               pix->colors[PIX16_C_YELLOW]);
      klen = pix16_strlen_glyphs(k);
      if (klen > 3)
         klen = 3;
      pix16_text(pix, kx + (key_w - (int)klen * PIX16_GLYPH_W) / 2,
            ky + 4, k,
            sel ? pix->colors[PIX16_C_DARK_TEXT] : pix->colors[PIX16_C_TEXT],
            1, klen);
   }
}

/* ------------------------------------------------------------------
 * Driver callbacks
 * ------------------------------------------------------------------ */

/* ------------------------------------------------------------------
 * JRPG theme screens
 * ------------------------------------------------------------------ */

#define PIX16_J_BAR_H   24
#define PIX16_J_ROW_H   14
#define PIX16_J_HELP_H  40

/* Command window with the tabs; the glove points at the active one */
static void pix16_j_tabs(pix16_t *pix, int active, int x, int y, int w, int h)
{
   int order[PIX16_TAB_COUNT];
   int n    = pix16_opt_tabs(order);
   int cur  = pix->opt_back ? n : pix16_opt_pos(active);
   int slot = (w - 12) / (n + 1);
   int i;

   pix16_window(pix, x, y, w, h);
   for (i = 0; i <= n; i++)
   {
      const char *name = msg_hash_to_str((i == n) ? MSG_PIX16_TAB_BACK
            : pix16_tabs[order[i]].name);
      int sx = x + 6 + i * slot;
      int ty = y + (h - PIX16_GLYPH_H) / 2;
      if (i == cur)
      {
         pix16_glove_cursor(pix, sx + 2, ty - 1);
         pix16_text(pix, sx + 18, ty, name, pix->colors[PIX16_C_J_TEXT], 1, 0);
      }
      else
         pix16_text(pix, sx + 18, ty, name,
               (i == n) ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_J_DIM],
               1, 0);
   }
}

static void pix16_j_title(pix16_t *pix, const char *title, bool clock,
      int x, int y, int w, int h)
{
   char buf[NAME_MAX_LENGTH];
   int ty        = y + (h - PIX16_GLYPH_H) / 2;
   size_t max_ch = (size_t)((w - 20 - (clock ? 48 : 0)) / PIX16_GLYPH_W);

   strlcpy(buf, title ? title : "", sizeof(buf));
   string_to_upper(buf);
   pix16_window(pix, x, y, w, h);
   pix16_text_fit(pix, x + 10, ty, buf, pix->colors[PIX16_C_J_GOLD], max_ch);
   if (clock)
      pix16_draw_clock(pix, x + w - 10, ty, pix->colors[PIX16_C_J_TEXT]);
}

/* List window: no highlight bar, just the glove */
/* About panel, shown in the list window while BACK is highlighted */
static void pix16_j_about(pix16_t *pix, int x, int y, int w, int h)
{
   settings_t *settings = config_get_ptr();
   struct video_viewport vp;
   char res[48], crt[32], pad[96];
   const char *rows[8][2];
   const char *dev;
   float hz;
   int n = 0, k, ty;
   int label_w = 14 * PIX16_GLYPH_W;
   size_t max_val = (size_t)((w - 20 - label_w) / PIX16_GLYPH_W);

   /* output mode right now (with CRT SwitchRes this is the real mode) */
   memset(&vp, 0, sizeof(vp));
   res[0] = '\0';
   if (video_driver_get_viewport_info(&vp) && VIDEO_SCALE_W(vp.full_dims))
   {
      hz = video_driver_get_window_refresh_rate();
      if (hz <= 0.0f)
         hz = video_driver_get_refresh_rate();
      if (hz > 0.0f)
         snprintf(res, sizeof(res), "%ux%u @ %.2fHZ",
               VIDEO_SCALE_W(vp.full_dims), VIDEO_SCALE_H(vp.full_dims), hz);
      else
         snprintf(res, sizeof(res), "%ux%u",
               VIDEO_SCALE_W(vp.full_dims), VIDEO_SCALE_H(vp.full_dims));
   }
   else
      strlcpy(res, "-", sizeof(res));

   if (!settings || !settings->uints.crt_switch_resolution)
      strlcpy(crt, msg_hash_to_str(MSG_PIX16_OFF), sizeof(crt));
   else if (settings->uints.crt_switch_resolution_super)
      snprintf(crt, sizeof(crt), msg_hash_to_str(MSG_PIX16_ON_SUPER),
            settings->uints.crt_switch_resolution_super);
   else
      strlcpy(crt, msg_hash_to_str(MSG_PIX16_ON), sizeof(crt));

   dev = input_config_get_device_display_name(0);
   if (string_is_empty(dev))
      dev = input_config_get_device_name(0);
   strlcpy(pad, string_is_empty(dev) ? msg_hash_to_str(MSG_PIX16_NONE) : dev, sizeof(pad));

   rows[n][0] = msg_hash_to_str(MSG_PIX16_ABOUT_RETROARCH);
   rows[n++][1] = PACKAGE_VERSION;
   rows[n][0] = msg_hash_to_str(MSG_PIX16_ABOUT_VIDEO_DRIVER);
   rows[n++][1] = settings ? settings->arrays.video_driver : "-";
   rows[n][0] = msg_hash_to_str(MSG_PIX16_ABOUT_RESOLUTION);
   rows[n++][1] = res;
   rows[n][0] = msg_hash_to_str(MSG_PIX16_ABOUT_CRT_SWITCHRES);
   rows[n++][1] = crt;
   rows[n][0] = msg_hash_to_str(MSG_PIX16_ABOUT_AUDIO_DRIVER);
   rows[n++][1] = settings ? settings->arrays.audio_driver : "-";
   rows[n][0] = msg_hash_to_str(MSG_PIX16_ABOUT_INPUT_DRIVER);
   rows[n++][1] = settings ? settings->arrays.input_joypad_driver : "-";
   rows[n][0] = msg_hash_to_str(MSG_PIX16_ABOUT_CONTROLLER);
   rows[n++][1] = pad;

   pix16_window(pix, x, y, w, h);
   ty = y + 10;
   pix16_text(pix, x + 10, ty, msg_hash_to_str(MSG_PIX16_ABOUT), pix->colors[PIX16_C_J_GOLD], 1, 0);
   ty += 15;
   for (k = 0; k < n && ty + PIX16_GLYPH_H <= y + h - 5; k++)
   {
      char val[96];
      strlcpy(val, rows[k][1] ? rows[k][1] : "-", sizeof(val));
      string_to_upper(val);
      pix16_text(pix, x + 10, ty, rows[k][0], pix->colors[PIX16_C_J_DIM], 1, 0);
      pix16_text_ticker(pix, x + 10 + label_w, ty, val,
            pix->colors[PIX16_C_J_VALUE], max_val, true);
      ty += PIX16_J_ROW_H;
   }
}

/* ------------------------------------------------------------------
 * Favourites: a toggle in a game's Start menu, hearts in the lists
 * ------------------------------------------------------------------ */

#define PIX16_FAV_LABEL "pix16_fav_toggle"

static bool pix16_is_favorite(const char *content_path)
{
   return g_defaults.content_favorites
       && !string_is_empty(content_path)
       && playlist_entry_exists(g_defaults.content_favorites, content_path);
}

/* The list on screen is the Favorites playlist itself */
static bool pix16_viewing_favorites(void)
{
   settings_t *settings = config_get_ptr();
   playlist_t *pl       = playlist_get_cached();
   const char *conf     = pl ? playlist_get_conf_path(pl) : NULL;
   return settings && !string_is_empty(conf)
       && string_is_equal(conf, settings->paths.path_content_favorites);
}

/* The game the Start menu (Run / Load State / ...) is for */
static const struct playlist_entry *pix16_fav_target(void)
{
   menu_handle_t *menu = menu_state_get_ptr()->driver_data;
   playlist_t *pl      = playlist_get_cached();
   const struct playlist_entry *e = NULL;
   if (!menu || !pl || menu->rpl_entry_selection_ptr >= playlist_size(pl))
      return NULL;
   playlist_get_index(pl, menu->rpl_entry_selection_ptr, &e);
   return (e && !string_is_empty(e->path)) ? e : NULL;
}

/* A game's Start menu (not in a game): put the toggle under "Run" */
static void pix16_fav_add_entry(void)
{
   file_list_t *list = pix16_selection_buf();
   const struct playlist_entry *e;
   size_t k;
   if (     !list || list->size < 1
         || list->list[0].type != FILE_TYPE_PLAYLIST_ENTRY
         || menu_is_running_quick_menu()
         || !g_defaults.content_favorites)
      return;
   for (k = 0; k < list->size; k++)
      if (list->list[k].label && string_is_equal(list->list[k].label, PIX16_FAV_LABEL))
         return;
   if (!(e = pix16_fav_target()))
      return;
   /* RetroArch's own "Add to Favorites" (when its setting is on) would
    * be a second, one-way copy of the toggle: take it out */
   for (k = 0; k < list->size; k++)
      if (     list->list[k].label
            && string_is_equal(list->list[k].label,
               MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST_STR))
      {
         struct item_file tmp = list->list[k];
         memmove(&list->list[k], &list->list[k + 1],
               (list->size - k - 1) * sizeof(struct item_file));
         list->list[list->size - 1] = tmp;
         file_list_pop(list, NULL);
         break;
      }
   if (menu_entries_append(list,
            pix16_is_favorite(e->path) ? msg_hash_to_str(MSG_PIX16_REMOVE_FROM_FAVORITES)
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES),
            PIX16_FAV_LABEL, MSG_UNKNOWN, MENU_SETTING_ACTION, 0, 0, NULL)
         && list->size >= 3)
   {
      /* move it from the end up to row 1 */
      struct item_file tmp = list->list[list->size - 1];
      memmove(&list->list[2], &list->list[1],
            (list->size - 2) * sizeof(struct item_file));
      list->list[1] = tmp;
   }
}

static void pix16_fav_toggle(pix16_t *pix)
{
   const struct playlist_entry *e = pix16_fav_target();
   playlist_t *fav = g_defaults.content_favorites;
   if (!e || !fav)
      return;
   if (playlist_entry_exists(fav, e->path))
      playlist_delete_by_path(fav, e->path);
   else
   {
      struct playlist_entry ne;
      playlist_t *pl         = playlist_get_cached();
      const char *core_path  = e->core_path;
      const char *core_name  = e->core_name;
      /* "DETECT": take the playlist's default core so it can launch */
      if (string_is_empty(core_path) || string_is_equal(core_path, "DETECT"))
      {
         core_path = pl ? playlist_get_default_core_path(pl) : NULL;
         core_name = pl ? playlist_get_default_core_name(pl) : NULL;
      }
      memset(&ne, 0, sizeof(ne));
      ne.path      = e->path;
      ne.label     = e->label;
      ne.core_path = (char*)(string_is_empty(core_path) ? "DETECT" : core_path);
      ne.core_name = (char*)(string_is_empty(core_name) ? "DETECT" : core_name);
      ne.db_name   = e->db_name;
      ne.crc32     = e->crc32;
      playlist_push(fav, &ne);
   }
   playlist_write_file(fav);
   pix16_sfx(PIX16_SFX_OK);
   /* rebuild the list so the toggle shows its new text */
   menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
}

/* 7x6 pixel heart */
static void pix16_heart(pix16_t *pix, int x, int y)
{
   static const char *hp[6] = {
      ".##.##.",
      "#WR####",
      "#######",
      ".#####.",
      "..###..",
      "...#..."
   };
   uint16_t red = pix->conv(0xFFF04868);
   uint16_t hi  = pix->conv(0xFFFFD0E0);
   uint16_t sh  = pix->colors[PIX16_C_J_SHADOW];
   int gx, gy;
   for (gy = 0; gy < 6; gy++)
      for (gx = 0; gx < 7; gx++)
         if (hp[gy][gx] != '.')
            pix16_fill(pix, x + gx + 1, y + gy + 1, 1, 1, sh);
   for (gy = 0; gy < 6; gy++)
      for (gx = 0; gx < 7; gx++)
      {
         char c = hp[gy][gx];
         if (c == '.')
            continue;
         pix16_fill(pix, x + gx, y + gy, 1, 1, c == 'W' ? hi : red);
      }
}

static void pix16_j_list(pix16_t *pix, int x, int y, int w, int h)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *list          = pix16_selection_buf();
   size_t selection           = menu_st->selection_ptr;
   size_t i, begin, rows, count;
   int row_y0, row_y;
   int glove_x = 0, glove_y = 0;
   bool has_glove             = false;
   bool show_glove;
   bool hearts                = false;

   pix16_window(pix, x, y, w, h);
   pix->sel_cut = false;

   count = list ? list->size : 0;
   rows  = (size_t)((h - 12) / PIX16_J_ROW_H);

   if (count == 0)
   {
      pix16_text(pix, x + 24, y + 9, msg_hash_to_str(MSG_PIX16_NO_ITEMS),
            pix->colors[PIX16_C_J_DIM], 1, 0);
      return;
   }

   begin      = pix16_scroll(selection, count, rows);
   row_y0     = y + 6 + ((h - 12) - (int)rows * PIX16_J_ROW_H) / 2;
   show_glove = !(pix->opt_back && pix16_stack_depth() == 1);

   /* Selection bar. It rides with the glove, so the highlight and the
    * finger move as one; drawn before the rows so their text stays on
    * top of it. */
   if (selection >= begin && selection < begin + rows)
   {
      int pad   = (PIX16_J_ROW_H - PIX16_GLYPH_H) / 2;
      int ty    = row_y0 + (int)(selection - begin) * PIX16_J_ROW_H + pad;
      int bar_y = ty - pad;
      if (show_glove)
      {
         pix16_glove_update(pix, x + 7, ty - 1,
               pix16_glove_context(pix, 2 + (unsigned)x), &glove_x, &glove_y);
         bar_y     = glove_y + 1 - pad;
         has_glove = true;
      }
      pix16_vgradient(pix, x + 6, bar_y, w - 12, PIX16_J_ROW_H,
            pix->sel_top, pix->sel_bot);
   }

   /* game lists get a heart column (not the Favorites list itself) */
   {
      size_t k;
      hearts = false;
      for (k = begin; k < count && k < begin + rows; k++)
         if (list->list[k].type == FILE_TYPE_RPL_ENTRY)
         {
            hearts = !pix16_viewing_favorites();
            break;
         }
   }

   for (i = begin, row_y = row_y0; i < count && i < begin + rows;
         i++, row_y += PIX16_J_ROW_H)
   {
      bool selected    = (i == selection);
      const char *label, *value;
      size_t value_len = 0, max_label;
      int ty           = row_y + (PIX16_J_ROW_H - PIX16_GLYPH_H) / 2;
      int right        = x + w - 12;

      pix16_get_entry(pix, i, false);
      label = pix16_entry_label(&pix->entry);
      value = pix16_entry_value(&pix->entry);
      if (list->list[i].label && string_is_equal(list->list[i].label, PIX16_FAV_LABEL))
         value = NULL;   /* the toggle has no "..." */

      if (!string_is_empty(value))
      {
         value_len = pix16_strlen_glyphs(value);
         if (value_len > 12)
            value_len = 12;
         pix16_text_fit(pix, right - (int)value_len * PIX16_GLYPH_W, ty,
               value, pix->colors[PIX16_C_J_VALUE], value_len);
      }
      if (pix->entry.flags & MENU_ENTRY_FLAG_CHECKED)
         pix16_glyph(pix, right - (int)(value_len + 2) * PIX16_GLYPH_W, ty,
               '*', pix->colors[PIX16_C_J_GOLD], 1);

      {
         int lx = x + 24 + (hearts ? 10 : 0);
         if (     hearts && list->list[i].type == FILE_TYPE_RPL_ENTRY
               && pix16_is_favorite(list->list[i].label))
            pix16_heart(pix, x + 23, ty + 1);
         max_label = (size_t)((right - lx - 4
                  - (value_len ? (int)(value_len + 1) * PIX16_GLYPH_W : 0))
                  / PIX16_GLYPH_W);
         pix16_text_ticker(pix, lx, ty, label,
               pix->colors[PIX16_C_J_TEXT], max_label, selected);
         if (selected && pix16_strlen_glyphs(label) > max_label)
         {
            pix->sel_cut = true;
            strlcpy(pix->sel_full, label, sizeof(pix->sel_full));
         }
      }
   }

   if (has_glove)
      pix16_glove_cursor(pix, glove_x, glove_y);

   /* more above / below: little gold arrows on the frame */
   if (begin > 0)
      pix16_text(pix, x + w - 14, y + 2, "^", pix->colors[PIX16_C_J_GOLD], 1, 0);
   if (begin + rows < count)
      pix16_text(pix, x + w - 14, y + h - 10, "v", pix->colors[PIX16_C_J_GOLD], 1, 0);

   /* scroll thumb: how far through the list this window sits */
   if (count > rows)
   {
      int top   = y + 6 + ((h - 12) - (int)rows * PIX16_J_ROW_H) / 2;
      int track = (int)rows * PIX16_J_ROW_H;
      int th    = (int)((long)track * (long)rows / (long)count);
      int ty;
      if (th < 4)
         th = 4;
      ty = top + (int)((long)(track - th) * (long)begin
            / (long)(count - rows));
      pix16_fill(pix, x + w - 4, top, 1, track, pix->colors[PIX16_C_J_EDGE]);
      pix16_fill(pix, x + w - 5, ty, 3, th, pix->colors[PIX16_C_J_GOLD]);
   }
}

static void pix16_j_help(pix16_t *pix, const char *text,
      int x, int y, int w, int h)
{
   pix16_window(pix, x, y, w, h);
   if (!string_is_empty(text))
      pix16_wrap(pix, x + 10, y + 9, text, pix->colors[PIX16_C_J_TEXT],
            (size_t)((w - 20) / PIX16_GLYPH_W), 2, 12);
}

/* Systems grid, JRPG style: every tile is a small window; the
 * selected one is brighter, framed in light and gold, with the glove */
static void pix16_j_tile(pix16_t *pix, int x, int y, int w, int h,
      bool selected)
{
   pix16_record(pix, x, y, w, h, PIX16_RECT_TILE);
   pix16_round_fill(pix, x, y, w, h, 1, pix->colors[PIX16_C_J_EDGE]);
   if (selected)
   {
      pix16_round_fill(pix, x + 1, y + 1, w - 2, h - 2, 1,
            pix->colors[PIX16_C_J_BORDER]);
      pix16_fill(pix, x + 2, y + 2, w - 4, h - 4, pix->colors[PIX16_C_J_GOLD]);
      pix16_vgradient(pix, x + 3, y + 3, w - 6, h - 6,
            pix->sel_top, pix->sel_bot);
   }
   else
   {
      pix16_round_fill(pix, x + 1, y + 1, w - 2, h - 2, 1,
            pix->colors[PIX16_C_J_BORDER_LO]);
      pix16_vgradient(pix, x + 2, y + 2, w - 4, h - 4,
            pix->win_top, pix->win_bot);
   }
}

static void pix16_j_grid_tile(pix16_t *pix, size_t list_idx, int tx,
      int ty, int tw, bool selected)
{
   pix16_tile_info_t t;
   int bob = 0;
   pix16_tile_info(pix, list_idx, &t);

   if (selected && !pix->reduced_motion)
   {
      /* the chosen console floats up and down a pixel */
      static const int8_t seq[6] = { 0, -1, -1, 0, 1, 1 };
      bob = seq[(menu_driver_get_current_time() / 1000 / 130) % 6];
   }
   pix16_j_tile(pix, tx, ty, tw, PIX16_TILE_H, selected);
   pix->text_shadow = true;
   pix16_tile_icon(pix, &t, tx, ty + 3 + bob, tw, pix->colors[PIX16_C_J_TEXT],
         pix->colors[PIX16_C_J_GOLD]);
   pix16_tile_label(pix, &t, tx,
         ty + PIX16_TILE_H - PIX16_LABEL_PAD - PIX16_GLYPH_H, tw,
         selected ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_J_TEXT],
         selected);
   pix->text_shadow = false;
}


/* ------------------------------------------------------------------
 * Button guide
 * ------------------------------------------------------------------ */

typedef struct
{
   const char *label;
   char key;          /* 'A' ok, 'B' back, 'L' shoulders, 'S' start */
} pix16_guide_item_t;

static bool pix16_is_grid_view(void);

/* What the buttons do right now */
static size_t pix16_guide_build(pix16_t *pix, pix16_guide_item_t *g)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *list          = pix16_selection_buf();
   size_t sel                 = menu_st->selection_ptr;
   size_t n                   = 0;

#define PIX16_G(k, l) do { g[n].key = (k); g[n].label = (l); n++; } while (0)
   if (pix->order_focus || pix->order_open)
   {
      PIX16_G('A', "ORDER");
      return n;
   }
   if (pix->quit_confirm)
   {
      PIX16_G('A', msg_hash_to_str(MSG_PIX16_GUIDE_OK));
      PIX16_G('B', msg_hash_to_str(MSG_PIX16_GUIDE_BACK));
   }
   else if (pix->wiz.open)
   {
      PIX16_G('A', msg_hash_to_str(MSG_PIX16_GUIDE_SELECT));
      PIX16_G('B', msg_hash_to_str(MSG_PIX16_GUIDE_BACK));
   }
   else if (pix->arcade_open)
   {
      PIX16_G('A', msg_hash_to_str(MSG_PIX16_GUIDE_OPEN));
      PIX16_G('B', msg_hash_to_str(MSG_PIX16_GUIDE_BACK));
   }
   else if (pix->game_menu_open)
   {
      PIX16_G('A', msg_hash_to_str(MSG_PIX16_GUIDE_OK));
      PIX16_G('B', msg_hash_to_str(MSG_PIX16_GUIDE_BACK));
   }
   else if (menu_is_running_quick_menu() && !pix->qm_full)
   {
      PIX16_G('A', msg_hash_to_str(MSG_PIX16_GUIDE_OK));
      PIX16_G('B', msg_hash_to_str(MSG_PIX16_GUIDE_RESUME));
      PIX16_G('S', msg_hash_to_str(MSG_PIX16_GUIDE_MORE));
   }
   else if (pix16_is_grid_view())
   {
      PIX16_G('A', pix->gear_focus ? msg_hash_to_str(MSG_PIX16_GUIDE_OPTIONS)
                 : pix->power_focus ? msg_hash_to_str(MSG_PIX16_GUIDE_EXIT)
                 : pix->add_focus ? msg_hash_to_str(MSG_PIX16_GUIDE_ADD_SYSTEM)
                 : msg_hash_to_str(MSG_PIX16_GUIDE_OPEN));
      if (pix->grid_pages > 1 && !PIX16_BAR_FOCUS(pix))
         PIX16_G('L', msg_hash_to_str(MSG_PIX16_GUIDE_PAGE));
   }
   else if (menu_is_running_quick_menu())
   {
      PIX16_G('A', msg_hash_to_str(MSG_PIX16_GUIDE_OK));
      PIX16_G('B', msg_hash_to_str(MSG_PIX16_GUIDE_BACK));
   }
   else if (list && sel < list->size
         && list->list[sel].type == FILE_TYPE_RPL_ENTRY)
   {
      PIX16_G('A', msg_hash_to_str(MSG_PIX16_GUIDE_PLAY));
      PIX16_G('B', msg_hash_to_str(MSG_PIX16_GUIDE_BACK));
      PIX16_G('S', msg_hash_to_str(MSG_PIX16_GUIDE_MORE));
      PIX16_G('L', msg_hash_to_str(MSG_PIX16_GUIDE_PAGE));
   }
   else
   {
      PIX16_G('A', msg_hash_to_str(MSG_PIX16_GUIDE_OK));
      PIX16_G('B', msg_hash_to_str(MSG_PIX16_GUIDE_BACK));
      PIX16_G('L', pix16_stack_depth() <= 1 ? msg_hash_to_str(MSG_PIX16_GUIDE_TABS) : msg_hash_to_str(MSG_PIX16_GUIDE_PAGE));
   }
#undef PIX16_G
   return n;
}

/* START as a small rounded pill */
static int pix16_pill(pix16_t *pix, int x, int y, const char *text,
      uint16_t bg, uint16_t fg)
{
   int pad = PIX16_PILL_PAD(pix);
   int w   = (int)strlen(text) * PIX16_GLYPH_W + pad;
   bool sh = pix->text_shadow;
   pix16_round_fill(pix, x, y, w, 10, 1, bg);
   pix->text_shadow = false;
   pix16_text(pix, x + pad / 2, y + 1, text, fg, 1, 0);
   pix->text_shadow = sh;
   return w;
}

static size_t pix16_guide_build(pix16_t *pix, pix16_guide_item_t *g);

/* Width of one guide item, its gap to the next included */
static int pix16_guide_item_width(pix16_t *pix, const pix16_guide_item_t *it)
{
   const char *l, *r, *start;
   int pad = PIX16_PILL_PAD(pix);
   int w   = (int)strlen(it->label) * PIX16_GLYPH_W + PIX16_GUIDE_GAP(pix);
   pix16_button_names(pix, &l, &r, &start);
   switch (it->key)
   {
      case 'A':
      case 'B':
         w += pix16_okback_width(pix, it->key == 'A');
         break;
      case 'L':
         w += (int)(strlen(l) + strlen(r)) * PIX16_GLYPH_W + 2 * pad
               + PIX16_PAIR_GAP(pix) + PIX16_PILL_GAP(pix);
         break;
      case 'S':
         w += (int)strlen(start) * PIX16_GLYPH_W + pad + PIX16_PILL_GAP(pix);
         break;
   }
   return w;
}

/* The guide's items that fit in max_w (the last ones are dropped
 * first); *w gets their width */
static size_t pix16_guide_fit(pix16_t *pix, pix16_guide_item_t *g,
      int max_w, int *w)
{
   size_t n = pix16_guide_build(pix, g), k;
   int gap   = PIX16_GUIDE_GAP(pix);
   int total = 0;
   for (k = 0; k < n; k++)
   {
      int iw = pix16_guide_item_width(pix, &g[k]);
      if (k > 0 && total + iw - gap > max_w)
         break;
      total += iw;
   }
   *w = total > 0 ? total - gap : 0;
   return k;
}

static int pix16_draw_guide(pix16_t *pix, int x, int right, int y, bool jrpg)
{
   pix16_guide_item_t g[6];
   uint16_t text  = jrpg ? pix->colors[PIX16_C_J_TEXT] : pix->colors[PIX16_C_TEXT];
   uint16_t lr_bg = jrpg ? pix->colors[PIX16_C_J_BORDER] : pix->colors[PIX16_C_LAVENDER];
   uint16_t lr_fg = jrpg ? pix->colors[PIX16_C_J_EDGE]   : pix->colors[PIX16_C_DARK_TEXT];
   const char *l, *r, *start;
   size_t n, k;
   int w;

   memset(g, 0, sizeof(g));
   n = pix16_guide_fit(pix, g, right - x, &w);
   pix16_button_names(pix, &l, &r, &start);
   for (k = 0; k < n; k++)
   {
      switch (g[k].key)
      {
         case 'A':
         case 'B':
            x += pix16_okback(pix, x, y, g[k].key == 'A', jrpg);
            break;
         case 'L':
            x += pix16_pill(pix, x, y, l, lr_bg, lr_fg) + PIX16_PAIR_GAP(pix);
            x += pix16_pill(pix, x, y, r, lr_bg, lr_fg) + PIX16_PILL_GAP(pix);
            break;
         case 'S':
            x += pix16_pill(pix, x, y, start, lr_bg, lr_fg) + PIX16_PILL_GAP(pix);
            break;
      }
      pix->text_shadow = jrpg;
      pix16_text(pix, x, y + 1, g[k].label, text, 1, 0);
      pix->text_shadow = false;
      x += (int)strlen(g[k].label) * PIX16_GLYPH_W + PIX16_GUIDE_GAP(pix);
   }
   return x;
}

/* Guide centred between left and right */
static int pix16_draw_guide_c(pix16_t *pix, int left, int right, int y, bool jrpg)
{
   pix16_guide_item_t g[6];
   int w;
   memset(g, 0, sizeof(g));
   pix16_guide_fit(pix, g, right - left, &w);
   return pix16_draw_guide(pix, left + (right - left - w) / 2, right, y, jrpg);
}

static const signed char pix16_sin64[64] = {
   0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126,
   127, 126, 125, 122, 117, 112, 106, 98, 90, 81, 71, 60, 49, 37, 25, 12,
   0, -12, -25, -37, -49, -60, -71, -81, -90, -98, -106, -112, -117, -122, -125, -126,
   -127, -126, -125, -122, -117, -112, -106, -98, -90, -81, -71, -60, -49, -37, -25, -12
};

/* v in 1/16 steps of bg_ramp, Bayer dithered between steps */
static uint16_t pix16_bg_level(const pix16_t *pix, int v, int x, int y)
{
   int l;
   if (v < 0)
      v = 0;
   l = v >> 4;
   if ((v & 15) > (int)pix16_bayer4[y & 3][x & 3])
      l++;
   return pix->bg_ramp[l > 7 ? 7 : l];
}

static void pix16_bg_plot(pix16_t *pix, int x, int y, uint16_t c,
      int x0, int y0, int x1, int y1)
{
   if (x >= x0 && x < x1 && y >= y0 && y < y1)
      pix->fb[(size_t)y * PIX16_FB_WIDTH + x] = c;
}

/* Soft dithered diamonds (Bayer 4x4, like the window gradients);
 * wavy shifts each scanline like an HDMA heat haze */
static void pix16_bg_diamonds(pix16_t *pix, int x0, int y0, int w, int h,
      int t, bool wavy)
{
   uint16_t dark  = pix->bg_dark;
   uint16_t light = pix->bg_light;
   int off = (t / 70) & 1023;
   int x, y;
   for (y = y0; y < y0 + h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * PIX16_FB_WIDTH;
      int v  = ((y + off / 2) & 31) - 16;
      int av = v < 0 ? -v : v;
      int sx = off;
      if (wavy)
         sx = pix16_sin64[((y >> 1) + t / 45) & 63] * 6 / 127;
      for (x = x0; x < x0 + w; x++)
      {
         int u   = ((x + sx) & 31) - 16;
         int d   = (u < 0 ? -u : u) + av;      /* 0 (centre) .. 32 */
         int lvl = (24 - d) * 16 / 14;         /* soft edge        */
         if (lvl > 9) lvl = 9;                 /* never full bright */
         row[x] = (lvl > (int)pix16_bayer4[y & 3][x & 3]) ? light : dark;
      }
   }
}

/* Checkered plane in perspective, gliding toward the viewer */
static void pix16_bg_mode7(pix16_t *pix, int x0, int y0, int w, int h, int t)
{
   int hz   = PIX16_FB_HEIGHT * 2 / 5;
   int cx   = PIX16_FB_WIDTH / 2;
   int sway = pix16_sin64[(t / 160) & 63];
   int x, y;
   for (y = y0; y < y0 + h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * PIX16_FB_WIDTH;
      if (y < hz)
      {
         int v = 4 + y * 36 / hz;
         for (x = x0; x < x0 + w; x++)
            row[x] = pix16_bg_level(pix, v, x, y);
      }
      else
      {
         int dy   = y - hz + 1;
         int z    = 2048 / dy;
         int fog  = dy * 16 / (PIX16_FB_HEIGHT - hz);
         int vv   = (z + t / 12) >> 4;
         int u    = (x0 - cx) * z + sway * z / 2;
         for (x = x0; x < x0 + w; x++, u += z)
         {
            int lv = (((u >> 9) ^ vv) & 1) ? 72 : 16;
            row[x] = pix16_bg_level(pix, 40 + (lv - 40) * fog / 16, x, y);
         }
      }
   }
}

static void pix16_bg_gradient(pix16_t *pix, int x0, int y0, int w, int h,
      int top, int bot)
{
   int x, y;
   for (y = y0; y < y0 + h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * PIX16_FB_WIDTH;
      int v = top + (bot - top) * y / PIX16_FB_HEIGHT;
      for (x = x0; x < x0 + w; x++)
         row[x] = pix16_bg_level(pix, v, x, y);
   }
}

/* Three layers of stars scrolling at different speeds */
static void pix16_bg_stars(pix16_t *pix, int x0, int y0, int w, int h, int t)
{
   static const int div[3] = { 90, 45, 20 };
   uint32_t seed = 0x1234567u;
   int fw = PIX16_FB_WIDTH;
   int i;
   pix16_bg_gradient(pix, x0, y0, w, h, 0, 20);
   for (i = 0; i < 120; i++)
   {
      int layer = i % 3;
      int sx, sy, x;
      uint16_t c;
      seed = seed * 1103515245u + 12345u;
      sx   = (int)((seed >> 8) % (uint32_t)fw);
      seed = seed * 1103515245u + 12345u;
      sy   = (int)((seed >> 8) % (uint32_t)PIX16_FB_HEIGHT);
      x    = (sx - (t / div[layer]) % fw + fw) % fw;
      c    = pix->bg_ramp[3 + layer * 2];
      if (layer == 2 && ((t / 250 + i) % 9) == 0)
         c = pix->bg_ramp[5];
      pix16_bg_plot(pix, x, sy, c, x0, y0, x0 + w, y0 + h);
      if (layer == 2)
      {
         pix16_bg_plot(pix, x - 1, sy, pix->bg_ramp[3], x0, y0, x0 + w, y0 + h);
         pix16_bg_plot(pix, x + 1, sy, pix->bg_ramp[3], x0, y0, x0 + w, y0 + h);
      }
   }
}

/* Fixed ripple field whose palette cycles, like SNES water */
static void pix16_bg_water(pix16_t *pix, int x0, int y0, int w, int h, int t)
{
   int phase = t / 25;
   int x, y;
   for (y = y0; y < y0 + h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * PIX16_FB_WIDTH;
      int wy = pix16_sin64[(y >> 1) & 63] >> 5;
      for (x = x0; x < x0 + w; x++)
      {
         int f = pix16_sin64[(x / 3 + wy) & 63]
               + pix16_sin64[(y / 2 + x / 5) & 63];
         int c = ((f + 256) / 2 + phase) & 127;
         row[x] = pix16_bg_level(pix, c < 64 ? c : 127 - c, x, y);
      }
   }
}

/* Two bands of clouds drifting at different speeds */
static void pix16_bg_clouds(pix16_t *pix, int x0, int y0, int w, int h, int t)
{
   static const int cy_n[3]  = { 1, 3, 9 };   /* band centre, of 12 */
   static const int div[3]   = { 70, 40, 25 };
   static const int base[3]  = { 28, 40, 52 };
   int fh = PIX16_FB_HEIGHT;
   int x, y, b;
   for (y = y0; y < y0 + h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * PIX16_FB_WIDTH;
      int sky = 6 + y * 24 / fh;
      for (x = x0; x < x0 + w; x++)
      {
         int v = sky;
         for (b = 0; b < 3; b++)
         {
            int dy = y - fh * cy_n[b] / 12;
            int hh = fh / 7;
            int band, xo, d;
            if (dy < 0)
               dy = -dy;
            if (dy >= hh)
               continue;
            band = 127 - dy * 127 / hh;
            xo   = x + t / div[b] + b * 97;
            d    = pix16_sin64[(xo / 4) & 63]
                 + pix16_sin64[(xo / 9 + 17) & 63]
                 + pix16_sin64[(xo / 2 + y / 3) & 63] / 2;
            d    = d + band * 2 - 190;
            if (d > 0)
               v = base[b] + (d > 96 ? 24 : d / 4);
         }
         row[x] = pix16_bg_level(pix, v, x, y);
      }
   }
}

static void pix16_bg_plasma(pix16_t *pix, int x0, int y0, int w, int h, int t)
{
   int x, y;
   for (y = y0; y < y0 + h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * PIX16_FB_WIDTH;
      int ry = pix16_sin64[(y / 4 - t / 70) & 63];
      for (x = x0; x < x0 + w; x++)
      {
         int a = pix16_sin64[(x / 5 + t / 50) & 63] + ry
               + pix16_sin64[((x + y) / 7 + t / 90) & 63]
               + pix16_sin64[((x - y + 512) / 6 - t / 60) & 63];
         int c = ((a + 512) / 4) & 127;
         row[x] = pix16_bg_level(pix, c < 64 ? c : 127 - c, x, y);
      }
   }
}

/* Slanted rain streaks, or snow swaying as it falls */
static void pix16_bg_weather(pix16_t *pix, int x0, int y0, int w, int h,
      int t, bool snow)
{
   uint32_t seed = 0x2468ACEu;
   int fw = PIX16_FB_WIDTH;
   int fh = PIX16_FB_HEIGHT + 8;
   int x1 = x0 + w, y1 = y0 + h;
   int i, k;
   pix16_bg_gradient(pix, x0, y0, w, h, 2, 26);
   for (i = 0; i < 90; i++)
   {
      int depth = i % 3;
      int sx, sy, x, y;
      seed = seed * 1103515245u + 12345u;
      sx   = (int)((seed >> 8) % (uint32_t)fw);
      seed = seed * 1103515245u + 12345u;
      sy   = (int)((seed >> 8) % (uint32_t)fh);
      if (snow)
      {
         uint16_t c = pix->bg_ramp[4 + depth + (depth == 2)];
         y = (sy + t * (depth + 1) / 60) % fh - 4;
         x = (sx + pix16_sin64[(t / 40 + i * 7) & 63] * (depth + 2) / 127
               + fw) % fw;
         pix16_bg_plot(pix, x, y, c, x0, y0, x1, y1);
         if (depth == 2)
         {
            pix16_bg_plot(pix, x + 1, y,     c, x0, y0, x1, y1);
            pix16_bg_plot(pix, x,     y + 1, c, x0, y0, x1, y1);
            pix16_bg_plot(pix, x + 1, y + 1, c, x0, y0, x1, y1);
         }
      }
      else
      {
         y = (sy + t * (depth + 2) / 8) % fh - 4;
         x = (sx - y / 3 + fw * 2) % fw;
         for (k = 0; k < 4 + depth * 2; k++)
            pix16_bg_plot(pix, x + k / 3, y - k,
                  pix->bg_ramp[k ? 3 + depth : 5 + depth], x0, y0, x1, y1);
      }
   }
}

/* Raster bars bobbing up and down, later bars drawn over earlier */
static void pix16_bg_copper(pix16_t *pix, int x0, int y0, int w, int h, int t)
{
   int fh = PIX16_FB_HEIGHT;
   int x, y, b;
   for (y = y0; y < y0 + h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * PIX16_FB_WIDTH;
      int v = 4;
      for (b = 0; b < 5; b++)
      {
         int yc = fh / 2 + pix16_sin64[(t / 90 + b * 9) & 63] * (fh * 3 / 8) / 127;
         int d  = y - yc;
         if (d < 0)
            d = -d;
         if (d < 14)
            v = 108 - d * d * 108 / 196;
      }
      for (x = x0; x < x0 + w; x++)
         row[x] = pix16_bg_level(pix, v, x, y);
   }
}

/* Animated backdrop behind the windows, picked by the Background setting */
static void pix16_j_background(pix16_t *pix, int x0, int y0, int w, int h)
{
   int t = (int)((menu_driver_get_current_time() / 1000) & 0x7FFFFFF);
   if (pix->reduced_motion)
      t = 0;                   /* the backdrop holds still */
   if (x0 < 0)
   {
      w += x0;
      x0 = 0;
   }
   if (y0 < 0)
   {
      h += y0;
      y0 = 0;
   }
   if (x0 + w > PIX16_FB_WIDTH)
      w = PIX16_FB_WIDTH  - x0;
   if (y0 + h > PIX16_FB_HEIGHT)
      h = PIX16_FB_HEIGHT - y0;
   if (w <= 0 || h <= 0)
      return;
   switch (pix->background)
   {
      case PIX16_BACKGROUND_MODE7:
         pix16_bg_mode7(pix, x0, y0, w, h, t);
         break;
      case PIX16_BACKGROUND_STARS:
         pix16_bg_stars(pix, x0, y0, w, h, t);
         break;
      case PIX16_BACKGROUND_WAVY:
         pix16_bg_diamonds(pix, x0, y0, w, h, t, true);
         break;
      case PIX16_BACKGROUND_WATER:
         pix16_bg_water(pix, x0, y0, w, h, t);
         break;
      case PIX16_BACKGROUND_CLOUDS:
         pix16_bg_clouds(pix, x0, y0, w, h, t);
         break;
      case PIX16_BACKGROUND_PLASMA:
         pix16_bg_plasma(pix, x0, y0, w, h, t);
         break;
      case PIX16_BACKGROUND_RAIN:
         pix16_bg_weather(pix, x0, y0, w, h, t, false);
         break;
      case PIX16_BACKGROUND_SNOW:
         pix16_bg_weather(pix, x0, y0, w, h, t, true);
         break;
      case PIX16_BACKGROUND_COPPER:
         pix16_bg_copper(pix, x0, y0, w, h, t);
         break;
      default:
         pix16_bg_diamonds(pix, x0, y0, w, h, t, false);
         break;
   }
}

/* Draws a window/tile at scale s (0..1), opening from a line across
 * its middle like an FF window */
static void pix16_rect_scaled(pix16_t *pix, const pix16_rect_t *r, float s)
{
   int h, y;
   if (s <= 0.0f)
      return;
   if (s > 1.0f)
      s = 1.0f;
   h = (int)(r->h * s + 0.5f);
   y = r->y + (r->h - h) / 2;
   if (h < 8)
   {
      pix16_fill(pix, r->x, r->y + r->h / 2 - 1, r->w, 2,
            pix->colors[PIX16_C_J_BORDER]);
      return;
   }
   if (r->kind == PIX16_RECT_TILE)
      pix16_j_tile(pix, r->x, y, r->w, h, false);
   else
      pix16_window(pix, r->x, y, r->w, h);
}

static float pix16_ease_out(float t)
{
   if (t < 0.0f)
      t = 0.0f;
   if (t > 1.0f)
      t = 1.0f;
   return 1.0f - (1.0f - t) * (1.0f - t);
}

/* smoothstep: leaves and arrives at rest, so the eye reads a glide */
static float pix16_ease_in_out(float t)
{
   if (t < 0.0f)
      t = 0.0f;
   if (t > 1.0f)
      t = 1.0f;
   return t * t * (3.0f - 2.0f * t);
}

/* Popup windows: open with a grow animation. Returns false while
 * opening (only the frame is drawn; the caller skips its contents). */
static bool pix16_popup_window(pix16_t *pix, int x, int y, int w, int h)
{
   retro_time_t now = menu_driver_get_current_time();
   float t = (float)(now - pix->pop_t0) / (PIX16_POP_MS * 1000.0f);
   pix->pop_rect.x = (int16_t)x; pix->pop_rect.y = (int16_t)y;
   pix->pop_rect.w = (int16_t)w; pix->pop_rect.h = (int16_t)h;
   pix->pop_rect.kind = PIX16_RECT_WIN;
   if (t < 1.0f && !pix->reduced_motion)
   {
      bool rec = pix->rec_on;
      pix->rec_on = false;
      pix16_rect_scaled(pix, &pix->pop_rect, pix16_ease_out(t));
      pix->rec_on = rec;
      pix16_record(pix, x, y, w, h, PIX16_RECT_WIN);
      pix->flags |= PIX16_FLAG_FORCE_REDRAW;
      return false;
   }
   pix16_window(pix, x, y, w, h);
   return true;
}

/* Screen identity: a change means "new screen", which closes the old
 * windows and opens the new ones */
static uint32_t pix16_screen_key(const pix16_t *pix, size_t depth, int tab,
      bool quick)
{
   uint32_t h = 2166136261u;
   const char *p;
   for (p = pix->title; *p; p++)
      h = (h ^ (unsigned char)*p) * 16777619u;
   h ^= (uint32_t)depth * 0x9E3779B1u;
   h ^= (uint32_t)(tab + 1) * 0x85EBCA77u;
   h ^= quick ? 0x27D4EB2Fu : 0;
   h ^= (quick && !pix->qm_full) ? 0x165667B1u : 0;
   return h;
}

/* After the frame is drawn: during a transition, put the background
 * back and draw the closing (old) or opening (new) windows instead */
static void pix16_transition_overlay(pix16_t *pix, bool scrim)
{
   retro_time_t now = menu_driver_get_current_time();
   float ms         = (float)(now - pix->tr_t0) / 1000.0f;
   float close_ms   = pix->tr_old_n ? (float)PIX16_TR_CLOSE_MS : 0.0f;
   bool rec         = pix->rec_on;
   int k;

   if (!pix->tr_active)
      return;
   if (pix->reduced_motion)
   {
      pix->tr_active = false;
      return;
   }
   if (ms >= close_ms + PIX16_TR_OPEN_MS)
   {
      pix->tr_active = false;
      return;
   }
   pix->rec_on = false;
   if (scrim)
      pix16_fill(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT,
            pix->colors[PIX16_C_SCRIM]);
   else
      pix16_j_background(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT);

   if (ms < close_ms)
   {
      float s = 1.0f - pix16_ease_in_out(ms / close_ms);
      for (k = 0; k < pix->tr_old_n; k++)
         pix16_rect_scaled(pix, &pix->tr_old[k], s);
   }
   else
   {
      float s = pix16_ease_in_out((ms - close_ms) / PIX16_TR_OPEN_MS);
      for (k = 0; k < pix->rec_n; k++)
         pix16_rect_scaled(pix, &pix->rec[k], s);
   }
   pix->rec_on = rec;
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
}

/* Under the grid: button guide, page dots on the right */
static void pix16_j_grid_footer(pix16_t *pix, int y)
{
   pix16_draw_guide_c(pix, PIX16_SAFE, PIX16_FB_WIDTH - PIX16_SAFE, y, true);
   pix16_page_dots(pix, PIX16_FB_WIDTH - PIX16_SAFE, y,
         pix->colors[PIX16_C_J_GOLD], pix->colors[PIX16_C_J_BORDER_LO]);
}

/* Everything except the systems grid, JRPG style */
static void pix16_j_screen(pix16_t *pix, bool quick, bool at_root,
      int tab, size_t depth)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *list          = pix16_selection_buf();
   size_t sel                 = menu_st->selection_ptr;
   int x                      = PIX16_SAFE;
   int y                      = PIX16_SAFE;
   int w                      = PIX16_FB_WIDTH  - 2 * PIX16_SAFE;
   int guide_y                = PIX16_FB_HEIGHT - PIX16_SAFE - PIX16_FOOTER_H;
   int bottom                 = guide_y - 4;
   int list_y                 = y + PIX16_J_BAR_H + 2;

   if (quick)
   {
      /* small windows over the paused game */
      int qw = w;   /* full width: long option names fit */
      pix16_fill(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT,
            pix->colors[PIX16_C_SCRIM]);
      pix16_j_title(pix, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS), false, x, y, qw, PIX16_J_BAR_H);
      pix16_j_list(pix, x, list_y, qw, bottom - list_y);
      pix16_draw_guide_c(pix, x, x + w, guide_y, true);
      return;
   }

   pix16_j_background(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT);

   if (at_root)
      pix16_j_tabs(pix, tab, x, y, w, PIX16_J_BAR_H);
   else
      pix16_j_title(pix, pix->shown_title, true, x, y, w, PIX16_J_BAR_H);

   if (at_root && pix->opt_back)
      pix16_j_about(pix, x, list_y, w, bottom - PIX16_J_HELP_H - 2 - list_y);
   else
      pix16_j_list(pix, x, list_y, w,
            bottom - PIX16_J_HELP_H - 2 - list_y);

   if (at_root && pix->opt_back)
      pix16_j_help(pix, msg_hash_to_str(MSG_PIX16_ABOUT_HELP),
            x, bottom - PIX16_J_HELP_H, w, PIX16_J_HELP_H);
   else if (list && sel < list->size && pix->sel_cut)
   {
      /* name too long for the list: in full here, then the description */
      int hx = x, hy = bottom - PIX16_J_HELP_H, cols = (w - 20) / PIX16_GLYPH_W;
      int lines;
      pix16_get_entry(pix, sel, true);
      pix16_window(pix, hx, hy, w, PIX16_J_HELP_H);
      lines = pix16_wrap(pix, hx + 10, hy + 9, pix->sel_full,
            pix->colors[PIX16_C_J_GOLD], (size_t)cols, 2, 12);
      if (lines < 2 && *pix->entry.sublabel
            && !string_is_equal(pix->entry.sublabel, pix->sel_full))
         pix16_wrap(pix, hx + 10, hy + 9 + 12 * lines, pix->entry.sublabel,
               pix->colors[PIX16_C_J_TEXT], (size_t)cols, 2 - lines, 12);
   }
   else if (list && sel < list->size)
   {
      pix16_get_entry(pix, sel, true);
      pix16_j_help(pix,
            *pix->entry.sublabel ? pix->entry.sublabel
                                 : pix16_entry_label(&pix->entry),
            x, bottom - PIX16_J_HELP_H, w, PIX16_J_HELP_H);
   }
   else
      pix16_j_help(pix, NULL, x, bottom - PIX16_J_HELP_H, w, PIX16_J_HELP_H);

   /* centred; at the top level the clock takes the right end */
   pix16_draw_guide_c(pix, x, x + w - (at_root ? 6 * PIX16_GLYPH_W : 0),
         guide_y, true);
   if (at_root) /* sub-menus show the clock in their title bar */
      pix16_draw_clock(pix, PIX16_FB_WIDTH - PIX16_SAFE, guide_y + 1,
            pix->colors[PIX16_C_J_DIM]);
}

/* ------------------------------------------------------------------
 * In-game pause window: RESUME GAME / QUIT GAME / SAVE STATE / LOAD STATE
 * ------------------------------------------------------------------ */

static int pix16_find_last_state(const struct playlist_entry *e);

static bool pix16_pause_active(const pix16_t *pix)
{
   return pix && !pix->qm_full && menu_is_running_quick_menu();
}

/* Newest save state of the running game */
static void pix16_pause_refresh_slot(pix16_t *pix)
{
   struct playlist_entry pe;
   const char *content = path_get(RARCH_PATH_CONTENT);
   memset(&pe, 0, sizeof(pe));
   pe.path      = (char*)content;
   pe.core_path = (char*)path_get(RARCH_PATH_CORE);
   pix->qm_slot = string_is_empty(content)
         ? PIX16_SLOT_NONE : pix16_find_last_state(&pe);
   pix->qm_slot_valid = true;
}

static bool pix16_pause_row_enabled(const pix16_t *pix, unsigned row)
{
   if (row == PIX16_QM_LOAD)
      return pix->qm_slot != PIX16_SLOT_NONE;
   return true;
}

/* Name of the running game: its playlist label when it was started from
 * a playlist (History's newest entry), else the file name */
static void pix16_pause_title(char *out, size_t len)
{
   const char *content = path_get(RARCH_PATH_CONTENT);
   out[0] = '\0';
   if (g_defaults.content_history && playlist_size(g_defaults.content_history) > 0)
   {
      const struct playlist_entry *e = NULL;
      playlist_get_index(g_defaults.content_history, 0, &e);
      if (     e && !string_is_empty(e->label) && !string_is_empty(content)
            && string_is_equal(e->path, content))
      {
         strlcpy(out, e->label, len);
         return;
      }
   }
   if (!string_is_empty(content))
   {
      char *hash;
      strlcpy(out, path_basename(content), len);
      if ((hash = strchr(out, '#')))
         *hash = '\0';
      path_remove_extension(out);
   }
   if (!*out)
      strlcpy(out, msg_hash_to_str(MSG_PIX16_PAUSED), len);
}

/* In a game, CRT SwitchRes leaves the screen in the game's mode (e.g.
 * 2560x224 or 256x224). A 320x240 menu scaled into it drops lines or
 * columns and the text looks squished, so only the part of the
 * framebuffer the mode can show 1:1 is sent: 224 lines lose the empty
 * border rows; narrow modes keep the centre (for the pause window). */
/* 480x270 when set up for 16:9 and the picture really is wide; 320x240
 * otherwise (a 4:3 game's pause menu on a widescreen TV, or 4:3 set) */
static void pix16_apply_size(pix16_t *pix)
{
   struct video_viewport vp;
   bool wide = pix->wide_cfg;
   int w, h;

   /* no game: the menu fills the screen (see pix16_set_texture), so
    * only a game's own picture can make it 4:3 */
   if (wide && pix16_content_running())
   {
      memset(&vp, 0, sizeof(vp));
      if (video_driver_get_viewport_info(&vp)
            && VIDEO_SCALE_W(vp.dims) && VIDEO_SCALE_H(vp.dims)
            && (float)VIDEO_SCALE_W(vp.dims)
               / (float)VIDEO_SCALE_H(vp.dims) < 1.55f)
         wide = false;
   }
   w = wide ? 480 : 320;
   h = wide ? 270 : 240;
   if (w == pix16_fb_w && h == pix16_fb_h)
      return;

   pix16_fb_w      = w;
   pix16_fb_h      = h;
   pix16_tile_gap  = wide ? 6 : 4;
   pix->view_w     = 0;              /* update_view sets the upload size */
   pix->view_h     = 0;
   pix->scr_key_valid = false;       /* no transitions across the switch */
   pix->slide_active  = false;
   pix->swirl_active  = false;
   pix->handover      = false;
   pix->tr_active     = false;
   pix->popc_active   = false;
   pix->glove_valid   = false;
   pix->grid_valid    = false;
   pix->last_n        = 0;
   pix16_clip_reset(pix);
   memset(pix->fb, 0, sizeof(uint16_t) * PIX16_FB_MAX_W * PIX16_FB_MAX_H);
}

static void pix16_update_view(pix16_t *pix, bool pause)
{
   struct video_viewport vp;
   int vw = PIX16_FB_WIDTH, vh = PIX16_FB_HEIGHT;

   memset(&vp, 0, sizeof(vp));
   if (pix16_content_running() && video_driver_get_viewport_info(&vp)
         && VIDEO_SCALE_W(vp.full_dims) && VIDEO_SCALE_H(vp.full_dims))
   {
      int lines = (int)VIDEO_SCALE_H(vp.full_dims);
      int cols  = (int)VIDEO_SCALE_W(vp.full_dims);
      if (lines > 300)
         lines /= 2;               /* interlaced: each menu line drawn twice */
      if (lines >= 2 * PIX16_SAFE + 192 && lines < PIX16_FB_HEIGHT)
         vh = lines;
      if (pause && cols % PIX16_FB_WIDTH != 0 && cols >= 240 && cols < PIX16_FB_WIDTH)
         vw = cols;
   }
   vw &= ~1;
   vh &= ~1;
   if (vw != pix->view_w || vh != pix->view_h)
   {
      gfx_display_t *p_disp = disp_get_ptr();
      pix->view_w = vw;
      pix->view_h = vh;
      pix->view_x = (PIX16_FB_WIDTH  - vw) / 2;
      pix->view_y = (PIX16_FB_HEIGHT - vh) / 2;
      p_disp->framebuf_dims  = VIDEO_SCALE_PACK(vw, vh);
      p_disp->framebuf_pitch = vw * sizeof(uint16_t);
   }
}

#define PIX16_QM_W     192
#define PIX16_QM_ROW_H 16

static void pix16_draw_pause(pix16_t *pix, bool jrpg)
{
   static const enum msg_hash_enums labels[PIX16_QM_ROWS] = {
      MSG_PIX16_RESUME_GAME, MSG_PIX16_QUIT_GAME,
      MSG_PIX16_SAVE_STATE,  MSG_PIX16_LOAD_STATE
   };
   char title[NAME_MAX_LENGTH];
   char slot_txt[16];
   int w = PIX16_QM_W;
   int h = 30 + PIX16_QM_ROWS * PIX16_QM_ROW_H + 6;
   int x = (PIX16_FB_WIDTH  - w) / 2;
   int y = (PIX16_FB_HEIGHT - h) / 2 - 8;
   unsigned k;

   if (!pix->qm_slot_valid)
      pix16_pause_refresh_slot(pix);
   if (!pix16_pause_row_enabled(pix, pix->qm_sel))
      pix->qm_sel = PIX16_QM_RESUME;

   /* the paused game stays visible, darkened */
   pix16_fill(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT,
         pix->colors[PIX16_C_SCRIM]);

   if (jrpg)
      pix16_window(pix, x, y, w, h);
   else
   {
      pix16_panel(pix, x, y, w, h, pix->colors[PIX16_C_PANEL]);
      pix16_fill(pix, x, y, w, 2, pix->colors[PIX16_C_RED]);
   }

   pix16_pause_title(title, sizeof(title));
   pix->text_shadow = jrpg;
   pix16_text_ticker(pix, x + 12, y + 10, title,
         jrpg ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW],
         (size_t)((w - 24) / PIX16_GLYPH_W), true);

   if (pix->qm_slot == -1)
      strlcpy(slot_txt, msg_hash_to_str(MSG_PIX16_AUTO), sizeof(slot_txt));
   else
      snprintf(slot_txt, sizeof(slot_txt), msg_hash_to_str(MSG_PIX16_SLOT), pix->qm_slot);

   for (k = 0; k < PIX16_QM_ROWS; k++)
   {
      int ty      = y + 30 + (int)k * PIX16_QM_ROW_H + 4;
      bool sel    = (k == pix->qm_sel);
      bool on     = pix16_pause_row_enabled(pix, k);
      uint16_t c  = !on ? (jrpg ? pix->colors[PIX16_C_J_DIM] : pix->colors[PIX16_C_TEXT_DIM])
                  : jrpg ? pix->colors[PIX16_C_J_TEXT]
                  : (sel ? pix->colors[PIX16_C_TEXT] : pix->colors[PIX16_C_TEXT_DIM]);
      pix16_text(pix, x + 30, ty, msg_hash_to_str(labels[k]), c, 1, 0);
      if (k == PIX16_QM_LOAD && on)
         pix16_text(pix, x + w - 12 - (int)strlen(slot_txt) * PIX16_GLYPH_W, ty,
               slot_txt, jrpg ? pix->colors[PIX16_C_J_VALUE] : pix->colors[PIX16_C_VALUE],
               1, 0);
      if (sel)
      {
         if (jrpg)
         {
            pix->text_shadow = false;
            pix16_glove_at(pix, x + 12, ty - 1, pix16_glove_context(pix, 12));
            pix->text_shadow = true;
         }
         else
            pix16_cursor(pix, x + 16, ty, pix->colors[PIX16_C_TEXT]);
      }
   }

   /* "STATE SAVED" for a couple of seconds under the window */
   if (pix->qm_status[0])
   {
      retro_time_t now = menu_driver_get_current_time();
      if (now - pix->qm_status_time < 2000000)
      {
         int tw = (int)strlen(pix->qm_status) * PIX16_GLYPH_W;
         pix16_text(pix, (PIX16_FB_WIDTH - tw) / 2, y + h + 6, pix->qm_status,
               jrpg ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW], 1, 0);
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
      }
      else
      {
         pix->qm_status[0] = '\0';
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
      }
   }

   /* the guide sits inside the part of the screen the game's mode shows */
   {
      int m  = (pix->view_w < PIX16_FB_WIDTH ? 6 : PIX16_SAFE);
      int gy = pix->view_y + pix->view_h
             - (pix->view_h < PIX16_FB_HEIGHT ? 6 : PIX16_SAFE) - PIX16_FOOTER_H;
      pix16_draw_guide_c(pix, pix->view_x + m, pix->view_x + pix->view_w - m, gy, jrpg);
   }
   pix->text_shadow = false;
}

static void pix16_pause_do(pix16_t *pix, unsigned row)
{
   settings_t *settings = config_get_ptr();
   switch (row)
   {
      case PIX16_QM_RESUME:
         command_event(CMD_EVENT_RESUME, NULL);
         break;
      case PIX16_QM_QUIT:
         pix->quit_confirm = true;      /* ask first */
         pix->confirm_exit = false;
         pix->quit_sel     = 1;         /* cursor on NO */
         break;
      case PIX16_QM_SAVE:
         command_event(CMD_EVENT_SAVE_STATE, NULL);
         /* the slot it went to is now the newest */
         pix->qm_slot       = settings->ints.state_slot;
         pix->qm_slot_valid = true;
         strlcpy(pix->qm_status, msg_hash_to_str(MSG_PIX16_STATE_SAVED), sizeof(pix->qm_status));
         pix->qm_status_time = menu_driver_get_current_time();
         break;
      case PIX16_QM_LOAD:
         if (pix->qm_slot == PIX16_SLOT_NONE)
            return;
         settings->ints.state_slot = pix->qm_slot;
         command_event(CMD_EVENT_LOAD_STATE, NULL);
         command_event(CMD_EVENT_RESUME, NULL);
         break;
   }
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
}

/* Returns true when the action was the pause window's */
static bool pix16_pause_input(pix16_t *pix, enum menu_action action)
{
   unsigned k;
   switch (action)
   {
      case MENU_ACTION_UP:
         for (k = pix->qm_sel; k-- > 0; )
            if (pix16_pause_row_enabled(pix, k))
            {
               pix->qm_sel = k;
               pix16_sfx(PIX16_SFX_UP);
               break;
            }
         break;
      case MENU_ACTION_DOWN:
         for (k = pix->qm_sel + 1; k < PIX16_QM_ROWS; k++)
            if (pix16_pause_row_enabled(pix, k))
            {
               pix->qm_sel = k;
               pix16_sfx(PIX16_SFX_DOWN);
               break;
            }
         break;
      case MENU_ACTION_OK:
         pix16_sfx(PIX16_SFX_OK);
         pix16_pause_do(pix, pix->qm_sel);
         break;
      case MENU_ACTION_CANCEL:
         pix16_sfx(PIX16_SFX_BACK);
         pix16_pause_do(pix, PIX16_QM_RESUME);
         break;
      case MENU_ACTION_START:
         /* RetroArch's full Quick Menu (shaders, core options, ...) */
         pix16_sfx(PIX16_SFX_OK);
         pix->qm_full = true;
         break;
      default:
         break;   /* everything else does nothing here */
   }
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
   return true;
}

/* ------------------------------------------------------------------
 * Boot screen (cold start only): BIOS text typing out, then the
 * title screen with the logo and PUSH ANY BUTTON TO START
 * ------------------------------------------------------------------ */

#define PIX16_BOOT_CHAR_MS   14     /* typing speed               */
#define PIX16_BOOT_BAR_MS    1100   /* "scanning" progress bar    */
#define PIX16_BOOT_HOLD_MS   900    /* "LOADING _" before the logo */
#define PIX16_LOGO_DROP_MS   650

enum
{
   PIX16_BL_TEXT = 0, /* fixed text (the BIOS's own name) */
   PIX16_BL_MSG,      /* a translated line */
   PIX16_BL_CHECK,    /* "NAME ........ OK" */
   PIX16_BL_MODE,     /* the video mode */
   PIX16_BL_BAR,
   PIX16_BL_COUNT
};

typedef struct
{
   const char *text;
   enum msg_hash_enums msg;
   int kind;
   int pause_ms;     /* after the line is done */
} pix16_boot_line_t;

static const pix16_boot_line_t pix16_boot_lines[] = {
   { "PIX16 SYSTEM BIOS  V1.0", MSG_UNKNOWN,              PIX16_BL_TEXT,  120 },
   { "(C) 199X RETROSYSTEM",    MSG_UNKNOWN,              PIX16_BL_TEXT,  380 },
   { "",                        MSG_UNKNOWN,              PIX16_BL_TEXT,    0 },
   { NULL,                      MSG_UNKNOWN,              PIX16_BL_MODE,  320 },
   { NULL,                      MSG_PIX16_BIOS_VIDEO,      PIX16_BL_CHECK, 160 },
   { NULL,                      MSG_PIX16_BIOS_CONTROLLER, PIX16_BL_CHECK, 160 },
   { NULL,                      MSG_PIX16_BIOS_MEMORY_CARD,PIX16_BL_CHECK, 360 },
   { "",                        MSG_UNKNOWN,              PIX16_BL_TEXT,    0 },
   { NULL,                      MSG_PIX16_BIOS_SCANNING,   PIX16_BL_MSG,   120 },
   { NULL,                      MSG_UNKNOWN,              PIX16_BL_BAR,   200 },
   { NULL,                      MSG_UNKNOWN,              PIX16_BL_COUNT, 480 },
   { "",                        MSG_UNKNOWN,              PIX16_BL_TEXT,    0 },
   { NULL,                      MSG_PIX16_BIOS_BOOTING,    PIX16_BL_MSG,   260 },
   { NULL,                      MSG_PIX16_BIOS_LOADING,    PIX16_BL_MSG,     0 },
};
#define PIX16_BOOT_LINES (sizeof(pix16_boot_lines) / sizeof(pix16_boot_lines[0]))
#define PIX16_CHECK_COL  23   /* where the dots of a CHECK line end */

/* Reads playlists a slice at a time (the frame must not wait on a large
 * library) and counts their entries */
#define PIX16_CNT_CHUNK     16384
#define PIX16_CNT_BUDGET_US 2000
#define PIX16_CNT_KEY       "\"path\""
static void pix16_boot_count_step(pix16_t *pix)
{
   retro_time_t until;
   size_t klen = sizeof(PIX16_CNT_KEY) - 1;

   if (pix->cnt_done)
      return;
   if (!pix->boot_counted)
   {
      settings_t *settings = config_get_ptr();
      pix->boot_counted = true;
      pix->boot_games   = 0;
      if (pix16_stack_depth() <= 1 && pix16_root_tab() == PIX16_TAB_LISTS)
      {
         pix16_grid_build(pix);
         pix->boot_systems = (int)pix->grid_count;
      }
      if (settings && !string_is_empty(settings->paths.directory_playlist))
         pix->cnt_files = dir_list_new(settings->paths.directory_playlist,
               "lpl", false, false, false, false);
      pix->cnt_buf = (char*)malloc(PIX16_CNT_CHUNK + 8);
      if (!pix->cnt_files || !pix->cnt_buf)
      {
         pix->cnt_done = true;
         return;
      }
   }

   until = cpu_features_get_time_usec() + PIX16_CNT_BUDGET_US;
   do
   {
      int64_t n;
      char *p, *end;
      if (!pix->cnt_fp)
      {
         const char *f, *b;
         if (pix->cnt_idx >= pix->cnt_files->size)
         {
            pix->cnt_done = true;
            break;
         }
         f = pix->cnt_files->elems[pix->cnt_idx++].data;
         b = path_basename(f);
         /* History and Favorites are not a system's games */
         if (!b || !strncmp(b, "content_", 8))
            continue;
         pix->cnt_fp   = filestream_open(f, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE);
         pix->cnt_tail = 0;
         continue;
      }
      /* the last few bytes of the previous chunk come first, so a key
       * split between two reads is still found */
      n = filestream_read(pix->cnt_fp, pix->cnt_buf + pix->cnt_tail,
            PIX16_CNT_CHUNK);
      if (n <= 0)
      {
         filestream_close(pix->cnt_fp);
         pix->cnt_fp = NULL;
         continue;
      }
      end = pix->cnt_buf + pix->cnt_tail + n;
      for (p = pix->cnt_buf; p + klen <= end; p++)
         if (*p == '"' && !memcmp(p, PIX16_CNT_KEY, klen))
         {
            pix->boot_games++;
            p += klen - 1;
         }
      pix->cnt_tail = (size_t)(end - pix->cnt_buf) < klen - 1
            ? (size_t)(end - pix->cnt_buf) : klen - 1;
      memmove(pix->cnt_buf, end - pix->cnt_tail, pix->cnt_tail);
   } while (cpu_features_get_time_usec() < until);

   if (pix->cnt_done)
   {
      string_list_free(pix->cnt_files);
      pix->cnt_files = NULL;
      free(pix->cnt_buf);
      pix->cnt_buf = NULL;
   }
}

static void pix16_boot_count_free(pix16_t *pix)
{
   if (pix->cnt_fp)
      filestream_close(pix->cnt_fp);
   if (pix->cnt_files)
      string_list_free(pix->cnt_files);
   free(pix->cnt_buf);
   pix->cnt_fp    = NULL;
   pix->cnt_files = NULL;
   pix->cnt_buf   = NULL;
   pix->cnt_done  = true;
}

/* The text of BIOS line i */
static size_t pix16_boot_line_text(pix16_t *pix, size_t i, char *buf,
      size_t len)
{
   const pix16_boot_line_t *l = &pix16_boot_lines[i];
   buf[0] = '\0';
   switch (l->kind)
   {
      case PIX16_BL_TEXT:
         return strlcpy(buf, l->text, len);
      case PIX16_BL_MSG:
         return strlcpy(buf, msg_hash_to_str(l->msg), len);
      case PIX16_BL_CHECK:
      {
         size_t n = strlcpy(buf, msg_hash_to_str(l->msg), len);
         if (n + 1 < len)
            buf[n++] = ' ';
         while (n + 1 < len && (n < PIX16_CHECK_COL || buf[n - 2] != '.'))
            buf[n++] = '.';
         buf[n] = '\0';
         return n + snprintf(buf + n, len - n, " %s",
               msg_hash_to_str(MSG_PIX16_BIOS_OK));
      }
      case PIX16_BL_MODE:
      {
         /* the real output mode, e.g. 1920x1080 @ 60HZ or 2560x240 */
         struct video_viewport vp;
         char mode[48];
         memset(&vp, 0, sizeof(vp));
         if (     video_driver_get_viewport_info(&vp)
               && VIDEO_SCALE_W(vp.full_dims) && VIDEO_SCALE_H(vp.full_dims))
         {
            float hz = video_driver_get_window_refresh_rate();
            if (hz <= 0.0f)
               hz = video_driver_get_refresh_rate();
            if (hz > 0.0f)
               snprintf(mode, sizeof(mode), "%ux%u @ %.0fHZ",
                     VIDEO_SCALE_W(vp.full_dims), VIDEO_SCALE_H(vp.full_dims),
                     hz);
            else
               snprintf(mode, sizeof(mode), "%ux%u",
                     VIDEO_SCALE_W(vp.full_dims), VIDEO_SCALE_H(vp.full_dims));
         }
         else
            snprintf(mode, sizeof(mode), "%dx%d", PIX16_FB_WIDTH,
                  PIX16_FB_HEIGHT);
         return snprintf(buf, len,
               msg_hash_to_str(MSG_PIX16_BIOS_DETECTED), mode);
      }
      case PIX16_BL_COUNT:
         return snprintf(buf, len, msg_hash_to_str(MSG_PIX16_BIOS_COUNT),
               pix->boot_systems, pix->boot_games);
      default:
         break;
   }
   return 0;
}

/* How long a line takes to appear */
static int pix16_boot_line_ms(pix16_t *pix, size_t i)
{
   char buf[96];
   if (pix16_boot_lines[i].kind == PIX16_BL_BAR)
      return PIX16_BOOT_BAR_MS;
   pix16_boot_line_text(pix, i, buf, sizeof(buf));
   return (int)strlen(buf) * PIX16_BOOT_CHAR_MS;
}

/* Right-hand panel of the BIOS screen (widescreen): what this machine
 * and RetroArch actually are. Gathered once per boot. */
#define PIX16_INFO_ROWS 16
#define PIX16_INFO_CHAR_MS 21   /* a little slower than the left column */
#define PIX16_INFO_COLS 25      /* text columns in the specs panel */
static char pix16_info[PIX16_INFO_ROWS][2][64];
static int  pix16_info_n = -1;

static void pix16_upper(char *s)
{
   char *p;
   for (p = s; *p; p++)
      /* keep the x in 1920x1080 */
      if (*p >= 'a' && *p <= 'z'
            && !(*p == 'x' && p > s && p[-1] >= '0' && p[-1] <= '9'
                 && p[1] >= '0' && p[1] <= '9'))
         *p -= 32;
}

static void pix16_info_add(const char *k, const char *v)
{
   if (pix16_info_n < 0 || pix16_info_n >= PIX16_INFO_ROWS)
      return;
   strlcpy(pix16_info[pix16_info_n][0], k, sizeof(pix16_info[0][0]));
   strlcpy(pix16_info[pix16_info_n][1], (v && *v) ? v : "-",
         sizeof(pix16_info[0][1]));
   pix16_upper(pix16_info[pix16_info_n][1]);
   pix16_info_n++;
}

static void pix16_info_build(void)
{
   settings_t *settings              = config_get_ptr();
   const frontend_ctx_driver_t *fe   = frontend_get_ptr();
   const char *cpu                   = frontend_driver_get_cpu_model_name();
   struct video_viewport vp;
   core_info_list_t *cl              = NULL;
   char v[96], arch[32];

   pix16_info_n = 0;

   pix16_info_add("RETROARCH", PACKAGE_VERSION);   /* the name */
   snprintf(v, sizeof(v), "%s", __DATE__);
   pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_BUILT), v);
   pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_MENU), "PIX16 V1.0");

   v[0] = '\0';
   if (fe && fe->get_os)
   {
      int major = 0, minor = 0;
      size_t n = fe->get_os(v, sizeof(v), &major, &minor);
      if (n < sizeof(v) && (major || minor))
         snprintf(v + n, sizeof(v) - n, " %d.%d", major, minor);
   }
   pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_OS), v);

   arch[0] = '\0';
   frontend_driver_get_cpu_architecture_str(arch, sizeof(arch));
   snprintf(v, sizeof(v), msg_hash_to_str(MSG_PIX16_INFO_THREADS),
         cpu_features_get_core_amount(), arch);
   pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_CPU), v);
   if (cpu && *cpu)
   {
      /* the model name gets a line of its own, without padding */
      char model[64];
      const char *c = cpu;
      size_t n = 0;
      while (*c == ' ')
         c++;
      for (; *c && n + 1 < sizeof(model); c++)
         if (!(*c == ' ' && n && model[n - 1] == ' '))
            model[n++] = *c;
      model[n] = '\0';
      /* shorten: "Intel(R) Core(TM) i7-9700K CPU @ 3.60GHz" ->
       * "Intel Core i7-9700K 3.60GHz" */
      {
         static const char *drop[] = { "(R)", "(r)", "(TM)", "(tm)",
            " CPU", " Processor", " processor", "@ ", NULL };
         int d;
         for (d = 0; drop[d]; d++)
         {
            char *f;
            while ((f = strstr(model, drop[d])))
               memmove(f, f + strlen(drop[d]), strlen(f + strlen(drop[d])) + 1);
         }
      }
      /* the model name gets lines of its own, without padding, wrapped
       * to the width of the specs panel */
      {
         char *p = model;
         while (*p)
         {
            char line[PIX16_INFO_COLS + 2];
            size_t n;
            while (*p == ' ')
               p++;
            if (!*p)
               break;
            n = strlen(p);
            if (n > PIX16_INFO_COLS)
            {
               size_t cut = PIX16_INFO_COLS;
               while (cut > 1 && p[cut] != ' ')
                  cut--;
               if (cut <= 1)
                  cut = PIX16_INFO_COLS;
               memcpy(line, p, cut);
               line[cut] = '\0';
               p += cut;
            }
            else
            {
               strlcpy(line, p, sizeof(line));
               p += n;
            }
            pix16_info_add("", line);
         }
      }
   }

   {
      uint64_t total = mem_stats_total();
      if (total)
      {
         snprintf(v, sizeof(v), "%u MB", (unsigned)(total / (1024 * 1024)));
         pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_MEMORY), v);
      }
   }

   pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_VIDEO), video_driver_get_ident());
   memset(&vp, 0, sizeof(vp));
   if (video_driver_get_viewport_info(&vp) && VIDEO_SCALE_W(vp.full_dims))
   {
      float hz = video_driver_get_window_refresh_rate();
      if (hz <= 0.0f)
         hz = video_driver_get_refresh_rate();
      if (hz > 0.0f)
         snprintf(v, sizeof(v), "%ux%u %.2fHZ", VIDEO_SCALE_W(vp.full_dims),
               VIDEO_SCALE_H(vp.full_dims), hz);
      else
         snprintf(v, sizeof(v), "%ux%u", VIDEO_SCALE_W(vp.full_dims),
               VIDEO_SCALE_H(vp.full_dims));
      pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_DISPLAY), v);
   }
   if (settings)
   {
      if (settings->uints.audio_output_sample_rate)
         snprintf(v, sizeof(v), "%s %uHZ", settings->arrays.audio_driver,
               settings->uints.audio_output_sample_rate);
      else
         strlcpy(v, settings->arrays.audio_driver, sizeof(v));
      pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_AUDIO), v);
      snprintf(v, sizeof(v), "%s / %s", settings->arrays.input_driver,
            settings->arrays.input_joypad_driver);
      pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_INPUT), v);
   }
   {
      const char *dev = input_config_get_device_display_name(0);
      pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_PAD),
            (dev && *dev) ? dev : msg_hash_to_str(MSG_PIX16_INFO_NOT_CONNECTED));
   }
   core_info_get_list(&cl);
   snprintf(v, sizeof(v), msg_hash_to_str(MSG_PIX16_INFO_INSTALLED), cl ? (unsigned)cl->count : 0u);
   pix16_info_add(msg_hash_to_str(MSG_PIX16_INFO_CORES), v);
}

/* When the panel's last row is typed (widescreen only), so the BIOS
 * screen stays up until it is done */
static int pix16_bios_panel_ms(void)
{
   int t = 320, k, kw = 10;
   int cols = (PIX16_FB_WIDTH - 12 - (20 + 28 * PIX16_GLYPH_W + 8) - 16)
            / PIX16_GLYPH_W;
   if (cols < 24)
      return 0;
   if (pix16_info_n < 0)
      pix16_info_build();
   for (k = 0; k < pix16_info_n; k++)
   {
      int len = (pix16_info[k][0][0] ? kw : 0) + (int)strlen(pix16_info[k][1]);
      if (len > cols)
         len = cols;
      t += len * PIX16_INFO_CHAR_MS + 90;
   }
   return t;
}

static void pix16_draw_bios_panel(pix16_t *pix, int ms, int x, int y, int w,
      uint16_t green, uint16_t dim, uint16_t bg)
{
   int rows, k, t, kw = 10;   /* key column: 10 characters */
   int lh = 12, h;
   int cols = (w - 16) / PIX16_GLYPH_W;
   const char *title = msg_hash_to_str(MSG_PIX16_INFO_TITLE);

   if (pix16_info_n < 0)
      pix16_info_build();
   rows = pix16_info_n;
   while (rows > 0 && !pix16_info[rows - 1][0][0])
      rows--;                       /* the build leaves spare rows empty */
   if (y + 28 + rows * lh > PIX16_FB_HEIGHT)
      rows = (PIX16_FB_HEIGHT - y - 28) / lh;   /* keep the table on screen */
   if (rows < 0)
      rows = 0;
   h    = 22 + rows * lh + 6;

   /* the frame draws itself first, top to bottom */
   {
      int fh = ms < 300 ? h * ms / 300 : h;
      if (fh <= 0)
         return;
      pix16_fill(pix, x, y, w, 1, dim);
      pix16_fill(pix, x, y, 1, fh, dim);
      pix16_fill(pix, x + w - 1, y, 1, fh, dim);
      if (fh >= h)
         pix16_fill(pix, x, y + h - 1, w, 1, dim);
   }
   /* solid body, so the backdrop does not show through the table */
   if (ms >= 300)
      pix16_fill(pix, x + 1, y + 1, w - 2, h - 2, bg);
   /* title bar: dark letters on green */
   if (ms >= 120)
   {
      int tw = (int)strlen(title) * PIX16_GLYPH_W;
      pix16_fill(pix, x + 1, y + 1, w - 2, 12, dim);
      pix16_text(pix, x + (w - tw) / 2, y + 3, title, bg, 1, 0);
   }

   /* rows type in one after another */
   t = 320;
   for (k = 0; k < rows; k++)
   {
      char line[160];
      int into = ms - t, n, len;
      int ry = y + 20 + k * lh;
      int rkw = pix16_info[k][0][0] ? kw : 0;   /* a value-only row */
      snprintf(line, sizeof(line), "%-*.63s%.63s", rkw, pix16_info[k][0],
            pix16_info[k][1]);
      len = (int)strlen(line);
      if (len > cols)
         len = cols;
      if (into < 0)
         break;
      n = into / PIX16_INFO_CHAR_MS;
      if (n > len)
         n = len;
      /* key dim, value bright */
      {
         char part[96];
         int kn = n < rkw ? n : rkw;
         memcpy(part, line, (size_t)kn);
         part[kn] = '\0';
         pix16_text(pix, x + 8, ry, part, dim, 1, 0);
         if (n > rkw)
         {
            memcpy(part, line + rkw, (size_t)(n - rkw));
            part[n - rkw] = '\0';
            pix16_text(pix, x + 8 + rkw * PIX16_GLYPH_W, ry, part, green, 1, 0);
         }
      }
      t += len * PIX16_INFO_CHAR_MS + 90;
   }
}

/* BIOS mascot: the RetroArch alien, a flat silhouette with antennae, a
 * brow, square eyes, side pincers and four legs. It blinks and its
 * antenna tips pulse. */
#define PIX16_ALIEN_W 23
#define PIX16_ALIEN_H 13
static const char *pix16_alien[PIX16_ALIEN_H] = {
   ".......#.......#.......",
   ".......##.....##.......",
   "##....###########....##",
   ".#...#############...#.",
   ".#..###ee#####ee###..#.",
   "#######ee#####ee#######",
   "#######################",
   "#######################",
   "...#################...",
   "....###############....",
   "....##..##...##..##....",
   "...##...##....##...##..",
   "..##...##......##...##."
};

static void pix16_draw_alien(pix16_t *pix, int x, int y, int bs, int ms)
{
   uint16_t body = pix->colors[PIX16_C_J_BORDER_LO];
   uint16_t brow = pix->colors[PIX16_C_J_BORDER];
   uint16_t tip  = ((ms / 300) & 1) ? pix->colors[PIX16_C_J_TEXT]
                                    : pix->colors[PIX16_C_J_GOLD];
   uint16_t hole = pix->colors[PIX16_C_J_BG];
   bool blink    = (ms % 3400) < 140;
   int r, c;
   for (r = 0; r < PIX16_ALIEN_H; r++)
      for (c = 0; c < PIX16_ALIEN_W; c++)
      {
         char ch = pix16_alien[r][c];
         uint16_t col;
         if (ch == '.')
            continue;
         if (ch == 'e')
            col = blink ? body : hole;
         else if (r == 0)
            col = tip;              /* antenna tips */
         else if (r == 2)
            col = brow;             /* bevel along the brow */
         else
            col = body;
         pix16_fill(pix, x + c * bs, y + r * bs, bs, bs, col);
      }
}

static void pix16_draw_bios(pix16_t *pix, int ms)
{
   uint16_t bg    = pix->colors[PIX16_C_J_BG];
   uint16_t green = pix->colors[PIX16_C_J_GOLD];
   uint16_t dim   = pix->colors[PIX16_C_J_DIM];
   bool wide      = PIX16_FB_WIDTH > 320;
   int x = 20, y = wide ? 14 : 18, lh = wide ? 10 : 12;
   int win_y = y - 8;
   int win_h = 14 + (int)PIX16_BOOT_LINES * lh;
   int t = 0;
   size_t i;

   pix16_boot_count_step(pix);
   pix16_j_background(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT);
   /* the readout sits in a window, like every other pix16 screen */
   pix16_window(pix, x - 8, win_y, 228, win_h);
   pix->text_shadow = true;

   for (i = 0; i < PIX16_BOOT_LINES; i++)
   {
      const pix16_boot_line_t *l = &pix16_boot_lines[i];
      int dur  = pix16_boot_line_ms(pix, i);
      int into = ms - t;
      if (into < 0)
         break;
      if (l->kind == PIX16_BL_BAR)
      {
         int bw = 26 * PIX16_GLYPH_W, fillw;
         float f = (float)into / (float)dur;
         if (f > 1.0f)
            f = 1.0f;
         fillw = (int)((bw - 4) * f);
         pix16_fill(pix, x, y, bw, 9, green);
         pix16_fill(pix, x + 1, y + 1, bw - 2, 7, bg);
         /* chunky blocks, 6px each */
         {
            int bx;
            for (bx = 0; bx + 5 <= fillw; bx += 6)
               pix16_fill(pix, x + 2 + bx, y + 2, 5, 5, green);
         }
      }
      else
      {
         char buf[96];
         size_t n, full = pix16_boot_line_text(pix, i, buf, sizeof(buf));
         n = (size_t)(into / PIX16_BOOT_CHAR_MS);
         if (n < full)
            buf[n] = '\0';
         pix16_text(pix, x, y, buf, (i < 2 || l->kind == PIX16_BL_COUNT)
               ? green : dim, 1, 0);
         /* "OK" in bright green once typed */
         if (l->kind == PIX16_BL_CHECK && n >= full)
         {
            const char *ok = msg_hash_to_str(MSG_PIX16_BIOS_OK);
            size_t ol      = strlen(ok);
            pix16_text(pix, x + (int)(full - ol) * PIX16_GLYPH_W, y, ok,
                  green, 1, 0);
         }
         /* blinking block cursor on the last line */
         if (i == PIX16_BOOT_LINES - 1 && n >= full)
         {
            if ((ms / 350) % 2 == 0)
               pix16_fill(pix, x + (int)(full + 1) * PIX16_GLYPH_W, y, 7, 8,
                     green);
         }
      }
      t += dur + l->pause_ms;
      y += lh;
   }

   /* the mascot: along the bottom, below the readout window, drawn in
    * by a scanning beam, then it idles */
   {
      int wb  = win_y + win_h;                /* window bottom */
      int bs  = wide ? 8 : 3;                 /* block size */
      int lw, lhh, lx, ly, rev, cy1;
      /* bottom-aligned; if the window leaves too little room, shrink it
       * until the mascot clears the window and fits on screen */
      while (bs > 2 && PIX16_FB_HEIGHT - 4 - PIX16_ALIEN_H * bs < wb + 4)
         bs--;
      lw  = PIX16_ALIEN_W * bs;
      lhh = PIX16_ALIEN_H * bs;
      lx  = wide ? 20 + (28 * PIX16_GLYPH_W - lw) / 2
                 : PIX16_FB_WIDTH - 12 - lw;
      ly  = PIX16_FB_HEIGHT - 4 - lhh;
      if (ly < wb + 4)
         ly = wb + 4;
      rev = ms < 900 ? lhh * ms / 900 : lhh;   /* rows revealed */
      cy1 = pix->clip_y1;
      if (rev > 0)
      {
         pix->clip_y1 = ly + rev < cy1 ? ly + rev : cy1;
         pix16_draw_alien(pix, lx, ly, bs, ms);
         pix->clip_y1 = cy1;
         if (rev < lhh)   /* the beam */
            pix16_fill(pix, lx - 4, ly + rev, lw + 8, 1, green);
      }
   }

   /* widescreen: the machine's specs down the right-hand side */
   {
      int px = 20 + 28 * PIX16_GLYPH_W + 8;
      int pw = PIX16_FB_WIDTH - 12 - px;
      if (pw >= 26 * PIX16_GLYPH_W)
         pix16_draw_bios_panel(pix, ms, px, 14, pw, green, dim, bg);
   }
   pix->text_shadow = false;
}

/* Total BIOS time */
static int pix16_bios_ms(pix16_t *pix)
{
   int t = 0;
   size_t i;
   int panel = pix16_bios_panel_ms();
   for (i = 0; i < PIX16_BOOT_LINES; i++)
      t += pix16_boot_line_ms(pix, i) + pix16_boot_lines[i].pause_ms;
   if (panel + 300 > t)
      t = panel + 300;          /* let the specs finish typing */
   return t + PIX16_BOOT_HOLD_MS;
}

/* 4-point sparkle, r = 0..3 */
static void pix16_sparkle(pix16_t *pix, int x, int y, int r)
{
   uint16_t w = pix->conv(0xFFFFFFFF);
   uint16_t c = pix->conv(0xFFC8E8FF);
   if (r <= 0)
      return;
   pix16_fill(pix, x - r - 1, y, 2 * r + 3, 1, c);
   pix16_fill(pix, x, y - r - 1, 1, 2 * r + 3, c);
   pix16_fill(pix, x - r, y, 2 * r + 1, 1, w);
   pix16_fill(pix, x, y - r, 1, 2 * r + 1, w);
   if (r >= 2)
   {
      pix16_fill(pix, x - 1, y - 1, 1, 1, c);
      pix16_fill(pix, x + 1, y - 1, 1, 1, c);
      pix16_fill(pix, x - 1, y + 1, 1, 1, c);
      pix16_fill(pix, x + 1, y + 1, 1, 1, c);
   }
}

/* Twinkling stars in two layers drifting left at different speeds */
static uint32_t pix16_hash(uint32_t x)
{
   x ^= x >> 16; x *= 0x7FEB352Du;
   x ^= x >> 15; x *= 0x846CA68Bu;
   x ^= x >> 16;
   return x;
}

static void pix16_starfield(pix16_t *pix, retro_time_t now_ms)
{
   int W = PIX16_FB_WIDTH, H = PIX16_FB_HEIGHT;
   int n = W * H / 1100;     /* about 70 stars at 320x240 */
   int i;
   uint16_t c_dim  = pix->conv(0xFF5A6CB0);
   uint16_t c_mid  = pix->conv(0xFF9CB0E8);
   uint16_t c_hi   = pix->conv(0xFFF0F4FF);
   for (i = 0; i < n; i++)
   {
      uint32_t h   = pix16_hash((uint32_t)i * 2654435761u + 12345u);
      int layer    = (int)(h & 1);               /* 0 far, 1 near */
      int speed    = layer ? 9 : 4;              /* pixels per second */
      int x        = (int)((h >> 8) % (uint32_t)W);
      int y        = (int)((h >> 20) % (uint32_t)H);
      int period   = 1400 + (int)((h >> 4) % 1600);
      int ph       = (int)((now_ms + (h % 997) * 7) % period);
      int bright;
      x = (int)(((x - (now_ms * speed) / 1000) % W + W) % W);
      /* twinkle: mostly dim, a short bright moment each period */
      bright = ph < 140 ? 2 : (ph < 320 ? 1 : 0);
      if (!layer && bright == 2)
         bright = 1;
      pix16_fill(pix, x, y, 1, 1, bright == 2 ? c_hi : bright ? c_mid : c_dim);
      if (bright == 2)
      {
         /* a little cross when it flares */
         pix16_fill(pix, x - 1, y, 3, 1, c_mid);
         pix16_fill(pix, x, y - 1, 1, 3, c_mid);
         pix16_fill(pix, x, y, 1, 1, c_hi);
      }
   }
}

/* A diagonal glint sweeping across the logo every few seconds */
#define PIX16_SHINE_EVERY_MS 3600
#define PIX16_SHINE_MS       680
static void pix16_logo_shine(pix16_t *pix, const struct pix16_art *art,
      int lx, int ly, int ms)
{
   int ph = ms % PIX16_SHINE_EVERY_MS;
   int w  = (int)art->w, h = (int)art->h;
   int span, pos, x, y;
   uint16_t core = pix->conv(0xFFFFFFFF);
   uint16_t edge = pix->conv(0xFFC8DCFF);
   if (ms < 400 || ph >= PIX16_SHINE_MS)
      return;
   span = w + h + 24;                      /* the band travels x + y */
   pos  = -12 + span * ph / PIX16_SHINE_MS;
   for (y = 0; y < h; y++)
   {
      int py = ly + y;
      if (py < pix->clip_y0 || py >= pix->clip_y1)
         continue;
      for (x = 0; x < w; x++)
      {
         int d = x + y - pos, px = lx + x;
         if (d < -6 || d > 6 || !art->mask[(size_t)y * art->w + x])
            continue;
         if (px < pix->clip_x0 || px >= pix->clip_x1)
            continue;
         /* white core, pale band, dithered fringe */
         if (d >= -1 && d <= 1)
            pix->fb[py * PIX16_FB_WIDTH + px] = core;
         else if (d >= -3 && d <= 3)
            pix->fb[py * PIX16_FB_WIDTH + px] = edge;
         else if (((x ^ y) & 1) == 0)
            pix->fb[py * PIX16_FB_WIDTH + px] = edge;
      }
   }
}

static void pix16_draw_logo_screen(pix16_t *pix, int ms)
{
   const struct pix16_art *logo = pix16_art_get(pix, "@boot_logo");
   int lw = logo ? (int)logo->w : 0;
   int lh = logo ? (int)logo->h : 0;
   int lx = (PIX16_FB_WIDTH - lw) / 2;
   /* where the logo settles: centred above the PUSH line */
   int ty = (PIX16_FB_HEIGHT - 30 - lh) / 2 < 12 ? 12
          : (PIX16_FB_HEIGHT - 30 - lh) / 2;
   int ly;
   const char *push = msg_hash_to_str(MSG_PIX16_PUSH_ANY_BUTTON);

   pix16_boot_count_step(pix);   /* finishes a big library */
   pix16_j_background(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT);
   pix16_starfield(pix, menu_driver_get_current_time() / 1000);   /* title screen only */

   /* drop in from above with a small bounce */
   if (ms < PIX16_LOGO_DROP_MS)
   {
      float t  = (float)ms / PIX16_LOGO_DROP_MS;
      float c1 = 1.70158f, c3 = c1 + 1.0f;
      float e  = 1.0f + c3 * (t - 1) * (t - 1) * (t - 1) + c1 * (t - 1) * (t - 1);
      ly = (int)(-lh + (ty + lh) * e);
   }
   else
      ly = ty;

   if (logo)
   {
      pix16_art_draw(pix, logo, lx, ly, lw, lh);
      if (ms >= PIX16_LOGO_DROP_MS)
         pix16_logo_shine(pix, logo, lx, ly, ms - PIX16_LOGO_DROP_MS);
   }
   else
   {
      pix->text_shadow = true;
      pix16_text(pix, (PIX16_FB_WIDTH - 9 * 16) / 2, ly + 30, "RETROARCH",
            pix->colors[PIX16_C_J_GOLD], 2, 0);
      pix->text_shadow = false;
   }

   if (ms >= PIX16_LOGO_DROP_MS && logo)
   {
      /* a glint every 700 ms at a spot on the logo (fixed sequence) */
      /* spots as fractions (of 255) of the logo's width and height:
       * the arrow tip, the ring, the title letters, the box corners */
      static const uint8_t spots[][2] = {
         { 208, 18 }, { 40, 100 }, { 26, 200 }, { 175, 92 }, { 128, 190 },
         { 12, 10 }, { 128, 158 }, { 230, 205 }, { 243, 12 }, { 80, 196 }
      };
      int since = ms - PIX16_LOGO_DROP_MS;
      int k     = (since / 700) % (int)(sizeof(spots) / sizeof(spots[0]));
      int ph    = since % 700;
      if (ph < 320)
      {
         static const int8_t rs[8] = { 1, 2, 3, 3, 2, 2, 1, 1 };
         int sx = lx + spots[k][0] * lw / 255;
         int sy = ly + spots[k][1] * lh / 255;
         pix16_sparkle(pix, sx, sy, rs[ph / 40]);
      }
   }

   /* PUSH ANY BUTTON TO START: breathes through a few shades; once
    * pressed it flashes quickly */
   if (ms >= PIX16_LOGO_DROP_MS + 250)
   {
      static const uint32_t shades[6] = {
         0xFF2C3C78, 0xFF4A64A8, 0xFF7890D0, 0xFFA8BCEC, 0xFFD4E0FA, 0xFFFFFFFF
      };
      int pw   = (int)strlen(push) * PIX16_GLYPH_W;
      int px   = (PIX16_FB_WIDTH - pw) / 2;
      int py   = PIX16_FB_HEIGHT - PIX16_SAFE - PIX16_GLYPH_H - 14;
      int show = 1, shade;
      if (pix->boot_go_t0)
      {
         int since = (int)((menu_driver_get_current_time() - pix->boot_go_t0) / 1000);
         show  = (since / 60) % 2 == 0;
         shade = 5;
      }
      else
      {
         /* 0..5..0 over 1.8 s, a smooth triangle wave */
         int ph = (ms - PIX16_LOGO_DROP_MS - 250) % 1800;
         int tri = ph < 900 ? ph : 1800 - ph;
         shade = tri * 6 / 901;
         if (shade > 5)
            shade = 5;
      }
      if (show)
      {
         pix->text_shadow = shade >= 2;
         pix16_text(pix, px, py, push, pix->conv(shades[shade]), 1, 0);
         pix->text_shadow = false;
      }
   }

   /* the screen "switches on" into the logo: a short white flash */
   if (ms < 90)
      pix16_fill(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT,
            pix->conv(ms < 45 ? 0xFFF0F4FF : 0xFF8890C0));

   if (!pix->screen_asked && ms >= PIX16_LOGO_DROP_MS)
      pix->screen_prompt = true;
}

/* First run: ask what the screen is, so the layout fits it */
static void pix16_draw_screen_prompt(pix16_t *pix, bool jrpg)
{
   static const enum msg_hash_enums rows[4] = {
      MSG_PIX16_SCREEN_43,   MSG_PIX16_SCREEN_43_FULL,
      MSG_PIX16_SCREEN_WIDE, MSG_PIX16_SCREEN_WIDE_FULL
   };
   int w    = 200;
   int row_h = 16;
   int h    = 30 + 4 * row_h + 6;
   int x    = (PIX16_FB_WIDTH  - w) / 2;
   int y    = (PIX16_FB_HEIGHT - h) / 2;
   int k;

   if (jrpg)
   {
      if (!pix16_popup_window(pix, x, y, w, h))
         return;
   }
   else
   {
      pix16_panel(pix, x, y, w, h, pix->colors[PIX16_C_PANEL]);
      pix16_fill(pix, x, y, w, 2, pix->tab_color[PIX16_TAB_SETUP]);
   }
   pix->text_shadow = jrpg;
   pix16_text(pix, x + 12, y + 10, msg_hash_to_str(MSG_PIX16_SCREEN),
         jrpg ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW],
         1, 0);

   for (k = 0; k < 4; k++)
   {
      int ty   = y + 30 + k * row_h + 4;
      bool sel = ((unsigned)k == pix->screen_sel);
      uint16_t c = jrpg ? pix->colors[PIX16_C_J_TEXT]
                   : (sel ? pix->colors[PIX16_C_TEXT]
                          : pix->colors[PIX16_C_TEXT_DIM]);
      const char *label = msg_hash_to_str(rows[k]);
      int lx = x + (w - (int)strlen(label) * PIX16_GLYPH_W) / 2;

      pix16_text(pix, lx, ty, label, c, 1, 0);
      if (sel)
      {
         if (jrpg)
         {
            pix->text_shadow = false;
            pix16_glove_at(pix, lx - 18, ty - 1, pix16_glove_context(pix, 12));
            pix->text_shadow = true;
         }
         else
            pix16_cursor(pix, lx - 14, ty, pix->colors[PIX16_C_TEXT]);
      }
   }
   pix->text_shadow = false;
}

/* Answer the question: the answer is the layout and the window state,
 * and it is asked once */
static void pix16_screen_choice(pix16_t *pix)
{
   settings_t *settings = config_get_ptr();
   bool wide            = pix->screen_sel >= 2;
   bool full            = (pix->screen_sel & 1) != 0;

   if (settings)
   {
      settings->bools.menu_pix16_widescreen    = wide;
      settings->bools.menu_pix16_screen_asked  = true;
   }
   pix->wide_cfg      = wide;
   pix->screen_asked  = true;
   pix->screen_prompt = false;
   pix->scr_key_valid = false;
   pix16_sfx(PIX16_SFX_OK);
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;

   if (settings && full != settings->bools.video_fullscreen)
   {
      settings->bools.video_fullscreen = full;
      command_event(CMD_EVENT_FULLSCREEN_APPLY, NULL);
   }
}

/* Draws the boot screen; false once it is over */
/* CRT power-on: a dot, then a bright line across the middle that
 * opens up into the whole picture */
#define PIX16_CRT_ON_MS 620
static void pix16_draw_crt_on(pix16_t *pix, int ms)
{
   int W = PIX16_FB_WIDTH, H = PIX16_FB_HEIGHT;
   uint16_t black = pix->conv(0xFF000000);
   uint16_t white = pix->colors[PIX16_C_J_TEXT];
   uint16_t glow  = pix->colors[PIX16_C_J_BORDER_LO];
   uint16_t bios  = pix->colors[PIX16_C_J_BG];
   pix16_fill(pix, 0, 0, W, H, black);
   if (ms < 120)
   {
      /* a dot that brightens */
      int r = 1 + ms / 40;
      pix16_fill(pix, W / 2 - r, H / 2, 2 * r, 1, ms < 60 ? glow : white);
   }
   else if (ms < 260)
   {
      /* the line stretches out sideways */
      float t = (float)(ms - 120) / 140.0f;
      int lw  = (int)(W * (1.0f - (1.0f - t) * (1.0f - t)));
      pix16_fill(pix, (W - lw) / 2, H / 2 - 1, lw, 3, glow);
      pix16_fill(pix, (W - lw) / 2, H / 2, lw, 1, white);
   }
   else if (ms < 520)
   {
      /* ...and opens up into the picture, bright edges */
      float t = (float)(ms - 260) / 260.0f;
      int h   = 2 + (int)((H - 2) * (1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t)));
      int y0  = (H - h) / 2;
      pix16_fill(pix, 0, y0, W, h, t < 0.5f ? glow : bios);
      pix16_fill(pix, 0, y0, W, 1, white);
      pix16_fill(pix, 0, y0 + h - 1, W, 1, white);
   }
   else
   {
      /* settles: the white fades out of the phosphor */
      pix16_fill(pix, 0, 0, W, H, ms < 570 ? glow : bios);
   }
}

static bool pix16_draw_boot(pix16_t *pix)
{
   retro_time_t now = menu_driver_get_current_time();

   /* the sequence starts once the first-run question is answered */
   if (pix->screen_prompt)
   {
      pix->boot_t0 = now;
      pix16_j_background(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT);
      return true;
   }
   if (!pix->boot_t0)
      pix->boot_t0 = now;
   if (pix->boot_phase == 0)
   {
      int ms = (int)((now - pix->boot_t0) / 1000);
      if (ms >= PIX16_CRT_ON_MS + pix16_bios_ms(pix))
      {
         if (!pix->start_enabled)
         {
            pix->boot_active   = false;
            pix->scr_key_valid = false;
            pix->last_n        = 0;
            pix->flags        |= PIX16_FLAG_FORCE_REDRAW;
            return false;
         }
         pix->boot_phase = 1;
         pix->logo_t0    = now;
      }
      else
      {
         if (ms < PIX16_CRT_ON_MS)
            pix16_draw_crt_on(pix, ms);
         else
            pix16_draw_bios(pix, ms - PIX16_CRT_ON_MS);
         return true;
      }
   }
   pix16_draw_logo_screen(pix, (int)((now - pix->logo_t0) / 1000));
   /* the button was pressed: after the prompt's quick flashes, go */
   if (pix->boot_go_t0 && now - pix->boot_go_t0 >= 420000)
   {
      pix->boot_go_t0    = 0;
      pix->boot_active   = false;
      pix->scr_key_valid = false;   /* the systems screen opens its windows */
      pix->last_n        = 0;
      pix->flags        |= PIX16_FLAG_FORCE_REDRAW;
   }
   return true;
}

/* A button during the boot screen: BIOS -> logo, logo -> menu */
static void pix16_boot_press(pix16_t *pix)
{
   retro_time_t now = menu_driver_get_current_time();
   if (pix->boot_phase == 0)
   {
      if (!pix->start_enabled)
      {
         pix->boot_active   = false;
         pix->scr_key_valid = false;
         pix->last_n        = 0;
         pix->flags        |= PIX16_FLAG_FORCE_REDRAW;
         return;
      }
      pix->boot_phase = 1;
      pix->logo_t0    = now;
      return;
   }
   if (now - pix->logo_t0 < (retro_time_t)PIX16_LOGO_DROP_MS * 1000)
   {
      pix->logo_t0 = now - (retro_time_t)PIX16_LOGO_DROP_MS * 1000; /* land it */
      return;
   }
   if (pix->boot_go_t0)
      return;                    /* already on its way */
   pix16_sfx(PIX16_SFX_OK);
   pix->boot_go_t0 = now;
   pix->flags     |= PIX16_FLAG_FORCE_REDRAW;
}

#define PIX16_TAB_SLIDE_MS 210

/* Options tabs: everything under the tab bar (list + description
 * windows) slides sideways, old tab out, new tab in */
static void pix16_tab_slide(pix16_t *pix)
{
   retro_time_t now = menu_driver_get_current_time();
   float t  = (float)(now - pix->slide_t0) / (PIX16_TAB_SLIDE_MS * 1000.0f);
   int x0   = PIX16_SAFE;
   int bw   = PIX16_FB_WIDTH - 2 * PIX16_SAFE;
   int y0   = PIX16_SAFE + PIX16_J_BAR_H + 2;
   int y1   = PIX16_FB_HEIGHT - PIX16_SAFE - PIX16_FOOTER_H - 4;
   int off, x, y;
   uint16_t row[PIX16_FB_MAX_W];

   if (t >= 1.0f || pix->reduced_motion)
   {
      pix->slide_active = false;
      return;
   }
   t   = pix16_ease_in_out(t);
   off = (int)(bw * t + 0.5f);
   for (y = y0; y < y1; y++)
   {
      uint16_t *dst       = pix->fb + (size_t)y * PIX16_FB_WIDTH + x0;
      const uint16_t *old = pix->slide_old + (size_t)y * PIX16_FB_WIDTH + x0;
      memcpy(row, dst, sizeof(uint16_t) * bw);         /* the new tab */
      for (x = 0; x < bw; x++)
      {
         if (pix->slide_dir > 0)
         {
            int p = x + off;                            /* moving left */
            dst[x] = (p < bw) ? old[p] : row[p - bw];
         }
         else
         {
            int p = x - off;                            /* moving right */
            dst[x] = (p >= 0) ? old[p] : row[p + bw];
         }
      }
   }
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
}

/* "QUIT GAME?" confirm window (pause window and full Quick Menu) */
static void pix16_draw_quit_confirm(pix16_t *pix, bool jrpg)
{
   const char *msg   = pix->confirm_exit ? msg_hash_to_str(MSG_PIX16_EXIT_CONFIRM)
                                         : msg_hash_to_str(MSG_PIX16_QUIT_CONFIRM);
   int w = (int)strlen(msg) * PIX16_GLYPH_W + 28;
   int h = 72;
   int x = (PIX16_FB_WIDTH  - w) / 2;
   int y = (PIX16_FB_HEIGHT - h) / 2;
   int k;

   if (jrpg)
   {
      if (!pix16_popup_window(pix, x, y, w, h))
         return;
   }
   else
   {
      pix16_panel(pix, x, y, w, h, pix->colors[PIX16_C_PANEL]);
      pix16_fill(pix, x, y, w, 2, pix->colors[PIX16_C_RED]);
   }
   pix->text_shadow = jrpg;
   pix16_text(pix, x + 14, y + 10, pix->confirm_exit ? msg_hash_to_str(MSG_PIX16_EXIT_TITLE) : msg_hash_to_str(MSG_PIX16_QUIT_TITLE),
         jrpg ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW], 1, 0);
   pix16_text(pix, x + 14, y + 28, msg,
         jrpg ? pix->colors[PIX16_C_J_TEXT] : pix->colors[PIX16_C_TEXT], 1, 0);
   for (k = 0; k < 2; k++)
   {
      const char *opt = k == 0 ? msg_hash_to_str(MSG_PIX16_YES) : msg_hash_to_str(MSG_PIX16_NO);
      int ox = x + w / 2 - 56 + k * 72;
      int oy = y + 50;
      bool sel = ((unsigned)k == pix->quit_sel);
      pix16_text(pix, ox + 18, oy, opt,
            jrpg ? pix->colors[PIX16_C_J_TEXT]
                 : (sel ? pix->colors[PIX16_C_TEXT] : pix->colors[PIX16_C_TEXT_DIM]),
            1, 0);
      if (sel)
      {
         if (jrpg)
         {
            pix->text_shadow = false;
            pix16_glove_at(pix, ox, oy - 1, pix16_glove_context(pix, 77));
            pix->text_shadow = true;
         }
         else
            pix16_cursor(pix, ox + 8, oy, pix->colors[PIX16_C_TEXT]);
      }
   }
   pix->text_shadow = false;
}

static bool pix16_quit_confirm_input(pix16_t *pix, enum menu_action action)
{
   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_UP:
         if (pix->quit_sel != 0)
         {
            pix->quit_sel = 0;
            pix16_sfx(PIX16_SFX_UP);
         }
         break;
      case MENU_ACTION_RIGHT:
      case MENU_ACTION_DOWN:
         if (pix->quit_sel != 1)
         {
            pix->quit_sel = 1;
            pix16_sfx(PIX16_SFX_DOWN);
         }
         break;
      case MENU_ACTION_OK:
         if (pix->quit_sel == 0)
         {
            pix16_sfx(PIX16_SFX_OK);
            pix->quit_confirm = false;
            pix->qm_full      = false;
            if (pix->confirm_exit)
            {
               pix->confirm_exit = false;
               command_event(CMD_EVENT_QUIT, NULL);
            }
            else
               command_event(CMD_EVENT_CLOSE_CONTENT, NULL);
         }
         else
         {
            pix16_sfx(PIX16_SFX_BACK);
            pix->quit_confirm = false;
         }
         break;
      case MENU_ACTION_CANCEL:
         pix16_sfx(PIX16_SFX_BACK);
         pix->quit_confirm = false;
         break;
      default:
         break;
   }
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
   return true;
}

/* ------------------------------------------------------------------ */
/* Add System wizard                                                   */
/* ------------------------------------------------------------------ */

static pix16_t *pix16_wiz_owner;      /* for the keyboard callback */
static void pix16_wiz_enter(pix16_t *pix, int step, bool push);

static int pix16_strcmp_nc(const char *a, const char *b)
{
   for (;; a++, b++)
   {
      int ca = (unsigned char)*a, cb = (unsigned char)*b;
      if (ca >= 'A' && ca <= 'Z')
         ca += 32;
      if (cb >= 'A' && cb <= 'Z')
         cb += 32;
      if (ca != cb || !ca)
         return ca - cb;
   }
}

static int pix16_core_cmp(const void *a, const void *b)
{
   const core_info_t *x = *(const core_info_t* const*)a;
   const core_info_t *y = *(const core_info_t* const*)b;
   return pix16_strcmp_nc(x->display_name ? x->display_name : "",
                          y->display_name ? y->display_name : "");
}

static void pix16_wiz_row(pix16_t *pix, const char *label, int kind,
      const char *val)
{
   union string_list_elem_attr a;
   a.i = kind;
   string_list_append(pix->wiz.rows, label ? label : "", a);
   a.i = 0;
   string_list_append(pix->wiz.vals, val ? val : "", a);
}

static int pix16_wiz_kind(const pix16_t *pix, size_t i)
{
   if (!pix->wiz.rows || i >= pix->wiz.rows->size)
      return PIX16_WR_INFO;
   return pix->wiz.rows->elems[i].attr.i;
}

/* the row with this value (and kind), or 'def' */
static size_t pix16_wiz_find(const pix16_t *pix, int kind, const char *val,
      size_t def)
{
   size_t i;
   if (!val || !*val)
      return def;
   for (i = 0; i < pix->wiz.rows->size; i++)
      if (     pix16_wiz_kind(pix, i) == kind
            && string_is_equal(pix->wiz.vals->elems[i].data, val))
         return i;
   return def;
}

/* keep the cursor on a row that can be chosen */
static void pix16_wiz_fix_sel(pix16_t *pix)
{
   size_t n = pix->wiz.rows ? pix->wiz.rows->size : 0, i;
   if (!n)
   {
      pix->wiz.sel = 0;
      return;
   }
   if (pix->wiz.sel >= n)
      pix->wiz.sel = n - 1;
   for (i = pix->wiz.sel; i < n; i++)
      if (pix16_wiz_kind(pix, i) != PIX16_WR_INFO)
      {
         pix->wiz.sel = i;
         return;
      }
   for (i = pix->wiz.sel + 1; i-- > 0; )
      if (pix16_wiz_kind(pix, i) != PIX16_WR_INFO)
      {
         pix->wiz.sel = i;
         return;
      }
}

static core_info_t *pix16_wiz_core(const pix16_t *pix)
{
   core_info_t *ci = NULL;
   if (!*pix->wiz.core_path || !core_info_find(pix->wiz.core_path, &ci))
      return NULL;
   return ci;
}

/* a playlist file name can't hold these */
static void pix16_safe_name(char *s)
{
   for (; *s; s++)
      if (strchr("\\/:*?\"<>|", *s))
         *s = '-';
}

static bool pix16_wiz_is_arcade(const pix16_t *pix)
{
   char name[NAME_MAX_LENGTH + 8];
   if (string_is_equal(pix->wiz.place, "ARCADE"))
      return true;
   if (*pix->wiz.place)
      return false;
   snprintf(name, sizeof(name), "%s.lpl", pix->wiz.system);
   return pix16_is_arcade_builtin(name);
}

/* "nes|fds" -> "nes fds zip 7z" (archives too, like RetroArch's own
 * playlists) */
static void pix16_wiz_exts(const pix16_t *pix, char *s, size_t len,
      char sep)
{
   core_info_t *ci = pix16_wiz_core(pix);
   char *p;
   s[0] = '\0';
   if (ci && ci->supported_extensions)
      strlcpy(s, ci->supported_extensions, len);
   for (p = s; *p; p++)
      if (*p == '|' || *p == ' ')
         *p = sep;
   if (*s)
   {
      char pat[8];
      snprintf(pat, sizeof(pat), "%czip", sep);
      if (!strstr(s, pat) && strncmp(s, "zip", 3))
      {
         size_t n = strlen(s);
         snprintf(s + n, len - n, "%czip", sep);
      }
      snprintf(pat, sizeof(pat), "%c7z", sep);
      if (!strstr(s, pat) && strncmp(s, "7z", 2))
      {
         size_t n = strlen(s);
         snprintf(s + n, len - n, "%c7z", sep);
      }
   }
}

static bool pix16_is_root_dir(const char *d)
{
   size_t n = strlen(d);
   if (string_is_equal(d, "/"))
      return true;
   return n >= 2 && n <= 3 && d[1] == ':';
}

static void pix16_wiz_walk_stop(pix16_t *pix)
{
   dir_list_iter_free(pix->wiz.walk);
   if (pix->wiz.walk_list)
      string_list_free(pix->wiz.walk_list);
   pix->wiz.walk      = NULL;
   pix->wiz.walk_list = NULL;
}

static bool pix16_wiz_walk_budget(void *userdata)
{
   return cpu_features_get_time_usec() < *(const retro_time_t*)userdata;
}

/* Reads the folder a slice per frame; when it is all read, its folders
 * and files replace the "READING..." row */
static void pix16_wiz_walk_poll(pix16_t *pix)
{
   pix16_wiz_t *w     = &pix->wiz;
   retro_time_t until = cpu_features_get_time_usec() + 2000;
   size_t i, files = 0;
   int ret;

   if (!w->walk)
      return;
   if (!(ret = dir_list_iter_step(w->walk, pix16_wiz_walk_budget, &until)))
      return;
   dir_list_iter_free(w->walk);
   w->walk = NULL;
   /* drop the READING row */
   if (w->rows && w->rows->size)
   {
      w->rows->size--;
      free(w->rows->elems[w->rows->size].data);
      w->vals->size--;
      free(w->vals->elems[w->vals->size].data);
   }
   if (ret > 0 && w->walk_list)
   {
      dir_list_sort(w->walk_list, true);
      for (i = 0; i < w->walk_list->size; i++)
      {
         const char *p = w->walk_list->elems[i].data;
         char label[NAME_MAX_LENGTH + 2];
         if (w->walk_list->elems[i].attr.i == RARCH_DIRECTORY)
         {
            snprintf(label, sizeof(label), "%s/", path_basename(p));
            pix16_wiz_row(pix, label, PIX16_WR_DIR, p);
         }
         else if (w->browse_for == PIX16_BROWSE_DIR)
            files++;
         else
            pix16_wiz_row(pix, path_basename(p), PIX16_WR_FILE, p);
      }
   }
   if (files)
   {
      char note[64];
      snprintf(note, sizeof(note), msg_hash_to_str(MSG_PIX16_WIZ_FILES),
            (unsigned)files);
      pix16_wiz_row(pix, note, PIX16_WR_INFO, NULL);
   }
   string_list_free(w->walk_list);
   w->walk_list = NULL;
   if (*w->hint)
   {
      w->sel     = pix16_wiz_find(pix, PIX16_WR_DIR, w->hint, w->sel);
      w->hint[0] = '\0';
   }
   pix16_wiz_fix_sel(pix);
}

/* "KEY    value" summary line; keys padded to one column */
static void pix16_wiz_kv(char *s, size_t len, enum msg_hash_enums key,
      const char *val)
{
   snprintf(s, len, "%-7s%s", msg_hash_to_str(key), val ? val : "");
}

/* Fills the rows for the current step. Returns false for a BIOS step
 * with nothing missing (the wizard goes straight on). */
static bool pix16_wiz_build(pix16_t *pix)
{
   pix16_wiz_t *w        = &pix->wiz;
   settings_t *settings  = config_get_ptr();
   bool keep             = true;

   pix16_wiz_walk_stop(pix);
   if (w->rows)
      string_list_free(w->rows);
   if (w->vals)
      string_list_free(w->vals);
   w->rows = string_list_new();
   w->vals = string_list_new();
   w->sel  = 0;
   w->top  = 0;
   if (!w->rows || !w->vals)
      return true;

   switch (w->step)
   {
      case PIX16_WIZ_CORE:
      {
         core_info_list_t *cl = NULL;
         core_info_t **sorted = NULL;
         size_t i, n = 0;
#ifdef HAVE_NETWORKING
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_DOWNLOAD), PIX16_WR_DOWNLOAD, NULL);
#endif
         core_info_get_list(&cl);
         if (cl && cl->count
               && (sorted = (core_info_t**)malloc(cl->count * sizeof(*sorted))))
         {
            for (i = 0; i < cl->count; i++)
            {
               core_info_t *ci = core_info_get(cl, i);
               if (ci && ci->path && ci->display_name && *ci->display_name)
                  sorted[n++] = ci;
            }
            qsort(sorted, n, sizeof(*sorted), pix16_core_cmp);
            for (i = 0; i < n; i++)
               pix16_wiz_row(pix, sorted[i]->display_name, PIX16_WR_CORE,
                     sorted[i]->path);
            free(sorted);
         }
         if (!n)
            pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_NO_CORES), PIX16_WR_INFO, NULL);
         /* the last core used, else the first core (not the download row) */
         w->sel = pix16_wiz_find(pix, PIX16_WR_CORE, w->core_path,
               (n && w->rows->size > n) ? w->rows->size - n : 0);
         break;
      }
      case PIX16_WIZ_SYSTEM:
      {
         core_info_t *ci = pix16_wiz_core(pix);
         size_t i;
         if (ci && ci->databases_list)
            for (i = 0; i < ci->databases_list->size; i++)
            {
               char name[NAME_MAX_LENGTH];
               strlcpy(name, ci->databases_list->elems[i].data, sizeof(name));
               if (string_is_equal_noncase(path_get_extension(name), "rdb"))
                  path_remove_extension(name);
               pix16_safe_name(name);
               if (*name && pix16_wiz_find(pix, PIX16_WR_SYS, name,
                        (size_t)-1) == (size_t)-1)
                  pix16_wiz_row(pix, name, PIX16_WR_SYS, name);
            }
         if (w->rows->size == 0 && ci && ci->systemname && *ci->systemname)
         {
            char name[NAME_MAX_LENGTH];
            if (ci->system_manufacturer && *ci->system_manufacturer)
               snprintf(name, sizeof(name), "%s - %s",
                     ci->system_manufacturer, ci->systemname);
            else
               strlcpy(name, ci->systemname, sizeof(name));
            pix16_safe_name(name);
            pix16_wiz_row(pix, name, PIX16_WR_SYS, name);
         }
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_TYPE_NAME), PIX16_WR_OTHER, NULL);
         w->sel = pix16_wiz_find(pix, PIX16_WR_SYS, w->system, 0);
         break;
      }
      case PIX16_WIZ_PLACE:
      {
         file_list_t *list = pix16_selection_buf();
         size_t k;
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_HOME), PIX16_WR_HOME, NULL);
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_INSIDE_ARCADE), PIX16_WR_ARCADE, "ARCADE");
         pix16_grid_build(pix);
         for (k = 0; list && k < pix->grid_count; k++)
         {
            size_t idx = pix->grid_map[k];
            char base[NAME_MAX_LENGTH], title[NAME_MAX_LENGTH];
            char label[NAME_MAX_LENGTH + 8];
            const char *p;
            if (pix16_arcade_grouped(pix) && idx == pix->arcade_rep)
               continue;
            if (idx >= list->size
                  || list->list[idx].type != FILE_TYPE_PLAYLIST_COLLECTION
                  || !(p = list->list[idx].path))
               continue;
            strlcpy(base, path_basename(p), sizeof(base));
            path_remove_extension(base);
            if (string_is_equal_noncase(base, w->system))
               continue;
            pix16_system_title(base, title, sizeof(title));
            snprintf(label, sizeof(label), msg_hash_to_str(MSG_PIX16_WIZ_INSIDE), title);
            pix16_wiz_row(pix, label, PIX16_WR_HOST, base);
         }
         if (*w->place)
            w->sel = pix16_wiz_find(pix,
                  string_is_equal(w->place, "ARCADE") ? PIX16_WR_ARCADE
                                                      : PIX16_WR_HOST,
                  w->place, 0);
         else
         {
            char name[NAME_MAX_LENGTH + 8];
            snprintf(name, sizeof(name), "%s.lpl", w->system);
            w->sel = pix16_is_arcade_builtin(name) ? 1 : 0;
         }
         break;
      }
      case PIX16_WIZ_SOURCE:
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_SCAN_FOLDER), PIX16_WR_SRC_DIR, NULL);
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_ONE_GAME), PIX16_WR_SRC_ONE, NULL);
         w->sel = (size_t)w->source;
         break;
      case PIX16_WIZ_BROWSE:
         if (!*w->dir)
         {
            /* the drives / storage the frontend knows of */
            file_list_t drives;
            size_t d;
            memset(&drives, 0, sizeof(drives));
            frontend_driver_parse_drive_list(&drives, false);
            for (d = 0; d < drives.size; d++)
               if (!string_is_empty(drives.list[d].path))
                  pix16_wiz_row(pix, drives.list[d].path, PIX16_WR_DIR,
                        drives.list[d].path);
            file_list_deinitialize(&drives);
            if (!w->rows->size)
               pix16_wiz_row(pix, "/", PIX16_WR_DIR, "/");
            if (settings && !string_is_empty(settings->paths.directory_menu_content))
               pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_START_FOLDER), PIX16_WR_DIR,
                     settings->paths.directory_menu_content);
         }
         else
         {
            char exts[1024];
            const char *filter = NULL;
            if (w->browse_for == PIX16_BROWSE_DIR)
               pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_USE_FOLDER),
                     PIX16_WR_USE_DIR, w->dir);
            pix16_wiz_row(pix, "..", PIX16_WR_UP, NULL);
            if (w->browse_for == PIX16_BROWSE_GAME)
            {
               pix16_wiz_exts(pix, exts, sizeof(exts), '|');
               filter = *exts ? exts : NULL;
            }
            else if (w->browse_for == PIX16_BROWSE_DAT)
               filter = "dat|xml";
            if (     (w->walk_list = string_list_new())
                  && (w->walk = dir_list_iter_new(w->dir, filter, true,
                        settings && settings->bools.show_hidden_files,
                        false, false, w->walk_list)))
               pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_READING),
                     PIX16_WR_INFO, NULL);
            else
               pix16_wiz_walk_stop(pix);
         }
         break;
      case PIX16_WIZ_DAT:
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_NO_DAT), PIX16_WR_NO_DAT, NULL);
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_PICK_DAT), PIX16_WR_PICK_DAT, NULL);
         w->sel = *w->dat ? 1 : 0;
         break;
      case PIX16_WIZ_BIOS:
      {
         core_info_t *ci = pix16_wiz_core(pix);
         core_info_ctx_firmware_t fw;
         const char *sysdir = settings ? settings->paths.directory_system : "";
         size_t i, shown = 0, missing = 0;
         if (!ci || !ci->firmware_count)
         {
            keep = false;
            break;
         }
         fw.path             = ci->path;
         fw.directory.system = sysdir;
         core_info_list_update_missing_firmware(&fw);
         for (i = 0; i < ci->firmware_count; i++)
            if (ci->firmware[i].missing && !ci->firmware[i].optional)
            {
               missing++;
               if (shown < 4 && ci->firmware[i].path)
               {
                  char line[NAME_MAX_LENGTH + 4];
                  snprintf(line, sizeof(line), "  %s", ci->firmware[i].path);
                  pix16_wiz_row(pix, line, PIX16_WR_INFO, NULL);
                  shown++;
               }
            }
         if (!missing)
         {
            keep = false;
            break;
         }
         if (missing > shown)
         {
            char more[48];
            snprintf(more, sizeof(more), msg_hash_to_str(MSG_PIX16_WIZ_MORE),
                  (unsigned)(missing - shown));
            pix16_wiz_row(pix, more, PIX16_WR_INFO, NULL);
         }
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_PUT_THEM_IN), PIX16_WR_INFO, NULL);
         pix16_wiz_row(pix, *sysdir ? sysdir : msg_hash_to_str(MSG_PIX16_WIZ_SYSTEM_FOLDER),
               PIX16_WR_INFO, NULL);
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_CHECK_AGAIN), PIX16_WR_RECHECK, NULL);
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_CONTINUE), PIX16_WR_CONTINUE, NULL);
         break;
      }
      case PIX16_WIZ_CONFIRM:
      {
         char line[PATH_MAX_LENGTH + 16];
         pix16_wiz_kv(line, sizeof(line), MSG_PIX16_WIZ_KEY_CORE, w->core_name);
         pix16_wiz_row(pix, line, PIX16_WR_INFO, NULL);
         pix16_wiz_kv(line, sizeof(line), MSG_PIX16_WIZ_KEY_SYSTEM, w->system);
         pix16_wiz_row(pix, line, PIX16_WR_INFO, NULL);
         pix16_wiz_kv(line, sizeof(line), MSG_PIX16_WIZ_KEY_WHERE, w->place_title);
         pix16_wiz_row(pix, line, PIX16_WR_INFO, NULL);
         pix16_wiz_kv(line, sizeof(line),
               w->source == 0 ? MSG_PIX16_WIZ_KEY_FOLDER : MSG_PIX16_WIZ_KEY_GAME,
               *path_basename(w->pick) ? path_basename(w->pick) : w->pick);
         pix16_wiz_row(pix, line, PIX16_WR_INFO, NULL);
         if (w->source == 0 && pix16_wiz_is_arcade(pix))
         {
            pix16_wiz_kv(line, sizeof(line), MSG_PIX16_WIZ_KEY_NAMES,
                  *w->dat ? path_basename(w->dat) : msg_hash_to_str(MSG_PIX16_WIZ_FILE_NAMES));
            pix16_wiz_row(pix, line, PIX16_WR_INFO, NULL);
         }
         pix16_wiz_row(pix, "", PIX16_WR_INFO, NULL);
         pix16_wiz_row(pix, msg_hash_to_str(MSG_PIX16_WIZ_ADD), PIX16_WR_ADD, NULL);
         break;
      }
      default:
         break;
   }
   pix16_wiz_fix_sel(pix);
   return keep;
}

static void pix16_wiz_enter(pix16_t *pix, int step, bool push)
{
   pix16_wiz_t *w = &pix->wiz;
   if (push && w->hist_n < PIX16_WIZ_HIST)
      w->hist[w->hist_n++] = w->step | (w->browse_for << 8);
   w->step = step;
   if (!pix16_wiz_build(pix) && step == PIX16_WIZ_BIOS)
   {
      /* nothing missing: on to the summary */
      w->step = PIX16_WIZ_CONFIRM;
      pix16_wiz_build(pix);
   }
}

static void pix16_wiz_open(pix16_t *pix)
{
   pix16_wiz_t *w = &pix->wiz;
   w->core_path[0] = w->core_name[0] = w->system[0] = '\0';
   w->place[0]     = w->place_title[0] = w->pick[0] = w->dat[0] = '\0';
   w->hint[0]      = '\0';
   w->hist_n       = 0;
   w->source       = 0;
   w->browse_for   = PIX16_BROWSE_DIR;
   w->step         = PIX16_WIZ_CORE;
   w->open         = true;
   w->resume       = false;
   pix16_wiz_owner = pix;
   pix16_wiz_build(pix);
}

static void pix16_wiz_close(pix16_t *pix)
{
   pix16_wiz_walk_stop(pix);
   pix->wiz.open = false;
   if (pix->wiz.rows)
      string_list_free(pix->wiz.rows);
   if (pix->wiz.vals)
      string_list_free(pix->wiz.vals);
   pix->wiz.rows = NULL;
   pix->wiz.vals = NULL;
}

/* after the games folder / the game was picked */
static void pix16_wiz_after_browse(pix16_t *pix)
{
   if (pix->wiz.source == 0 && pix16_wiz_is_arcade(pix))
      pix16_wiz_enter(pix, PIX16_WIZ_DAT, true);
   else
      pix16_wiz_enter(pix, PIX16_WIZ_BIOS, true);
}

static void pix16_wiz_kb_cb(void *userdata, const char *str)
{
   pix16_t *pix = pix16_wiz_owner;
   const char *line = menu_input_dialog_get_buffer();
   if (pix && pix->wiz.open && str && line && *line)
   {
      strlcpy(pix->wiz.system, line, sizeof(pix->wiz.system));
      pix16_safe_name(pix->wiz.system);
      pix16_trim(pix->wiz.system);
      if (*pix->wiz.system)
         pix16_wiz_enter(pix, PIX16_WIZ_PLACE, true);
   }
   menu_input_dialog_end();
}

static void pix16_wiz_open_downloader(pix16_t *pix)
{
#ifdef HAVE_NETWORKING
   core_updater_list_t *cl = core_updater_list_get_cached();
   if (!cl)
   {
      core_updater_list_init_cached();
      cl = core_updater_list_get_cached();
   }
   if (!cl)
      return;
   menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH;
   command_event(CMD_EVENT_NETWORK_INIT, NULL);
   task_push_get_core_updater_list(cl, false, true);
   pix16_wiz_close(pix);
   pix->wiz.resume = true;      /* back here when the downloader closes */
   generic_action_ok_displaylist_push("", NULL,
         MENU_ENUM_LABEL_CORE_UPDATER_LIST_STR, 0, 0, 0,
         ACTION_OK_DL_CORE_UPDATER_LIST);
#endif
}

static void pix16_notify(const char *msg)
{
   runloop_msg_queue_push(msg, strlen(msg), 1, 240, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

/* Adds the system: scans the folder (or adds the one game), and
 * records where its tile goes */
static void pix16_wiz_run(pix16_t *pix)
{
   pix16_wiz_t *w       = &pix->wiz;
   settings_t *settings = config_get_ptr();
   char msg[NAME_MAX_LENGTH * 2 + 64];

   if (!settings || string_is_empty(settings->paths.directory_playlist))
   {
      pix16_notify(msg_hash_to_str(MSG_PIX16_WIZ_NO_PLAYLIST_DIR));
      return;
   }

   if (w->source == 0)
   {
      manual_content_scan_saved_t *saved;
      char exts[1024];
      if (task_dbscan_chain_refuse())
         return;   /* a scan is running: RetroArch says so */
      saved = manual_content_scan_save_settings();
      manual_content_scan_set_menu_content_dir_kind(w->pick,
            MANUAL_CONTENT_SCAN_PATH_FOLDER);
      manual_content_scan_set_menu_scan_method(MANUAL_CONTENT_SCAN_METHOD_CUSTOM);
      strlcpy(manual_content_scan_get_system_name_custom_ptr(), w->system,
            manual_content_scan_get_system_name_custom_size());
      manual_content_scan_set_menu_system_name(
            MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, w->system);
      manual_content_scan_set_menu_core_name(MANUAL_CONTENT_SCAN_CORE_SET,
            w->core_name);
      pix16_wiz_exts(pix, exts, sizeof(exts), ' ');
      strlcpy(manual_content_scan_get_file_exts_custom_ptr(), exts,
            manual_content_scan_get_file_exts_custom_size());
      if (*w->dat)
      {
         strlcpy(manual_content_scan_get_dat_file_path_ptr(), w->dat,
               manual_content_scan_get_dat_file_path_size());
         manual_content_scan_set_menu_scan_use_db(
               MANUAL_CONTENT_SCAN_USE_DB_DAT_LOOSE);
      }
      else
         manual_content_scan_set_menu_scan_use_db(MANUAL_CONTENT_SCAN_USE_DB_NONE);
      *manual_content_scan_get_search_recursively_ptr() = true;
      *manual_content_scan_get_search_archives_ptr()    = false;
      *manual_content_scan_get_overwrite_playlist_ptr() = false;
      *manual_content_scan_get_validate_entries_ptr()   = false;
      if (!task_push_manual_content_scan(true, NULL, NULL))
      {
         manual_content_scan_restore_settings(saved);
         return;   /* RetroArch shows why */
      }
      manual_content_scan_restore_settings(saved);
      snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_PIX16_WIZ_SCANNING), w->system);
      pix16_notify(msg);
   }
   else
   {
      char file[NAME_MAX_LENGTH + 8];
      char label[NAME_MAX_LENGTH];
      playlist_config_t *cfg;
      playlist_t *pl;
      playlist_t *cached = playlist_get_cached();
      struct playlist_entry e = {0};

      /* holds two path buffers: on the heap */
      if (!(cfg = (playlist_config_t*)calloc(1, sizeof(*cfg))))
         return;
      snprintf(file, sizeof(file), "%s.lpl", w->system);
      fill_pathname_join_special(cfg->path, settings->paths.directory_playlist,
            file, sizeof(cfg->path));
      cfg->capacity            = COLLECTION_SIZE;
      cfg->old_format          = settings->bools.playlist_use_old_format;
      cfg->compress            = settings->bools.playlist_compression;
      cfg->fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
      playlist_config_set_base_content_directory(cfg,
            settings->bools.playlist_portable_paths
            ? settings->paths.directory_menu_content : NULL);

      /* the cached copy would be stale (and written back over ours) */
      if (cached && string_is_equal(playlist_get_conf_path(cached), cfg->path))
         playlist_free_cached();

      pl = playlist_init(cfg);
      free(cfg);
      if (!pl)
      {
         pix16_notify(msg_hash_to_str(MSG_PIX16_WIZ_PLAYLIST_FAILED));
         return;
      }
      fill_pathname(label, path_basename(w->pick), "", sizeof(label));
      e.path      = w->pick;
      e.label     = label;
      e.core_path = w->core_path;
      e.core_name = w->core_name;
      e.crc32     = (char*)"00000000|crc";
      e.db_name   = file;
      if (string_is_empty(playlist_get_default_core_path(pl)))
      {
         playlist_set_default_core_path(pl, w->core_path);
         playlist_set_default_core_name(pl, w->core_name);
      }
      playlist_push(pl, &e);
      playlist_write_file(pl);
      playlist_free(pl);
      snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_PIX16_WIZ_ADDED), label, w->system);
      pix16_notify(msg);
   }

   pix16_groups_set(w->system, *w->place ? w->place : "HOME");
   pix16_wiz_close(pix);
   menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   pix16_sfx(PIX16_SFX_OK);
}

static void pix16_wiz_input(pix16_t *pix, enum menu_action action)
{
   pix16_wiz_t *w = &pix->wiz;
   size_t n       = w->rows ? w->rows->size : 0;
   size_t i;

   switch (action)
   {
      case MENU_ACTION_UP:
         for (i = w->sel; i-- > 0; )
            if (pix16_wiz_kind(pix, i) != PIX16_WR_INFO)
            {
               w->sel = i;
               pix16_sfx(PIX16_SFX_UP);
               break;
            }
         break;
      case MENU_ACTION_DOWN:
         for (i = w->sel + 1; i < n; i++)
            if (pix16_wiz_kind(pix, i) != PIX16_WR_INFO)
            {
               w->sel = i;
               pix16_sfx(PIX16_SFX_DOWN);
               break;
            }
         break;
      case MENU_ACTION_LEFT:
      case MENU_ACTION_SCROLL_UP:
         if (w->sel > 0)
         {
            w->sel = w->sel > PIX16_WIZ_ROWS ? w->sel - PIX16_WIZ_ROWS : 0;
            pix16_wiz_fix_sel(pix);
            pix16_sfx(PIX16_SFX_UP);
         }
         break;
      case MENU_ACTION_RIGHT:
      case MENU_ACTION_SCROLL_DOWN:
         if (n && w->sel + 1 < n)
         {
            w->sel += PIX16_WIZ_ROWS;
            pix16_wiz_fix_sel(pix);
            pix16_sfx(PIX16_SFX_DOWN);
         }
         break;
      case MENU_ACTION_CANCEL:
         pix16_sfx(PIX16_SFX_BACK);
         if (w->step == PIX16_WIZ_BROWSE && *w->dir)
         {
            /* up a folder */
            char from[PATH_MAX_LENGTH];
            strlcpy(from, w->dir, sizeof(from));
            if (pix16_is_root_dir(w->dir))
               w->dir[0] = '\0';
            else
               path_parent_dir(w->dir, strlen(w->dir));
            pix16_wiz_build(pix);
            strlcpy(w->hint, from, sizeof(w->hint));
         }
         else if (w->hist_n > 0)
         {
            int h         = w->hist[--w->hist_n];
            w->step       = h & 0xff;
            w->browse_for = h >> 8;
            pix16_wiz_build(pix);
         }
         else
            pix16_wiz_close(pix);
         break;
      case MENU_ACTION_OK:
      {
         int kind        = pix16_wiz_kind(pix, w->sel);
         const char *lbl = (w->sel < n) ? w->rows->elems[w->sel].data : "";
         const char *val = (w->sel < n) ? w->vals->elems[w->sel].data : "";
         if (kind != PIX16_WR_INFO)
            pix16_sfx(PIX16_SFX_OK);
         switch (kind)
         {
            case PIX16_WR_CORE:
               if (!string_is_equal(w->core_path, val))
                  w->system[0] = '\0';
               strlcpy(w->core_path, val, sizeof(w->core_path));
               strlcpy(w->core_name, lbl, sizeof(w->core_name));
               pix16_wiz_enter(pix, PIX16_WIZ_SYSTEM, true);
               break;
            case PIX16_WR_DOWNLOAD:
               pix16_wiz_open_downloader(pix);
               break;
            case PIX16_WR_SYS:
               strlcpy(w->system, val, sizeof(w->system));
               pix16_wiz_enter(pix, PIX16_WIZ_PLACE, true);
               break;
            case PIX16_WR_OTHER:
            {
               menu_input_ctx_line_t line;
               memset(&line, 0, sizeof(line));
               line.label         = msg_hash_to_str(MSG_PIX16_WIZ_SYSTEM_NAME);
               line.label_setting = "pix16_system_name";
               line.text_type     = MENU_INPUT_DIALOG_KB_TYPE_TEXT;
               line.cb            = pix16_wiz_kb_cb;
               pix16_wiz_owner    = pix;
               menu_input_dialog_start(&line);
               break;
            }
            case PIX16_WR_HOME:
               w->place[0] = '\0';
               strlcpy(w->place_title, msg_hash_to_str(MSG_PIX16_WIZ_HOME), sizeof(w->place_title));
               pix16_wiz_enter(pix, PIX16_WIZ_SOURCE, true);
               break;
            case PIX16_WR_ARCADE:
               strlcpy(w->place, "ARCADE", sizeof(w->place));
               strlcpy(w->place_title, msg_hash_to_str(MSG_PIX16_WIZ_INSIDE_ARCADE), sizeof(w->place_title));
               pix16_wiz_enter(pix, PIX16_WIZ_SOURCE, true);
               break;
            case PIX16_WR_HOST:
               strlcpy(w->place, val, sizeof(w->place));
               strlcpy(w->place_title, lbl, sizeof(w->place_title));
               pix16_wiz_enter(pix, PIX16_WIZ_SOURCE, true);
               break;
            case PIX16_WR_SRC_DIR:
            case PIX16_WR_SRC_ONE:
               w->source     = (kind == PIX16_WR_SRC_ONE) ? 1 : 0;
               w->browse_for = w->source ? PIX16_BROWSE_GAME : PIX16_BROWSE_DIR;
               pix16_wiz_enter(pix, PIX16_WIZ_BROWSE, true);
               break;
            case PIX16_WR_DIR:
               strlcpy(w->dir, val, sizeof(w->dir));
               pix16_wiz_build(pix);
               break;
            case PIX16_WR_UP:
            {
               char from[PATH_MAX_LENGTH];
               strlcpy(from, w->dir, sizeof(from));
               if (pix16_is_root_dir(w->dir))
                  w->dir[0] = '\0';
               else
                  path_parent_dir(w->dir, strlen(w->dir));
               pix16_wiz_build(pix);
               strlcpy(w->hint, from, sizeof(w->hint));
               break;
            }
            case PIX16_WR_USE_DIR:
               strlcpy(w->pick, w->dir, sizeof(w->pick));
               pix16_wiz_after_browse(pix);
               break;
            case PIX16_WR_FILE:
               if (w->browse_for == PIX16_BROWSE_DAT)
               {
                  strlcpy(w->dat, val, sizeof(w->dat));
                  pix16_wiz_enter(pix, PIX16_WIZ_BIOS, true);
               }
               else
               {
                  strlcpy(w->pick, val, sizeof(w->pick));
                  pix16_wiz_after_browse(pix);
               }
               break;
            case PIX16_WR_NO_DAT:
               w->dat[0] = '\0';
               pix16_wiz_enter(pix, PIX16_WIZ_BIOS, true);
               break;
            case PIX16_WR_PICK_DAT:
               w->browse_for = PIX16_BROWSE_DAT;
               pix16_wiz_enter(pix, PIX16_WIZ_BROWSE, true);
               break;
            case PIX16_WR_RECHECK:
               if (!pix16_wiz_build(pix))
               {
                  w->step = PIX16_WIZ_CONFIRM;
                  pix16_wiz_build(pix);
               }
               else
                  w->sel = pix16_wiz_find(pix, PIX16_WR_RECHECK, "", w->sel);
               break;
            case PIX16_WR_CONTINUE:
               pix16_wiz_enter(pix, PIX16_WIZ_CONFIRM, true);
               break;
            case PIX16_WR_ADD:
               pix16_wiz_run(pix);
               break;
            default:
               break;
         }
         break;
      }
      default:
         break;
   }
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
}

static const char *pix16_wiz_question(const pix16_t *pix)
{
   switch (pix->wiz.step)
   {
      case PIX16_WIZ_CORE:    return msg_hash_to_str(MSG_PIX16_WIZ_Q_CORE);
      case PIX16_WIZ_SYSTEM:  return msg_hash_to_str(MSG_PIX16_WIZ_Q_SYSTEM);
      case PIX16_WIZ_PLACE:   return msg_hash_to_str(MSG_PIX16_WIZ_Q_PLACE);
      case PIX16_WIZ_SOURCE:  return msg_hash_to_str(MSG_PIX16_WIZ_Q_SOURCE);
      case PIX16_WIZ_BROWSE:
         if (pix->wiz.browse_for == PIX16_BROWSE_DAT)
            return msg_hash_to_str(MSG_PIX16_WIZ_Q_DAT_FILE);
         if (pix->wiz.browse_for == PIX16_BROWSE_GAME)
            return msg_hash_to_str(MSG_PIX16_WIZ_Q_GAME);
         return msg_hash_to_str(MSG_PIX16_WIZ_Q_FOLDER);
      case PIX16_WIZ_DAT:
         return msg_hash_to_str(MSG_PIX16_WIZ_Q_DAT);
      case PIX16_WIZ_BIOS:    return msg_hash_to_str(MSG_PIX16_WIZ_Q_BIOS);
      case PIX16_WIZ_CONFIRM: return msg_hash_to_str(MSG_PIX16_WIZ_Q_CONFIRM);
      default:                return "";
   }
}

#define PIX16_WIZ_W  (PIX16_FB_WIDTH - 40)
#define PIX16_WIZ_H  (PIX16_FB_HEIGHT - 52)
#define PIX16_WIZ_RH 14

static void pix16_draw_wizard(pix16_t *pix, bool jrpg)
{
   pix16_wiz_t *w = &pix->wiz;
   int x = (PIX16_FB_WIDTH - PIX16_WIZ_W) / 2;
   int y = PIX16_SAFE + 10;
   int cols = (PIX16_WIZ_W - 24) / PIX16_GLYPH_W;
   int row_y0 = y + 64;
   size_t n = w->rows ? w->rows->size : 0, k;
   char step[24], sub[PATH_MAX_LENGTH];
   uint16_t gold = jrpg ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW];
   uint16_t text = jrpg ? pix->colors[PIX16_C_J_TEXT] : pix->colors[PIX16_C_TEXT];
   uint16_t dim  = jrpg ? pix->colors[PIX16_C_J_DIM]  : pix->colors[PIX16_C_TEXT_DIM];
   uint16_t val  = jrpg ? pix->colors[PIX16_C_J_VALUE] : pix->colors[PIX16_C_TEXT_DIM];

   pix16_wiz_walk_poll(pix);
   n = w->rows ? w->rows->size : 0;

   if (jrpg)
   {
      if (!pix16_popup_window(pix, x, y, PIX16_WIZ_W, PIX16_WIZ_H))
         return;
   }
   else
   {
      pix16_panel(pix, x, y, PIX16_WIZ_W, PIX16_WIZ_H, pix->colors[PIX16_C_PANEL]);
      pix16_fill(pix, x, y, PIX16_WIZ_W, 2, pix->conv(0xFF52C86A));
   }
   pix->text_shadow = jrpg;

   pix16_text(pix, x + 12, y + 9, msg_hash_to_str(MSG_PIX16_WIZ_TITLE), gold, 1, 0);
   snprintf(step, sizeof(step), msg_hash_to_str(MSG_PIX16_WIZ_STEP), w->hist_n + 1);
   pix16_text(pix, x + PIX16_WIZ_W - 12 - (int)strlen(step) * PIX16_GLYPH_W,
         y + 9, step, dim, 1, 0);

   pix16_wrap(pix, x + 12, y + 24, pix16_wiz_question(pix), text,
         (size_t)cols, 2, 10);

   /* what was picked so far / where the browser is */
   sub[0] = '\0';
   switch (w->step)
   {
      case PIX16_WIZ_SYSTEM:
         strlcpy(sub, w->core_name, sizeof(sub));
         break;
      case PIX16_WIZ_PLACE:
      case PIX16_WIZ_SOURCE:
         strlcpy(sub, w->system, sizeof(sub));
         break;
      case PIX16_WIZ_BROWSE:
         strlcpy(sub, *w->dir ? w->dir : msg_hash_to_str(MSG_PIX16_WIZ_THIS_COMPUTER), sizeof(sub));
         break;
      default:
         break;
   }
   if (*sub)
   {
      /* long paths: show the end */
      size_t len = strlen(sub);
      const char *s = sub;
      if (len > (size_t)cols)
      {
         s = sub + len - (cols - 2);
         pix16_text(pix, x + 12, y + 47, "..", val, 1, 0);
         pix16_text(pix, x + 12 + 2 * PIX16_GLYPH_W, y + 47, s, val, 1, 0);
      }
      else
         pix16_text(pix, x + 12, y + 47, s, val, 1, 0);
   }
   if (jrpg)
      pix16_fill(pix, x + 10, y + 58, PIX16_WIZ_W - 20, 1,
            pix->colors[PIX16_C_J_BORDER_LO]);

   if (w->sel < w->top)
      w->top = w->sel;
   if (w->sel >= w->top + PIX16_WIZ_ROWS)
      w->top = w->sel - PIX16_WIZ_ROWS + 1;

   for (k = 0; k < PIX16_WIZ_ROWS; k++)
   {
      size_t a = w->top + k;
      int ty   = row_y0 + (int)k * PIX16_WIZ_RH;
      int kind;
      bool sel;
      const char *label;
      if (a >= n)
         break;
      kind  = pix16_wiz_kind(pix, a);
      label = w->rows->elems[a].data;
      sel   = (a == w->sel && kind != PIX16_WR_INFO);
      if (kind == PIX16_WR_INFO)
      {
         pix16_text_fit(pix, x + 16, ty, label,
               w->step == PIX16_WIZ_CONFIRM ? text : dim, (size_t)cols - 1);
         continue;
      }
      if (sel)
         pix16_vgradient(pix, x + 6, ty - 3, PIX16_WIZ_W - 12, PIX16_WIZ_RH - 1,
               pix->sel_top, pix->sel_bot);
      pix16_text_ticker(pix, x + 28, ty,
            label,
            (kind == PIX16_WR_ADD || kind == PIX16_WR_USE_DIR) ? gold
            : (!jrpg && sel) ? pix->colors[PIX16_C_DARK_TEXT] : text,
            (size_t)((PIX16_WIZ_W - 40) / PIX16_GLYPH_W), sel);
      if (sel && jrpg)
      {
         pix->text_shadow = false;
         pix16_glove_at(pix, x + 9, ty - 1, pix16_glove_context(pix, 100));
         pix->text_shadow = true;
      }
      else if (sel)
         pix16_cursor(pix, x + 12, ty, pix->colors[PIX16_C_DARK_TEXT]);
   }
   if (w->top > 0)
      pix16_text(pix, x + PIX16_WIZ_W - 14, row_y0 - 4, "^", gold, 1, 0);
   if (w->top + PIX16_WIZ_ROWS < n)
      pix16_text(pix, x + PIX16_WIZ_W - 14, y + PIX16_WIZ_H - 11, "v", gold, 1, 0);
   pix->text_shadow = false;
}

/* ------------------------------------------------------------------
 * Battle swirl: the FF7-style launch transition
 *
 * The menu spins into a vortex, its centre twisting further than the
 * edges, the picture receding as it goes, then a white flash hands over
 * to the game. Everything is sampled from a copy of the frame drawn this
 * tick, so the effect is a post-process on the finished screen.
 * ------------------------------------------------------------------ */

#define PIX16_SWIRL_MS      900      /* length of the whole effect */
#define PIX16_SWIRL_TURNS   1.7f     /* full rotations, eased in */
#define PIX16_SWIRL_TWIST   2.6f     /* extra twist at the centre, radians */
#define PIX16_SWIRL_SHRINK  0.60f    /* how fast the picture recedes */
#define PIX16_SWIRL_FLASH   0.70f    /* where the white wash starts */

/* 0 at the start of the effect, 1 when it is done */
static float pix16_swirl_progress(const pix16_t *pix)
{
   retro_time_t ms = (menu_driver_get_current_time() - pix->swirl_t0) / 1000;
   float t         = (float)ms / (float)PIX16_SWIRL_MS;
   if (t < 0.0f)
      t = 0.0f;
   if (t > 1.0f)
      t = 1.0f;
   return t;
}

/* Lifts a packed pixel toward white, in the framebuffer's own format */
static uint16_t pix16_toward_white(uint16_t c, unsigned f, bool fmt_565)
{
   unsigned r, g, b;
   if (fmt_565)
   {
      r = (c >> 11) & 0x1F;
      g = (c >>  5) & 0x3F;
      b =  c        & 0x1F;
      r += ((31u - r) * f) / 255u;
      g += ((63u - g) * f) / 255u;
      b += ((31u - b) * f) / 255u;
      return (uint16_t)((r << 11) | (g << 5) | b);
   }
   /* the xxxx4444 layouts differ in channel order, so lift each nibble
    * on its own: 0xFFFF is white in all of them */
   {
      unsigned n[4];
      int k;
      for (k = 0; k < 4; k++)
      {
         n[k] = (unsigned)((c >> (12 - 4 * k)) & 0xF);
         n[k] += ((15u - n[k]) * f) / 255u;
      }
      return (uint16_t)((n[0] << 12) | (n[1] << 8) | (n[2] << 4) | n[3]);
   }
}

/* Fills the whole framebuffer with one colour */
static void pix16_fill_flat(pix16_t *pix, uint16_t c)
{
   size_t i, n = (size_t)PIX16_FB_WIDTH * (size_t)PIX16_FB_HEIGHT;
   for (i = 0; i < n; i++)
      pix->fb[i] = c;
}

/* The swirl is over and the core is loading: hold the flash, fade it out,
 * then stay black until the game has a frame. Nothing else is drawn, so
 * the menu cannot flash up between the two. */
static void pix16_draw_handover(pix16_t *pix)
{
   retro_time_t ms = (menu_driver_get_current_time() - pix->handover_t0) / 1000;
   unsigned f = 255;

   /* reduced motion: no flash, just hold black */
   if (ms > 120)
   {
      int t = (int)(ms - 120);
      if (t > 300)
         t = 300;
      f = (unsigned)(255 - (t * 255) / 300);
   }
   pix16_fill_flat(pix, pix16_toward_white(0, f, pix->fmt_565));
}

/* Spins the finished frame into a vortex, in place */
static void pix16_battle_swirl(pix16_t *pix, float t)
{
   int w = PIX16_FB_WIDTH;
   int h = PIX16_FB_HEIGHT;
   int cx = w / 2;
   int cy = h / 2;
   float rmax   = (float)((w < h) ? w : h) * 0.5f;
   float spin   = t * t * PIX16_SWIRL_TURNS * 6.28318531f;
   float twist  = PIX16_SWIRL_TWIST * t;
   float shrink = 1.0f + PIX16_SWIRL_SHRINK * t;
   unsigned flash = 0;
   uint16_t empty  = pix->colors[PIX16_C_J_BG];
   int x, y;

   if (!pix->upload)
      return;

   memcpy(pix->upload, pix->fb, (size_t)w * (size_t)h * sizeof(uint16_t));

   if (t > PIX16_SWIRL_FLASH)
   {
      float f = (t - PIX16_SWIRL_FLASH) / (1.0f - PIX16_SWIRL_FLASH);
      flash = (unsigned)(f * f * 255.0f);
      if (flash > 255)
         flash = 255;
   }

   for (y = 0; y < h; y++)
   {
      float dy = (float)(y - cy);
      uint16_t *dst = pix->fb + (size_t)y * (size_t)w;
      for (x = 0; x < w; x++)
      {
         float dx = (float)(x - cx);
         float r  = sqrtf(dx * dx + dy * dy);
         float a  = atan2f(dy, dx) - spin
                  + twist * (1.0f - r / rmax);
         float sr = r * shrink;
         int sx   = cx + (int)(sr * cosf(a) + 0.5f);
         int sy   = cy + (int)(sr * sinf(a) + 0.5f);
         uint16_t c;
         if (sx < 0 || sx >= w || sy < 0 || sy >= h)
            c = empty;
         else
            c = pix->upload[(size_t)sy * (size_t)w + sx];
         if (flash)
            c = pix16_toward_white(c, flash, pix->fmt_565);
         dst[x] = c;
      }
   }
}

/* Dims a packed pixel toward black */
static uint16_t pix16_toward_black(uint16_t c, unsigned f, bool fmt_565)
{
   if (fmt_565)
   {
      unsigned r = ((c >> 11) & 0x1F) * (255u - f) / 255u;
      unsigned g = ((c >>  5) & 0x3F) * (255u - f) / 255u;
      unsigned b = ( c        & 0x1F) * (255u - f) / 255u;
      return (uint16_t)((r << 11) | (g << 5) | b);
   }
   else
   {
      unsigned n[4];
      int k;
      for (k = 0; k < 4; k++)
         n[k] = ((c >> (12 - 4 * k)) & 0xF) * (255u - f) / 255u;
      return (uint16_t)((n[0] << 12) | (n[1] << 8) | (n[2] << 4) | n[3]);
   }
}

/* The picture breaks into blocks that grow until it is gone */
static void pix16_launch_mosaic(pix16_t *pix, float t)
{
   int w     = PIX16_FB_WIDTH;
   int h     = PIX16_FB_HEIGHT;
   int block = 2 + (int)(t * t * 42.0f);
   unsigned dim = 0;
   int x, y;

   if (block > 64)
      block = 64;
   if (t > 0.55f)
      dim = (unsigned)((t - 0.55f) / 0.45f * 255.0f);

   memcpy(pix->upload, pix->fb, (size_t)w * (size_t)h * sizeof(uint16_t));

   for (y = 0; y < h; y += block)
   {
      for (x = 0; x < w; x += block)
      {
         uint16_t c = pix->upload[(size_t)y * (size_t)w + x];
         int bx, by;
         if (dim)
            c = pix16_toward_black(c, dim, pix->fmt_565);
         for (by = y; by < y + block && by < h; by++)
         {
            uint16_t *row = pix->fb + (size_t)by * (size_t)w;
            for (bx = x; bx < x + block && bx < w; bx++)
               row[bx] = c;
         }
      }
   }
}

/* The picture squashes to a line, the line to a dot, like a CRT switching off */
static void pix16_launch_crt(pix16_t *pix, float t)
{
   int w = PIX16_FB_WIDTH;
   int h = PIX16_FB_HEIGHT;
   float nh = (t < 0.5f) ? (1.0f - t / 0.5f) : 0.0f;
   float nw = (t < 0.5f) ? 1.0f : (1.0f - (t - 0.5f) / 0.35f);
   int hb, wb, y0, x0, x, y;
   size_t i, n = (size_t)w * (size_t)h;
   uint16_t black = pix->conv(0xFF000000);

   if (nw < 0.0f)
      nw = 0.0f;
   hb = (int)((float)h * nh);
   wb = (int)((float)w * nw);
   if (hb < 1)
      hb = 1;
   if (wb < 1)
      wb = 1;
   y0 = (h - hb) / 2;
   x0 = (w - wb) / 2;

   memcpy(pix->upload, pix->fb, n * sizeof(uint16_t));
   for (i = 0; i < n; i++)
      pix->fb[i] = black;

   for (y = 0; y < hb; y++)
   {
      const uint16_t *srow = pix->upload + (size_t)(y * h / hb) * (size_t)w;
      uint16_t *drow = pix->fb + (size_t)(y0 + y) * (size_t)w;
      for (x = 0; x < wb; x++)
         drow[x0 + x] = srow[x * w / wb];
   }

   if (t > 0.55f)
   {
      /* the phosphor brightens as it collapses */
      unsigned glow = (unsigned)((t - 0.55f) / 0.45f * 255.0f);
      for (y = 0; y < hb; y++)
      {
         uint16_t *row = pix->fb + (size_t)(y0 + y) * (size_t)w;
         for (x = 0; x < wb; x++)
            row[x0 + x] = pix16_toward_white(row[x0 + x], glow, pix->fmt_565);
      }
   }
}

/* The menu rushes toward the viewer and flashes out */
static void pix16_launch_zoom(pix16_t *pix, float t)
{
   int w  = PIX16_FB_WIDTH;
   int h  = PIX16_FB_HEIGHT;
   int cx = w / 2;
   int cy = h / 2;
   float s = 1.0f + 3.0f * t * t;
   unsigned flash = 0;
   int x, y;

   if (t > 0.6f)
      flash = (unsigned)((t - 0.6f) / 0.4f * 255.0f);
   memcpy(pix->upload, pix->fb, (size_t)w * (size_t)h * sizeof(uint16_t));

   for (y = 0; y < h; y++)
   {
      uint16_t *drow = pix->fb + (size_t)y * (size_t)w;
      int sy = cy + (int)((float)(y - cy) / s);
      for (x = 0; x < w; x++)
      {
         int sx = cx + (int)((float)(x - cx) / s);
         uint16_t c;
         if (sx < 0 || sx >= w || sy < 0 || sy >= h)
            c = pix->colors[PIX16_C_J_BG];
         else
            c = pix->upload[(size_t)sy * (size_t)w + sx];
         if (flash)
            c = pix16_toward_white(c, flash, pix->fmt_565);
         drow[x] = c;
      }
   }
}

/* Late in an effect that ends dark: wash to white, so the handover's
 * flash does not cut in */
static void pix16_launch_flashout(pix16_t *pix, float t)
{
   size_t i, n = (size_t)PIX16_FB_WIDTH * (size_t)PIX16_FB_HEIGHT;
   unsigned f;
   if (t <= 0.85f)
      return;
   f = (unsigned)((t - 0.85f) / 0.15f * 255.0f);
   if (f > 255)
      f = 255;
   for (i = 0; i < n; i++)
      pix->fb[i] = pix16_toward_white(pix->fb[i], f, pix->fmt_565);
}

/* The picture closes down to a point behind a bright iris */
static void pix16_launch_iris(pix16_t *pix, float t)
{
   int w  = PIX16_FB_WIDTH;
   int h  = PIX16_FB_HEIGHT;
   int cx = w / 2;
   int cy = h / 2;
   float rmax = (float)((w < h) ? w : h) * 0.62f;
   float r    = rmax * (1.0f - t);
   uint16_t rim   = pix->conv(0xFFFFFFFF);
   uint16_t empty = pix->colors[PIX16_C_J_BG];
   int x, y;

   memcpy(pix->upload, pix->fb, (size_t)w * (size_t)h * sizeof(uint16_t));
   for (y = 0; y < h; y++)
   {
      float dy = (float)(y - cy);
      uint16_t *row = pix->fb + (size_t)y * (size_t)w;
      for (x = 0; x < w; x++)
      {
         float dx = (float)(x - cx);
         float d  = sqrtf(dx * dx + dy * dy);
         if (d <= r)
            row[x] = pix->upload[(size_t)y * (size_t)w + x];
         else if (d <= r + 2.0f)
            row[x] = rim;
         else
            row[x] = empty;
      }
   }
   pix16_launch_flashout(pix, t);
}

/* A wipe line sweeps down the screen, black behind it */
static void pix16_launch_wipe(pix16_t *pix, float t)
{
   int w = PIX16_FB_WIDTH;
   int h = PIX16_FB_HEIGHT;
   float line = t * (float)(h + 10);
   uint16_t empty = pix->colors[PIX16_C_J_BG];
   int x, y;

   memcpy(pix->upload, pix->fb, (size_t)w * (size_t)h * sizeof(uint16_t));
   for (y = 0; y < h; y++)
   {
      float d = line - (float)y;
      uint16_t *row = pix->fb + (size_t)y * (size_t)w;
      for (x = 0; x < w; x++)
      {
         uint16_t c = pix->upload[(size_t)y * (size_t)w + x];
         if (d > 8.0f)
            c = empty;
         else if (d > 0.0f)
            c = pix16_toward_white(c, (unsigned)(d / 8.0f * 255.0f),
                  pix->fmt_565);
         row[x] = c;
      }
   }
   pix16_launch_flashout(pix, t);
}

/* The picture crumbles away pixel by pixel */
static void pix16_launch_dissolve(pix16_t *pix, float t)
{
   int w = PIX16_FB_WIDTH;
   int h = PIX16_FB_HEIGHT;
   unsigned cut   = (unsigned)(t * 300.0f);
   uint16_t empty = pix->colors[PIX16_C_J_BG];
   int x, y;

   for (y = 0; y < h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * (size_t)w;
      for (x = 0; x < w; x++)
      {
         unsigned n = pix16_hash((uint32_t)(y * w + x) * 2654435761u);
         if ((n & 0xFF) < cut)
            row[x] = empty;
      }
   }
   pix16_launch_flashout(pix, t);
}

/* The screen splits down the middle and the halves slide apart */
static void pix16_launch_split(pix16_t *pix, float t)
{
   int w   = PIX16_FB_WIDTH;
   int h   = PIX16_FB_HEIGHT;
   int off = (int)(t * t * (float)w * 0.7f);
   uint16_t empty = pix->colors[PIX16_C_J_BG];
   uint16_t rim   = pix->conv(0xFFFFFFFF);
   int x, y;

   memcpy(pix->upload, pix->fb, (size_t)w * (size_t)h * sizeof(uint16_t));
   for (y = 0; y < h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * (size_t)w;
      for (x = 0; x < w; x++)
      {
         if (x < w / 2)
         {
            int src = x + off;
            row[x]  = (src < w / 2)
                  ? pix->upload[(size_t)y * (size_t)w + src] : empty;
         }
         else
         {
            int src = x - off;
            row[x]  = (src >= w / 2)
                  ? pix->upload[(size_t)y * (size_t)w + src] : empty;
         }
      }
   }
   /* the parting edges */
   if (w / 2 - off - 1 >= 0)
      pix16_fill(pix, w / 2 - off - 1, 0, 1, h, rim);
   if (w / 2 + off < w)
      pix16_fill(pix, w / 2 + off, 0, 1, h, rim);
   pix16_launch_flashout(pix, t);
}

/* A white blast grows from the middle and swallows the screen */
static void pix16_launch_blast(pix16_t *pix, float t)
{
   int w  = PIX16_FB_WIDTH;
   int h  = PIX16_FB_HEIGHT;
   int cx = w / 2;
   int cy = h / 2;
   float rmax = (float)((w < h) ? w : h) * 0.75f;
   float r    = t * rmax * 1.5f;
   uint16_t white = pix->conv(0xFFFFFFFF);
   uint16_t glow  = pix->conv(0xFFE8F0FF);
   int x, y;

   memcpy(pix->upload, pix->fb, (size_t)w * (size_t)h * sizeof(uint16_t));
   for (y = 0; y < h; y++)
   {
      float dy = (float)(y - cy);
      uint16_t *row = pix->fb + (size_t)y * (size_t)w;
      for (x = 0; x < w; x++)
      {
         float dx = (float)(x - cx);
         float d  = sqrtf(dx * dx + dy * dy);
         if (d <= r)
            row[x] = white;
         else if (d <= r + 6.0f)
            row[x] = glow;
         else
            row[x] = pix->upload[(size_t)y * (size_t)w + x];
      }
   }
}

/* The picture rolls up, a blanking bar sweeping through it */
static void pix16_launch_roll(pix16_t *pix, float t)
{
   int w = PIX16_FB_WIDTH;
   int h = PIX16_FB_HEIGHT;
   int shift = (int)(t * t * (float)h * 2.4f);
   int band  = h / 5;
   uint16_t empty = pix->colors[PIX16_C_J_BG];
   int x, y;

   memcpy(pix->upload, pix->fb, (size_t)w * (size_t)h * sizeof(uint16_t));
   for (y = 0; y < h; y++)
   {
      int sy = (y + shift) % h;
      uint16_t *row = pix->fb + (size_t)y * (size_t)w;
      if (sy < band)
      {
         for (x = 0; x < w; x++)
            row[x] = empty;
      }
      else
         memcpy(row, pix->upload + (size_t)sy * (size_t)w,
               sizeof(uint16_t) * (size_t)w);
   }
   pix16_launch_flashout(pix, t);
}

/* The picture breaks up into noise and cuts out */
static void pix16_launch_static(pix16_t *pix, float t)
{
   int w = PIX16_FB_WIDTH;
   int h = PIX16_FB_HEIGHT;
   unsigned seed  = (unsigned)(t * 60.0f) * 2654435761u;
   unsigned cut   = (unsigned)(t * t * 320.0f);
   uint16_t empty = pix->colors[PIX16_C_J_BG];
   uint16_t white = pix->conv(0xFFFFFFFF);
   int x, y;

   for (y = 0; y < h; y++)
   {
      uint16_t *row = pix->fb + (size_t)y * (size_t)w;
      for (x = 0; x < w; x++)
      {
         unsigned n = pix16_hash((uint32_t)(y * w + x) ^ seed);
         if ((n & 0xFF) < cut)
            row[x] = ((n >> 9) & 7) ? empty : white;
      }
   }
   pix16_launch_flashout(pix, t);
}

static void pix16_launch_effect(pix16_t *pix, float t)
{
   switch (pix->launch_kind)
   {
      case PIX16_TRANSITION_MOSAIC:
         pix16_launch_mosaic(pix, t);
         break;
      case PIX16_TRANSITION_CRT:
         pix16_launch_crt(pix, t);
         break;
      case PIX16_TRANSITION_ZOOM:
         pix16_launch_zoom(pix, t);
         break;
      case PIX16_TRANSITION_IRIS:
         pix16_launch_iris(pix, t);
         break;
      case PIX16_TRANSITION_WIPE:
         pix16_launch_wipe(pix, t);
         break;
      case PIX16_TRANSITION_DISSOLVE:
         pix16_launch_dissolve(pix, t);
         break;
      case PIX16_TRANSITION_SPLIT:
         pix16_launch_split(pix, t);
         break;
      case PIX16_TRANSITION_BLAST:
         pix16_launch_blast(pix, t);
         break;
      case PIX16_TRANSITION_ROLL:
         pix16_launch_roll(pix, t);
         break;
      case PIX16_TRANSITION_STATIC:
         pix16_launch_static(pix, t);
         break;
      default:
         pix16_battle_swirl(pix, t);
         break;
   }
}

static void pix16_render(void *data, unsigned dims, bool is_idle)
{
   pix16_t *pix               = (pix16_t*)data;
   struct menu_state *menu_st = menu_state_get_ptr();
   gfx_display_t *p_disp      = disp_get_ptr();
   gfx_animation_t *p_anim    = anim_get_ptr();
   size_t depth;
   int tab;
   bool quick, at_root;

   if (!pix || !pix->fb)
      return;
   pix16_load_settings(pix);

   /* first run: the question comes up over the first thing drawn,
    * a first-run asset prompt included */
   if (!pix->screen_asked)
      pix->screen_prompt = true;

   /* First time the menu comes up with nothing running: open the
    * systems (playlists) list instead of the main menu */
   if (pix->flags & PIX16_FLAG_BOOT_PENDING)
   {
      /* wait for any first-run question to be answered */
      bool dialog = menu_st->dialog_st.confirm_msg
                 && menu_st->dialog_st.confirm_cmd;
      if (     !dialog
            && pix16_stack_depth() == 1
            && !MENU_ENTRIES_NEEDS_REFRESH(menu_st))
      {
         pix->flags &= ~PIX16_FLAG_BOOT_PENDING;
         if (!pix16_content_running() && pix16_root_tab() == PIX16_TAB_MAIN)
         {
            pix16_switch_tab(PIX16_TAB_LISTS);
            pix->flags |= PIX16_FLAG_FORCE_REDRAW
                       |  PIX16_FLAG_SELECT_SYSTEM;
            return;
         }
      }
   }

   /* Boot screen; a question RetroArch asks (first-run download)
    * or a game starting ends it */
   if (pix->boot_active)
   {
      bool dialog = menu_st->dialog_st.confirm_msg
                 && menu_st->dialog_st.confirm_cmd;
      if (dialog || pix16_content_running() || !string_is_empty(pix->msg))
      {
         pix->boot_active   = false;
         pix->scr_key_valid = false;
         pix->last_n        = 0;
      }
      else
      {
         pix16_apply_size(pix);
         pix16_update_view(pix, false);
         pix16_clip_reset(pix);
         pix16_draw_boot(pix);
         if (pix->screen_prompt)
            pix16_draw_screen_prompt(pix, pix->theme == PIX16_THEME_JRPG);
         pix->flags    |= PIX16_FLAG_FORCE_REDRAW;
         p_disp->flags |= GFX_DISP_FLAG_FB_DIRTY;
         GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
         return;
      }
   }

   /* A game started with A has been closed: step back out of its
    * options list so the cursor is on that game in its list */
   if (pix->launch_state != PIX16_LAUNCH_NONE)
   {
      bool running = pix16_content_running();
      if (pix->launch_state == PIX16_LAUNCH_STARTING && running)
         pix->launch_state = PIX16_LAUNCH_RUNNING;
      else if (pix->launch_state == PIX16_LAUNCH_RUNNING && !running
            && !MENU_ENTRIES_NEEDS_REFRESH(menu_st))
      {
         file_list_t *list = pix16_selection_buf();
         pix->launch_state = PIX16_LAUNCH_NONE;
         if (     pix16_stack_depth() == pix->auto_run_depth
               && list && list->size > 0
               && list->list[0].type == FILE_TYPE_PLAYLIST_ENTRY)
         {
            /* pix->entry: a menu_entry_t is too big for the stack */
            MENU_ENTRY_INITIALIZE(pix->entry);
            menu_entry_get(&pix->entry, 0, menu_st->selection_ptr, NULL, true);
            generic_menu_entry_action(pix, &pix->entry, menu_st->selection_ptr,
                  MENU_ACTION_CANCEL);
            pix->flags |= PIX16_FLAG_FORCE_REDRAW;
            return;
         }
      }
      else if (pix->launch_state == PIX16_LAUNCH_STARTING && !running
            && pix16_stack_depth() != pix->auto_run_depth)
         pix->launch_state = PIX16_LAUNCH_NONE; /* launch abandoned */
   }

   /* The swirl handed over: keep the menu off the screen until the game
    * is up, so nothing flashes between the two */
   if (pix->handover)
   {
      retro_time_t held = (menu_driver_get_current_time() - pix->handover_t0) / 1000;
      bool done         = pix16_content_running()
                        || pix->launch_state == PIX16_LAUNCH_NONE
                        || !string_is_empty(pix->msg)
                        || (menu_st->dialog_st.confirm_msg
                              && menu_st->dialog_st.confirm_cmd)
                        || held > 10000;
      if (done)
         pix->handover = false;
      else
      {
         pix16_apply_size(pix);
         pix16_update_view(pix, false);
         pix16_clip_reset(pix);
         pix16_draw_handover(pix);
         pix->flags    |= PIX16_FLAG_FORCE_REDRAW;
         p_disp->flags |= GFX_DISP_FLAG_FB_DIRTY;
         GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
         return;
      }
   }

   /* The game's options list just opened because A was pressed:
    * choose its first item, "Run", without drawing the list */
   if (pix->flags & PIX16_FLAG_AUTO_RUN)
   {
      bool launch = true;
      if (MENU_ENTRIES_NEEDS_REFRESH(menu_st))
         return;
      if (pix16_stack_depth() == pix->auto_run_depth)
      {
         /* play the battle swirl first, then hand over to the game */
         if (!pix->swirl_active)
         {
            retro_time_t now  = menu_driver_get_current_time();
            pix->swirl_active = true;
            pix->swirl_t0     = now;
            pix->launch_kind  = pix->transition;
            if (pix->launch_kind == PIX16_TRANSITION_RANDOM)
               pix->launch_kind = (unsigned)((now / 7919)
                     % (unsigned)PIX16_TRANSITION_RANDOM);
            pix->flags       |= PIX16_FLAG_FORCE_REDRAW;
            launch            = false;
         }
         else if (pix16_swirl_progress(pix) < 1.0f)
            launch = false;
         else
         {
            /* hold the screen until the game has a frame of its own */
            pix->swirl_active = false;
            pix->handover     = true;
            pix->handover_t0  = menu_driver_get_current_time();
         }
      }
      if (launch)
      {
         pix->flags &= ~PIX16_FLAG_AUTO_RUN;
         if (pix16_stack_depth() == pix->auto_run_depth)
         {
            file_list_t *list = pix16_selection_buf();
            if (list && list->size > 0
                  && list->list[0].type == FILE_TYPE_PLAYLIST_ENTRY)
            {
               MENU_ENTRY_INITIALIZE(pix->entry);
               menu_entry_get(&pix->entry, 0, 0, NULL, true);
               menu_st->selection_ptr = 0;
               pix->launch_state      = PIX16_LAUNCH_STARTING;
               if (pix->pending_slot != PIX16_SLOT_NONE && menu_st->driver_data)
               {
                  /* RetroArch loads this slot as the game starts */
                  menu_st->driver_data->state_slot_run         = (int16_t)pix->pending_slot;
                  menu_st->driver_data->state_slot_run_pending = true;
               }
               pix->pending_slot = PIX16_SLOT_NONE;
               generic_menu_entry_action(pix, &pix->entry, 0, MENU_ACTION_OK);
               pix->flags |= PIX16_FLAG_FORCE_REDRAW;
               return;
            }
         }
      }
   }

   /* playlists changed by a scan: rebuild the systems screen once it
    * is the screen showing */
   if (     (pix->flags & PIX16_FLAG_LISTS_CHANGED)
         && !MENU_ENTRIES_NEEDS_REFRESH(menu_st)
         && pix16_stack_depth() <= 1
         && pix16_root_tab() == PIX16_TAB_LISTS
         && !menu_is_running_quick_menu()
         && !menu_input_dialog_get_display_kb())
   {
      pix->flags     &= ~PIX16_FLAG_LISTS_CHANGED;
      menu_st->flags |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      menu_st->flags &= ~MENU_ST_FLAG_PREVENT_POPULATE;
      pix->flags     |=  PIX16_FLAG_FORCE_REDRAW;
      return;
   }

   if (     (pix->flags & PIX16_FLAG_SELECT_SYSTEM)
         && !MENU_ENTRIES_NEEDS_REFRESH(menu_st))
   {
      pix->flags &= ~PIX16_FLAG_SELECT_SYSTEM;
      if (pix16_stack_depth() == 1 && pix16_root_tab() == PIX16_TAB_LISTS)
         pix16_select_first_system(pix);
      pix->flags |= PIX16_FLAG_FORCE_REDRAW;
   }

   if (!(pix->flags & PIX16_FLAG_FORCE_REDRAW))
   {
      if (MENU_ENTRIES_NEEDS_REFRESH(menu_st))
         return;
      if (is_idle && string_is_empty(pix->msg)
            && !menu_input_dialog_get_display_kb())
         return;
   }
   pix->flags &= ~PIX16_FLAG_FORCE_REDRAW;

   depth   = pix16_stack_depth();
   tab     = pix16_root_tab();
   at_root = (depth <= 1);
   quick   = menu_is_running_quick_menu();

   /* the gear only exists on the systems page */
   if (quick || !at_root || tab != PIX16_TAB_LISTS)
   {
      pix->gear_focus  = false;
      pix->power_focus = false;
      pix->add_focus   = false;
      pix->arcade_open = false;
      if (pix->wiz.open)
         pix16_wiz_close(pix);
   }

   if (!MENU_ENTRIES_NEEDS_REFRESH(menu_st))
      pix16_fav_add_entry();

   menu_entries_get_title(pix->title, sizeof(pix->title));
   /* a system's game list: its clean name instead of the playlist's */
   if (!quick && tab == PIX16_TAB_LISTS && depth == 2)
      pix16_system_title(pix->title, pix->shown_title, sizeof(pix->shown_title));
   else
      strlcpy(pix->shown_title, pix->title, sizeof(pix->shown_title));

   pix->text_shadow = false;
   pix16_apply_size(pix);
   pix16_clip_reset(pix);

   /* window transitions: a new screen closes the old windows and opens
    * the new ones; popups grow open / shrink shut on their own */
   {
      retro_time_t now = menu_driver_get_current_time();
      bool jrpg        = (pix->theme == PIX16_THEME_JRPG);
      uint32_t key     = pix16_screen_key(pix, depth, tab, quick);
      int pop          = pix->quit_confirm ? 4
                       : pix->wiz.open ? 5
                       : pix->arcade_open ? (pix->picker_pce ? 2 : 1)
                       : (pix->game_menu_open ? 3 : 0);
      bool opt_now     = !quick && depth <= 1 && tab != PIX16_TAB_LISTS;
      bool grid_now    = !quick && depth <= 1 && tab == PIX16_TAB_LISTS
            && pix16_is_grid_view();
      if (     pix->scr_key_valid && key != pix->scr_key
            && pix->slide_old
            && !pix->reduced_motion
            && (  (opt_now && pix->prev_opt && tab != pix->prev_tab)
               || grid_now))
      {
         /* MAIN / SETUP / NET: same screen, the contents slide over.
          * fb still holds the frame shown last and is redrawn in full,
          * so the two buffers trade places. */
         uint16_t *shown   = pix->fb;
         pix->fb           = pix->slide_old;
         pix->slide_old    = shown;
         pix->scr_key      = key;
         pix->slide_dir    = (pix16_opt_pos(tab) > pix16_opt_pos(pix->prev_tab)) ? 1 : -1;
         pix->slide_t0     = now;
         pix->slide_active = jrpg;
         pix->tr_active    = false;
      }
      else if (!pix->scr_key_valid || key != pix->scr_key)
      {
         pix->slide_active = false;
         pix->tr_old_n = pix->scr_key_valid ? pix->last_n : 0;
         if (pix->tr_old_n)
            memcpy(pix->tr_old, pix->last, sizeof(pix16_rect_t) * pix->tr_old_n);
         /* a list that is still being fetched (the core downloader asks
          * the network first): do not shrink the previous screen's windows
          * over it, or they look like a flash of the wrong menu */
         pix->tr_active     = jrpg && !MENU_ENTRIES_NEEDS_REFRESH(menu_st);
         if (!pix->tr_active)
            pix->tr_old_n = 0;
         pix->tr_t0         = now;
         pix->scr_key       = key;
         pix->scr_key_valid = true;
         pix->popc_active   = false;
         if (pop)
            pix->pop_t0 = now;
      }
      else if (pop != pix->pop_prev)
      {
         if (pop)
            pix->pop_t0 = now;
         if (pix->pop_prev && jrpg && !pix->reduced_motion)
         {
            /* the popup that just closed shrinks away (unless another
             * one replaced it straight away) */
            pix->popc_active = !pop;
            pix->popc_t0     = now;
            pix->popc_rect   = pix->pop_rect;
         }
      }
      pix->pop_prev = pop;
      pix->prev_tab = tab;
      pix->prev_opt = opt_now;
      pix->rec_n    = 0;
      pix->rec_on   = jrpg;
   }

   pix16_update_view(pix, quick && !pix->qm_full);

   if (quick && !pix->qm_full)
      pix16_draw_pause(pix, pix->theme == PIX16_THEME_JRPG);
   else if (     pix->theme == PIX16_THEME_JRPG
         && !(at_root && tab == PIX16_TAB_LISTS && !quick))
   {
      pix->text_shadow = true;
      pix16_j_screen(pix, quick, at_root, tab, depth);
   }
   else if (pix->theme == PIX16_THEME_JRPG)
   {
      /* systems grid keeps its tiles, under a JRPG command window */
      pix16_j_background(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT);
      pix16_systems_screen(pix, true);
      pix16_j_grid_footer(pix,
            PIX16_SAFE + PIX16_J_BAR_H + 3 + PIX16_GRID_DY
            + PIX16_GRID_ROWS * PIX16_ROW_STRIDE + 2);
   }
   else if (quick)
   {
      /* In-game quick menu: content stays visible behind a scrim */
      int w = PIX16_QUICK_W;
      int h = PIX16_FB_HEIGHT - 2 * PIX16_SAFE;
      pix16_fill(pix, 0, 0, PIX16_FB_WIDTH, PIX16_FB_HEIGHT,
            pix->colors[PIX16_C_SCRIM]);
      pix16_draw_title_bar(pix, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS), pix->colors[PIX16_C_RED],
            pix->colors[PIX16_C_TEXT], false);
      /* title bar draws full width; trim it back to the panel */
      pix16_fill(pix, PIX16_SAFE + w + PIX16_SHADOW, PIX16_SAFE,
            PIX16_FB_WIDTH, PIX16_BAR_H + PIX16_SHADOW,
            pix->colors[PIX16_C_SCRIM]);
      pix16_fill(pix, PIX16_SAFE + w, PIX16_SAFE + PIX16_SHADOW,
            PIX16_SHADOW, PIX16_BAR_H, pix->colors[PIX16_C_SHADOW]);
      pix16_draw_list(pix, PIX16_SAFE, PIX16_SAFE + PIX16_BAR_H + 2, w,
            h - PIX16_BAR_H - 2,
            pix->colors[PIX16_C_YELLOW], pix->colors[PIX16_C_DARK_TEXT],
            false);
   }
   else
   {
      int list_y      = PIX16_SAFE + PIX16_BAR_H + PIX16_GAP;
      int footer_y    = PIX16_FB_HEIGHT - PIX16_SAFE - PIX16_FOOTER_H;
      int full_w      = PIX16_FB_WIDTH - 2 * PIX16_SAFE;
      bool systems    = at_root && tab == PIX16_TAB_LISTS;
      uint16_t hl_bg  = pix->tab_color[tab];
      uint16_t hl_fg  = pix->tab_fg[tab];

      pix16_checkerboard(pix);

      if (systems)
         ; /* the systems screen draws its own top buttons */
      else if (at_root)
         pix16_draw_tab_bar(pix, tab);
      else
      {
         /* inside a system's game list, the title bar takes that
          * system's colour */
         uint16_t bg = pix->colors[PIX16_C_PANEL];
         uint16_t fg = pix->colors[PIX16_C_YELLOW];
         if (tab == PIX16_TAB_LISTS && depth == 2)
         {
            char short_name[64];
            pix16_system_lookup(pix, pix->title, short_name,
                  sizeof(short_name), &bg, &fg);
            hl_bg = bg;
            hl_fg = fg;
         }
         pix16_draw_title_bar(pix, pix->shown_title, bg, fg, false);
      }

      if (systems)
         pix16_systems_screen(pix, false);
      else
      {
         int desc_y = footer_y - PIX16_GAP - PIX16_DESC_H;
         size_t sel = menu_st->selection_ptr;
         file_list_t *list = pix16_selection_buf();

         pix16_draw_list(pix, PIX16_SAFE, list_y, full_w,
               desc_y - PIX16_GAP - list_y, hl_bg, hl_fg, false);

         if (list && sel < list->size)
         {
            pix16_get_entry(pix, sel, true);
            /* no help text (e.g. games in a playlist): show the full
             * name instead, since long names get cut in the list */
            pix16_draw_desc(pix, PIX16_SAFE, desc_y, full_w,
                  *pix->entry.sublabel
                  ? pix->entry.sublabel
                  : pix16_entry_label(&pix->entry));
         }
         else
            pix16_draw_desc(pix, PIX16_SAFE, desc_y, full_w, NULL);
      }

      if (systems)
         pix16_draw_grid_footer(pix);
      else
         pix16_draw_footer(pix, at_root);
   }

   if (pix->game_menu_open)
   {
      file_list_t *gl = pix16_selection_buf();
      if (     !gl || pix->game_menu_idx >= gl->size
            || gl->list[pix->game_menu_idx].type != FILE_TYPE_RPL_ENTRY)
         pix->game_menu_open = false;   /* list changed under us */
      else
         pix16_draw_game_menu(pix, pix->theme == PIX16_THEME_JRPG);
   }

   if (pix->slide_active)
      pix16_tab_slide(pix);

   if (pix->quit_confirm)
   {
      if (!pix->confirm_exit && !pix16_content_running())
         pix->quit_confirm = false;
      else
         pix16_draw_quit_confirm(pix, pix->theme == PIX16_THEME_JRPG);
   }

   if (pix->theme == PIX16_THEME_JRPG)
   {
      /* a popup that just closed, shrinking */
      if (pix->popc_active)
      {
         float t = (float)(menu_driver_get_current_time() - pix->popc_t0)
                 / (PIX16_POP_MS * 1000.0f);
         if (t < 1.0f)
         {
            pix->rec_on = false;
            pix16_rect_scaled(pix, &pix->popc_rect, 1.0f - pix16_ease_out(t));
         }
         else
            pix->popc_active = false;
      }
      pix16_transition_overlay(pix, quick);
      pix->rec_on = false;
      memcpy(pix->last, pix->rec, sizeof(pix16_rect_t) * pix->rec_n);
      pix->last_n = pix->rec_n;
      /* background drift, glove bounce, sprite bob: keep drawing */
      pix->flags |= PIX16_FLAG_FORCE_REDRAW;
   }
   pix->rec_on = false;

   pix->text_shadow = (pix->theme == PIX16_THEME_JRPG);

   if (menu_input_dialog_get_display_kb())
      pix16_draw_osk(pix);

   if (!string_is_empty(pix->msg))
   {
      pix16_draw_messagebox(pix, pix->msg);
      pix->msg[0] = '\0';
   }
   /* the first-run question sits over whatever the frame shows */
   if (pix->screen_prompt)
      pix16_draw_screen_prompt(pix, pix->theme == PIX16_THEME_JRPG);
   pix->text_shadow = false;

   /* the launch effect post-processes the frame that was just drawn */
   if (pix->swirl_active)
   {
      float t = pix16_swirl_progress(pix);
      if (t >= 1.0f)
         pix->swirl_active = false;   /* the launch takes over next tick */
      else
         pix16_launch_effect(pix, t);
   }

   p_disp->flags |= GFX_DISP_FLAG_FB_DIRTY;
   GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
}

static void pix16_render_messagebox(void *data, const char *msg)
{
   pix16_t *pix = (pix16_t*)data;
   if (!pix || string_is_empty(msg))
      return;
   strlcpy(pix->msg, msg, sizeof(pix->msg));
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
}

static void pix16_set_texture(void *data)
{
   pix16_t *pix                   = (pix16_t*)data;
   video_driver_state_t *video_st = video_state_get_ptr();
   gfx_display_t *p_disp          = disp_get_ptr();

   if (!pix || !pix->fb)
      return;

   if (menu_driver_media_startup_token())
   {
      memset(pix->fb, 0, PIX16_FB_WIDTH * PIX16_FB_HEIGHT * sizeof(uint16_t));
      p_disp->flags |= GFX_DISP_FLAG_FB_DIRTY;
   }
   else if (!(p_disp->flags & GFX_DISP_FLAG_FB_DIRTY))
      return;
   else
      p_disp->flags &= ~GFX_DISP_FLAG_FB_DIRTY;

   if (video_st->poke && video_st->poke->set_texture_frame)
   {
      /* full-width rows are contiguous: hand them over in place */
      const uint16_t *src = pix->fb + (size_t)pix->view_y * PIX16_FB_WIDTH;
      /* a narrower view has a different stride and the poke interface
       * takes no pitch, so it is packed */
      if (     pix->view_w && pix->view_w != PIX16_FB_WIDTH
            && pix->view_h && pix->upload)
      {
         int y;
         for (y = 0; y < pix->view_h; y++)
            memcpy(pix->upload + (size_t)y * pix->view_w,
                  pix->fb + (size_t)(pix->view_y + y) * PIX16_FB_WIDTH + pix->view_x,
                  pix->view_w * sizeof(uint16_t));
         src = pix->upload;
      }
      video_st->poke->set_texture_frame(video_st->data, src, false,
            p_disp->framebuf_dims, 1.0f);

   }
}

static void pix16_frame(void *data, video_frame_info_t *video_info)
{
   pix16_t *pix                   = (pix16_t*)data;
   video_driver_state_t *video_st = video_state_get_ptr();
   (void)video_info;
   /* widescreen with no game: the menu fills the whole screen whatever
    * the aspect ratio setting is (that one is for the games) */
   if (     pix && pix->wide_cfg && !pix16_content_running()
         && video_st->poke && video_st->poke->set_texture_enable)
      video_st->poke->set_texture_enable(video_st->data, true, true);
}

static void pix16_animation_update_time(float *ticker_pixel_increment,
      unsigned video_dims)
{
   /* framebuffer size is independent of display resolution */
   *ticker_pixel_increment *= 0.25f;
}

static void *pix16_init(void **userdata, bool video_is_threaded)
{
   gfx_display_t *p_disp   = disp_get_ptr();
   gfx_animation_t *p_anim = anim_get_ptr();
   menu_handle_t *menu     = (menu_handle_t*)calloc(1, sizeof(*menu));
   pix16_t *pix;

   if (!menu)
      return NULL;

   pix = (pix16_t*)calloc(1, sizeof(*pix));
   if (!pix)
      goto error;

   pix->fb = (uint16_t*)calloc(PIX16_FB_MAX_W * PIX16_FB_MAX_H,
         sizeof(uint16_t));
   if (!pix->fb)
      goto error;
   pix->upload = (uint16_t*)calloc(PIX16_FB_MAX_W * PIX16_FB_MAX_H,
         sizeof(uint16_t));
   pix->slide_old = (uint16_t*)calloc(PIX16_FB_MAX_W * PIX16_FB_MAX_H,
         sizeof(uint16_t));
   pix->view_w = PIX16_FB_WIDTH;
   pix->view_h = PIX16_FB_HEIGHT;

   pix16_select_pixel_format(pix);
   pix16_prepare_colors(pix);
   {
      settings_t *settings = config_get_ptr();
      pix->theme        = PIX16_THEME_JRPG;
      pix->boot_enabled = settings && settings->bools.menu_pix16_boot_screen;
      pix16_load_settings(pix);
   }
   pix16_groups_load();
   pix->picker_host = PIX16_TOP_NONE;
   pix16_clip_reset(pix);

   p_disp->framebuf_dims  = VIDEO_SCALE_PACK(PIX16_FB_WIDTH, PIX16_FB_HEIGHT);
   p_disp->framebuf_pitch = PIX16_FB_WIDTH * sizeof(uint16_t);
   p_disp->header_height  = PIX16_BAR_H;
   p_disp->flags         |= GFX_DISP_FLAG_FB_DIRTY;

   pix->flags             = PIX16_FLAG_BOOT_PENDING | PIX16_FLAG_FORCE_REDRAW;
   pix->boot_active       = (pix->boot_enabled || pix->start_enabled)
                         && pix->theme == PIX16_THEME_JRPG
                         && !pix16_content_running();
   if (pix->boot_active && !pix->boot_enabled)
   {
      /* the boot screen is off: the start screen comes up on its own */
      pix->boot_phase = 1;
      pix->logo_t0    = menu_driver_get_current_time();
   }
   pix->pending_slot      = PIX16_SLOT_NONE;
   pix->game_menu_slot    = PIX16_SLOT_NONE;
   p_anim->updatetime_cb  = pix16_animation_update_time;

   menu_state_get_ptr()->entries.begin = 0;

   *userdata = pix;
   return menu;

error:
   if (pix)
   {
      free(pix->fb);
      free(pix->upload);
      free(pix->slide_old);
      free(pix);
   }
   free(menu);
   return NULL;
}

static void pix16_free(void *data)
{
   pix16_t *pix = (pix16_t*)data;
   if (!pix)
      return;
   pix16_art_free_all(pix);
   free(pix->grid_map);
   pix->grid_map = NULL;
   free(pix->arcade_idx);
   free(pix->sub_idx);
   pix16_boot_count_free(pix);
   free(pix16_groups);
   pix16_groups   = NULL;
   pix16_groups_n = 0;
   free(pix->sub_host);
   pix16_wiz_close(pix);
   if (pix16_wiz_owner == pix)
      pix16_wiz_owner = NULL;
   pix->arcade_idx = NULL;
   free(pix->fb);
   pix->fb = NULL;
   free(pix->upload);
   pix->upload = NULL;
   free(pix->slide_old);
   pix->slide_old = NULL;
}

static void pix16_context_reset(void *data, bool is_threaded)
{
   pix16_t *pix          = (pix16_t*)data;
   gfx_display_t *p_disp = disp_get_ptr();

   if (!pix)
      return;

   /* the video driver may have changed: re-pick the pixel format
    * (and reconvert art on next use) */
   pix16_select_pixel_format(pix);
   pix16_art_free_all(pix);
   pix16_prepare_colors(pix);

   if (!p_disp->dispctx)
      gfx_display_init_first_driver(p_disp, is_threaded);
   video_driver_monitor_reset();
   p_disp->framebuf_dims  = VIDEO_SCALE_PACK(PIX16_FB_WIDTH, PIX16_FB_HEIGHT);
   p_disp->framebuf_pitch = PIX16_FB_WIDTH * sizeof(uint16_t);
   p_disp->flags         |= GFX_DISP_FLAG_FB_DIRTY;
   pix->flags            |= PIX16_FLAG_FORCE_REDRAW;
   pix->view_x = pix->view_y = 0;   /* full view again; next render */
   pix->view_w = PIX16_FB_WIDTH;    /* recomputes it for the mode   */
   pix->view_h = PIX16_FB_HEIGHT;
}

static void pix16_context_destroy(void *data)
{
   (void)data;
}

static void pix16_populate_entries(void *data, const char *path,
      const char *label, unsigned k)
{
   pix16_t *pix = (pix16_t*)data;
   if (!pix)
      return;
   pix->flags |= PIX16_FLAG_FORCE_REDRAW;
}

/* A content scan finished: its playlists are new or changed, so the
 * systems screen is built again (now if it is showing, else when it
 * next shows) */
static int pix16_environ(enum menu_environ_cb type, void *data,
      void *userdata)
{
   pix16_t *pix = (pix16_t*)userdata;
   if (!pix)
      return -1;
   if (type == MENU_ENVIRON_RESET_HORIZONTAL_LIST)
   {
      RARCH_LOG("[pix16] Playlists changed: rebuilding the systems screen.\n");
      pix16_groups_load();
      pix->flags |= PIX16_FLAG_LISTS_CHANGED | PIX16_FLAG_FORCE_REDRAW;
      return 0;
   }
   return -1;
}

static void pix16_toggle(void *userdata, bool menu_on)
{
   pix16_t *pix = (pix16_t*)userdata;
   if (pix && menu_on)
   {
      pix->flags        |= PIX16_FLAG_FORCE_REDRAW;
      /* each time the menu opens in a game: the pause window, on RESUME */
      pix->qm_full       = false;
      pix->qm_sel        = PIX16_QM_RESUME;
      pix->qm_slot_valid = false;
      pix->qm_status[0]  = '\0';
      pix->quit_confirm  = false;
      pix->scr_key_valid = false;   /* windows open fresh, nothing closes */
      pix->last_n        = 0;
      /* opened in a game started from a list: rebuild that list so it
       * becomes the game's in-game menu (Resume, Close...) rather than
       * the stale Run / Load State / Rename list */
      if (pix16_content_running())
         menu_state_get_ptr()->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   }
}

static void pix16_navigation_clear(void *data, bool pending_push)
{
   menu_state_get_ptr()->entries.begin = 0;
}

static void pix16_navigation_set(void *data, bool scroll)
{
   pix16_t *pix = (pix16_t*)data;
   if (pix)
      pix->flags |= PIX16_FLAG_FORCE_REDRAW;
}

/* ------------------------------------------------------------------
 * RUN / LOAD SAVE window
 * ------------------------------------------------------------------ */

/* Newest save state in one folder for a content name; updates *best */
static void pix16_scan_states(const char *dir, const char *name,
      int *slot, time_t *best)
{
   struct string_list *files;
   size_t i, nlen = strlen(name);

   if (string_is_empty(dir) || !path_is_directory(dir))
      return;
   if (!(files = dir_list_new(dir, NULL, false, false, false, false)))
      return;

   for (i = 0; i < files->size; i++)
   {
      const char *full = files->elems[i].data;
      const char *base = path_basename(full);
      const char *suf;
      struct stat st;
      int this_slot;

      if (!base || strncmp(base, name, nlen) || strncmp(base + nlen, ".state", 6))
         continue;
      suf = base + nlen + 6;
      if (*suf == '\0')
         this_slot = 0;                       /* name.state  -> slot 0 */
      else if (string_is_equal(suf, ".auto"))
         this_slot = -1;                      /* name.state.auto */
      else
      {
         char *end = NULL;
         long v    = strtol(suf, &end, 10);   /* name.stateN -> slot N */
         if (!end || *end || v < 0 || v > 999)
            continue;
         this_slot = (int)v;
      }
      if (stat(full, &st) != 0)
         continue;
      if (*slot == PIX16_SLOT_NONE || st.st_mtime > *best)
      {
         *slot = this_slot;
         *best = st.st_mtime;
      }
   }
   string_list_free(files);
}

/* The same, in every sub-folder of dir (per-core state folders) */
static void pix16_scan_state_subdirs(const char *dir, const char *name,
      int *slot, time_t *best)
{
   struct string_list *entries;
   size_t i;
   if (string_is_empty(dir) || !path_is_directory(dir))
      return;
   if (!(entries = dir_list_new(dir, NULL, true, false, false, false)))
      return;
   for (i = 0; i < entries->size; i++)
      if (path_is_directory(entries->elems[i].data))
         pix16_scan_states(entries->elems[i].data, name, slot, best);
   string_list_free(entries);
}

/* Most recently written save state for a playlist entry, following
 * RetroArch's save state folder settings. PIX16_SLOT_NONE if none. */
static int pix16_find_last_state(const struct playlist_entry *e)
{
   settings_t *settings = config_get_ptr();
   char name[NAME_MAX_LENGTH];
   char content_dir_name[NAME_MAX_LENGTH];
   const char *core_name = NULL;
   core_info_t *core_info = NULL;
   int slot               = PIX16_SLOT_NONE;
   time_t best            = 0;
   char *hash, *content, *base_dir, *dir, *dir2;
   char *buf;

   if (!e || string_is_empty(e->path))
      return slot;
   /* four path buffers: too big for the stack */
   if (!(buf = (char*)malloc(4 * PATH_MAX_LENGTH)))
      return slot;
   content  = buf;
   base_dir = buf + PATH_MAX_LENGTH;
   dir      = buf + 2 * PATH_MAX_LENGTH;
   dir2     = buf + 3 * PATH_MAX_LENGTH;

   /* "game.zip#rom.sfc" -> "game.zip"; states are named after it */
   strlcpy(content, e->path, PATH_MAX_LENGTH);
   if ((hash = strchr(content, '#')))
      *hash = '\0';
   strlcpy(name, path_basename(content), sizeof(name));
   path_remove_extension(name);

   {
      const char *state_dir = dir_get_ptr(RARCH_DIR_SAVESTATE);
      if (     string_is_empty(state_dir)
            || settings->bools.savestates_in_content_dir)
         fill_pathname_basedir(base_dir, content, PATH_MAX_LENGTH);
      else
         strlcpy(base_dir, state_dir, PATH_MAX_LENGTH);
   }

   content_dir_name[0] = '\0';
   fill_pathname_parent_dir_name(content_dir_name, content,
         sizeof(content_dir_name));

   if (!string_is_empty(e->core_path) && core_info_find(e->core_path, &core_info)
         && core_info)
      core_name = core_info->core_name;

   /* the plain folder, plus the per-content-folder and per-core
    * sub-folders RetroArch may be sorting states into */
   pix16_scan_states(base_dir, name, &slot, &best);
   if (*content_dir_name)
   {
      fill_pathname_join_special(dir, base_dir, content_dir_name, PATH_MAX_LENGTH);
      pix16_scan_states(dir, name, &slot, &best);
      if (core_name)
      {
         fill_pathname_join_special(dir2, dir, core_name, PATH_MAX_LENGTH);
         pix16_scan_states(dir2, name, &slot, &best);
      }
   }
   if (core_name)
   {
      fill_pathname_join_special(dir, base_dir, core_name, PATH_MAX_LENGTH);
      pix16_scan_states(dir, name, &slot, &best);
   }
   else
   {
      /* core unknown (no core info file): look in every core folder */
      pix16_scan_state_subdirs(base_dir, name, &slot, &best);
      if (*content_dir_name)
      {
         fill_pathname_join_special(dir, base_dir, content_dir_name, PATH_MAX_LENGTH);
         pix16_scan_state_subdirs(dir, name, &slot, &best);
      }
   }
   free(buf);
   return slot;
}

/* Opens the window for the game at list index idx */
static void pix16_game_menu_open(pix16_t *pix, size_t idx)
{
   file_list_t *list                  = pix16_selection_buf();
   playlist_t *playlist               = playlist_get_cached();
   const struct playlist_entry *pe    = NULL;

   pix->game_menu_open = true;
   pix->game_menu_sel  = 0;
   pix->game_menu_idx  = idx;
   pix->game_menu_slot = PIX16_SLOT_NONE;

   if (playlist && list && idx < list->size)
      playlist_get_index(playlist, list->list[idx].entry_idx, &pe);
   if (pe)
      pix->game_menu_slot = pix16_find_last_state(pe);
}

static void pix16_draw_game_menu(pix16_t *pix, bool jrpg)
{
   char title[NAME_MAX_LENGTH];
   char slot_txt[16];
   bool has_save = (pix->game_menu_slot != PIX16_SLOT_NONE);
   int rows      = has_save ? 2 : 1;
   int w         = PIX16_FB_WIDTH > 320 ? 248 : 176, row_h = 16;
   int h         = 30 + rows * row_h + 6;
   int x         = (PIX16_FB_WIDTH  - w) / 2;
   int y         = (PIX16_FB_HEIGHT - h) / 2;
   int k;

   pix16_get_entry(pix, pix->game_menu_idx, false);
   strlcpy(title, pix16_entry_label(&pix->entry), sizeof(title));

   if (jrpg)
   {
      if (!pix16_popup_window(pix, x, y, w, h))
         return;
   }
   else
   {
      pix16_panel(pix, x, y, w, h, pix->colors[PIX16_C_PANEL]);
      pix16_fill(pix, x, y, w, 2, pix->tab_color[PIX16_TAB_LISTS]);
   }
   pix->text_shadow = jrpg;
   pix16_text_ticker(pix, x + 12, y + 10, title,
         jrpg ? pix->colors[PIX16_C_J_GOLD] : pix->colors[PIX16_C_YELLOW],
         (size_t)((w - 24) / PIX16_GLYPH_W), true);

   if (pix->game_menu_slot == -1)
      strlcpy(slot_txt, msg_hash_to_str(MSG_PIX16_AUTO), sizeof(slot_txt));
   else
      snprintf(slot_txt, sizeof(slot_txt), msg_hash_to_str(MSG_PIX16_SLOT), pix->game_menu_slot);

   for (k = 0; k < rows; k++)
   {
      int ty   = y + 30 + k * row_h + 4;
      bool sel = ((unsigned)k == pix->game_menu_sel);
      uint16_t c = jrpg ? pix->colors[PIX16_C_J_TEXT]
                 : (sel ? pix->colors[PIX16_C_TEXT] : pix->colors[PIX16_C_TEXT_DIM]);
      pix16_text(pix, x + 30, ty, k == 0 ? msg_hash_to_str(MSG_PIX16_RUN) : msg_hash_to_str(MSG_PIX16_LOAD_SAVE), c, 1, 0);
      if (k == 1)
         pix16_text(pix, x + w - 12 - (int)strlen(slot_txt) * PIX16_GLYPH_W, ty,
               slot_txt, jrpg ? pix->colors[PIX16_C_J_VALUE] : pix->colors[PIX16_C_VALUE],
               1, 0);
      if (sel)
      {
         if (jrpg)
         {
            pix->text_shadow = false;
            pix16_glove_at(pix, x + 12, ty - 1, pix16_glove_context(pix, 11));
            pix->text_shadow = true;
         }
         else
            pix16_cursor(pix, x + 16, ty, pix->colors[PIX16_C_TEXT]);
      }
   }
   pix->text_shadow = false;
}

/* Menu sounds for pix16's own moves (RetroArch plays them for its
 * normal list moves; the grid, gear and pickers are ours) */

static void pix16_sfx(enum pix16_sfx which)
{
#ifdef HAVE_AUDIOMIXER
   settings_t *settings = config_get_ptr();
   if (!settings || !settings->bools.audio_enable_menu)
      return;
   switch (which)
   {
      case PIX16_SFX_UP:
      case PIX16_SFX_DOWN:
         audio_driver_mixer_play_scroll_sound(which == PIX16_SFX_UP);
         break;
      case PIX16_SFX_OK:
         if (settings->bools.audio_enable_menu_ok)
            audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_OK);
         break;
      case PIX16_SFX_BACK:
         if (settings->bools.audio_enable_menu_cancel)
            audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_CANCEL);
         break;
   }
#else
   (void)which;
#endif
}

/* Cursor tick for a d-pad move: "up" sound for up/left */
static void pix16_sfx_move(enum menu_action action)
{
   pix16_sfx((action == MENU_ACTION_UP || action == MENU_ACTION_LEFT
            || action == MENU_ACTION_SCROLL_UP) ? PIX16_SFX_UP : PIX16_SFX_DOWN);
}

/* First letter or digit of a list entry, upper-cased ('#' if none) */
static char pix16_entry_initial(pix16_t *pix, size_t idx)
{
   const char *l;
   pix16_get_entry(pix, idx, false);
   for (l = pix16_entry_label(&pix->entry); *l; l++)
   {
      unsigned char c = (unsigned char)*l;
      if (c >= 'a' && c <= 'z')
         return (char)(c - 'a' + 'A');
      if ((c >= 'A' && c <= 'Z'))
         return (char)c;
      if (c >= '0' && c <= '9')
         return '#';
   }
   return '#';
}

/* Start of the next (dir > 0) or previous (dir < 0) letter group */
static size_t pix16_letter_jump(pix16_t *pix, size_t sel, size_t n, int dir)
{
   char cur = pix16_entry_initial(pix, sel);
   size_t j;

   if (dir > 0)
   {
      for (j = sel + 1; j < n; j++)
         if (pix16_entry_initial(pix, j) != cur)
            return j;
      return 0; /* past the last letter: back to the top */
   }

   /* not at the start of this group yet: go to its start */
   if (sel > 0 && pix16_entry_initial(pix, sel - 1) == cur)
   {
      while (sel > 0 && pix16_entry_initial(pix, sel - 1) == cur)
         sel--;
      return sel;
   }
   if (sel == 0)
      return 0;
   /* otherwise: start of the previous group */
   j   = sel - 1;
   cur = pix16_entry_initial(pix, j);
   while (j > 0 && pix16_entry_initial(pix, j - 1) == cur)
      j--;
   return j;
}

/* Page and letter jumps in long lists. L/R page; L2/R2 (RetroArch's
 * letter scroll) and Select in a game list jump by first letter. */
static bool pix16_fast_scroll(pix16_t *pix, enum menu_action action)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *list          = pix16_selection_buf();
   size_t sel, n, page;
   bool letters;

   if (!list || list->size == 0)
      return false;
   sel     = menu_st->selection_ptr;
   n       = list->size;
   page    = 8; /* rows visible in a list window */
   letters = (menu_st->scroll.mode == MENU_SCROLL_START_LETTER);

   switch (action)
   {
      case MENU_ACTION_SCROLL_UP:
         if (letters)
            sel = pix16_letter_jump(pix, sel, n, -1);
         else
            sel = (sel > page) ? sel - page : 0;
         break;
      case MENU_ACTION_SCROLL_DOWN:
         if (letters)
            sel = pix16_letter_jump(pix, sel, n, 1);
         else
            sel = (sel + page < n) ? sel + page : n - 1;
         break;
      case MENU_ACTION_INFO:
         /* Select: only in game lists, where Info has little to say */
         if (sel >= n || list->list[sel].type != FILE_TYPE_RPL_ENTRY)
            return false;
         sel = pix16_letter_jump(pix, sel, n, 1);
         break;
      default:
         return false;
   }

   if (sel != menu_st->selection_ptr)
      pix16_sfx_move(action == MENU_ACTION_SCROLL_UP
            ? MENU_ACTION_UP : MENU_ACTION_DOWN);
   menu_st->selection_ptr = sel;
   pix->flags            |= PIX16_FLAG_FORCE_REDRAW;
   return true;
}

static int pix16_menu_entry_action(void *userdata, menu_entry_t *entry,
      size_t i, enum menu_action action)
{
   pix16_t *pix = (pix16_t*)userdata;

   /* the launch swirl owns the screen until it hands over */
   if (pix && pix->swirl_active)
      return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);

   /* the first-run question takes every button while it is up */
   if (pix && pix->screen_prompt)
   {
      switch (action)
      {
         case MENU_ACTION_LEFT:
         case MENU_ACTION_UP:
            pix->screen_sel = (pix->screen_sel + 3) % 4;
            pix16_sfx(PIX16_SFX_UP);
            break;
         case MENU_ACTION_RIGHT:
         case MENU_ACTION_DOWN:
            pix->screen_sel = (pix->screen_sel + 1) % 4;
            pix16_sfx(PIX16_SFX_DOWN);
            break;
         case MENU_ACTION_OK:
            pix16_screen_choice(pix);
            break;
         default:
            break;
      }
      pix->flags |= PIX16_FLAG_FORCE_REDRAW;
      return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
   }

   /* boot screen: any button moves it along */
   if (pix && pix->boot_active)
   {
      if (action != MENU_ACTION_NOOP)
         pix16_boot_press(pix);
      return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
   }

   /* Add to / Remove from Favorites */
   if (pix && action == MENU_ACTION_OK)
   {
      file_list_t *fl = pix16_selection_buf();
      size_t fs       = menu_state_get_ptr()->selection_ptr;
      if (     fl && fs < fl->size && fl->list[fs].label
            && string_is_equal(fl->list[fs].label, PIX16_FAV_LABEL))
      {
         pix16_fav_toggle(pix);
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }
   }

   /* QUIT GAME? window takes every button */
   if (pix && pix->quit_confirm && pix16_quit_confirm_input(pix, action))
      return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);

   /* "Close Content" in RetroArch's full Quick Menu asks first too */
   if (     pix && action == MENU_ACTION_OK && pix->qm_full
         && menu_is_running_quick_menu())
   {
      file_list_t *ql = pix16_selection_buf();
      size_t qs       = menu_state_get_ptr()->selection_ptr;
      if (     ql && qs < ql->size && ql->list[qs].label
            && string_is_equal(ql->list[qs].label, MENU_ENUM_LABEL_CLOSE_CONTENT_STR))
      {
         pix16_sfx(PIX16_SFX_OK);
         pix->quit_confirm = true;
         pix->confirm_exit = false;
         pix->quit_sel     = 1;
         pix->flags       |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }
   }

   /* in a game: the pause window takes every button */
   if (pix16_pause_active(pix) && pix16_pause_input(pix, action))
      return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);

   /* B at the top of RetroArch's full Quick Menu: back to the pause
    * window instead of out to the main menu */
   if (     pix && pix->qm_full && action == MENU_ACTION_CANCEL
         && menu_is_running_quick_menu())
   {
      pix->qm_full = false;
      pix->flags  |= PIX16_FLAG_FORCE_REDRAW;
      pix16_sfx(PIX16_SFX_BACK);
      return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
   }

   /* In a game list, A plays the game straight away; Start opens
    * RetroArch's options for it (core, favourites, delete...) */
   if (pix && !menu_is_running_quick_menu()
         && (action == MENU_ACTION_OK || action == MENU_ACTION_START))
   {
      file_list_t *list = pix16_selection_buf();
      if (     !pix->game_menu_open
            && list && i < list->size
            && list->list[i].type == FILE_TYPE_RPL_ENTRY)
      {
         if (action == MENU_ACTION_OK)
         {
            /* A: the RUN / LOAD SAVE window */
            pix16_sfx(PIX16_SFX_OK);
            pix16_game_menu_open(pix, i);
            pix->flags |= PIX16_FLAG_FORCE_REDRAW;
            return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
         }
         /* Start: RetroArch's full options for the game */
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_OK);
      }
   }

   /* RUN / LOAD SAVE window */
   if (pix && pix->game_menu_open)
   {
      unsigned rows = (pix->game_menu_slot != PIX16_SLOT_NONE) ? 2 : 1;
      switch (action)
      {
         case MENU_ACTION_UP:
            if (pix->game_menu_sel > 0)
            {
               pix->game_menu_sel--;
               pix16_sfx(PIX16_SFX_UP);
            }
            break;
         case MENU_ACTION_DOWN:
            if (pix->game_menu_sel + 1 < rows)
            {
               pix->game_menu_sel++;
               pix16_sfx(PIX16_SFX_DOWN);
            }
            break;
         case MENU_ACTION_CANCEL:
            pix->game_menu_open = false;
            pix16_sfx(PIX16_SFX_BACK);
            break;
         case MENU_ACTION_OK:
         {
            size_t idx          = pix->game_menu_idx;
            pix->game_menu_open = false;
            pix->pending_slot   = (pix->game_menu_sel == 1)
                                ? pix->game_menu_slot : PIX16_SLOT_NONE;
            pix->flags         |= PIX16_FLAG_AUTO_RUN;
            pix->auto_run_depth = pix16_stack_depth() + 1;
            menu_state_get_ptr()->selection_ptr = idx;
            pix16_get_entry(pix, idx, false);
            return generic_menu_entry_action(userdata, &pix->entry, idx,
                  MENU_ACTION_OK);
         }
         default:
            break;
      }
      pix->flags |= PIX16_FLAG_FORCE_REDRAW;
      return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
   }

   /* Systems grid: the d-pad moves in 2D, L/R switch tabs */
   if (pix && pix16_is_grid_view())
   {
      struct menu_state *menu_st = menu_state_get_ptr();
      size_t pos, cnt;
      bool moved = false;

      /* Add System wizard */
      if (pix->wiz.open)
      {
         if (menu_input_dialog_get_display_kb())
            return generic_menu_entry_action(userdata, entry, i, action);
         pix16_wiz_input(pix, action);
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* L/R: previous / next page of the grid (same spot on the page) */
      if (action == MENU_ACTION_SCROLL_UP || action == MENU_ACTION_SCROLL_DOWN)
      {
         size_t per = PIX16_GRID_PER_PAGE, n, pages, page, in, target;
         pix16_grid_build(pix);
         n = pix->grid_count;
         if (n > 0 && !pix->arcade_open && !pix->game_menu_open)
         {
            pages = (n + per - 1) / per;
            if (pix16_grid_has(pix, menu_st->selection_ptr)
                  && !PIX16_BAR_FOCUS(pix))
            {
               size_t pos = pix16_grid_pos(pix, menu_st->selection_ptr);
               page = pos / per;
               in   = pos % per;
            }
            else
            {
               page = pix->grid_page;
               in   = 0;
            }
            if (action == MENU_ACTION_SCROLL_UP ? page > 0 : page + 1 < pages)
            {
               page   = (action == MENU_ACTION_SCROLL_UP) ? page - 1 : page + 1;
               target = page * per + in;
               if (target >= n)
                  target = n - 1;
               pix->gear_focus        = false;
               menu_st->selection_ptr = pix->grid_map[target];
               pix->flags            |= PIX16_FLAG_FORCE_REDRAW;
               pix16_sfx_move(action);
            }
         }
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      pix16_grid_build(pix);
      cnt = pix->grid_count;

      /* Actions for one system */
      if (pix->order_open && pix->mg_open)
      {
         pix16_mg_input(pix, action);
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* System order window */
      if (pix->order_open)
      {
         switch (action)
         {
            case MENU_ACTION_UP:
               if (pix->order_sel > 0)
               {
                  pix->order_sel--;
                  pix16_sfx(PIX16_SFX_UP);
               }
               break;
            case MENU_ACTION_DOWN:
               if ((size_t)(pix->order_sel + 1) < pix->order_n)
               {
                  pix->order_sel++;
                  pix16_sfx(PIX16_SFX_DOWN);
               }
               break;
            case MENU_ACTION_LEFT:
               if (pix->order_sel > 0)
               {
                  size_t s = (size_t)pix->order_sel, tmp = pix->order_map[s];
                  char tl[40];
                  size_t k;
                  for (k = 0; k < 40; k++)
                     tl[k] = pix->order_label[s][k];
                  pix->order_map[s]     = pix->order_map[s - 1];
                  pix->order_map[s - 1] = tmp;
                  for (k = 0; k < 40; k++)
                     pix->order_label[s][k] = pix->order_label[s - 1][k];
                  for (k = 0; k < 40; k++)
                     pix->order_label[s - 1][k] = tl[k];
                  pix->order_sel--;
                  pix16_sfx(PIX16_SFX_UP);
               }
               break;
            case MENU_ACTION_RIGHT:
               if ((size_t)(pix->order_sel + 1) < pix->order_n)
               {
                  size_t s = (size_t)pix->order_sel, tmp = pix->order_map[s];
                  char tl[40];
                  size_t k;
                  for (k = 0; k < 40; k++)
                     tl[k] = pix->order_label[s][k];
                  pix->order_map[s]     = pix->order_map[s + 1];
                  pix->order_map[s + 1] = tmp;
                  for (k = 0; k < 40; k++)
                     pix->order_label[s][k] = pix->order_label[s + 1][k];
                  for (k = 0; k < 40; k++)
                     pix->order_label[s + 1][k] = tl[k];
                  pix->order_sel++;
                  pix16_sfx(PIX16_SFX_DOWN);
               }
               break;
            case MENU_ACTION_OK:
            case MENU_ACTION_START:
               {
                  file_list_t *ol = pix16_selection_buf();
                  size_t k;
                  pix16_order_n = 0;
                  if (ol)
                     for (k = 0; k < pix->order_n && pix16_order_n < PIX16_ORDER_MAX; k++)
                     {
                        char base[NAME_MAX_LENGTH];
                        pix16_order_base(ol->list[pix->order_map[k]].path, base,
                              sizeof(base));
                        if (base[0])
                           strlcpy(pix16_order[pix16_order_n++], base,
                                 sizeof(pix16_order[0]));
                     }
                  pix16_order_save();
                  pix->order_open  = false;
                  pix->order_focus = false;
                  pix16_sfx(PIX16_SFX_OK);
               }
               break;
            case MENU_ACTION_INFO:   /* Select */
            case MENU_ACTION_SCAN:   /* Y / triangle */
               pix16_mg_open(pix);
               break;
            case MENU_ACTION_CANCEL:
               pix->order_open  = false;
               pix->order_focus = false;
               pix16_sfx(PIX16_SFX_BACK);
               break;
            default:
               break;
         }
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* Arcade collection / PC Engine picker */
      if (pix->arcade_open)
      {
         switch (action)
         {
            case MENU_ACTION_UP:
               if (pix->arcade_sel > 0)
               {
                  pix->arcade_sel--;
                  pix16_sfx(PIX16_SFX_UP);
               }
               break;
            case MENU_ACTION_DOWN:
               if (pix->arcade_sel + 1 < pix16_picker_items(pix, pix->pick, PIX16_PICK_MAX))
               {
                  pix->arcade_sel++;
                  pix16_sfx(PIX16_SFX_DOWN);
               }
               break;
            case MENU_ACTION_CANCEL:
               pix->arcade_open = false;
               pix16_sfx(PIX16_SFX_BACK);
               break;
            case MENU_ACTION_OK:
               if (pix->arcade_sel < pix16_picker_items(pix, pix->pick, PIX16_PICK_MAX))
               {
                  size_t idx       = pix->pick[pix->arcade_sel];
                  pix->arcade_open = false;
                  menu_st->selection_ptr = idx;
                  pix16_get_entry(pix, idx, false);
                  return generic_menu_entry_action(userdata, &pix->entry, idx,
                        MENU_ACTION_OK);
               }
               break;
            default:
               break;
         }
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* On the power button */
      if (pix->power_focus)
      {
         switch (action)
         {
            case MENU_ACTION_RIGHT:
               pix->power_focus = false;
               pix16_sfx(PIX16_SFX_DOWN);
               if (pix->top_idx[0] != PIX16_TOP_NONE)
                  menu_st->selection_ptr = pix->top_idx[0];
               else if (pix->top_idx[1] != PIX16_TOP_NONE)
                  menu_st->selection_ptr = pix->top_idx[1];
               else
                  pix->add_focus = true;
               break;
            case MENU_ACTION_DOWN:
               if (cnt > 0)
               {
                  pix->power_focus       = false;
                  menu_st->selection_ptr = pix->grid_map[pix->grid_page * PIX16_GRID_PER_PAGE];
                  pix16_sfx(PIX16_SFX_DOWN);
               }
               break;
            case MENU_ACTION_OK:
               pix16_sfx(PIX16_SFX_OK);
               pix->quit_confirm = true;
               pix->confirm_exit = true;
               pix->quit_sel     = 1;      /* cursor on NO */
               break;
            default:
               break;
         }
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* On the + (Add System) button */
      if (pix->add_focus)
      {
         switch (action)
         {
            case MENU_ACTION_LEFT:
               pix->add_focus   = false;
               pix->order_focus = true;
               pix16_sfx(PIX16_SFX_UP);
               break;
            case MENU_ACTION_RIGHT:
               pix->add_focus  = false;
               pix->gear_focus = true;
               pix16_sfx(PIX16_SFX_DOWN);
               break;
            case MENU_ACTION_DOWN:
               if (cnt > 0)
               {
                  size_t d = pix->grid_page * PIX16_GRID_PER_PAGE + PIX16_GRID_COLS - 2;
                  if (d >= cnt)
                     d = cnt - 1;
                  pix->add_focus         = false;
                  menu_st->selection_ptr = pix->grid_map[d];
                  pix16_sfx(PIX16_SFX_DOWN);
               }
               break;
            case MENU_ACTION_OK:
               pix16_sfx(PIX16_SFX_OK);
               pix16_wiz_open(pix);
               break;
            case MENU_ACTION_CANCEL:
               if (cnt > 1)
               {
                  pix->add_focus   = false;
                  pix->gear_focus  = false;
                  pix->power_focus = false;
                  pix16_order_fill(pix);
                  pix->order_sel  = 0;
                  pix->order_open = true;
                  pix16_sfx(PIX16_SFX_OK);
                  break;
               }
               return generic_menu_entry_action(userdata, entry, i, action);
            default:
               break;
         }
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* A on a tile that has systems inside it: choose one */
      if (     action == MENU_ACTION_OK && !PIX16_BAR_FOCUS(pix)
            && !(pix16_pce_grouped(pix)
                 && pix16_canon(pix, menu_st->selection_ptr) == pix->pce_cart)
            && !(pix16_arcade_grouped(pix)
                 && pix16_canon(pix, menu_st->selection_ptr) == pix->arcade_rep)
            && pix16_has_subs(pix, pix16_canon(pix, menu_st->selection_ptr)))
      {
         size_t *pick = pix->pick, np, k;
         pix->arcade_open = true;
         pix->picker_pce  = false;
         pix->picker_host = pix16_canon(pix, menu_st->selection_ptr);
         pix->arcade_sel  = 0;
         np = pix16_picker_items(pix, pix->pick, PIX16_PICK_MAX);
         for (k = 0; k < np; k++)
            if (pick[k] == menu_st->selection_ptr)
               pix->arcade_sel = k;
         pix16_sfx(PIX16_SFX_OK);
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* A on the ARCADE tile: choose a collection */
      if (     action == MENU_ACTION_OK && !PIX16_BAR_FOCUS(pix)
            && pix16_arcade_grouped(pix)
            && pix16_canon(pix, menu_st->selection_ptr) == pix->arcade_rep)
      {
         size_t k;
         pix->arcade_open = true;
         pix->picker_pce  = false;
         pix->picker_host = PIX16_TOP_NONE;
         pix->arcade_sel  = 0;
         pix16_sfx(PIX16_SFX_OK);
         /* start on the collection last opened */
         for (k = 0; k < pix->arcade_count; k++)
            if (pix->arcade_idx[k] == menu_st->selection_ptr)
               pix->arcade_sel = k;
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* A on the PC Engine tile: CARTRIDGE or CD-ROM? */
      if (     action == MENU_ACTION_OK && !PIX16_BAR_FOCUS(pix)
            && pix16_pce_grouped(pix)
            && pix16_canon(pix, menu_st->selection_ptr) == pix->pce_cart)
      {
         size_t *pick = pix->pick, np, k;
         pix->arcade_open = true;
         pix->picker_pce  = true;
         pix->picker_host = PIX16_TOP_NONE;
         pix->arcade_sel  = (menu_st->selection_ptr == pix->pce_cd) ? 1 : 0;
         np = pix16_picker_items(pix, pix->pick, PIX16_PICK_MAX);
         for (k = 2; k < np; k++)
            if (pick[k] == menu_st->selection_ptr)
               pix->arcade_sel = k;
         pix16_sfx(PIX16_SFX_OK);
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* On the options gear */
      if (pix->order_focus)
      {
         switch (action)
         {
            case MENU_ACTION_LEFT:
               pix->order_focus = false;
               pix16_sfx(PIX16_SFX_UP);
               if (pix->top_idx[1] != PIX16_TOP_NONE)
                  menu_st->selection_ptr = pix->top_idx[1];
               else if (pix->top_idx[0] != PIX16_TOP_NONE)
                  menu_st->selection_ptr = pix->top_idx[0];
               else
                  pix->power_focus = true;
               break;
            case MENU_ACTION_RIGHT:
               pix->order_focus = false;
               pix->add_focus   = true;
               pix16_sfx(PIX16_SFX_DOWN);
               break;
            case MENU_ACTION_DOWN:
               if (cnt > 0)
               {
                  size_t d = pix->grid_page * PIX16_GRID_PER_PAGE + PIX16_GRID_COLS - 2;
                  if (d >= cnt)
                     d = cnt - 1;
                  pix->order_focus       = false;
                  menu_st->selection_ptr = pix->grid_map[d];
                  pix16_sfx(PIX16_SFX_DOWN);
               }
               break;
            case MENU_ACTION_OK:
            case MENU_ACTION_START:
               if (cnt > 1)
               {
                  pix->order_focus = true;
                  pix16_order_fill(pix);
                  pix->order_sel  = 0;
                  pix->order_open = true;
                  pix16_sfx(PIX16_SFX_OK);
               }
               break;
            default:
               break;
         }
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }
      if (pix->gear_focus)
      {
         switch (action)
         {
            case MENU_ACTION_LEFT:
               pix->gear_focus = false;
               pix->add_focus  = true;
               pix16_sfx(PIX16_SFX_UP);
               break;
            case MENU_ACTION_DOWN:
               if (cnt > 0)
               {
                  size_t d = pix->grid_page * PIX16_GRID_PER_PAGE + PIX16_GRID_COLS - 1;
                  if (d >= cnt)
                     d = cnt - 1;
                  pix->gear_focus        = false;
                  menu_st->selection_ptr = pix->grid_map[d];
                  pix16_sfx(PIX16_SFX_DOWN);
               }
               break;
            case MENU_ACTION_OK:
               pix->gear_focus = false;
               pix->from_gear  = true;
               pix16_sfx(PIX16_SFX_OK);
               pix16_switch_tab(PIX16_TAB_SETUP);
               break;
            case MENU_ACTION_CANCEL:
               return generic_menu_entry_action(userdata, entry, i, action);
            default:
               break;
         }
         pix->flags |= PIX16_FLAG_FORCE_REDRAW;
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      /* On the HISTORY / FAVORITES buttons */
      if (     (action == MENU_ACTION_LEFT || action == MENU_ACTION_RIGHT
             || action == MENU_ACTION_UP   || action == MENU_ACTION_DOWN)
            && pix16_top_focus(pix, menu_st->selection_ptr) >= 0)
      {
         int f     = pix16_top_focus(pix, menu_st->selection_ptr);
         int other = 1 - f;
         if (     action == MENU_ACTION_LEFT
               && (f == 0 || pix->top_idx[0] == PIX16_TOP_NONE))
         {
            pix->power_focus = true;   /* on to the power button */
            moved            = true;
         }
         else if (action == MENU_ACTION_RIGHT
               && (f == 1 || pix->top_idx[1] == PIX16_TOP_NONE))
         {
            pix->order_focus = true;  /* on to the arrange button */
            moved           = true;
         }
         else if ((action == MENU_ACTION_RIGHT && f == 0)
               || (action == MENU_ACTION_LEFT  && f == 1))
         {
            if (pix->top_idx[other] != PIX16_TOP_NONE)
            {
               menu_st->selection_ptr = pix->top_idx[other];
               moved = true;
            }
         }
         else if (action == MENU_ACTION_DOWN && cnt > 0)
         {
            /* into the grid, on the page that was showing */
            size_t d = pix->grid_page * PIX16_GRID_PER_PAGE
                     + (f == 1 ? PIX16_GRID_COLS / 2 : 0);
            if (d >= cnt)
               d = cnt - 1;
            menu_st->selection_ptr = pix->grid_map[d];
            moved = true;
         }
         if (moved)
         {
            pix->flags |= PIX16_FLAG_FORCE_REDRAW;
            pix16_sfx_move(action);
         }
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }

      if (     cnt > 0
            && (   action == MENU_ACTION_LEFT || action == MENU_ACTION_RIGHT
                || action == MENU_ACTION_UP   || action == MENU_ACTION_DOWN))
      {
         size_t per   = PIX16_GRID_PER_PAGE;
         size_t page, in, col, row, pages;
         pos   = pix16_grid_pos(pix, menu_st->selection_ptr);
         page  = pos / per;
         in    = pos % per;
         col   = in % PIX16_GRID_COLS;
         row   = in / PIX16_GRID_COLS;
         pages = (cnt + per - 1) / per;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (col > 0)
               {
                  pos--;
                  moved = true;
               }
               else if (page > 0)
               {
                  /* previous page, same row, right-hand column */
                  pos   = (page - 1) * per + row * PIX16_GRID_COLS
                        + PIX16_GRID_COLS - 1;
                  moved = true;
               }
               break;
            case MENU_ACTION_RIGHT:
               if (col + 1 < pix16_grid_row_count(page, row, cnt)
                     && pos + 1 < cnt)
               {
                  pos++;
                  moved = true;
               }
               else if (page + 1 < pages)
               {
                  /* next page, same row, left-hand column (or its last
                   * tile when that page is short) */
                  pos   = (page + 1) * per + row * PIX16_GRID_COLS;
                  if (pos >= cnt)
                     pos = cnt - 1;
                  moved = true;
               }
               break;
            case MENU_ACTION_UP:
               if (row > 0)
               {
                  size_t n = pix16_grid_row_count(page, row - 1, cnt);
                  if (n)
                  {
                     pos = page * per + (row - 1) * PIX16_GRID_COLS
                         + pix16_grid_col_for(row - 1, n, in, row);
                     moved = true;
                  }
               }
               else
               {
                  /* right-hand column: up to the gear */
                  int want = (col < (size_t)PIX16_GRID_COLS / 2) ? 0 : 1;
                  if (col == (size_t)PIX16_GRID_COLS - 1)
                  {
                     pix16_sfx(PIX16_SFX_UP);
                     pix->gear_focus = true;
                     if (pix->top_idx[1] != PIX16_TOP_NONE)
                        menu_st->selection_ptr = pix->top_idx[1];
                     else if (pix->top_idx[0] != PIX16_TOP_NONE)
                        menu_st->selection_ptr = pix->top_idx[0];
                     pix->flags |= PIX16_FLAG_FORCE_REDRAW;
                     return generic_menu_entry_action(userdata, entry, i,
                           MENU_ACTION_NOOP);
                  }
                  /* up to HISTORY (left half) or FAVORITES (right half) */
                  if (pix->top_idx[want] == PIX16_TOP_NONE)
                     want = 1 - want;
                  if (pix->top_idx[want] != PIX16_TOP_NONE)
                  {
                     pix16_sfx(PIX16_SFX_UP);
                     menu_st->selection_ptr = pix->top_idx[want];
                     pix->flags            |= PIX16_FLAG_FORCE_REDRAW;
                     return generic_menu_entry_action(userdata, entry, i,
                           MENU_ACTION_NOOP);
                  }
               }
               break;
            case MENU_ACTION_DOWN:
               if (row + 1 < (size_t)PIX16_GRID_ROWS)
               {
                  size_t n = pix16_grid_row_count(page, row + 1, cnt);
                  if (n)
                  {
                     pos = page * per + (row + 1) * PIX16_GRID_COLS
                         + pix16_grid_col_for(row + 1, n, in, row);
                     moved = true;
                  }
               }
               break;
            default:
               break;
         }

         if (moved)
         {
            menu_st->selection_ptr = pix->grid_map[pos];
            pix->flags            |= PIX16_FLAG_FORCE_REDRAW;
            pix16_sfx_move(action);
         }
         return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
      }
   }

   /* B at the top of MAIN / SETUP / NET goes back to the systems page */
   if (     pix && action == MENU_ACTION_CANCEL
         && pix16_stack_depth() == 1
         && !menu_is_running_quick_menu()
         && pix16_root_tab() != PIX16_TAB_LISTS)
   {
      pix16_sfx(PIX16_SFX_BACK);
      pix->opt_back = false;
      pix16_switch_tab(PIX16_TAB_LISTS);
      pix->flags |= pix->from_gear ? PIX16_FLAG_RETURN_GEAR
                                   : PIX16_FLAG_SELECT_SYSTEM;
      pix->from_gear = false;
      return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
   }

   /* Inside lists: L/R page, Select jumps to the next letter */
   if (     pix
         && (pix16_stack_depth() > 1 || menu_is_running_quick_menu())
         && (   action == MENU_ACTION_SCROLL_UP || action == MENU_ACTION_SCROLL_DOWN
             || action == MENU_ACTION_INFO)
         && pix16_fast_scroll(pix, action))
      return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);

   /* Options screen (MAIN / SETUP / NET): the top bar */
   if (     pix && pix16_stack_depth() == 1
         && !menu_is_running_quick_menu()
         && pix16_root_tab() != PIX16_TAB_LISTS)
   {
      int order[PIX16_TAB_COUNT];
      int n   = pix16_opt_tabs(order);
      int pos = pix16_opt_pos(pix16_root_tab());

      switch (action)
      {
         case MENU_ACTION_SCROLL_UP:     /* L / R: the option tabs only */
         case MENU_ACTION_SCROLL_DOWN:
            pos = (action == MENU_ACTION_SCROLL_UP) ? (pos + n - 1) % n : (pos + 1) % n;
            pix->opt_back = false;
            pix16_switch_tab(order[pos]);
            pix16_sfx_move(action);
            pix->flags |= PIX16_FLAG_FORCE_REDRAW;
            return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
         case MENU_ACTION_LEFT:
            if (pix->opt_back)
               pix->opt_back = false;          /* back onto the last tab */
            else if (pos > 0)
               pix16_switch_tab(order[pos - 1]);
            else
               return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
            pix16_sfx(PIX16_SFX_UP);
            pix->flags |= PIX16_FLAG_FORCE_REDRAW;
            return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
         case MENU_ACTION_RIGHT:
            if (pix->opt_back)
               return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
            if (pos + 1 < n)
               pix16_switch_tab(order[pos + 1]);
            else
               pix->opt_back = true;           /* onto BACK */
            pix16_sfx(PIX16_SFX_DOWN);
            pix->flags |= PIX16_FLAG_FORCE_REDRAW;
            return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
         case MENU_ACTION_UP:
         case MENU_ACTION_DOWN:
            if (pix->opt_back)
            {
               pix->opt_back = false;          /* back into the list */
               pix->flags   |= PIX16_FLAG_FORCE_REDRAW;
               return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
            }
            break;
         case MENU_ACTION_OK:
            if (pix->opt_back)
            {
               pix16_sfx(PIX16_SFX_BACK);
               pix->opt_back = false;
               pix16_switch_tab(PIX16_TAB_LISTS);
               pix->flags |= pix->from_gear ? PIX16_FLAG_RETURN_GEAR
                                            : PIX16_FLAG_SELECT_SYSTEM;
               pix->from_gear = false;
               return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
            }
            break;
         default:
            if (pix->opt_back)   /* nothing else acts on the BACK button */
               return generic_menu_entry_action(userdata, entry, i, MENU_ACTION_NOOP);
            break;
      }
   }

   return generic_menu_entry_action(userdata, entry, i, action);
}

/* The empty library window is a button: a click opens the wizard */
static int pix16_pointer_up(void *data, unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs, menu_entry_t *entry, unsigned action)
{
   pix16_t *pix = (pix16_t*)data;
   const pix16_rect_t *r;

   (void)ptr;
   (void)cbs;
   (void)entry;
   (void)action;

   if (!pix || !pix->empty_show)
      return -1;
   if (     gesture != MENU_INPUT_GESTURE_TAP
         && gesture != MENU_INPUT_GESTURE_SHORT_PRESS)
      return -1;

   r = &pix->empty_rect;
   if (     (int)x < r->x || (int)x >= r->x + r->w
         || (int)y < r->y || (int)y >= r->y + r->h)
      return -1;

   pix16_wiz_open(pix);
   pix->add_focus = true;
   pix->flags    |= PIX16_FLAG_FORCE_REDRAW;
   return 0;
}

menu_ctx_driver_t menu_ctx_pix16 = {   pix16_set_texture,
   pix16_render_messagebox,
   pix16_render,
   pix16_frame,
   pix16_init,
   pix16_free,
   pix16_context_reset,
   pix16_context_destroy,
   pix16_populate_entries,
   pix16_toggle,
   pix16_navigation_clear,
   NULL,                               /* navigation_decrement */
   NULL,                               /* navigation_increment */
   pix16_navigation_set,
   NULL,                               /* navigation_set_last */
   NULL,                               /* navigation_descend_alphabet */
   NULL,                               /* navigation_ascend_alphabet */
   NULL,                               /* lists_init */
   NULL,                               /* list_insert */
   NULL,                               /* list_prepend */
   NULL,                               /* list_free */
   NULL,                               /* list_clear */
   NULL,                               /* list_cache */
   NULL,                               /* list_push */
   NULL,                               /* list_get_selection */
   NULL,                               /* list_get_size */
   NULL,                               /* list_get_entry */
   NULL,                               /* list_set_selection */
   NULL,                               /* bind_init */
   NULL,                               /* load_image */
   "pix16",
   pix16_environ,                      /* environ_cb */
   NULL,                               /* update_thumbnail_path */
   NULL,                               /* update_thumbnail_image */
   NULL,                               /* refresh_thumbnail_image */
   NULL,                               /* set_thumbnail_content */
   NULL,                               /* osk_ptr_at_pos */
   NULL,                               /* osk_pointer_over_textbox */
   NULL,                               /* update_savestate_thumbnail_path */
   NULL,                               /* update_savestate_thumbnail_image */
   NULL,                               /* pointer_down */
   pix16_pointer_up,                   /* pointer_up */
   pix16_menu_entry_action,
   NULL                                /* wheel_scroll */
};
