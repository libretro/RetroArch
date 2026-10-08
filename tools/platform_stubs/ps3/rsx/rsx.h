/* Compile-only stand-in for PSL1GHT's <rsx/rsx.h> (with the parts of
 * <rsx/mm.h>, <rsx/rsx_program.h> and <rsx/commands.h> it pulls in):
 * the RSX calls and program layouts gfx/drivers/rsx_gfx.c uses, in
 * PSL1GHT's signatures. Declare what a file needs as one arrives.
 * Not a runtime shim: a host test defines the calls. */
#ifndef PS3STUB_RSX_RSX_H
#define PS3STUB_RSX_RSX_H

#include <ppu-types.h>
#include <rsx/gcm_sys.h>

typedef struct rsx_vp
{
   u16 magic;
   u16 _pad0;
   u16 num_regs;
   u16 num_attr;
   u16 num_const;
   u16 num_insn;
   u32 attr_off;
   u32 const_off;
   u32 ucode_off;
   u32 input_mask;
   u32 output_mask;
   u16 const_start;
   u16 insn_start;
} rsxVertexProgram;

typedef struct rsx_fp
{
   u16 magic;
   u16 _pad0;
   u16 num_regs;
   u16 num_attr;
   u16 num_const;
   u16 num_insn;
   u32 attr_off;
   u32 const_off;
   u32 ucode_off;
   u32 fp_control;
   u16 texcoords;
   u16 texcoord2D;
   u16 texcoord3D;
   u16 _pad1;
} rsxFragmentProgram;

typedef struct rsx_const
{
   u32 name_off;
   u32 index;
   u8  type;
   u8  is_internal;
   u8  count;
   u8  _pad0;
   union
   {
      u32 u;
      f32 f;
   } values[4];
} rsxProgramConst;

typedef struct rsx_attrib
{
   u32 name_off;
   u32 index;
   u8  type;
   u8  _pad0[3];
} rsxProgramAttrib;

/* <rsx/mm.h> */
void *rsxMemalign(u32 alignment, u32 size);
void  rsxFree(void *ptr);

/* <rsx/rsx.h>; an inline over the RSX's mapping in PSL1GHT */
s32 rsxInit(gcmContextData **context, u32 cmdSize, u32 ioSize,
      const void *ioAddress);
s32 rsxAddressToOffset(const void *ptr, u32 *offset);

/* <rsx/rsx_program.h> */
void rsxVertexProgramGetUCode(const rsxVertexProgram *vp, void **ucode,
      u32 *size);
rsxProgramConst  *rsxVertexProgramGetConst(const rsxVertexProgram *vp,
      const char *name);
rsxProgramAttrib *rsxVertexProgramGetAttrib(const rsxVertexProgram *vp,
      const char *name);
void rsxFragmentProgramGetUCode(const rsxFragmentProgram *fp,
      void **ucode, u32 *size);
rsxProgramConst  *rsxFragmentProgramGetConst(const rsxFragmentProgram *fp,
      const char *name);
rsxProgramAttrib *rsxFragmentProgramGetAttrib(const rsxFragmentProgram *fp,
      const char *name);

/* <rsx/commands.h> */
void rsxBindVertexArrayAttrib(gcmContextData *context, u8 attr,
      u16 frequency, u32 offset, u8 stride, u8 elems, u8 dtype,
      u8 location);
void rsxClearSurface(gcmContextData *context, u32 clear_mask);
void rsxDrawVertexArray(gcmContextData *context, u32 type, u32 start,
      u32 count);
void rsxFinish(gcmContextData *context, u32 ref_value);
void rsxFlushBuffer(gcmContextData *context);
void rsxInvalidateTextureCache(gcmContextData *context, u32 type);
void rsxLoadFragmentProgramLocation(gcmContextData *context,
      const rsxFragmentProgram *program, u32 offset, u32 location);
void rsxLoadTexture(gcmContextData *context, u8 index,
      const gcmTexture *texture);
void rsxLoadVertexProgram(gcmContextData *context,
      const rsxVertexProgram *program, const void *ucode);
void rsxSetAlphaFunc(gcmContextData *context, u32 alphaFunc, u32 ref);
void rsxSetAlphaTestEnable(gcmContextData *context, u32 enable);
void rsxSetBlendEnable(gcmContextData *context, u32 enable);
void rsxSetBlendEnableMrt(gcmContextData *context, u32 mrt1, u32 mrt2,
      u32 mrt3);
void rsxSetBlendEquation(gcmContextData *context, u16 color, u16 alpha);
void rsxSetBlendFunc(gcmContextData *context, u16 sfcolor, u16 dfcolor,
      u16 sfalpha, u16 dfalpha);
void rsxSetClearColor(gcmContextData *context, u32 color);
void rsxSetClearDepthStencil(gcmContextData *context, u32 value);
void rsxSetColorMask(gcmContextData *context, u32 mask);
void rsxSetColorMaskMrt(gcmContextData *context, u32 mask);
void rsxSetDepthFunc(gcmContextData *context, u32 func);
void rsxSetDepthTestEnable(gcmContextData *context, u32 enable);
void rsxSetScissor(gcmContextData *context, u16 x, u16 y, u16 w, u16 h);
void rsxSetSurface(gcmContextData *context, const gcmSurface *surface);
void rsxSetUserClipPlaneControl(gcmContextData *context, u32 plane0,
      u32 plane1, u32 plane2, u32 plane3, u32 plane4, u32 plane5);
void rsxSetVertexProgramParameter(gcmContextData *context,
      const rsxVertexProgram *program, const rsxProgramConst *param,
      const f32 *value);
void rsxSetViewport(gcmContextData *context, u16 x, u16 y, u16 width,
      u16 height, f32 min, f32 max, const f32 scale[4],
      const f32 offset[4]);
void rsxSetViewportClip(gcmContextData *context, u8 sel, u16 width,
      u16 height);
void rsxSetWaitLabel(gcmContextData *context, u8 index, u32 value);
void rsxSetWriteBackendLabel(gcmContextData *context, u8 index,
      u32 value);
void rsxSetZMinMaxControl(gcmContextData *context, u8 cullNearFar,
      u8 zClampEnable, u8 cullIgnoreW);
void rsxTextureControl(gcmContextData *context, u8 index, u32 enable,
      u16 minlod, u16 maxlod, u8 maxaniso);
void rsxTextureFilter(gcmContextData *context, u8 index, u16 bias,
      u8 min, u8 mag, u8 conv);
void rsxTextureWrapMode(gcmContextData *context, u8 index, u8 wraps,
      u8 wrapt, u8 wrapr, u8 unsignedRemap, u8 zfunc, u8 gamma);

#endif
