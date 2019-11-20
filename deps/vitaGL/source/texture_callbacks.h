/* 
 * texture_callbacks.h:
 * Header file for texture data reading/writing callbacks exposed by texture_callbacks.c
 */

#ifndef _TEXTURE_CALLBACKS_H_
#define _TEXTURE_CALLBACKS_H_

// Read callbacks
uint32_t readR(void *data);
uint32_t readRG(void *data);
uint32_t readRGB(void *data);
uint32_t readRGBA(void *data);
uint32_t readRGBA5551(void *data);

// Write callbacks
void writeR(void *data, uint32_t color);
void writeRG(void *data, uint32_t color);
void writeRA(void *data, uint32_t color);
void writeRGB(void *data, uint32_t color);
void writeRGBA(void *data, uint32_t color);

#endif
