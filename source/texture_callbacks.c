/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * texture_callbacks.c:
 * Implementation for texture data reading/writing callbacks
 */

#include <stdlib.h>
#include <vitasdk.h>

#include "vitaGL.h"
#include "texture_callbacks.h"

#define convert_u16_to_u32_cspace(color, lshift, rshift, mask) ((((color << lshift) >> rshift) & mask) * 0xFF) / mask

// Read callback for 32bpp unsigned RGBA format
uint32_t readRGBA(void *data) {
	uint32_t res;
	memcpy_neon(&res, data, 4);
	return res;
}

// Read callback for 16bpp unsigned RGBA5551 format
uint32_t readRGBA5551(void *data) {
	uint16_t clr;
	uint32_t r, g, b, a;
	memcpy_neon(&clr, data, 2);
	r = convert_u16_to_u32_cspace(clr,  0, 11, 0x1F);
	g = convert_u16_to_u32_cspace(clr,  5, 11, 0x1F);
	b = convert_u16_to_u32_cspace(clr, 10, 11, 0x1F);
	a = convert_u16_to_u32_cspace(clr, 15, 15, 0x01);
	return ((a << 24) | (b << 16) | (g << 8) | r);
}

// Read callback for 16bpp unsigned RGBA4444 format
uint32_t readRGBA4444(void *data) {
	uint16_t clr;
	uint32_t r, g, b, a;
	memcpy_neon(&clr, data, 2);
	r = convert_u16_to_u32_cspace(clr,  0, 12, 0x0F);
	g = convert_u16_to_u32_cspace(clr,  4, 12, 0x0F);
	b = convert_u16_to_u32_cspace(clr,  8, 12, 0x0F);
	a = convert_u16_to_u32_cspace(clr, 12, 12, 0x0F);
	return ((a << 24) | (b << 16) | (g << 8) | r);
}

// Read callback for 16bpp unsigned RGB565 format
uint32_t readRGB565(void *data) {
	uint16_t clr;
	uint32_t r, g, b;
	memcpy_neon(&clr, data, 2);
	r = convert_u16_to_u32_cspace(clr,   0, 11, 0x1F);
	g = convert_u16_to_u32_cspace(clr,   5, 11, 0x3F);
	b = convert_u16_to_u32_cspace(clr,  11, 11, 0x1F);
	return ((0xFF << 24) | (b << 16) | (g << 8) | r);
}

// Read callback for 24bpp unsigned RGB format
uint32_t readRGB(void *data) {
	uint32_t res = 0xFFFFFFFF;
	memcpy_neon(&res, data, 3);
	return res;
}

// Read callback for 16bpp unsigned RG format
uint32_t readRG(void *data) {
	uint32_t res = 0xFFFFFFFF;
	memcpy_neon(&res, data, 2);
	return res;
}

// Read callback for 8bpp unsigned R format
uint32_t readR(void *data) {
	uint32_t res = 0xFFFFFFFF;
	memcpy_neon(&res, data, 1);
	return res;
}

// Write callback for 32bpp unsigned RGBA format
void writeRGBA(void *data, uint32_t color) {
	memcpy_neon(data, &color, 4);
}

// Write callback for 24bpp unsigned RGB format
void writeRGB(void *data, uint32_t color) {
	memcpy_neon(data, &color, 3);
}

// Write callback for 16bpp unsigned RG format
void writeRG(void *data, uint32_t color) {
	memcpy_neon(data, &color, 2);
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
	memcpy_neon(data, &color, 1);
}