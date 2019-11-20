/* 
 * texture_callbacks.c:
 * Implementation for texture data reading/writing callbacks
 */

#include <stdlib.h>
#include <vitasdk.h>

#include "texture_callbacks.h"

// Read callback for 32bpp unsigned RGBA format
uint32_t readRGBA(void *data) {
	uint32_t res;
	memcpy(&res, data, 4);
	return res;
}

// Read callback for 16bpp unsigned RGBA5551 format
uint32_t readRGBA5551(void *data) {
	uint16_t clr;
	uint32_t r, g, b, a;
	memcpy(&clr, data, 2);
	r = (((clr >> 11) & 0x1F) * 0xFF) / 0x1F;
	g = ((((clr << 5) >> 11) & 0x1F) * 0xFF) / 0x1F;
	b = ((((clr << 10) >> 11) & 0x1F) * 0xFF) / 0x1F;
	a = (((clr << 15) >> 15) & 0x1) == 1 ? 0xFF : 0x00;
	return ((a << 24) | (b << 16) | (g << 8) | r);
}

// Read callback for 24bpp unsigned RGB format
uint32_t readRGB(void *data) {
	uint32_t res = 0xFFFFFFFF;
	memcpy(&res, data, 3);
	return res;
}

// Read callback for 16bpp unsigned RG format
uint32_t readRG(void *data) {
	uint32_t res = 0xFFFFFFFF;
	memcpy(&res, data, 2);
	return res;
}

// Read callback for 8bpp unsigned R format
uint32_t readR(void *data) {
	uint32_t res = 0xFFFFFFFF;
	memcpy(&res, data, 1);
	return res;
}

// Write callback for 32bpp unsigned RGBA format
void writeRGBA(void *data, uint32_t color) {
	memcpy(data, &color, 4);
}

// Write callback for 24bpp unsigned RGB format
void writeRGB(void *data, uint32_t color) {
	memcpy(data, &color, 3);
}

// Write callback for 16bpp unsigned RG format
void writeRG(void *data, uint32_t color) {
	memcpy(data, &color, 2);
}

// Write callback for 16bpp unsigned RA format
void writeRA(void *data, uint32_t color) {
	uint8_t *dst = (uint8_t *)data;
	uint8_t *src = (uint8_t *)&color;
	dst[0] = src[0];
	dst[1] = src[3];
}

// Write callback for 8bpp unsigned R format
void writeR(void *data, uint32_t color) {
	memcpy(data, &color, 1);
}