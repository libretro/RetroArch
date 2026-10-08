/* Compile-only stand-in for PSL1GHT's <rsx/gcm_sys.h>: the GCM
 * structures, values and calls gfx/drivers/rsx_gfx.c uses, in
 * PSL1GHT's layouts and values. Declare what a file needs as one
 * arrives. Not a runtime shim. */
#ifndef PS3STUB_RSX_GCM_SYS_H
#define PS3STUB_RSX_GCM_SYS_H

#include <ppu-types.h>

#define GCM_ALWAYS                               0x0207
#define GCM_CLEAR_A                              0x80
#define GCM_CLEAR_B                              0x40
#define GCM_CLEAR_G                              0x20
#define GCM_CLEAR_R                              0x10
#define GCM_CLEAR_S                              0x02
#define GCM_CLEAR_Z                              0x01
#define GCM_COLOR_MASK_A                         0x01000000
#define GCM_COLOR_MASK_B                         0x00000001
#define GCM_COLOR_MASK_G                         0x00000100
#define GCM_COLOR_MASK_R                         0x00010000
#define GCM_FALSE                                0
#define GCM_FLIP_VSYNC                           2
#define GCM_FUNC_ADD                             0x8006
#define GCM_INVALIDATE_TEXTURE                   1
#define GCM_LESS                                 0x0201
#define GCM_LOCATION_RSX                         0
#define GCM_MAX_MRT_COUNT                        4
#define GCM_ONE_MINUS_SRC_ALPHA                  0x0303
#define GCM_SRC_ALPHA                            0x0302
#define GCM_SURFACE_CENTER_1                     0
#define GCM_SURFACE_TARGET_0                     1
#define GCM_SURFACE_TYPE_LINEAR                  1
#define GCM_SURFACE_X8R8G8B8                     5
#define GCM_SURFACE_ZETA_Z24S8                   2
#define GCM_TEXTURE_CLAMP_TO_EDGE                3
#define GCM_TEXTURE_CONVOLUTION_QUINCUNX         1
#define GCM_TEXTURE_DIMS_2D                      2
#define GCM_TEXTURE_FORMAT_A4R4G4B4              3
#define GCM_TEXTURE_FORMAT_A8R8G8B8              5
#define GCM_TEXTURE_FORMAT_B8                    1
#define GCM_TEXTURE_FORMAT_DXT1                  6
#define GCM_TEXTURE_FORMAT_DXT23                 7
#define GCM_TEXTURE_FORMAT_DXT45                 8
#define GCM_TEXTURE_FORMAT_LIN                   0x20
#define GCM_TEXTURE_FORMAT_R5G6B5                4
#define GCM_TEXTURE_LINEAR                       2
#define GCM_TEXTURE_MAX_ANISO_1                  0
#define GCM_TEXTURE_MIRRORED_REPEAT              2
#define GCM_TEXTURE_NEAREST                      1
#define GCM_TEXTURE_REMAP_COLOR_A                0
#define GCM_TEXTURE_REMAP_COLOR_A_SHIFT          0
#define GCM_TEXTURE_REMAP_COLOR_B                3
#define GCM_TEXTURE_REMAP_COLOR_B_SHIFT          6
#define GCM_TEXTURE_REMAP_COLOR_G                2
#define GCM_TEXTURE_REMAP_COLOR_G_SHIFT          4
#define GCM_TEXTURE_REMAP_COLOR_R                1
#define GCM_TEXTURE_REMAP_COLOR_R_SHIFT          2
#define GCM_TEXTURE_REMAP_TYPE_A_SHIFT           8
#define GCM_TEXTURE_REMAP_TYPE_B_SHIFT           14
#define GCM_TEXTURE_REMAP_TYPE_G_SHIFT           12
#define GCM_TEXTURE_REMAP_TYPE_REMAP             2
#define GCM_TEXTURE_REMAP_TYPE_R_SHIFT           10
#define GCM_TEXTURE_REPEAT                       1
#define GCM_TEXTURE_ZFUNC_LESS                   1
#define GCM_TRUE                                 1
#define GCM_TYPE_TRIANGLES                       5
#define GCM_TYPE_TRIANGLE_STRIP                  6
#define GCM_USER_CLIP_PLANE_DISABLE              0
#define GCM_VERTEX_DATA_TYPE_F32                 2

typedef struct _gcmCtxData
{
   u32 *begin;
   u32 *end;
   u32 *current;
   void *callback;
} gcmContextData;

typedef struct _gcmSurface
{
   u8  type;
   u8  antiAlias;
   u8  colorFormat;
   u8  colorTarget;
   u8  colorLocation[GCM_MAX_MRT_COUNT];
   u32 colorOffset[GCM_MAX_MRT_COUNT];
   u32 colorPitch[GCM_MAX_MRT_COUNT];
   u8  depthFormat;
   u8  depthLocation;
   u8  _pad[2];
   u32 depthOffset;
   u32 depthPitch;
   u16 width;
   u16 height;
   u16 x;
   u16 y;
} gcmSurface;

typedef struct _gcmTexture
{
   u8  format;
   u8  mipmap;
   u8  dimension;
   u8  cubemap;
   u32 remap;
   u16 width;
   u16 height;
   u16 depth;
   u8  location;
   u8  _pad;
   u32 pitch;
   u32 offset;
} gcmTexture;

u32  gcmGetFlipStatus(void);
u32 *gcmGetLabelAddress(const u8 index);
void gcmResetFlipStatus(void);
s32  gcmSetDisplayBuffer(const u8 bufferId, const u32 offset,
      const u32 pitch, const u32 width, const u32 height);
s32  gcmSetFlip(gcmContextData *context, const u8 bufferId);
void gcmSetFlipMode(const u32 mode);
void gcmSetWaitFlip(gcmContextData *context);

#endif
