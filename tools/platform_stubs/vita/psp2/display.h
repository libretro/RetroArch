/* Compile-only Vita stub for the matrix's gxm video lane, carrying
 * the display calls and formats that driver names. Each
 * declaration is the shape the real header gives it. */
#ifndef STUB_PSP2_DISPLAY
#define STUB_PSP2_DISPLAY
#include <psp2/types.h>

typedef enum SceDisplayErrorCode {
   SCE_DISPLAY_ERROR_INVALID_RESOLUTION    = 0x80290005
} SceDisplayErrorCode;

typedef struct SceDisplayFrameBuf {
	SceSize size;
	void *base;
	unsigned int pitch;
	unsigned int pixelformat;
	unsigned int width;
	unsigned int height;
} SceDisplayFrameBuf;

typedef enum SceDisplayPixelFormat {
   SCE_DISPLAY_PIXELFORMAT_A8B8G8R8    = 0x00000000U
} SceDisplayPixelFormat;

typedef enum SceDisplaySetBufSync {
   SCE_DISPLAY_SETBUF_NEXTFRAME = 1
} SceDisplaySetBufSync;

int sceDisplayWaitVblankStart(void);
int sceDisplaySetFrameBuf(const SceDisplayFrameBuf *pParam, SceDisplaySetBufSync sync);

#endif
