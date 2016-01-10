/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Tobias Jakobi
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

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "../../general.h"
#include "../../retroarch.h"
#include "../video_viewport.h"
#include "../video_monitor.h"
#include "../font_renderer_driver.h"

//MAC Specific includes for the driver
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <signal.h>
#include <pthread.h>

// Lowlevel SunxiG2D functions block

/* for tracking the ioctls API/ABI */
#define SUNXI_DISP_VERSION_MAJOR 1
#define SUNXI_DISP_VERSION_MINOR 0

#define SUNXI_DISP_VERSION ((SUNXI_DISP_VERSION_MAJOR << 16) | SUNXI_DISP_VERSION_MINOR)

#define FBIOGET_LAYER_HDL_0 0x4700
#define FBIOGET_LAYER_HDL_1 0x4701

#define FALLBACK_BLT() sunxi_g2d_try_fallback_blt(self, src_bits,        \
                                                  dst_bits, src_stride,  \
                                                  dst_stride, src_bpp,   \
                                                  dst_bpp, src_x, src_y, \
                                                  dst_x, dst_y, w, h);

#define __bool signed char

typedef struct {
	__u8 alpha;
	__u8 red;
	__u8 green;
	__u8 blue;
} __disp_color_t;
typedef struct {
	__s32 x;
	__s32 y;
	__u32 width;
	__u32 height;
} __disp_rect_t;
typedef struct {
	__u32 width;
	__u32 height;
} __disp_rectsz_t;
typedef struct {
	__s32 x;
	__s32 y;
} __disp_pos_t;

typedef enum tag_DISP_CMD {
	/* ----disp global---- */
	DISP_CMD_VERSION = 0x00,
	DISP_CMD_RESERVE1 = 0x01,
	/* fail when the value is 0x02 in linux,why??? */
	DISP_CMD_SET_BKCOLOR = 0x3f,
	DISP_CMD_GET_BKCOLOR = 0x03,
	DISP_CMD_SET_COLORKEY = 0x04,
	DISP_CMD_GET_COLORKEY = 0x05,
	DISP_CMD_SET_PALETTE_TBL = 0x06,
	DISP_CMD_GET_PALETTE_TBL = 0x07,
	DISP_CMD_SCN_GET_WIDTH = 0x08,
	DISP_CMD_SCN_GET_HEIGHT = 0x09,
	DISP_CMD_GET_OUTPUT_TYPE = 0x0a,
	DISP_CMD_SET_EXIT_MODE = 0x0c,
	DISP_CMD_SET_GAMMA_TABLE = 0x0d,
	DISP_CMD_GAMMA_CORRECTION_ON = 0x0e,
	DISP_CMD_GAMMA_CORRECTION_OFF = 0x0f,
	DISP_CMD_START_CMD_CACHE = 0x10,
	DISP_CMD_EXECUTE_CMD_AND_STOP_CACHE = 0x11,
	DISP_CMD_SET_BRIGHT = 0x12,
	DISP_CMD_SET_CONTRAST = 0x13,
	DISP_CMD_SET_SATURATION = 0x14,
	DISP_CMD_GET_BRIGHT = 0x16,
	DISP_CMD_GET_CONTRAST = 0x17,
	DISP_CMD_GET_SATURATION = 0x18,
	DISP_CMD_ENHANCE_ON = 0x1a,
	DISP_CMD_ENHANCE_OFF = 0x1b,
	DISP_CMD_GET_ENHANCE_EN = 0x1c,
	DISP_CMD_CLK_ON = 0x1d,
	DISP_CMD_CLK_OFF = 0x1e,
	/*
	 * when the screen is not used to display(lcd/tv/vga/hdmi) directly,
	 * maybe capture the screen and scaler to dram, or as a layer of
	 * another screen
	 */
	DISP_CMD_SET_SCREEN_SIZE = 0x1f,
	DISP_CMD_CAPTURE_SCREEN = 0x20,	/* caputre screen and scaler to dram */
	DISP_CMD_DE_FLICKER_ON = 0x21,
	DISP_CMD_DE_FLICKER_OFF = 0x22,
	DISP_CMD_SET_HUE = 0x23,
	DISP_CMD_GET_HUE = 0x24,
	DISP_CMD_DRC_OFF = 0x25,
	DISP_CMD_GET_DRC_EN = 0x26,
	DISP_CMD_DE_FLICKER_SET_WINDOW = 0x27,
	DISP_CMD_DRC_SET_WINDOW = 0x28,
	DISP_CMD_DRC_ON = 0x29,
	DISP_CMD_GET_DE_FLICKER_EN = 0x2a,

	/* ----layer---- */
	DISP_CMD_LAYER_REQUEST = 0x40,
	DISP_CMD_LAYER_RELEASE = 0x41,
	DISP_CMD_LAYER_OPEN = 0x42,
	DISP_CMD_LAYER_CLOSE = 0x43,
	DISP_CMD_LAYER_SET_FB = 0x44,
	DISP_CMD_LAYER_GET_FB = 0x45,
	DISP_CMD_LAYER_SET_SRC_WINDOW = 0x46,
	DISP_CMD_LAYER_GET_SRC_WINDOW = 0x47,
	DISP_CMD_LAYER_SET_SCN_WINDOW = 0x48,
	DISP_CMD_LAYER_GET_SCN_WINDOW = 0x49,
	DISP_CMD_LAYER_SET_PARA = 0x4a,
	DISP_CMD_LAYER_GET_PARA = 0x4b,
	DISP_CMD_LAYER_ALPHA_ON = 0x4c,
	DISP_CMD_LAYER_ALPHA_OFF = 0x4d,
	DISP_CMD_LAYER_GET_ALPHA_EN = 0x4e,
	DISP_CMD_LAYER_SET_ALPHA_VALUE = 0x4f,
	DISP_CMD_LAYER_GET_ALPHA_VALUE = 0x50,
	DISP_CMD_LAYER_CK_ON = 0x51,
	DISP_CMD_LAYER_CK_OFF = 0x52,
	DISP_CMD_LAYER_GET_CK_EN = 0x53,
	DISP_CMD_LAYER_SET_PIPE = 0x54,
	DISP_CMD_LAYER_GET_PIPE = 0x55,
	DISP_CMD_LAYER_TOP = 0x56,
	DISP_CMD_LAYER_BOTTOM = 0x57,
	DISP_CMD_LAYER_GET_PRIO = 0x58,
	DISP_CMD_LAYER_SET_SMOOTH = 0x59,
	DISP_CMD_LAYER_GET_SMOOTH = 0x5a,
	DISP_CMD_LAYER_SET_BRIGHT = 0x5b, /* brightness */
	DISP_CMD_LAYER_SET_CONTRAST = 0x5c, /* contrast */
	DISP_CMD_LAYER_SET_SATURATION = 0x5d, /* saturation */
	DISP_CMD_LAYER_SET_HUE = 0x5e, /* hue, chroma */
	DISP_CMD_LAYER_GET_BRIGHT = 0x5f,
	DISP_CMD_LAYER_GET_CONTRAST = 0x60,
	DISP_CMD_LAYER_GET_SATURATION = 0x61,
	DISP_CMD_LAYER_GET_HUE = 0x62,
	DISP_CMD_LAYER_ENHANCE_ON = 0x63,
	DISP_CMD_LAYER_ENHANCE_OFF = 0x64,
	DISP_CMD_LAYER_GET_ENHANCE_EN = 0x65,
	DISP_CMD_LAYER_VPP_ON = 0x67,
	DISP_CMD_LAYER_VPP_OFF = 0x68,
	DISP_CMD_LAYER_GET_VPP_EN = 0x69,
	DISP_CMD_LAYER_SET_LUMA_SHARP_LEVEL = 0x6a,
	DISP_CMD_LAYER_GET_LUMA_SHARP_LEVEL = 0x6b,
	DISP_CMD_LAYER_SET_CHROMA_SHARP_LEVEL = 0x6c,
	DISP_CMD_LAYER_GET_CHROMA_SHARP_LEVEL = 0x6d,
	DISP_CMD_LAYER_SET_WHITE_EXTEN_LEVEL = 0x6e,
	DISP_CMD_LAYER_GET_WHITE_EXTEN_LEVEL = 0x6f,
	DISP_CMD_LAYER_SET_BLACK_EXTEN_LEVEL = 0x70,
	DISP_CMD_LAYER_GET_BLACK_EXTEN_LEVEL = 0x71,

	/* ----scaler---- */
	DISP_CMD_SCALER_REQUEST = 0x80,
	DISP_CMD_SCALER_RELEASE = 0x81,
	DISP_CMD_SCALER_EXECUTE = 0x82,

	/* ----hwc---- */
	DISP_CMD_HWC_OPEN = 0xc0,
	DISP_CMD_HWC_CLOSE = 0xc1,
	DISP_CMD_HWC_SET_POS = 0xc2,
	DISP_CMD_HWC_GET_POS = 0xc3,
	DISP_CMD_HWC_SET_FB = 0xc4,
	DISP_CMD_HWC_SET_PALETTE_TABLE = 0xc5,

	/* ----video---- */
	DISP_CMD_VIDEO_START = 0x100,
	DISP_CMD_VIDEO_STOP = 0x101,
	DISP_CMD_VIDEO_SET_FB = 0x102,
	DISP_CMD_VIDEO_GET_FRAME_ID = 0x103,
	DISP_CMD_VIDEO_GET_DIT_INFO = 0x104,

	/* ----lcd---- */
	DISP_CMD_LCD_ON = 0x140,
	DISP_CMD_LCD_OFF = 0x141,
	DISP_CMD_LCD_SET_BRIGHTNESS = 0x142,
	DISP_CMD_LCD_GET_BRIGHTNESS = 0x143,
	DISP_CMD_LCD_CPUIF_XY_SWITCH = 0x146,
	DISP_CMD_LCD_CHECK_OPEN_FINISH = 0x14a,
	DISP_CMD_LCD_CHECK_CLOSE_FINISH = 0x14b,
	DISP_CMD_LCD_SET_SRC = 0x14c,
	DISP_CMD_LCD_USER_DEFINED_FUNC = 0x14d,

	/* ----tv---- */
	DISP_CMD_TV_ON = 0x180,
	DISP_CMD_TV_OFF = 0x181,
	DISP_CMD_TV_SET_MODE = 0x182,
	DISP_CMD_TV_GET_MODE = 0x183,
	DISP_CMD_TV_AUTOCHECK_ON = 0x184,
	DISP_CMD_TV_AUTOCHECK_OFF = 0x185,
	DISP_CMD_TV_GET_INTERFACE = 0x186,
	DISP_CMD_TV_SET_SRC = 0x187,
	DISP_CMD_TV_GET_DAC_STATUS = 0x188,
	DISP_CMD_TV_SET_DAC_SOURCE = 0x189,
	DISP_CMD_TV_GET_DAC_SOURCE = 0x18a,

	/* ----hdmi---- */
	DISP_CMD_HDMI_ON = 0x1c0,
	DISP_CMD_HDMI_OFF = 0x1c1,
	DISP_CMD_HDMI_SET_MODE = 0x1c2,
	DISP_CMD_HDMI_GET_MODE = 0x1c3,
	DISP_CMD_HDMI_SUPPORT_MODE = 0x1c4,
	DISP_CMD_HDMI_GET_HPD_STATUS = 0x1c5,
	DISP_CMD_HDMI_SET_SRC = 0x1c6,

	/* ----vga---- */
	DISP_CMD_VGA_ON = 0x200,
	DISP_CMD_VGA_OFF = 0x201,
	DISP_CMD_VGA_SET_MODE = 0x202,
	DISP_CMD_VGA_GET_MODE = 0x203,
	DISP_CMD_VGA_SET_SRC = 0x204,

	/* ----sprite---- */
	DISP_CMD_SPRITE_OPEN = 0x240,
	DISP_CMD_SPRITE_CLOSE = 0x241,
	DISP_CMD_SPRITE_SET_FORMAT = 0x242,
	DISP_CMD_SPRITE_GLOBAL_ALPHA_ENABLE = 0x243,
	DISP_CMD_SPRITE_GLOBAL_ALPHA_DISABLE = 0x244,
	DISP_CMD_SPRITE_GET_GLOBAL_ALPHA_ENABLE = 0x252,
	DISP_CMD_SPRITE_SET_GLOBAL_ALPHA_VALUE = 0x245,
	DISP_CMD_SPRITE_GET_GLOBAL_ALPHA_VALUE = 0x253,
	DISP_CMD_SPRITE_SET_ORDER = 0x246,
	DISP_CMD_SPRITE_GET_TOP_BLOCK = 0x250,
	DISP_CMD_SPRITE_GET_BOTTOM_BLOCK = 0x251,
	DISP_CMD_SPRITE_SET_PALETTE_TBL = 0x247,
	DISP_CMD_SPRITE_GET_BLOCK_NUM = 0x259,
	DISP_CMD_SPRITE_BLOCK_REQUEST = 0x248,
	DISP_CMD_SPRITE_BLOCK_RELEASE = 0x249,
	DISP_CMD_SPRITE_BLOCK_OPEN = 0x257,
	DISP_CMD_SPRITE_BLOCK_CLOSE = 0x258,
	DISP_CMD_SPRITE_BLOCK_SET_SOURCE_WINDOW = 0x25a,
	DISP_CMD_SPRITE_BLOCK_GET_SOURCE_WINDOW = 0x25b,
	DISP_CMD_SPRITE_BLOCK_SET_SCREEN_WINDOW = 0x24a,
	DISP_CMD_SPRITE_BLOCK_GET_SCREEN_WINDOW = 0x24c,
	DISP_CMD_SPRITE_BLOCK_SET_FB = 0x24b,
	DISP_CMD_SPRITE_BLOCK_GET_FB = 0x24d,
	DISP_CMD_SPRITE_BLOCK_SET_PARA = 0x25c,
	DISP_CMD_SPRITE_BLOCK_GET_PARA = 0x25d,
	DISP_CMD_SPRITE_BLOCK_SET_TOP = 0x24e,
	DISP_CMD_SPRITE_BLOCK_SET_BOTTOM = 0x24f,
	DISP_CMD_SPRITE_BLOCK_GET_PREV_BLOCK = 0x254,
	DISP_CMD_SPRITE_BLOCK_GET_NEXT_BLOCK = 0x255,
	DISP_CMD_SPRITE_BLOCK_GET_PRIO = 0x256,

	/* ----framebuffer---- */
	DISP_CMD_FB_REQUEST = 0x280,
	DISP_CMD_FB_RELEASE = 0x281,
	DISP_CMD_FB_GET_PARA = 0x282,
	DISP_CMD_GET_DISP_INIT_PARA = 0x283,

	/* ---for Displayer Test -------- */
	DISP_CMD_MEM_REQUEST = 0x2c0,
	DISP_CMD_MEM_RELASE = 0x2c1,
	DISP_CMD_MEM_GETADR = 0x2c2,
	DISP_CMD_MEM_SELIDX = 0x2c3,

	DISP_CMD_SUSPEND = 0x2d0,
	DISP_CMD_RESUME = 0x2d1,

	DISP_CMD_PRINT_REG = 0x2e0,

	/* ---pwm -------- */
	DISP_CMD_PWM_SET_PARA = 0x300,
	DISP_CMD_PWM_GET_PARA = 0x301,
} __disp_cmd_t;

typedef enum {
	DISP_FORMAT_1BPP = 0x0,
	DISP_FORMAT_2BPP = 0x1,
	DISP_FORMAT_4BPP = 0x2,
	DISP_FORMAT_8BPP = 0x3,
	DISP_FORMAT_RGB655 = 0x4,
	DISP_FORMAT_RGB565 = 0x5,
	DISP_FORMAT_RGB556 = 0x6,
	DISP_FORMAT_ARGB1555 = 0x7,
	DISP_FORMAT_RGBA5551 = 0x8,
	DISP_FORMAT_ARGB888 = 0x9, /* alpha padding to 0xff */
	DISP_FORMAT_ARGB8888 = 0xa,
	DISP_FORMAT_RGB888 = 0xb,
	DISP_FORMAT_ARGB4444 = 0xc,

	DISP_FORMAT_YUV444 = 0x10,
	DISP_FORMAT_YUV422 = 0x11,
	DISP_FORMAT_YUV420 = 0x12,
	DISP_FORMAT_YUV411 = 0x13,
	DISP_FORMAT_CSIRGB = 0x14,
} __disp_pixel_fmt_t;

typedef enum {
	/* interleaved,1 address */
	DISP_MOD_INTERLEAVED = 0x1,
	/*
	 * No macroblock plane mode, 3 address, RGB/YUV each channel were stored
	 */
	DISP_MOD_NON_MB_PLANAR = 0x0,
	/* No macroblock UV packaged mode, 2 address, Y and UV were stored */
	DISP_MOD_NON_MB_UV_COMBINED = 0x2,
	/* Macroblock plane mode, 3 address,RGB/YUV each channel were stored */
	DISP_MOD_MB_PLANAR = 0x4,
	/* Macroblock UV packaged mode, 2 address, Y and UV were stored */
	DISP_MOD_MB_UV_COMBINED = 0x6,
} __disp_pixel_mod_t;

typedef enum {
	/* for interleave argb8888 */
	DISP_SEQ_ARGB = 0x0,	/* A at a high level */
	DISP_SEQ_BGRA = 0x2,

	/* for interleaved yuv422 */
	DISP_SEQ_UYVY = 0x3,
	DISP_SEQ_YUYV = 0x4,
	DISP_SEQ_VYUY = 0x5,
	DISP_SEQ_YVYU = 0x6,

	/* for interleaved yuv444 */
	DISP_SEQ_AYUV = 0x7,
	DISP_SEQ_VUYA = 0x8,

	/* for uv_combined yuv420 */
	DISP_SEQ_UVUV = 0x9,
	DISP_SEQ_VUVU = 0xa,

	/* for 16bpp rgb */
	DISP_SEQ_P10 = 0xd,	/* p1 high */
	DISP_SEQ_P01 = 0xe,	/* p0 high */

	/* for planar format or 8bpp rgb */
	DISP_SEQ_P3210 = 0xf,	/* p3 high */
	DISP_SEQ_P0123 = 0x10,	/* p0 high */

	/* for 4bpp rgb */
	DISP_SEQ_P76543210 = 0x11,
	DISP_SEQ_P67452301 = 0x12,
	DISP_SEQ_P10325476 = 0x13,
	DISP_SEQ_P01234567 = 0x14,

	/* for 2bpp rgb */
	/* 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 */
	DISP_SEQ_2BPP_BIG_BIG = 0x15,
	/* 12,13,14,15,8,9,10,11,4,5,6,7,0,1,2,3 */
	DISP_SEQ_2BPP_BIG_LITTER = 0x16,
	/* 3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12 */
	DISP_SEQ_2BPP_LITTER_BIG = 0x17,
	/* 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 */
	DISP_SEQ_2BPP_LITTER_LITTER = 0x18,

	/* for 1bpp rgb */
	/*
	 * 31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,
	 * 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
	 */
	DISP_SEQ_1BPP_BIG_BIG = 0x19,
	/*
	 * 24,25,26,27,28,29,30,31,16,17,18,19,20,21,22,23,
	 *  8, 9,10,11,12,13,14,15, 0, 1, 2, 3, 4, 5, 6, 7
	 */
	DISP_SEQ_1BPP_BIG_LITTER = 0x1a,
	/*
	 *  7, 6, 5, 4, 3, 2, 1, 0,15,14,13,12,11,10, 9, 8,
	 * 23,22,21,20,19,18,17,16,31,30,29,28,27,26,25,24
	 */
	DISP_SEQ_1BPP_LITTER_BIG = 0x1b,
	/*
	 *  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	 * 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	 */
	DISP_SEQ_1BPP_LITTER_LITTER = 0x1c,
} __disp_pixel_seq_t;

typedef enum {
	DISP_3D_SRC_MODE_TB = 0x0, /* top bottom */
	DISP_3D_SRC_MODE_FP = 0x1, /* frame packing */
	DISP_3D_SRC_MODE_SSF = 0x2, /* side by side full */
	DISP_3D_SRC_MODE_SSH = 0x3, /* side by side half */
	DISP_3D_SRC_MODE_LI = 0x4, /* line interleaved */
} __disp_3d_src_mode_t;

typedef enum {
	DISP_3D_OUT_MODE_CI_1 = 0x5, /* column interlaved 1 */
	DISP_3D_OUT_MODE_CI_2 = 0x6, /* column interlaved 2 */
	DISP_3D_OUT_MODE_CI_3 = 0x7, /* column interlaved 3 */
	DISP_3D_OUT_MODE_CI_4 = 0x8, /* column interlaved 4 */
	DISP_3D_OUT_MODE_LIRGB = 0x9, /* line interleaved rgb */

	DISP_3D_OUT_MODE_TB = 0x0, /* top bottom */
	DISP_3D_OUT_MODE_FP = 0x1, /* frame packing */
	DISP_3D_OUT_MODE_SSF = 0x2, /* side by side full */
	DISP_3D_OUT_MODE_SSH = 0x3, /* side by side half */
	DISP_3D_OUT_MODE_LI = 0x4, /* line interleaved */
	DISP_3D_OUT_MODE_FA = 0xa, /* field alternative */
} __disp_3d_out_mode_t;

typedef enum {
	DISP_BT601 = 0,
	DISP_BT709 = 1,
	DISP_YCC = 2,
	DISP_VXYCC = 3,
} __disp_cs_mode_t;

typedef enum {
	DISP_LAYER_WORK_MODE_NORMAL = 0, /* normal work mode */
	DISP_LAYER_WORK_MODE_PALETTE = 1, /* palette work mode */
	/* internal frame buffer work mode */
	DISP_LAYER_WORK_MODE_INTER_BUF = 2,
	DISP_LAYER_WORK_MODE_GAMMA = 3, /* gamma correction work mode */
	DISP_LAYER_WORK_MODE_SCALER = 4, /* scaler work mode */
} __disp_layer_work_mode_t;

typedef struct {
    int                 fd_fb;
    int                 fd_disp;
    int                 fd_g2d;
    int                 fb_id;             /* /dev/fb0 = 0, /dev/fb1 = 1 */

    int                 xres, yres, bits_per_pixel;
    uint8_t            *framebuffer_addr;  /* mmapped address */
    uintptr_t           framebuffer_paddr; /* physical address */
    uint32_t            framebuffer_size;  /* total size of the framebuffer */
    int                 framebuffer_height;/* virtual vertical resolution */
    uint32_t            gfx_layer_size;    /* the size of the primary layer */

    uint8_t            *xserver_fbmem; /* framebuffer mapping done by xserver */

    /* Hardware cursor support */
    int                 cursor_enabled;
    int                 cursor_x, cursor_y;

    /* Layers support */
    int                 gfx_layer_id;
    int                 layer_id;
    int                 layer_has_scaler;

    int                 layer_buf_x, layer_buf_y, layer_buf_w, layer_buf_h;
    int                 layer_win_x, layer_win_y;
    int                 layer_scaler_is_enabled;
    int                 layer_format;

    /* G2D accelerated implementation of blt2d_i interface */
    //blt2d_i             blt2d;
    /* Optional fallback interface to handle unsupported operations */
    //blt2d_i            *fallback_blt2d;
} sunxi_disp_t;

typedef struct {
	/*
	 * The way these are treated today, these are physical addresses. Are
	 * there any actual userspace applications out there that use this?
	 * -- libv.
	 */
	/*
	 * the contents of the frame buffer address for rgb type only addr[0]
	 * valid
	 */
	__u32 addr[3];
	__disp_rectsz_t size; /* unit is pixel */
	__disp_pixel_fmt_t format;
	__disp_pixel_seq_t seq;
	__disp_pixel_mod_t mode;
	/*
	 * blue red color swap flag, FALSE:RGB; TRUE:BGR,only used in rgb format
	 */
	__bool br_swap;
	__disp_cs_mode_t cs_mode; /* color space */
	__bool b_trd_src; /* if 3d source, used for scaler mode layer */
	/* source 3d mode, used for scaler mode layer */
	__disp_3d_src_mode_t trd_mode;
	__u32 trd_right_addr[3]; /* used when in frame packing 3d mode */
} __disp_fb_t;

typedef struct {
	__disp_layer_work_mode_t mode; /* layer work mode */
	__bool b_from_screen;
	 /*
	  * layer pipe,0/1,if in scaler mode, scaler0 must be pipe0,
	  * scaler1 must be pipe1
	  */
	__u8 pipe;
	/*
	 * layer priority,can get layer prio,but never set layer prio.
	 * From bottom to top, priority from low to high
	 */
	__u8 prio;
	__bool alpha_en; /* layer global alpha enable */
	__u16 alpha_val; /* layer global alpha value */
	__bool ck_enable; /* layer color key enable */
	/*  framebuffer source window,only care x,y if is not scaler mode */
	__disp_rect_t src_win;
	__disp_rect_t scn_win; /* screen window */
	__disp_fb_t fb; /* framebuffer */
	__bool b_trd_out; /* if output 3d mode, used for scaler mode layer */
	/* output 3d mode, used for scaler mode layer */
	__disp_3d_out_mode_t out_trd_mode;
} __disp_layer_info_t;

int sunxi_hw_cursor_hide(sunxi_disp_t *ctx)
{
    int result;
    uint32_t tmp[4];
    tmp[0] = ctx->fb_id;
    result = ioctl(ctx->fd_disp, DISP_CMD_HWC_CLOSE, &tmp);
    if (result >= 0)
        ctx->cursor_enabled = 0;
    return result;
}

/*****************************************************************************
 * Support for scaled layers                                                 *
 *****************************************************************************/

static int sunxi_layer_change_work_mode(sunxi_disp_t *ctx, int new_mode)
{
    __disp_layer_info_t layer_info;
    uint32_t tmp[4];

    if (ctx->layer_id < 0)
        return -1;

    tmp[0] = ctx->fb_id;
    tmp[1] = ctx->layer_id;
    tmp[2] = (uintptr_t)&layer_info;
    if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_GET_PARA, tmp) < 0)
        return -1;

    layer_info.mode = new_mode;

    tmp[0] = ctx->fb_id;
    tmp[1] = ctx->layer_id;
    tmp[2] = (uintptr_t)&layer_info;
    return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_PARA, tmp);
}

int sunxi_layer_reserve(sunxi_disp_t *ctx)
{
    __disp_layer_info_t layer_info;
    uint32_t tmp[4];

    /* try to allocate a layer */

    tmp[0] = ctx->fb_id;
    tmp[1] = DISP_LAYER_WORK_MODE_NORMAL;
    ctx->layer_id = ioctl(ctx->fd_disp, DISP_CMD_LAYER_REQUEST, &tmp);
    if (ctx->layer_id < 0)
        return -1;

    /* Initially set the layer configuration to something reasonable */

    tmp[0] = ctx->fb_id;
    tmp[1] = ctx->layer_id;
    tmp[2] = (uintptr_t)&layer_info;
    if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_GET_PARA, tmp) < 0)
        return -1;

    /* the screen and overlay layers need to be in different pipes */
    layer_info.pipe      = 1;
    layer_info.alpha_en  = 1;
    layer_info.alpha_val = 255;

    layer_info.fb.addr[0] = ctx->framebuffer_paddr;
    layer_info.fb.size.width = 1;
    layer_info.fb.size.height = 1;
    layer_info.fb.format = DISP_FORMAT_ARGB8888;
    layer_info.fb.seq = DISP_SEQ_ARGB;
    layer_info.fb.mode = DISP_MOD_INTERLEAVED;

    tmp[0] = ctx->fb_id;
    tmp[1] = ctx->layer_id;
    tmp[2] = (uintptr_t)&layer_info;
    if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_PARA, tmp) < 0)
        return -1;

    /* Now probe the scaler mode to see if there is a free scaler available */
    if (sunxi_layer_change_work_mode(ctx, DISP_LAYER_WORK_MODE_SCALER) == 0)
        ctx->layer_has_scaler = 1;

    /* Revert back to normal mode */
    sunxi_layer_change_work_mode(ctx, DISP_LAYER_WORK_MODE_NORMAL);
    ctx->layer_scaler_is_enabled = 0;
    ctx->layer_format = DISP_FORMAT_ARGB8888;

    return ctx->layer_id;
}

int sunxi_layer_set_output_window(sunxi_disp_t *ctx, int x, int y, int w, int h)
{
    __disp_rect_t buf_rect = {
        ctx->layer_buf_x, ctx->layer_buf_y,
        ctx->layer_buf_w, ctx->layer_buf_h
    };
    __disp_rect_t win_rect = { x, y, w, h };
    uint32_t tmp[4];
    int err;

    if (ctx->layer_id < 0 || w <= 0 || h <= 0)
        return -1;

    /*
     * Handle negative window Y coordinates (workaround a bug).
     * The Allwinner A10/A13 display controller hardware is expected to
     * support negative coordinates of the top left corners of the layers.
     * But there is some bug either in the kernel driver or in the hardware,
     * which messes up the picture on screen when the Y coordinate is negative
     * for YUV layer. Negative X coordinates are not affected. RGB formats
     * are not affected too.
     *
     * We fix this by just recalculating which part of the buffer in memory
     * corresponds to Y=0 on screen and adjust the input buffer settings.
     */
    if (ctx->layer_format == DISP_FORMAT_YUV420 &&
                                  (y < 0 || ctx->layer_win_y < 0)) {
        if (win_rect.y < 0) {
            int y_shift = -(double)y * buf_rect.height / win_rect.height;
            buf_rect.y      += y_shift;
            buf_rect.height -= y_shift;
            win_rect.height += win_rect.y;
            win_rect.y       = 0;
        }

        if (buf_rect.height <= 0 || win_rect.height <= 0) {
            /* No part of the window is visible. Just construct a fake rectangle
             * outside the screen as a window placement (but with a non-negative Y
             * coordinate). Do this to avoid passing bogus negative heights to
             * the kernel driver (who knows how it would react?) */
            win_rect.x = -1;
            win_rect.y = 0;
            win_rect.width = 1;
            win_rect.height = 1;
            tmp[0] = ctx->fb_id;
            tmp[1] = ctx->layer_id;
            tmp[2] = (uintptr_t)&win_rect;
            return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_SCN_WINDOW, &tmp);
        }

        tmp[0] = ctx->fb_id;
        tmp[1] = ctx->layer_id;
        tmp[2] = (uintptr_t)&buf_rect;
        if ((err = ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_SRC_WINDOW, &tmp)))
            return err;
    }
    /* Save the new non-adjusted window position */
    ctx->layer_win_x = x;
    ctx->layer_win_y = y;

    tmp[0] = ctx->fb_id;
    tmp[1] = ctx->layer_id;
    tmp[2] = (uintptr_t)&win_rect;
    return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_SCN_WINDOW, &tmp);
}

int sunxi_layer_show(sunxi_disp_t *ctx)
{
    uint32_t tmp[4];

    if (ctx->layer_id < 0)
        return -1;

    /* YUV formats need to use a scaler */
    if (ctx->layer_format == DISP_FORMAT_YUV420 && !ctx->layer_scaler_is_enabled) {
        if (sunxi_layer_change_work_mode(ctx, DISP_LAYER_WORK_MODE_SCALER) == 0)
            ctx->layer_scaler_is_enabled = 1;
    }

    tmp[0] = ctx->fb_id;
    tmp[1] = ctx->layer_id;
    return ioctl(ctx->fd_disp, DISP_CMD_LAYER_OPEN, &tmp);
}

int sunxi_layer_release(sunxi_disp_t *ctx)
{
    int result;
    uint32_t tmp[4];

    if (ctx->layer_id < 0)
        return -1;

    tmp[0] = ctx->fb_id;
    tmp[1] = ctx->layer_id;
    ioctl(ctx->fd_disp, DISP_CMD_LAYER_RELEASE, &tmp);

    ctx->layer_id = -1;
    ctx->layer_has_scaler = 0;
    return 0;
}


int sunxi_layer_set_rgb_input_buffer(sunxi_disp_t *ctx,
                                     int           bpp,
                                     uint32_t      offset_in_framebuffer,
                                     int           width,
                                     int           height,
                                     int           stride)
{
    __disp_fb_t fb;
    __disp_rect_t rect = { 0, 0, width, height };
    uint32_t tmp[4];
    memset(&fb, 0, sizeof(fb));

    if (ctx->layer_id < 0)
        return -1;

    if (!ctx->layer_scaler_is_enabled) {
        if (sunxi_layer_change_work_mode(ctx, DISP_LAYER_WORK_MODE_SCALER) == 0)
            ctx->layer_scaler_is_enabled = 1;
        else
            return -1;
    }

    fb.addr[0] = ctx->framebuffer_paddr + offset_in_framebuffer;
    fb.size.height = height;
    if (bpp == 32) {
        fb.format = DISP_FORMAT_ARGB8888;
        fb.seq = DISP_SEQ_ARGB;
        fb.mode = DISP_MOD_INTERLEAVED;
        fb.size.width = stride;
    } else if (bpp == 16) {
        fb.format = DISP_FORMAT_RGB565;
        fb.seq = DISP_SEQ_P10;
        fb.mode = DISP_MOD_INTERLEAVED;
        fb.size.width = stride * 2;
    } else {
        return -1;
    }

    tmp[0] = ctx->fb_id;
    tmp[1] = ctx->layer_id;
    tmp[2] = (uintptr_t)&fb;
    if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_FB, &tmp) < 0)
        return -1;

    ctx->layer_buf_x = rect.x;
    ctx->layer_buf_y = rect.y;
    ctx->layer_buf_w = rect.width;
    ctx->layer_buf_h = rect.height;
    ctx->layer_format = fb.format;

    tmp[0] = ctx->fb_id;
    tmp[1] = ctx->layer_id;
    tmp[2] = (uintptr_t)&rect;
    return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_SRC_WINDOW, &tmp);
}

sunxi_disp_t *sunxi_disp_init(const char *device, void *xserver_fbmem)
{
    sunxi_disp_t *ctx = calloc(sizeof(sunxi_disp_t), 1);
    struct fb_var_screeninfo fb_var;
    struct fb_fix_screeninfo fb_fix;

    int tmp, version;
    int gfx_layer_size;
    int ovl_layer_size;

    /* use /dev/fb0 by default */
    if (!device)
        device = "/dev/fb0";

    if (strcmp(device, "/dev/fb0") == 0) {
        ctx->fb_id = 0;
    }
    else if (strcmp(device, "/dev/fb1") == 0) {
        ctx->fb_id = 1;
    }
    else
    {
        free(ctx);
        return NULL;
    }

    /* store the already existing mapping done by xserver */
    ctx->xserver_fbmem = xserver_fbmem;

    ctx->fd_disp = open("/dev/disp", O_RDWR);

    /* maybe it's even not a sunxi hardware */
    if (ctx->fd_disp < 0) {
        free(ctx);
        return NULL;
    }

    /* version check */
    tmp = SUNXI_DISP_VERSION;
    version = ioctl(ctx->fd_disp, DISP_CMD_VERSION, &tmp);
    if (version < 0) {
        close(ctx->fd_disp);
        free(ctx);
        return NULL;
    }

    ctx->fd_fb = open(device, O_RDWR);
    if (ctx->fd_fb < 0) {
        close(ctx->fd_disp);
        free(ctx);
        return NULL;
    }

    if (ioctl(ctx->fd_fb, FBIOGET_VSCREENINFO, &fb_var) < 0 ||
        ioctl(ctx->fd_fb, FBIOGET_FSCREENINFO, &fb_fix) < 0)
    {
        close(ctx->fd_fb);
        close(ctx->fd_disp);
        free(ctx);
        return NULL;
    }

    ctx->xres = fb_var.xres;
    ctx->yres = fb_var.yres;
    ctx->bits_per_pixel = fb_var.bits_per_pixel;
    ctx->framebuffer_paddr = fb_fix.smem_start;
    ctx->framebuffer_size = fb_fix.smem_len;
    ctx->framebuffer_height = ctx->framebuffer_size /
                              (ctx->xres * ctx->bits_per_pixel / 8);
    ctx->gfx_layer_size = ctx->xres * ctx->yres * fb_var.bits_per_pixel / 8;

    if (ctx->framebuffer_size < ctx->gfx_layer_size) {
        close(ctx->fd_fb);
        close(ctx->fd_disp);
        free(ctx);
        return NULL;
    }

    if (ctx->xserver_fbmem) {
        /* use already existing mapping */
        ctx->framebuffer_addr = ctx->xserver_fbmem;
    }
    else {
        /* mmap framebuffer memory */
        ctx->framebuffer_addr = (uint8_t *)mmap(0, ctx->framebuffer_size,
                                                PROT_READ | PROT_WRITE,
                                                MAP_SHARED, ctx->fd_fb, 0);
        if (ctx->framebuffer_addr == MAP_FAILED) {
            close(ctx->fd_fb);
            close(ctx->fd_disp);
            free(ctx);
            return NULL;
        }
    }

    ctx->cursor_enabled = 0;
    ctx->cursor_x = -1;
    ctx->cursor_y = -1;

    /* Get the id of the screen layer */
    if (ioctl(ctx->fd_fb,
              ctx->fb_id == 0 ? FBIOGET_LAYER_HDL_0 : FBIOGET_LAYER_HDL_1,
              &ctx->gfx_layer_id))
    {
        close(ctx->fd_fb);
        close(ctx->fd_disp);
        free(ctx);
        return NULL;
    }

    if (sunxi_layer_reserve(ctx) < 0)
    {
        close(ctx->fd_fb);
        close(ctx->fd_disp);
        free(ctx);
        return NULL;
    }

    ctx->fd_g2d = open("/dev/g2d", O_RDWR);

    return ctx;
}

int sunxi_disp_close(sunxi_disp_t *ctx)
{
    if (ctx->fd_disp >= 0) {
        if (ctx->fd_g2d >= 0) {
            close(ctx->fd_g2d);
        }
        /* release layer */
        sunxi_layer_release(ctx);
        /* disable cursor */
        if (ctx->cursor_enabled)
            sunxi_hw_cursor_hide(ctx);
        /* close descriptors */
        if (!ctx->xserver_fbmem)
            munmap(ctx->framebuffer_addr, ctx->framebuffer_size);
        close(ctx->fd_fb);
        close(ctx->fd_disp);
        ctx->fd_disp = -1;
        free(ctx);
    }
    return 0;
}

int sunxi_wait_for_vsync(sunxi_disp_t *ctx)
{
    return ioctl(ctx->fd_fb, FBIO_WAITFORVSYNC, 0);
}

// END of lowlevel SunxiG2D functions block

void pixman_composite_src_0565_8888_asm_neon(int width,
	int height,
	uint32_t *dst,
	int dst_stride_pixels,
	uint16_t *src,
	int src_stride_pixels);

void pixman_composite_src_8888_8888_asm_neon(int width,
	int height,
	uint32_t *dst,
	int dst_stride_pixels,
	uint16_t *src,
	int src_stride_pixels);

// Pointer to the blitting function. Will be asigned when we find out what bpp the core uses.
void(*pixman_blit)();

extern void *memcpy_neon(void *dst, const void *src, size_t n);

static void *vsync_thread_func (void *arg);

pthread_t vsync_thread;

pthread_cond_t vsync_condition;	
pthread_mutex_t queue_mutex;
pthread_mutex_t vsync_cond_mutex;

sunxi_disp_t *disp;

struct sunxi_page {
	unsigned numpage;
	unsigned offset;
	unsigned yoffset;
	bool used;
	// Since each page has it's own used bool, it needs it's own mutex
	// to isolate write access to that bool.	
	pthread_mutex_t page_used_mutex;
	struct sunxi_page *next;
};

struct sunxi_video {
	void *font;
	const font_renderer_driver_t *font_driver;

	uint8_t font_rgb[4];

	/* current dimensions of the emulator fb */
	unsigned src_width;
	unsigned src_height;
	unsigned src_pitch;
	unsigned src_bpp;
	unsigned src_bytes_per_pixel;
	unsigned dst_pitch;
	unsigned visible_width;	
	unsigned bytes_per_pixel;	
	unsigned numpages;
	
	struct sunxi_page *pages;	
	struct sunxi_page *queue;	
	struct sunxi_page *current_page;	
	unsigned pageflip_pending;
	
	// Keep the vsync while loop going. Set to false to exit.
	bool keep_vsync;

	// Variables to restore screen on exit
	unsigned screensize;
	char *screen_bck;
	
	/* menu data */
	unsigned menu_rotation;
	bool menu_active;

	bool aspect_changed;
};

int sunxi_wait_flip()
{
    	// Wait for next vsync
	return ioctl(disp->fd_fb, FBIO_WAITFORVSYNC, 0);
}

void queue_page (struct sunxi_page *page, void *data) {
	struct sunxi_video *_dispvars = data;
	struct sunxi_page *ppage = _dispvars->queue;
	if (ppage == NULL)
		_dispvars->queue = page;
	else {
		while (ppage->next != NULL) {
			ppage = ppage->next;
		}
		ppage->next = page;
		page->next = NULL;
	}
}

struct sunxi_page *unqueue_page (void *data) {
	struct sunxi_video *_dispvars = data;
	struct sunxi_page *page;
	page = _dispvars->queue;	
	if (page != NULL) {
		_dispvars->queue = page->next;
		page->next = NULL;
		return page;
	}
	else return NULL;
}

/* Find a free page, clear it if necessary, and return the page. If  *
 * no free page is available when called, wait for a page flip.      */
static struct sunxi_page *sunxi_get_free_page(void *data)
{
	struct sunxi_video *_dispvars = data;
	struct sunxi_page *page = NULL;
	unsigned i;

	/* Wait until a free page is available. */
	while (page == NULL) {
		for (i = 0; i < _dispvars->numpages; ++i) {
			if (!_dispvars->pages[i].used){
				page = &_dispvars->pages[i];
				break;
			}
		}
		if (page == NULL) {
			pthread_mutex_lock (&vsync_cond_mutex);
			pthread_cond_wait (&vsync_condition, &vsync_cond_mutex);
			pthread_mutex_unlock (&vsync_cond_mutex);
		}
	}

	pthread_mutex_lock(&page->page_used_mutex);
	page->used = true;
	pthread_mutex_unlock(&page->page_used_mutex);
	return page;
}

void sunxi_blank_console (void *data) {
	struct sunxi_video *_dispvars = data;

	// Disable cursor blinking so it's not visible in RetroArch
	system("setterm -cursor off");

	// Figure out the size of the screen in bytes
    	_dispvars->screensize = disp->xres * disp->yres * disp->bits_per_pixel / 8;
	
	// Backup screen contents
	_dispvars->screen_bck = (char *) malloc (_dispvars->screensize * sizeof (char));
	memcpy ((char*)_dispvars->screen_bck, (char*)disp->framebuffer_addr, _dispvars->screensize);
	
	// Blank screen
	memset ((char*)(disp->framebuffer_addr), 0x00, _dispvars->screensize);
}

void sunxi_unblank_console (void *data) {
	struct sunxi_video *_dispvars = data;
		
	system("setterm -cursor on");
	//memcpy ((char*)disp->framebuffer_addr, (char*)_dispvars->screen_bck, _dispvars->screensize);
	free (_dispvars->screen_bck);
}

static void *sunxi_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data) {
	struct sunxi_video *_dispvars;
	int i;
	
	_dispvars = calloc(1, sizeof(struct sunxi_video));
	_dispvars->src_bytes_per_pixel = video->rgb32 ? 4 : 2;

	disp = sunxi_disp_init("/dev/fb0", NULL);

	// Blank text console and disable cursor blinking
	sunxi_blank_console(_dispvars);
	
	_dispvars->numpages = 2;
	_dispvars->pages = calloc (_dispvars->numpages, sizeof (struct sunxi_page));
	
	_dispvars->dst_pitch = disp->xres * disp->bits_per_pixel / 8;
	_dispvars->pageflip_pending = 0;
	_dispvars->current_page = NULL;
	_dispvars->queue = NULL;
	_dispvars->keep_vsync = true;

	_dispvars->src_bpp = video->rgb32 ? 32 : 16;	
	_dispvars->bytes_per_pixel = _dispvars->src_bpp / 8;

	switch (_dispvars->src_bpp){
		case 16:
			pixman_blit = pixman_composite_src_0565_8888_asm_neon;
			break;
		case 32:
			pixman_blit = pixman_composite_src_8888_8888_asm_neon;
			break;
		default:
			return NULL;
	}

	pthread_mutex_init(&queue_mutex, NULL);		
	pthread_mutex_init(&vsync_cond_mutex, NULL);		
	pthread_cond_init(&vsync_condition, NULL);
	
	for (i = 0; i < _dispvars->numpages; i++) {
		pthread_mutex_init(&_dispvars->pages[i].page_used_mutex, NULL);		
		_dispvars->pages[i].numpage = i;
		_dispvars->pages[i].next = NULL;
	}

	if (input && input_data) {
		*input = NULL;
	}
	
	// Launching vsync thread
	pthread_create(&vsync_thread, NULL, vsync_thread_func, _dispvars);

	return _dispvars;
}

static void *vsync_thread_func (void *arg) {
	struct sunxi_video *_dispvars = arg;
	struct sunxi_page *page;
	while (_dispvars->keep_vsync) {
		sunxi_wait_flip(disp);
				
		pthread_mutex_lock(&vsync_cond_mutex);
		pthread_cond_signal (&vsync_condition);
		pthread_mutex_unlock(&vsync_cond_mutex);
	
		pthread_mutex_lock(&queue_mutex);
		page = unqueue_page((void*)_dispvars);
		//_dispvars->pageflip_pending--;	
		pthread_mutex_unlock(&queue_mutex);
		
		// We mark as free the page that was visible until now.
		if (_dispvars->current_page != NULL) {
			pthread_mutex_lock (&_dispvars->current_page->page_used_mutex);
			_dispvars->current_page->used = false;
			pthread_mutex_unlock (&_dispvars->current_page->page_used_mutex);
		}
		// The page on which we just issued a flip becomes the visible one, with the only purpose that
		// we can mark it as free next time we get here.
		// This variable is only accessed from this same thread over and over, so
		// it doesn't need to be isolated.
		_dispvars->current_page = page;
	}	
	return 0;
}

static void sunxi_gfx_free(void *data) {
	struct sunxi_video *_dispvars = data;
	int i;

	// Stop the vsync thread	
	_dispvars->keep_vsync = false;
	pthread_join(vsync_thread, NULL);

	for (i = 0; i < _dispvars->numpages; i++)
	{
		pthread_mutex_destroy(&_dispvars->pages[i].page_used_mutex);		
	}
	pthread_mutex_destroy(&queue_mutex);		
	pthread_mutex_destroy(&vsync_cond_mutex);		
	pthread_cond_destroy(&vsync_condition);

	free(_dispvars->pages);	
	
	// Restore text console contents and reactivate cursor blinking	
	sunxi_unblank_console(_dispvars);
	sunxi_disp_close(disp);
	free(_dispvars);
}



void sunxi_blit_flip (struct sunxi_page *page, const void *frame, void *data) {
	struct sunxi_video *_dispvars = data;	
	
	pixman_blit(
		_dispvars->src_width,
		_dispvars->src_height,
		((uint32_t*) disp->framebuffer_addr + (disp->yres + page->yoffset) * _dispvars->dst_pitch/4),
		_dispvars->dst_pitch/4,
		(uint16_t*)frame,
		_dispvars->src_pitch/_dispvars->bytes_per_pixel
	);
     
	// We DO allow queue multiple page flips in this backend, that's why this is commented: 
	// since we have 2 pages, multiple flip issuing can not happen anyway: the game will have 
	// to wait the second time it completes a loop because one page is the one in the screen (used)
	// and the other is the one the game used in the previous loop.
	/*if (_dispvars->pageflip_pending > 0) {
		pthread_mutex_lock(&vsync_cond_mutex);
		pthread_cond_wait (&vsync_condition, &vsync_cond_mutex);
		pthread_mutex_unlock(&vsync_cond_mutex);
	}*/
	
	// Issue pageflip. Will flip on next vsync.
	sunxi_layer_set_rgb_input_buffer(disp, disp->bits_per_pixel, (disp->yres + page->yoffset) * disp->xres * 4, 
		_dispvars->src_width, _dispvars->src_height, disp->xres);
	
	pthread_mutex_lock(&queue_mutex);
	queue_page(page, (void *)_dispvars);
	//_dispvars->pageflip_pending++;	
	pthread_mutex_unlock(&queue_mutex);
}

static bool sunxi_gfx_frame(void *data, const void *frame, unsigned width,
                             unsigned height, unsigned pitch, const char *msg) {
  
	struct sunxi_video *_dispvars = data;

	if (_dispvars->src_width != width || _dispvars->src_height != height) {
		// Sanity check on new dimensions
      		if (width == 0 || height == 0) return true;
		RARCH_LOG("video_sunxi: internal resolution changed by core: %ux%u -> %ux%u\n",
                	_dispvars->src_width, _dispvars->src_height, width, height);

		_dispvars->src_width  = width;
		_dispvars->src_height = height;
		// Total pitch, including things the cores render between "visible" scanlines
		_dispvars->src_pitch  = pitch;	
	
		// Incremental offset that sums up on each previous page offset.
		// Total offset of each page has to be adjusted when internal resolution changes.
		unsigned inc_yoffset = _dispvars->src_height;
		int i;
		for (i = 0; i < _dispvars->numpages; i++)
			_dispvars->pages[i].yoffset = i * inc_yoffset;

		float aspect;
        	switch (g_settings.video.aspect_ratio_idx){
        		case ASPECT_RATIO_4_3:
                		aspect = (float)4 / (float)3;
                		break;
        		case ASPECT_RATIO_16_9:
                		aspect = (float)16 / (float)9;
                		break;
        		case ASPECT_RATIO_16_10: 
                		aspect = (float)16 / (float)10;
                		break;
        		case ASPECT_RATIO_16_15: 
                		aspect = (float)16 / (float)15;
                		break;
        		case ASPECT_RATIO_CORE: 
                		aspect = (float)_dispvars->src_width / (float)_dispvars->src_height;
                		break;
        		default: 
                		aspect = (float)_dispvars->src_width / (float)_dispvars->src_height;
                		break;
        	}        

		unsigned visible_width = disp->yres * aspect;
		unsigned xpos = (disp->xres - visible_width) / 2; 
		
		// setup layer window
		sunxi_layer_set_output_window(disp, xpos, 0, visible_width, disp->yres);
	
		// make the layer visible
		sunxi_layer_show(disp);
	}

	struct sunxi_page *page = NULL;
	page = sunxi_get_free_page((void*)_dispvars);
	sunxi_blit_flip(page, frame, (void *)_dispvars);

  	return true;
}

static void sunxi_gfx_set_nonblock_state(void *data, bool state) {
  struct sunxi_video *vid = data;

  //vid->data->sync = !state;
}

static bool sunxi_gfx_alive(void *data) {
  (void)data;
  return true; /* always alive */
}

static bool sunxi_gfx_focus(void *data) {
  (void)data;
  return true; /* fb device always has focus */
}

static void sunxi_gfx_set_rotation(void *data, unsigned rotation) {
  (void)data;
}

static bool sunxi_gfx_has_windowed(void *data)
{
   (void)data;

   return false;
}

static bool sunxi_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static void sunxi_gfx_viewport_info(void *data, struct video_viewport *vp) {
  struct sunxi_video *_dispvars = data;

  vp->x = vp->y = 0;

  vp->width  = vp->full_width  = _dispvars->src_width;
  vp->height = vp->full_height = _dispvars->src_height;
}

static bool sunxi_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false; 
}

static const video_poke_interface_t sunxi_poke_interface = {
  NULL, /* set_video_mode */
  NULL, /* set_filtering */
  NULL, /* get_video_output_size */
  NULL, /* get_video_output_prev */
  NULL, /* get_video_output_next */
#ifdef HAVE_FBO
  NULL, /* get_current_framebuffer */
#endif
  NULL, /* get_proc_address */
  //sunxi_set_aspect_ratio,
  //sunxi_apply_state_changes,
#ifdef HAVE_MENU
  //sunxi_set_texture_frame,
  //sunxi_set_texture_enable,
#endif
  //sunxi_set_osd_msg,
  //sunxi_show_mouse
};

static void sunxi_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &sunxi_poke_interface;
}

video_driver_t video_sunxi = {
  sunxi_gfx_init,
  sunxi_gfx_frame,
  sunxi_gfx_set_nonblock_state,
  sunxi_gfx_alive,
  sunxi_gfx_focus,
  sunxi_gfx_suppress_screensaver,
  sunxi_gfx_has_windowed,
  sunxi_gfx_set_shader,
  sunxi_gfx_free,
  "sunxi",
  sunxi_gfx_set_rotation,
  sunxi_gfx_viewport_info,
  NULL, /* read_viewport */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  sunxi_gfx_get_poke_interface
};
