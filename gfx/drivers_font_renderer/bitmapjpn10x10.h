/* 이 폰트는 PixelMPlus 입니다.
 * 입수 경로:http://itouhiro.hatenablog.com/entry/20130602/font
 * 생성일 : 2020.11.10
 * These fonts are free software.
 * Unlimited permission is granted to use, copy, and distribute them, with
 * or without modification, either commercially or noncommercially.
 * THESE FONTS ARE PROVIDED "AS IS" WITHOUT WARRANTY.
 * http://mplus-fonts.sourceforge.jp/mplus-outline-fonts/
 */

#ifndef __RARCH_FONT_BITMAPJPN_H
#define __RARCH_FONT_BITMAPJPN_H

#define FONT_JPN_WIDTH 10
#define FONT_JPN_HEIGHT 10
/* FONT_HEIGHT_BASELINE_OFFSET:
 * Distance in pixels from top of character
 * to baseline */
#define FONT_JPN_HEIGHT_BASELINE_OFFSET 8
#define FONT_JPN_WIDTH_STRIDE (FONT_JPN_WIDTH + 1)
#define FONT_JPN_HEIGHT_STRIDE (FONT_JPN_HEIGHT + 1)

#define FONT_JPN_OFFSET(x) ((x) * ((FONT_JPN_HEIGHT * FONT_JPN_WIDTH + 7) / 8))

static unsigned char *rgui_bitmap_jpn_bin = NULL;

#endif
