static inline GLuint rglPlatformGetBitsPerPixel (GLenum internalFormat)
{
   switch (internalFormat)
   {
      case RGLGCM_ALPHA16:
      case RGLGCM_HILO8:
      case RGLGCM_RGB5_A1_SCE:
      case RGLGCM_RGB565_SCE:
         return 16;
      case RGLGCM_ALPHA8:
         return 8;
      case RGLGCM_RGBX8:
      case RGLGCM_RGBA8:
      case RGLGCM_ABGR8:
      case RGLGCM_ARGB8:
      case RGLGCM_BGRA8:
      case RGLGCM_FLOAT_R32:
      case RGLGCM_HILO16:
      case RGLGCM_XBGR8:
         return 32;
      default:
         return 0;
   }
}

static inline void rglGcmSetVertexProgramParameterBlock(struct CellGcmContextData *thisContext, uint32_t baseConst,
      uint32_t constCount, const float * __restrict value)
{
   uint32_t blockCount, blockRemain, i;

   blockCount  = (constCount * 4) >> 5;
   blockRemain = (constCount * 4) & 0x1f;

   for (i=0; i < blockCount; i++)
   {
      uint32_t loadAt = baseConst + i * 8;

      thisContext->current[0] = (((33) << (18)) | ((0x00001efc)));
      thisContext->current[1] = (loadAt);

      memcpy(&thisContext->current[2], value, 16 * sizeof(float));
      memcpy(&thisContext->current[18], &value[16], 16 * sizeof(float));
      thisContext->current += 34;
      value += 32;
   }

   if(blockRemain)
   {
      thisContext->current[0] = (((blockRemain+1) << (18)) | ((0x00001efc)));
      thisContext->current[1] = (baseConst + blockCount * 8);
      thisContext->current += 2;

      blockRemain >>= 2;
      for (i=0; i < blockRemain; ++i)
      {
         memcpy(thisContext->current, value, 4 * sizeof(float));
         thisContext->current += 4;
         value += 4;
      }
   }
}

#define SUBPIXEL_BITS 12
#define SUBPIXEL_ADJUST (0.5/(1<<SUBPIXEL_BITS))

#define rglDisableVertexAttribArrayNVInline(context, index) \
 RGLBIT_FALSE(context->attribs->EnabledMask, index); \
 RGLBIT_TRUE(context->attribs->DirtyMask, index);

#define rglEnableVertexAttribArrayNVInline(context, index) \
 RGLBIT_TRUE(context->attribs->EnabledMask, index); \
 RGLBIT_TRUE(context->attribs->DirtyMask, index);

#define rglGcmSetVertexData4f(thisContext, index, v) \
 thisContext->current[0] = (((4) << (18)) | (CELL_GCM_NV4097_SET_VERTEX_DATA4F_M + (index) * 16)); \
 memcpy(&thisContext->current[1], v, sizeof(float)*4); \
 thisContext->current += 5;

#define rglGcmSetVertexDataArray(thisContext, index, frequency, stride, size, type, location, offset) \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_VERTEX_DATA_ARRAY_FORMAT + ((index)) * 4)); \
 (thisContext->current)[1] = ((((frequency)) << 16) | (((stride)) << 8) | (((size)) << 4) | ((type))); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_VERTEX_DATA_ARRAY_OFFSET + ((index)) * 4)); \
 (thisContext->current)[1] = ((((location)) << 31) | (offset)); \
 (thisContext->current) += 2;

#define rglGcmSetInlineTransferPointer(thisContext, offset, count, pointer) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV3062_SET_OFFSET_DESTIN); \
 (thisContext->current)[1] = (offset & ~63); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((2) << (18)) | CELL_GCM_NV3062_SET_COLOR_FORMAT); \
 (thisContext->current)[1] = (CELL_GCM_TRANSFER_SURFACE_FORMAT_Y32); \
 (thisContext->current)[2] = ((0x1000) | ((0x1000) << 16)); \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((3) << (18)) | CELL_GCM_NV308A_POINT); \
 (thisContext->current)[1] = (((0) << 16) | ((offset & 63) >> 2)); \
 (thisContext->current)[2] = (((1) << 16) | (count)); \
 (thisContext->current)[3] = (((1) << 16) | (count)); \
 (thisContext->current) += 4; \
 thisContext->current[0] = ((((count + 1) & ~1) << (18)) | CELL_GCM_NV308A_COLOR); \
 thisContext->current += 1; \
 pointer = thisContext->current; \
 thisContext->current += ((count + 1) & ~1);

#define rglGcmSetWriteBackEndLabel(thisContext, index, value) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_SEMAPHORE_OFFSET); \
 (thisContext->current)[1] = 0x10 * index; /* offset */ \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_BACK_END_WRITE_SEMAPHORE_RELEASE); \
 (thisContext->current)[1] = ( value & 0xff00ff00) | ((value >> 16) & 0xff) | (((value >> 0 ) & 0xff) << 16); \
 (thisContext->current) += 2;

#define rglGcmSetWaitLabel(thisContext, index, value) \
 (thisContext->current)[0] = (((1) << (18)) | ((CELL_GCM_NV406E_SEMAPHORE_OFFSET))); \
 (thisContext->current)[1] = 0x10 * index; \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((CELL_GCM_NV406E_SEMAPHORE_ACQUIRE))); \
 (thisContext->current)[1] = (value); \
 (thisContext->current) += 2;

#define rglGcmSetInvalidateVertexCache(thisContext) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_INVALIDATE_VERTEX_CACHE_FILE); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_INVALIDATE_VERTEX_FILE); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_INVALIDATE_VERTEX_FILE); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_INVALIDATE_VERTEX_FILE); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2;

#define rglGcmSetClearSurface(thisContext, mask) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_CLEAR_SURFACE); \
 (thisContext->current)[1] = (mask); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_NO_OPERATION); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2;

#define rglGcmSetTextureControl(thisContext, index, enable, minlod, maxlod, maxaniso) \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_CONTROL0 + 0x20 * ((index)))); \
 (thisContext->current)[1] = ((((0) << 2) | ((maxaniso)) << 4) | (((maxlod)) << 7) | (((minlod)) << 19) | ((enable) << 31)); \
 (thisContext->current) += 2;

#define rglGcmSetTextureRemap(thisContext, index, remap) \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_CONTROL1 + ((index)) * 32)); \
 (thisContext->current)[1] = (remap); \
 (thisContext->current) += 2;

#define rglGcmSetTransferLocation(thisContext, location) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV3062_SET_CONTEXT_DMA_IMAGE_DESTIN); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + location); \
 (thisContext->current) += 2;

#define rglGcmInlineTransfer(thisContext, dstOffset, srcAdr, sizeInWords, location) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV3062_SET_CONTEXT_DMA_IMAGE_DESTIN); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + location); \
 (thisContext->current) += 2; \
  cellGcmSetInlineTransferUnsafeInline(thisContext, dstOffset, srcAdr, sizeInWords);

#define rglGcmSetClearColor(thisContext, color) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_COLOR_CLEAR_VALUE); \
 (thisContext->current)[1] = (color); \
 (thisContext->current) += 2;

#define rglGcmSetTextureBorder(thisContext, index, texture, border) \
 uint32_t format, offset, control1, control3, imagerect; \
 offset = texture->offset; \
 format = (texture->location + 1) | (texture->cubemap << 2) | (border << 3) | (texture->dimension << 4) | (texture->format << 8) | (texture->mipmap << 16); \
 imagerect = texture->height | (texture->width << 16); \
 control1 = texture->remap; \
 control3 = texture->pitch | (texture->depth << 20); \
 (thisContext->current)[0] = (((2) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_OFFSET + ((index)) * 32)); \
 (thisContext->current)[1] = (offset); \
 (thisContext->current)[2] = (format); \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_IMAGE_RECT + ((index)) * 32)); \
 (thisContext->current)[1] = (imagerect); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_CONTROL3 + ((index)) * 4)); \
 (thisContext->current)[1] = (control3); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_CONTROL1 + ((index)) * 32)); \
 (thisContext->current)[1] = (control1); \
 (thisContext->current) += 2;

#define rglGcmSetJumpCommand(thisContext, offset) \
 thisContext->current[0] = ((offset) | (0x20000000)); \
 thisContext->current += 1

#define rglGcmSetBlendEnable(thisContext, enable) \
{ \
 bool continue_func = true; \
 if(thisContext->current + (2) > thisContext->end) \
 { \
   if((*thisContext->callback)(thisContext, (2)) != 0) \
    continue_func = false; \
 } \
 if (continue_func) \
 { \
    (thisContext->current)[0] = (((1) << (18)) | ((0x00000310))); \
    (thisContext->current)[1] = (enable); \
    (thisContext->current) += 2; \
 } \
}

#define rglGcmSetBlendEnableMrt(thisContext, mrt1, mrt2, mrt3) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x0000036c))); \
 (thisContext->current)[1] = (((mrt1) << 1)|((mrt2) << 2)|((mrt3) << 3)); \
 (thisContext->current) += 2

#define rglGcmSetBlendEquation(thisContext, color, alpha) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00000320))); \
 (thisContext->current)[1] = (((color)) | (((alpha)) << 16)); \
 (thisContext->current) += 2

#define rglGcmSetBlendFunc(thisContext, sfcolor, dfcolor, sfalpha, dfalpha) \
 (thisContext->current)[0] = (((2) << (18)) | ((0x00000314))); \
 (thisContext->current)[1] = (((sfcolor)) | (((sfalpha)) << 16)); \
 (thisContext->current)[2] = (((dfcolor)) | (((dfalpha)) << 16)); \
 (thisContext->current) += 3

#define rglGcmSetUserClipPlaneControl(thisContext, plane0, plane1, plane2, plane3, plane4, plane5) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_USER_CLIP_PLANE_CONTROL); \
 (thisContext->current)[1] = ((plane0) | ((plane1) << 4) | ((plane2) << 8) | ((plane3) << 12) | ((plane4) << 16) | ((plane5) << 20)); \
 (thisContext->current) += 2;

#define rglGcmSetInvalidateTextureCache(thisContext, value) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_INVALIDATE_L2); \
 (thisContext->current)[1] = (value); \
 (thisContext->current) += 2;

#define rglGcmSetViewport(thisContext, x, y, w, h, min, max, scale, offset) \
 CellGcmCast d0,d1; \
 d0.f = min; \
 d1.f = max; \
 CellGcmCast o[4],s[4]; \
 o[0].f = offset[0]; \
 o[1].f = offset[1]; \
 o[2].f = offset[2]; \
 o[3].f = offset[3]; \
 s[0].f = scale[0]; \
 s[1].f = scale[1]; \
 s[2].f = scale[2]; \
 s[3].f = scale[3]; \
 (thisContext->current)[0] = (((2) << (18)) | CELL_GCM_NV4097_SET_VIEWPORT_HORIZONTAL); \
 (thisContext->current)[1] = (((x)) | (((w)) << 16)); \
 (thisContext->current)[2] = (((y)) | (((h)) << 16)); \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((2) << (18)) | CELL_GCM_NV4097_SET_CLIP_MIN); \
 (thisContext->current)[1] = (d0.u); \
 (thisContext->current)[2] = (d1.u); \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((8) << (18)) | CELL_GCM_NV4097_SET_VIEWPORT_OFFSET); \
 (thisContext->current)[1] = (o[0].u); \
 (thisContext->current)[2] = (o[1].u); \
 (thisContext->current)[3] = (o[2].u); \
 (thisContext->current)[4] = (o[3].u); \
 (thisContext->current)[5] = (s[0].u); \
 (thisContext->current)[6] = (s[1].u); \
 (thisContext->current)[7] = (s[2].u); \
 (thisContext->current)[8] = (s[3].u); \
 (thisContext->current) += 9; \
 (thisContext->current)[0] = (((8) << (18)) | CELL_GCM_NV4097_SET_VIEWPORT_OFFSET); \
 (thisContext->current)[1] = (o[0].u); \
 (thisContext->current)[2] = (o[1].u); \
 (thisContext->current)[3] = (o[2].u); \
 (thisContext->current)[4] = (o[3].u); \
 (thisContext->current)[5] = (s[0].u); \
 (thisContext->current)[6] = (s[1].u); \
 (thisContext->current)[7] = (s[2].u); \
 (thisContext->current)[8] = (s[3].u); \
 (thisContext->current) += 9;

#define rglGcmSetDitherEnable(thisContext, enable) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_DITHER_ENABLE); \
 (thisContext->current)[1] = (enable); \
 (thisContext->current) += 2;

#define rglGcmSetReferenceCommand(thisContext, ref) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV406E_SET_REFERENCE); \
 (thisContext->current)[1] = (ref); \
 (thisContext->current) += 2; 

#define rglGcmSetZMinMaxControl(thisContext, cullNearFarEnable, zclampEnable, cullIgnoreW) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_ZMIN_MAX_CONTROL); \
 (thisContext->current)[1] = ((cullNearFarEnable) | ((zclampEnable) << 4) | ((cullIgnoreW)<<8)); \
 (thisContext->current) += 2;

#define rglGcmSetVertexAttribOutputMask(thisContext, mask) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_VERTEX_ATTRIB_OUTPUT_MASK); \
 (thisContext->current)[1] = (mask); \
 (thisContext->current) += 2;

#define rglGcmSetNopCommand(thisContext, i, count) \
 for(i=0;i<count;i++) \
  thisContext->current[i] = 0; \
 thisContext->current += count;

#define rglGcmSetAntiAliasingControl(thisContext, enable, alphaToCoverage, alphaToOne, sampleMask) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_ANTI_ALIASING_CONTROL); \
 (thisContext->current)[1] = ((enable) | ((alphaToCoverage) << 4) | ((alphaToOne) << 8) | ((sampleMask) << 16)); \
 (thisContext->current) += 2; 

#define rglGcmFifoFinish(ref, offset_bytes) \
 ref = rglGcmFifoPutReference( fifo ); \
 rglGcmFifoFlush( fifo, offset_bytes ); \
 do {} while (rglGcmFifoReferenceInUse(fifo, ref));

#define rglGcmFifoReadReference(fifo) (fifo->lastHWReferenceRead = *((volatile GLuint *)&fifo->dmaControl->Reference))

#define rglGcmFifoFlush(fifo, offsetInBytes) \
 cellGcmAddressToOffset( fifo->ctx.current, ( uint32_t * )&offsetInBytes ); \
 cellGcmFlush(); \
 fifo->dmaControl->Put = offsetInBytes; \
 fifo->lastPutWritten = fifo->ctx.current; \
 fifo->lastSWReferenceFlushed = fifo->lastSWReferenceWritten;

#define rglGcmSetSurface(thisContext, surface, origin, pixelCenter, log2Width, log2Height) \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_CONTEXT_DMA_COLOR_A); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->colorLocation[0]); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_CONTEXT_DMA_COLOR_B); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->colorLocation[1]); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((2) << (18)) | CELL_GCM_NV4097_SET_CONTEXT_DMA_COLOR_C); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->colorLocation[2]); \
 (thisContext->current)[2] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->colorLocation[3]); \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_CONTEXT_DMA_ZETA); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->depthLocation); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((6) << (18)) | CELL_GCM_NV4097_SET_SURFACE_FORMAT); \
 (thisContext->current)[1] = ((surface->colorFormat) | ((surface->depthFormat) << 5) | ((surface->type) << 8) | ((surface->antialias) << 12) | ((log2Width) << 16) | ((log2Height) << 24)); \
 (thisContext->current)[2] = (surface->colorPitch[0]); \
 (thisContext->current)[3] = (surface->colorOffset[0]); \
 (thisContext->current)[4] = (surface->depthOffset); \
 (thisContext->current)[5] = (surface->colorOffset[1]); \
 (thisContext->current)[6] = (surface->colorPitch[1]); \
 (thisContext->current) += 7; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_SURFACE_PITCH_Z); \
 (thisContext->current)[1] = (surface->depthPitch); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((4) << (18)) | CELL_GCM_NV4097_SET_SURFACE_PITCH_C); \
 (thisContext->current)[1] = (surface->colorPitch[2]); \
 (thisContext->current)[2] = (surface->colorPitch[3]); \
 (thisContext->current)[3] = (surface->colorOffset[2]); \
 (thisContext->current)[4] = (surface->colorOffset[3]); \
 (thisContext->current) += 5; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_SURFACE_COLOR_TARGET); \
 (thisContext->current)[1] = ((surface->colorTarget)); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_WINDOW_OFFSET); \
 (thisContext->current)[1] = ((surface->x) | ((surface->y) << 16)); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((2) << (18)) | CELL_GCM_NV4097_SET_SURFACE_CLIP_HORIZONTAL); \
 (thisContext->current)[1] = ((surface->x) | ((surface->width) << 16)); \
 (thisContext->current)[2] = ((surface->y) | ((surface->height) << 16));  \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_SHADER_WINDOW); \
 (thisContext->current)[1] = ((surface->height - (((surface->height) & 0x1000) >> 12)) | ((origin) << 12) | ((pixelCenter) << 16)); \
 (thisContext->current) += 2;

#define rglGcmSend(dstId, dstOffset, pitch, src, size) \
   GLuint id = gmmAlloc(size); \
   memcpy( gmmIdToAddress(id), (src), size ); \
   rglGcmTransferData( dstId, dstOffset, size, id, 0, size, size, 1 ); \
   gmmFree( id )

#define rglGcmSetUpdateFragmentProgramParameter(thisContext, offset, location) \
 (thisContext->current)[0] = (((1) << (18)) | ((CELL_GCM_NV4097_SET_SHADER_PROGRAM))); \
 (thisContext->current)[1] = ((location+1) | (offset)); \
 (thisContext->current) += 2

#define rglGcmSetBlendColor(thisContext, color, color2) \
 (thisContext->current)[0] = (((1) << (18)) | ((CELL_GCM_NV4097_SET_BLEND_COLOR))); \
 (thisContext->current)[1] = (color); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((CELL_GCM_NV4097_SET_BLEND_COLOR2))); \
 (thisContext->current)[1] = (color2); \
 (thisContext->current) += 2

static inline void rglGcmSetFragmentProgramLoad(struct CellGcmContextData *thisContext, const CellCgbFragmentProgramConfiguration *conf, const uint32_t location)
{
   uint32_t rawData = ((conf->offset)&0x1fffffff);
   uint32_t shCtrl0;
   uint32_t registerCount;
   uint32_t texMask;
   uint32_t inMask;
   uint32_t texMask2D;
   uint32_t texMaskCentroid;
   uint32_t i;

   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_SHADER_PROGRAM);
   (thisContext->current)[1] = ((location+1) | (rawData));
   (thisContext->current) += 2;

   inMask = conf->attributeInputMask;

   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_VERTEX_ATTRIB_OUTPUT_MASK);
   (thisContext->current)[1] = (inMask);
   (thisContext->current) += 2;


   texMask = conf->texCoordsInputMask;
   texMask2D = conf->texCoords2D;
   texMaskCentroid = conf->texCoordsCentroid;

   for(i=0; texMask; i++)
   {
      if (texMask & 1)
      {
         uint32_t hwTexCtrl = (texMask2D & 1) | ((texMaskCentroid & 1) << 4);
         (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEX_COORD_CONTROL + (i) * 4));
         (thisContext->current)[1] = (hwTexCtrl);
         (thisContext->current) += 2;
      }
      texMask >>= 1;
      texMask2D >>= 1;
      texMaskCentroid >>= 1;
   }


   registerCount = conf->registerCount;

   if (registerCount < 2)
      registerCount = 2;

   shCtrl0 = conf->fragmentControl | (registerCount << 24);
   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_SHADER_CONTROL);
   (thisContext->current)[1] = (shCtrl0);
   (thisContext->current) += 2;
}

static void rglGcmSetDrawArraysSlow(struct CellGcmContextData *thisContext, uint8_t mode,
      uint32_t first, uint32_t count)
{
   uint32_t lcount;

   --count;
   lcount = count & 0xff;
   count >>= 8;

   uint32_t loop, rest;
   loop = count / CELL_GCM_MAX_METHOD_COUNT;
   rest = count % CELL_GCM_MAX_METHOD_COUNT;

   (thisContext->current)[0] = (((3) << (18)) | CELL_GCM_NV4097_INVALIDATE_VERTEX_FILE | (0x40000000));
   (thisContext->current)[1] = 0;
   (thisContext->current)[2] = 0;
   (thisContext->current)[3] = 0;
   (thisContext->current) += 4;

   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_BEGIN_END);
   (thisContext->current)[1] = ((mode));
   (thisContext->current) += 2;
   
   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_DRAW_ARRAYS);
   (thisContext->current)[1] = ((first) | ((lcount)<<24));
   (thisContext->current) += 2;
   first += lcount + 1;

   uint32_t i,j;

   for(i=0;i<loop;i++)
   {
      thisContext->current[0] = ((((2047)) << (18)) | CELL_GCM_NV4097_DRAW_ARRAYS | (0x40000000));
      thisContext->current++;

      for(j=0;j<(2047);j++)
      {
         thisContext->current[0] = ((first) | ((255U)<<24));
         thisContext->current++;
         first += 256;
      }
   }

   if(rest)
   {
      thisContext->current[0] = (((rest) << (18)) | CELL_GCM_NV4097_DRAW_ARRAYS | (0x40000000));
      thisContext->current++;

      for(j=0;j<rest;j++)
      {
         thisContext->current[0] = ((first) | ((255U)<<24));
         thisContext->current++;
         first += 256;
      }
   }

   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_BEGIN_END);
   (thisContext->current)[1] = (0);
   (thisContext->current) += 2;
}

static inline void rglGcmSetDrawArrays(struct CellGcmContextData *thisContext, uint8_t mode,
      uint32_t first, uint32_t count)
{
   if (mode == GL_TRIANGLE_STRIP && first == 0 && count == 4)
   {
      (thisContext->current)[0] = (((3) << (18)) | CELL_GCM_NV4097_INVALIDATE_VERTEX_FILE | (0x40000000));
      (thisContext->current)[1] = 0;
      (thisContext->current)[2] = 0;
      (thisContext->current)[3] = 0;
      (thisContext->current) += 4;

      (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_BEGIN_END);
      (thisContext->current)[1] = ((mode));
      (thisContext->current) += 2;

      (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_DRAW_ARRAYS);
      (thisContext->current)[1] = ((first) | (3 <<24));
      (thisContext->current) += 2;
      first += 4;

      (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_BEGIN_END);
      (thisContext->current)[1] = (0);
      (thisContext->current) += 2;
   }
   else
      rglGcmSetDrawArraysSlow(thisContext, mode, first, count);
}

static inline void rglGcmSetVertexProgramLoad(struct CellGcmContextData *thisContext, const CellCgbVertexProgramConfiguration *conf, const void *ucode)
{
   const uint32_t *rawData;
   uint32_t instCount;
   uint32_t instIndex;

   rawData = (const uint32_t*)ucode;
   instCount = conf->instructionCount;
   instIndex = conf->instructionSlot;

   uint32_t loop, rest;
   loop = instCount / 8;
   rest = (instCount % 8) * 4;

   (thisContext->current)[0] = (((2) << (18)) | CELL_GCM_NV4097_SET_TRANSFORM_PROGRAM_LOAD);
   (thisContext->current)[1] = (instIndex);
   (thisContext->current)[2] = (instIndex);
   (thisContext->current) += 3;

   uint32_t i, j;

   for (i = 0; i < loop; i++)
   {
      thisContext->current[0] = (((32) << (18)) | CELL_GCM_NV4097_SET_TRANSFORM_PROGRAM);
      memcpy(&thisContext->current[1], &rawData[0], sizeof(uint32_t)*16);
      memcpy(&thisContext->current[17], &rawData[16], sizeof(uint32_t)*16);

      thisContext->current += (1 + 32);
      rawData += 32;
   }

   if (rest > 0)
   {
      thisContext->current[0] = (((rest) << (18)) | CELL_GCM_NV4097_SET_TRANSFORM_PROGRAM);
      for (j = 0; j < rest; j++)
         thisContext->current[j+1] = rawData[j];
      thisContext->current += (1 + rest);
   }


   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_VERTEX_ATTRIB_INPUT_MASK);
   (thisContext->current)[1] = ((conf->attributeInputMask));
   (thisContext->current) += 2;

   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV4097_SET_TRANSFORM_TIMEOUT);

   if (conf->registerCount <= 32)
      (thisContext->current)[1] = ((0xFFFF) | ((32) << 16));
   else
      (thisContext->current)[1] = ((0xFFFF) | ((48) << 16));
   (thisContext->current) += 2;
}

static inline void rglGcmFifoGlViewport(void *data, GLclampf zNear, GLclampf zFar)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   rglGcmViewportState *vp = (rglGcmViewportState*)data;
   rglGcmRenderTarget *rt = &rglGcmState_i.renderTarget;

   GLint clipY0, clipY1;
   GLint clipX0 = vp->x;
   GLint clipX1 = vp->x + vp->w;

   if (rt->yInverted)
   {
      clipY0 = rt->gcmRenderTarget.height - (vp->y + vp->h);
      clipY1 = rt->gcmRenderTarget.height - vp->y;
   }
   else
   {
      clipY0 = vp->y;
      clipY1 = vp->y + vp->h;
   }

   if (clipX0 < 0)
      clipX0 = 0;
   if (clipY0 < 0)
      clipY0 = 0;

   if (clipX1 >= RGLGCM_MAX_RT_DIMENSION)
      clipX1 = RGLGCM_MAX_RT_DIMENSION;

   if (clipY1 >= RGLGCM_MAX_RT_DIMENSION)
      clipY1 = RGLGCM_MAX_RT_DIMENSION;

   if ((clipX1 <= clipX0) || (clipY1 <= clipY0))
      clipX0 = clipY0 = clipX1 = clipY1 = 0;

   // update viewport info
   vp->xScale = vp->w * 0.5f;
   vp->xCenter = (GLfloat)(vp->x + vp->xScale + RGLGCM_SUBPIXEL_ADJUST);

   if (rt->yInverted)
   {
      vp->yScale = vp->h * -0.5f;
      vp->yCenter = (GLfloat)(rt->gcmRenderTarget.height - RGLGCM_VIEWPORT_EPSILON - vp->y +  vp->yScale + RGLGCM_SUBPIXEL_ADJUST);
   }
   else
   {
      vp->yScale = vp->h * 0.5f;
      vp->yCenter = (GLfloat)(vp->y + vp->yScale + RGLGCM_SUBPIXEL_ADJUST);
   }

   // compute viewport values for hw [no doubles, so we might loose a few lsb]
   GLfloat z_scale = ( GLfloat )( 0.5f * ( zFar - zNear ) );
   GLfloat z_center = ( GLfloat )( 0.5f * ( zFar + zNear ) );

   // hw zNear/zFar clipper
   if (zNear > zFar)
   {
      GLclampf tmp = zNear;
      zNear = zFar;
      zFar = tmp;
   }

   float scale[4] = { vp->xScale,  vp->yScale,  z_scale, 0.0f};
   float offset[4] = { vp->xCenter,  vp->yCenter,  z_center, 0.0f};

   rglGcmSetViewport(thisContext, clipX0, clipY0, clipX1 - clipX0,
         clipY1 - clipY0, zNear, zFar, scale, offset );
}

#define BLOCKSIZE_MAX_DIMENSIONS 1024

static inline void rglGcmSetTransferImage(struct CellGcmContextData *thisContext, uint8_t mode, uint32_t dstOffset, uint32_t dstPitch, uint32_t dstX, uint32_t dstY, uint32_t srcOffset, uint32_t srcPitch, uint32_t srcX, uint32_t srcY, uint32_t width, uint32_t height, uint32_t bytesPerPixel)
{
   uint32_t srcFormat, dstFormat, x, y, finalDstX, finalDstY;

   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV3062_SET_CONTEXT_DMA_IMAGE_DESTIN);
   (thisContext->current)[1] = CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER; /* CELL_GCM_TRANSFER_LOCAL_TO_LOCAL */
   (thisContext->current) += 2;

   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV3089_SET_CONTEXT_DMA_IMAGE);
   (thisContext->current)[1] = CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER; /* CELL_GCM_TRANSFER_LOCAL_TO_LOCAL */

   (thisContext->current) += 2;

   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV3089_SET_CONTEXT_SURFACE);
   (thisContext->current)[1] = ((0x313371C3));
   (thisContext->current) += 2;

   srcFormat = 0;
   dstFormat = 0;

   switch (bytesPerPixel)
   {
      case 2:
         srcFormat = CELL_GCM_TRANSFER_SCALE_FORMAT_R5G6B5;
         dstFormat = CELL_GCM_TRANSFER_SURFACE_FORMAT_R5G6B5;
         break;
      case 4:
         srcFormat = CELL_GCM_TRANSFER_SCALE_FORMAT_A8R8G8B8;
         dstFormat = CELL_GCM_TRANSFER_SURFACE_FORMAT_A8R8G8B8;
         break;
   }


   finalDstX = dstX + width;
   finalDstY = dstY + height;

   for (y = dstY; y < finalDstY;)
   {
      uint32_t dstTop = y & ~(BLOCKSIZE_MAX_DIMENSIONS - 1);
      uint32_t dstBltHeight = (( (dstTop + BLOCKSIZE_MAX_DIMENSIONS) < finalDstY)
            ? (dstTop + BLOCKSIZE_MAX_DIMENSIONS) : finalDstY) - y;

      for (x = dstX; x < finalDstX;)
      {
         uint32_t dstLeft = x & ~(BLOCKSIZE_MAX_DIMENSIONS - 1);
         uint32_t dstRight = dstLeft + BLOCKSIZE_MAX_DIMENSIONS;
         uint32_t dstBltWidth = ((dstRight < finalDstX) ? dstRight : finalDstX) - x;
         uint32_t dstBlockOffset = bytesPerPixel * (dstLeft & ~(BLOCKSIZE_MAX_DIMENSIONS - 1)) + dstPitch * dstTop;
         uint32_t srcBlockOffset = bytesPerPixel * (srcX + x-dstX) + srcPitch * (srcY + y-dstY);
         uint32_t safeDstBltWidth = (dstBltWidth < 16) ? 16 : (dstBltWidth + 1) & ~1;

         (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV3062_SET_OFFSET_DESTIN);
         (thisContext->current)[1] = dstOffset + dstBlockOffset;
         (thisContext->current) += 2;

         (thisContext->current)[0] = (((2) << (18)) | CELL_GCM_NV3062_SET_COLOR_FORMAT);
         (thisContext->current)[1] = (dstFormat);
         (thisContext->current)[2] = ((dstPitch) | ((dstPitch) << 16));
         (thisContext->current) += 3;

         (thisContext->current)[0] = (((9) << (18)) | CELL_GCM_NV3089_SET_COLOR_CONVERSION);
         (thisContext->current)[1] = (CELL_GCM_TRANSFER_CONVERSION_TRUNCATE);
         (thisContext->current)[2] = (srcFormat);
         (thisContext->current)[3] = (CELL_GCM_TRANSFER_OPERATION_SRCCOPY);
         (thisContext->current)[4] = (((y - dstTop) << 16) | (x - dstLeft));
         (thisContext->current)[5] = (((dstBltHeight) << 16) | (dstBltWidth));
         (thisContext->current)[6] = (((y - dstTop) << 16) | (x - dstLeft));
         (thisContext->current)[7] = (((dstBltHeight) << 16) | (dstBltWidth));
         (thisContext->current)[8] = 1048576;
         (thisContext->current)[9] = 1048576;
         (thisContext->current) += 10;

         (thisContext->current)[0] = (((4) << (18)) | CELL_GCM_NV3089_IMAGE_IN_SIZE);
         (thisContext->current)[1] = (((dstBltHeight) << 16) | (safeDstBltWidth));
         (thisContext->current)[2] = ((srcPitch) | ((CELL_GCM_TRANSFER_ORIGIN_CORNER) << 16) | ((CELL_GCM_TRANSFER_INTERPOLATOR_ZOH) << 24));
         (thisContext->current)[3] = (srcOffset + srcBlockOffset);
         (thisContext->current)[4] = 0;
         (thisContext->current) += 5;

         x += dstBltWidth;
      }
      y += dstBltHeight;
   }
}

static inline GLuint rglGcmMapMinTextureFilter( GLenum filter )
{
   switch (filter)
   {
      case GL_NEAREST:
         return CELL_GCM_TEXTURE_NEAREST;
         break;
      case GL_LINEAR:
         return CELL_GCM_TEXTURE_LINEAR;
         break;
      case GL_NEAREST_MIPMAP_NEAREST:
         return CELL_GCM_TEXTURE_NEAREST_NEAREST;
         break;
      case GL_NEAREST_MIPMAP_LINEAR:
         return CELL_GCM_TEXTURE_NEAREST_LINEAR;
         break;
      case GL_LINEAR_MIPMAP_NEAREST:
         return CELL_GCM_TEXTURE_LINEAR_NEAREST;
         break;
      case GL_LINEAR_MIPMAP_LINEAR:
         return CELL_GCM_TEXTURE_LINEAR_LINEAR;
         break;
      default:
         return 0;
   }
   return filter;
}

static inline GLuint rglGcmMapWrapMode( GLuint mode )
{
   switch ( mode )
   {
      case RGLGCM_CLAMP:
         return CELL_GCM_TEXTURE_CLAMP;
         break;
      case RGLGCM_REPEAT:
         return CELL_GCM_TEXTURE_WRAP;
         break;
      case RGLGCM_CLAMP_TO_EDGE:
         return CELL_GCM_TEXTURE_CLAMP_TO_EDGE;
         break;
      case RGLGCM_CLAMP_TO_BORDER:
         return CELL_GCM_TEXTURE_BORDER;
         break;
      case RGLGCM_MIRRORED_REPEAT:
         return CELL_GCM_TEXTURE_MIRROR;
         break;
      case RGLGCM_MIRROR_CLAMP_TO_EDGE:
         return CELL_GCM_TEXTURE_MIRROR_ONCE_CLAMP_TO_EDGE;
         break;
      case RGLGCM_MIRROR_CLAMP_TO_BORDER:
         return CELL_GCM_TEXTURE_MIRROR_ONCE_BORDER;
         break;
      case RGLGCM_MIRROR_CLAMP:
         return CELL_GCM_TEXTURE_MIRROR_ONCE_CLAMP;
         break;
      default:
         return 0;
         break;
   }
   return 0;
}

// Fast conversion for values between 0.0 and 65535.0
static inline GLuint RGLGCM_QUICK_FLOAT2UINT (const GLfloat f)
{
   union
   {
      GLfloat f;
      GLuint ui;
   } t;
   t.f = f + RGLGCM_F0_DOT_0;
   return t.ui & 0xffff;
}

// construct a packed unsigned int ARGB8 color
static inline void RGLGCM_CALC_COLOR_LE_ARGB8( GLuint *color0, const GLfloat r, const GLfloat g, const GLfloat b, const GLfloat a )
{
   GLuint r2 = RGLGCM_QUICK_FLOAT2UINT( r * 255.0f );
   GLuint g2 = RGLGCM_QUICK_FLOAT2UINT( g * 255.0f );
   GLuint b2 = RGLGCM_QUICK_FLOAT2UINT( b * 255.0f );
   GLuint a2 = RGLGCM_QUICK_FLOAT2UINT( a * 255.0f );
   *color0 = ( a2 << 24 ) | ( r2 << 16 ) | ( g2 << 8 ) | ( b2 << 0 );
}

#define RGLGCM_UTIL_LABEL_INDEX 253

// Utility to let RSX wait for complete RSX pipeline idle
static inline void rglGcmUtilWaitForIdle (void)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;

   // set write label command in push buffer, and wait
   // NOTE: this is for RSX to wailt
   rglGcmSetWriteBackEndLabel(thisContext, RGLGCM_UTIL_LABEL_INDEX, rglGcmState_i.labelValue );
   rglGcmSetWaitLabel(thisContext, RGLGCM_UTIL_LABEL_INDEX, rglGcmState_i.labelValue);

   // increment label value for next time. 
   rglGcmState_i.labelValue++; 

   // make sure the entire pipe in clear not just the front end 
   // Utility function that does GPU 'finish'.
   rglGcmSetWriteBackEndLabel(thisContext, RGLGCM_UTIL_LABEL_INDEX, rglGcmState_i.labelValue );
   cellGcmFlush();

   while( *(cellGcmGetLabelAddress( RGLGCM_UTIL_LABEL_INDEX)) != rglGcmState_i.labelValue);

   rglGcmState_i.labelValue++;
}

// Prints out an int in hexedecimal and binary, broken into bytes.
// Can be used for printing out macro and constant values.
// example: rglPrintIt( RGLGCM_3DCONST(SET_SURFACE_FORMAT, COLOR, LE_A8R8G8B8) );
//          00 00 00 08 : 00000000 00000000 00000000 00001000 */
static inline void rglPrintIt (unsigned int v)
{
   // HEX (space between bytes)
   printf( "%02x %02x %02x %02x : ", ( v >> 24 )&0xff, ( v >> 16 )&0xff, ( v >> 8 )&0xff, v&0xff );

   // BINARY (space between bytes)
   for ( unsigned int mask = ( 0x1 << 31 ), i = 1; mask != 0; mask >>= 1, i++ )
      printf( "%d%s", ( v & mask ) ? 1 : 0, ( i % 8 == 0 ) ? " " : "" );
   printf( "\n" );
}

// prints the last numWords of the command fifo
static inline void rglPrintFifoFromPut( unsigned int numWords ) 
{
   for ( int i = -numWords; i <= -1; i++ )
      rglPrintIt((( uint32_t* )rglGcmState_i.fifo.ctx.current )[i] );
}

// prints the last numWords of the command fifo
static inline void rglPrintFifoFromGet( unsigned int numWords ) 
{
   for ( int i = -numWords; i <= -1; i++ )
      rglPrintIt((( uint32_t* )rglGcmState_i.fifo.lastGetRead )[i] );
}

#define CL0039_MIN_PITCH -32768
#define CL0039_MAX_PITCH 32767
#define CL0039_MAX_LINES 0x3fffff
#define CL0039_MAX_ROWS 0x7ff

static inline void rglGcmTransferData
(
 GLuint dstId,
 GLuint dstIdOffset, 
 GLint dstPitch,
 GLuint srcId,
 GLuint srcIdOffset,
 GLint srcPitch,
 GLint bytesPerRow,
 GLint rowCount
 )
{
   uint32_t colCount, rows, cols;
   GLuint dstOffset, srcOffset;
   struct CellGcmContextData *thisContext = gCellGcmCurrentContext;

   dstOffset = gmmIdToOffset(dstId) + dstIdOffset;
   srcOffset = gmmIdToOffset(srcId) + srcIdOffset;

   (thisContext->current)[0] = (((2) << (18)) | CELL_GCM_NV0039_SET_CONTEXT_DMA_BUFFER_IN);
   (thisContext->current)[1] = CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER; /* CELL_GCM_TRANSFER_LOCAL_TO_LOCAL */
   (thisContext->current)[2] = CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER; /* CELL_GCM_TRANSFER_LOCAL_TO_LOCAL */
   (thisContext->current) += 3;

   if ((srcPitch == bytesPerRow) && (dstPitch == bytesPerRow))
   {
      bytesPerRow *= rowCount;
      rowCount = 1;
      srcPitch = 0;
      dstPitch = 0;
   }

   while(--rowCount >= 0)
   {
      for(colCount = bytesPerRow; colCount>0; colCount -= cols)
      {
         cols = (colCount > CL0039_MAX_LINES) ? CL0039_MAX_LINES : colCount;

         (thisContext->current)[0] = (((8) << (18)) | CELL_GCM_NV0039_OFFSET_IN);
         (thisContext->current)[1] = (srcOffset + (bytesPerRow - colCount));
         (thisContext->current)[2] = (dstOffset + (bytesPerRow - colCount));
         (thisContext->current)[3] = (0);
         (thisContext->current)[4] = (0);
         (thisContext->current)[5] = (cols);
         (thisContext->current)[6] = (1);
         (thisContext->current)[7] = (((1) << 8) | (1));
         (thisContext->current)[8] = (0);
         (thisContext->current) += 9;
      }

      dstOffset += dstPitch;
      srcOffset += srcPitch;
   }

   (thisContext->current)[0] = (((1) << (18)) | CELL_GCM_NV0039_OFFSET_OUT);
   (thisContext->current)[1] = (0);
   (thisContext->current) += 2;
}
