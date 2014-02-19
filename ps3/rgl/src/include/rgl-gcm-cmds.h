#ifndef _GCM_CMDS_H
#define _GCM_CMDS_H

#define rglGcmSetTextureAddress(thisContext, index, wraps, wrapt, wrapr, unsignedRemap, zfunc, gamma) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_TEXTURE_ADDRESS + 0x20 * ((index)), 1); \
 gcm_emit_at(thisContext->current, 1, (((wraps)) | ((0) << 4) | (((wrapt)) << 8) | (((unsignedRemap)) << 12) | (((wrapr)) << 16) | (((gamma)) << 20) |((0) << 24) | (((zfunc)) << 28))); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetTextureFilter(thisContext, index, bias, min, mag, conv) \
{ \
 bool continue_func = true; \
 if(thisContext->current + (2) > thisContext->end) \
 { \
    if((*thisContext->callback)(thisContext, (2)) != 0) \
     continue_func = false; \
 } \
 if (continue_func) \
 { \
  gcm_emit_method_at(thisContext->current, 0, (CELL_GCM_NV4097_SET_TEXTURE_FILTER + 0x20 * ((index))), 1); \
  gcm_emit_at(thisContext->current, 1, (((bias)) | (((conv)) << 13) | (((min)) << 16) | (((mag)) << 24) | ((0) << 28) | ((0) << 29) | ((0) << 30) | ((0) << 31))); \
  gcm_finish_n_commands(thisContext->current, 2); \
 } \
}

#define rglGcmSetReferenceCommandInline(thisContext, ref) \
{ \
 bool continue_func = true; \
 if(thisContext->current + (2) > thisContext->end) \
 { \
    if((*thisContext->callback)(thisContext, (2)) != 0) \
     continue_func = false; \
 } \
 if (continue_func) \
 { \
    gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV406E_SET_REFERENCE, 1); \
    gcm_emit_at(thisContext->current, 1, ref); \
    gcm_finish_n_commands(thisContext->current, 2); \
 } \
}

#define rglGcmSetTextureBorderColor(thisContext, index, color) \
 gcm_emit_method_at(thisContext->current, 0, (CELL_GCM_NV4097_SET_TEXTURE_BORDER_COLOR + 0x20 * ((index))), 1); \
 gcm_emit_at(thisContext->current, 1, color); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglDisableVertexAttribArrayNVInline(context, index) \
 RGLBIT_FALSE(context->attribs->EnabledMask, index); \
 RGLBIT_TRUE(context->attribs->DirtyMask, index);

#define rglEnableVertexAttribArrayNVInline(context, index) \
 RGLBIT_TRUE(context->attribs->EnabledMask, index); \
 RGLBIT_TRUE(context->attribs->DirtyMask, index);

#define rglGcmSetVertexData4f(thisContext, index, v) \
 gcm_emit_method_at(thisContext->current, 0, (CELL_GCM_NV4097_SET_VERTEX_DATA4F_M + (index) * 16), 4); \
 memcpy(&thisContext->current[1], v, sizeof(float)*4); \
 gcm_finish_n_commands(thisContext->current, 5);

#define rglGcmSetJumpCommand(thisContext, offset) \
 gcm_emit_at(thisContext->current, 0, ((offset) | (0x20000000))); \
 gcm_finish_n_commands(thisContext->current, 1);

#define rglGcmSetVertexDataArray(thisContext, index, frequency, stride, size, type, location, offset) \
 gcm_emit_method_at(thisContext->current, 0, (CELL_GCM_NV4097_SET_VERTEX_DATA_ARRAY_FORMAT + ((index)) * 4), 1); \
 gcm_emit_at(thisContext->current, 1, ((((frequency)) << 16) | (((stride)) << 8) | (((size)) << 4) | ((type)))); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, (CELL_GCM_NV4097_SET_VERTEX_DATA_ARRAY_OFFSET + ((index)) * 4), 1); \
 (thisContext->current)[1] = ((((location)) << 31) | (offset)); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetInlineTransferPointer(thisContext, offset, count, pointer) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV3062_SET_OFFSET_DESTIN, 1); \
 gcm_emit_at(thisContext->current, 1, (offset & ~63)); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV3062_SET_COLOR_FORMAT, 2); \
 gcm_emit_at(thisContext->current, 1, CELL_GCM_TRANSFER_SURFACE_FORMAT_Y32); \
 (thisContext->current)[2] = ((0x1000) | ((0x1000) << 16)); \
 gcm_finish_n_commands(thisContext->current, 3); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV308A_POINT, 3); \
 (thisContext->current)[1] = (((0) << 16) | ((offset & 63) >> 2)); \
 (thisContext->current)[2] = (((1) << 16) | (count)); \
 (thisContext->current)[3] = (((1) << 16) | (count)); \
 gcm_finish_n_commands(thisContext->current, 4); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV308A_COLOR, ((count + 1) & ~1)); \
 gcm_finish_n_commands(thisContext->current, 1); \
 pointer = thisContext->current; \
 gcm_finish_n_commands(thisContext->current, ((count + 1) & ~1));

#define rglGcmSetWriteBackEndLabel(thisContext, index, value) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_SEMAPHORE_OFFSET, 1); \
 gcm_emit_at(thisContext->current, 1, 0x10 * index); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_BACK_END_WRITE_SEMAPHORE_RELEASE, 1); \
 gcm_emit_at(thisContext->current, 1, ( value & 0xff00ff00) | ((value >> 16) & 0xff) | (((value >> 0 ) & 0xff) << 16)); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetWaitLabel(thisContext, index, value) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV406E_SEMAPHORE_OFFSET, 1); \
 gcm_emit_at(thisContext->current, 1, 0x10 * index); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV406E_SEMAPHORE_ACQUIRE, 1); \
 gcm_emit_at(thisContext->current, 1, value); \
 gcm_finish_n_commands(thisContext->current, 2);


#define rglGcmSetInvalidateVertexCache(thisContext) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_INVALIDATE_VERTEX_CACHE_FILE, 1); \
 gcm_emit_at(thisContext->current, 1, 0); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_INVALIDATE_VERTEX_FILE, 1); \
 gcm_emit_at(thisContext->current, 1, 0); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_INVALIDATE_VERTEX_FILE, 1); \
 gcm_emit_at(thisContext->current, 1, 0); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_INVALIDATE_VERTEX_FILE, 1); \
 gcm_emit_at(thisContext->current, 1, 0); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetClearSurface(thisContext, mask) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_CLEAR_SURFACE, 1); \
 gcm_emit_at(thisContext->current, 1, mask); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_NO_OPERATION, 1); \
 gcm_emit_at(thisContext->current, 1, 0); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetTextureControl(thisContext, index, enable, minlod, maxlod, maxaniso) \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_CONTROL0 + 0x20 * ((index)))); \
 (thisContext->current)[1] = ((((0) << 2) | ((maxaniso)) << 4) | (((maxlod)) << 7) | (((minlod)) << 19) | ((enable) << 31)); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetTextureRemap(thisContext, index, remap) \
 gcm_emit_method_at(thisContext->current, 0, (CELL_GCM_NV4097_SET_TEXTURE_CONTROL1 + ((index)) * 32), 1); \
 gcm_emit_at(thisContext->current, 1, remap); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetTransferLocation(thisContext, location) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV3062_SET_CONTEXT_DMA_IMAGE_DESTIN, 1); \
 gcm_emit_at(thisContext->current, 1, (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + location)); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmInlineTransfer(thisContext, dstOffset, srcAdr, sizeInWords, location) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV3062_SET_CONTEXT_DMA_IMAGE_DESTIN, 1); \
 gcm_emit_at(thisContext->current, 1, (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + location)); \
 gcm_finish_n_commands(thisContext->current, 2); \
 rglGcmSetInlineTransfer(thisContext, dstOffset, srcAdr, sizeInWords);

#define rglGcmSetClearColor(thisContext, color) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_COLOR_CLEAR_VALUE, 1); \
 gcm_emit_at(thisContext->current, 1, color); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetTextureBorder(thisContext, index, texture, border) \
 (thisContext->current)[0] = (((2) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_OFFSET + ((index)) * 32)); \
 (thisContext->current)[1] = texture->offset; \
 (thisContext->current)[2] = (texture->location + 1) | (texture->cubemap << 2) | (border << 3) | (texture->dimension << 4) | (texture->format << 8) | (texture->mipmap << 16); \
 gcm_finish_n_commands(thisContext->current, 3); \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_IMAGE_RECT + ((index)) * 32)); \
 (thisContext->current)[1] = texture->height | (texture->width << 16); \
 gcm_finish_n_commands(thisContext->current, 2); \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_CONTROL3 + ((index)) * 4)); \
 (thisContext->current)[1] = texture->pitch | (texture->depth << 20); \
 gcm_finish_n_commands(thisContext->current, 2); \
 (thisContext->current)[0] = (((1) << (18)) | (CELL_GCM_NV4097_SET_TEXTURE_CONTROL1 + ((index)) * 32)); \
 (thisContext->current)[1] = texture->remap; \
 gcm_finish_n_commands(thisContext->current, 2);


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
    gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_BLEND_ENABLE, 1); \
    gcm_emit_at(thisContext->current, 1, enable); \
    gcm_finish_n_commands(thisContext->current, 2); \
 } \
}

#define rglGcmSetBlendEnableMrt(thisContext, mrt1, mrt2, mrt3) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_BLEND_ENABLE_MRT, 1); \
 (thisContext->current)[1] = (((mrt1) << 1)|((mrt2) << 2)|((mrt3) << 3)); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetBlendEquation(thisContext, color, alpha) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_BLEND_EQUATION, 1); \
 gcm_emit_at(thisContext->current, 1, (((color)) | (((alpha)) << 16))); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetBlendFunc(thisContext, sfcolor, dfcolor, sfalpha, dfalpha) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_BLEND_FUNC_SFACTOR, 2); \
 (thisContext->current)[1] = (((sfcolor)) | (((sfalpha)) << 16)); \
 (thisContext->current)[2] = (((dfcolor)) | (((dfalpha)) << 16)); \
 gcm_finish_n_commands(thisContext->current, 3);

#define rglGcmSetUserClipPlaneControl(thisContext, plane0, plane1, plane2, plane3, plane4, plane5) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_USER_CLIP_PLANE_CONTROL, 1); \
 (thisContext->current)[1] = ((plane0) | ((plane1) << 4) | ((plane2) << 8) | ((plane3) << 12) | ((plane4) << 16) | ((plane5) << 20)); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetInvalidateTextureCache(thisContext, value) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_INVALIDATE_L2, 1); \
 gcm_emit_at(thisContext->current, 1, value); \
 gcm_finish_n_commands(thisContext->current, 2);

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
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_VIEWPORT_HORIZONTAL, 2); \
 (thisContext->current)[1] = (((x)) | (((w)) << 16)); \
 (thisContext->current)[2] = (((y)) | (((h)) << 16)); \
 gcm_finish_n_commands(thisContext->current, 3); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_CLIP_MIN, 2); \
 (thisContext->current)[1] = (d0.u); \
 (thisContext->current)[2] = (d1.u); \
 gcm_finish_n_commands(thisContext->current, 3); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_VIEWPORT_OFFSET, 8); \
 (thisContext->current)[1] = (o[0].u); \
 (thisContext->current)[2] = (o[1].u); \
 (thisContext->current)[3] = (o[2].u); \
 (thisContext->current)[4] = (o[3].u); \
 (thisContext->current)[5] = (s[0].u); \
 (thisContext->current)[6] = (s[1].u); \
 (thisContext->current)[7] = (s[2].u); \
 (thisContext->current)[8] = (s[3].u); \
 gcm_finish_n_commands(thisContext->current, 9); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_VIEWPORT_OFFSET, 8); \
 (thisContext->current)[1] = (o[0].u); \
 (thisContext->current)[2] = (o[1].u); \
 (thisContext->current)[3] = (o[2].u); \
 (thisContext->current)[4] = (o[3].u); \
 (thisContext->current)[5] = (s[0].u); \
 (thisContext->current)[6] = (s[1].u); \
 (thisContext->current)[7] = (s[2].u); \
 (thisContext->current)[8] = (s[3].u); \
 gcm_finish_n_commands(thisContext->current, 9);

#define rglGcmSetDitherEnable(thisContext, enable) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_DITHER_ENABLE, 1); \
 gcm_emit_at(thisContext->current, 1, enable); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetReferenceCommand(thisContext, ref) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV406E_SET_REFERENCE, 1); \
 gcm_emit_at(thisContext->current, 1, ref); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetZMinMaxControl(thisContext, cullNearFarEnable, zclampEnable, cullIgnoreW) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_ZMIN_MAX_CONTROL, 1); \
 gcm_emit_at(thisContext->current, 1, ((cullNearFarEnable) | ((zclampEnable) << 4) | ((cullIgnoreW)<<8))); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetVertexAttribOutputMask(thisContext, mask) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_VERTEX_ATTRIB_OUTPUT_MASK, 1); \
 gcm_emit_at(thisContext->current, 1, mask); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetNopCommand(thisContext, i, count) \
 for(i = 0;i < count; i++) \
  gcm_emit_at(thisContext->current, i, 0); \
 gcm_finish_n_commands(thisContext->current, count);

#define rglGcmSetAntiAliasingControl(thisContext, enable, alphaToCoverage, alphaToOne, sampleMask) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_ANTI_ALIASING_CONTROL, 1); \
 (thisContext->current)[1] = ((enable) | ((alphaToCoverage) << 4) | ((alphaToOne) << 8) | ((sampleMask) << 16)); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmFifoFinish(ref, offset_bytes) \
 ref = rglGcmFifoPutReference( fifo ); \
 rglGcmFifoFlush( fifo, offset_bytes ); \
 while (rglGcmFifoReferenceInUse(fifo, ref));

#define rglGcmFifoReadReference(fifo) (fifo->lastHWReferenceRead = *((volatile GLuint *)&fifo->dmaControl->Reference))

#define rglGcmFifoFlush(fifo, offsetInBytes) \
 cellGcmAddressToOffset( fifo->ctx.current, ( uint32_t * )&offsetInBytes ); \
 rglGcmFlush(gCellGcmCurrentContext); \
 fifo->dmaControl->Put = offsetInBytes; \
 fifo->lastPutWritten = fifo->ctx.current; \
 fifo->lastSWReferenceFlushed = fifo->lastSWReferenceWritten;

#define rglGcmFlush(thisContext) cellGcmFlushUnsafe(thisContext) 

#define rglGcmSetSurface(thisContext, surface, origin, pixelCenter, log2Width, log2Height) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_CONTEXT_DMA_COLOR_A, 1); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->colorLocation[0]); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_CONTEXT_DMA_COLOR_B, 1); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->colorLocation[1]); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_CONTEXT_DMA_COLOR_C, 2); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->colorLocation[2]); \
 (thisContext->current)[2] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->colorLocation[3]); \
 gcm_finish_n_commands(thisContext->current, 3); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_CONTEXT_DMA_ZETA, 1); \
 (thisContext->current)[1] = (CELL_GCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER + surface->depthLocation); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_SURFACE_FORMAT, 6); \
 (thisContext->current)[1] = ((surface->colorFormat) | ((surface->depthFormat) << 5) | ((surface->type) << 8) | ((surface->antialias) << 12) | ((log2Width) << 16) | ((log2Height) << 24)); \
 (thisContext->current)[2] = (surface->colorPitch[0]); \
 (thisContext->current)[3] = (surface->colorOffset[0]); \
 (thisContext->current)[4] = (surface->depthOffset); \
 (thisContext->current)[5] = (surface->colorOffset[1]); \
 (thisContext->current)[6] = (surface->colorPitch[1]); \
 gcm_finish_n_commands(thisContext->current, 7); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_SURFACE_PITCH_Z, 1); \
 (thisContext->current)[1] = (surface->depthPitch); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_SURFACE_PITCH_C, 4); \
 (thisContext->current)[1] = (surface->colorPitch[2]); \
 (thisContext->current)[2] = (surface->colorPitch[3]); \
 (thisContext->current)[3] = (surface->colorOffset[2]); \
 (thisContext->current)[4] = (surface->colorOffset[3]); \
 gcm_finish_n_commands(thisContext->current, 5); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_SURFACE_COLOR_TARGET, 1); \
 gcm_emit_at(thisContext->current, 1, surface->colorTarget); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_WINDOW_OFFSET, 1); \
 (thisContext->current)[1] = ((surface->x) | ((surface->y) << 16)); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_SURFACE_CLIP_HORIZONTAL, 2); \
 (thisContext->current)[1] = ((surface->x) | ((surface->width) << 16)); \
 (thisContext->current)[2] = ((surface->y) | ((surface->height) << 16));  \
 gcm_finish_n_commands(thisContext->current, 3); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_SHADER_WINDOW, 1); \
 (thisContext->current)[1] = ((surface->height - (((surface->height) & 0x1000) >> 12)) | ((origin) << 12) | ((pixelCenter) << 16)); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSend(dstId, dstOffset, pitch, src, size) \
   GLuint id = gmmAlloc(size); \
   memcpy( gmmIdToAddress(id), (src), size ); \
   rglGcmTransferData( dstId, dstOffset, size, id, 0, size, size, 1 ); \
   gmmFree( id )

#define rglGcmSetUpdateFragmentProgramParameter(thisContext, offset, location) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_SHADER_PROGRAM, 1); \
 gcm_emit_at(thisContext->current, 1, ((location+1) | (offset))); \
 gcm_finish_n_commands(thisContext->current, 2);

#define rglGcmSetBlendColor(thisContext, color, color2) \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_BLEND_COLOR, 1); \
 gcm_emit_at(thisContext->current, 1, color); \
 gcm_finish_n_commands(thisContext->current, 2); \
 gcm_emit_method_at(thisContext->current, 0, CELL_GCM_NV4097_SET_BLEND_COLOR2, 1); \
 gcm_emit_at(thisContext->current, 1, color2); \
 gcm_finish_n_commands(thisContext->current, 2);

#endif
