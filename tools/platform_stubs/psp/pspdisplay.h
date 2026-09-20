/* Hermetic compile-only stand-in: only the names psp1_gfx.c uses. */
#ifndef STUB_psp_pspdisplay_h
#define STUB_psp_pspdisplay_h

#define PSP_DISPLAY_PIXEL_FORMAT_565  0
#define PSP_DISPLAY_PIXEL_FORMAT_5551 1
#define PSP_DISPLAY_PIXEL_FORMAT_4444 2
#define PSP_DISPLAY_PIXEL_FORMAT_8888 3

#define PSP_DISPLAY_SETBUF_IMMEDIATE  0
#define PSP_DISPLAY_SETBUF_NEXTFRAME  1

int sceDisplayWaitVblankStart(void);
int sceDisplaySetFrameBuf(void *topaddr, int bufferwidth,
      int pixelformat, int sync);
int sceDisplayGetFrameBuf(void **topaddr, int *bufferwidth,
      int *pixelformat, int sync);

#endif
