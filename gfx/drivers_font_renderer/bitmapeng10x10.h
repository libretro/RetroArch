/*  
 * 폰트명 : 갈무리(galmuri)
 * 입수 경로 : https://tbh.kr/galmuri
 * 갈무리는 SIL 오픈 폰트 라이선스 1.1에 따라 이용할 수 있으며 그 자체로 판매되지 않는 한 자유롭게 사용, 연구, 수정, 재배포할 수 있습니다.
 */

#ifndef __RARCH_FONT_BITMAPENG10X10_H
#define __RARCH_FONT_BITMAPENG10X10_H

#define FONT_ENG_WIDTH 10
#define FONT_ENG_HEIGHT 10
/* FONT_HEIGHT_BASELINE_OFFSET:
 * Distance in pixels from top of character
 * to baseline */
#define FONT_ENG_HEIGHT_BASELINE_OFFSET 8
#define FONT_ENG_WIDTH_STRIDE (FONT_ENG_WIDTH + 1)
#define FONT_ENG_HEIGHT_STRIDE (FONT_ENG_HEIGHT + 1)

#define FONT_ENG_OFFSET(x) ((x) * ((FONT_ENG_HEIGHT * FONT_ENG_WIDTH + 7) / 8))

static unsigned char *rgui_bitmap_eng_bin = NULL;

#endif
