#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OSScreenID
{
   SCREEN_TV      = 0,
   SCREEN_DRC     = 1,
} OSScreenID;

void OSScreenInit();
uint32_t OSScreenGetBufferSizeEx(OSScreenID screen);
void OSScreenSetBufferEx(OSScreenID screen, void *addr);
void OSScreenClearBufferEx(OSScreenID screen, uint32_t colour);
void OSScreenFlipBuffersEx(OSScreenID screen);
void OSScreenPutFontEx(OSScreenID screen, uint32_t row, uint32_t column, const char *buffer);
void OSScreenPutPixelEx(OSScreenID screen, uint32_t x, uint32_t y, uint32_t colour);
void OSScreenEnableEx(OSScreenID screen, BOOL enable);

#ifdef __cplusplus
}
#endif
