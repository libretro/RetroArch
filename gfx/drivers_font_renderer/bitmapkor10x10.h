/*  
 * 폰트명 : 갈무리(galmuri)
 * 입수 경로 : https://tbh.kr/galmuri
 * 갈무리는 SIL 오픈 폰트 라이선스 1.1에 따라 이용할 수 있으며 그 자체로 판매되지 않는 한 자유롭게 사용, 연구, 수정, 재배포할 수 있습니다.
 */

#ifndef __RARCH_FONT_BITMAPKOR10X10_H
#define __RARCH_FONT_BITMAPKOR10X10_H

#define FONT_KOR_WIDTH 10
#define FONT_KOR_HEIGHT 10
/* FONT_HEIGHT_BASELINE_OFFSET:
 * Distance in pixels from top of character
 * to baseline */
#define FONT_KOR_HEIGHT_BASELINE_OFFSET 8
#define FONT_KOR_WIDTH_STRIDE (FONT_KOR_WIDTH + 1)
#define FONT_KOR_HEIGHT_STRIDE (FONT_KOR_HEIGHT + 1)

#define FONT_KOR_OFFSET(x) ((x) * ((FONT_KOR_HEIGHT * FONT_KOR_WIDTH + 7) / 8))

static unsigned char *rgui_bitmap_kor_bin = NULL;

#endif
