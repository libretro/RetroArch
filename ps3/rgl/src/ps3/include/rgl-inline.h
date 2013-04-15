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

#define SUBPIXEL_BITS 12
#define SUBPIXEL_ADJUST (0.5/(1<<SUBPIXEL_BITS))

#define rglGcmSetVertexData4f(thisContext, index, v) \
 thisContext->current[0] = (((4) << (18)) | ((0x00001c00) + (index) * 16)); \
 __builtin_memcpy(&thisContext->current[1], v, sizeof(float)*4); \
 thisContext->current += 5;

#define rglGcmSetVertexDataArray(thisContext, index, frequency, stride, size, type, location, offset) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001740) + ((index)) * 4)); \
 (thisContext->current)[1] = ((((frequency)) << 16) | (((stride)) << 8) | (((size)) << 4) | ((type))); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001680) + ((index)) * 4)); \
 (thisContext->current)[1] = ((((location)) << 31) | (offset)); \
 (thisContext->current) += 2;

#define rglGcmSetInlineTransferPointer(thisContext, offset, count, pointer) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x0000630C))); \
 (thisContext->current)[1] = (offset & ~63); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((2) << (18)) | ((0x00006300))); \
 (thisContext->current)[1] = (CELL_GCM_TRANSFER_SURFACE_FORMAT_Y32); \
 (thisContext->current)[2] = ((0x1000) | ((0x1000) << 16)); \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((3) << (18)) | ((0x0000A304))); \
 (thisContext->current)[1] = (((0) << 16) | ((offset & 63) >> 2)); \
 (thisContext->current)[2] = (((1) << 16) | (count)); \
 (thisContext->current)[3] = (((1) << 16) | (count)); \
 (thisContext->current) += 4; \
 thisContext->current[0] = ((((count + 1) & ~1) << (18)) | ((0x0000A400))); \
 thisContext->current += 1; \
 pointer = thisContext->current; \
 thisContext->current += ((count + 1) & ~1);

#define rglGcmSetWriteBackEndLabel(thisContext, index, value) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001d6c))); \
 (thisContext->current)[1] = 0x10 * index; /* offset */ \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001d70))); \
 (thisContext->current)[1] = ( value & 0xff00ff00) | ((value >> 16) & 0xff) | (((value >> 0 ) & 0xff) << 16); \
 (thisContext->current) += 2;

#define rglGcmSetWaitLabel(thisContext, index, value) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00000064))); \
 (thisContext->current)[1] = 0x10 * index; \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00000068))); \
 (thisContext->current)[1] = (value); \
 (thisContext->current) += 2;

#define rglGcmSetInvalidateVertexCache(thisContext) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001710))); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001714))); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001714))); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001714))); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2;

#define rglGcmSetClearSurface(thisContext, mask) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001d94))); \
 (thisContext->current)[1] = (mask); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00000100))); \
 (thisContext->current)[1] = 0; \
 (thisContext->current) += 2;

#define rglGcmSetTextureControl(thisContext, index, enable, minlod, maxlod, maxaniso) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001a0c) + 0x20 * ((index)))); \
 (thisContext->current)[1] = ((((0) << 2) | ((maxaniso)) << 4) | (((maxlod)) << 7) | (((minlod)) << 19) | ((enable) << 31)); \
 (thisContext->current) += 2;

#define rglGcmSetTextureRemap(thisContext, index, remap) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001a10) + ((index)) * 32)); \
 (thisContext->current)[1] = (remap); \
 (thisContext->current) += 2;

#define rglGcmSetTransferLocation(thisContext, location) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00006188))); \
 (thisContext->current)[1] = ((0xFEED0000) + location); \
 (thisContext->current) += 2;

#define rglGcmInlineTransfer(thisContext, dstOffset, srcAdr, sizeInWords, location) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00006188))); \
 (thisContext->current)[1] = ((0xFEED0000) + location); \
 (thisContext->current) += 2; \
  cellGcmSetInlineTransferUnsafeInline(thisContext, dstOffset, srcAdr, sizeInWords);

#define rglGcmSetClearColor(thisContext, color) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001d90))); \
 (thisContext->current)[1] = (color); \
 (thisContext->current) += 2;

#define rglGcmSetTextureBorder(thisContext, index, texture, border) \
 uint32_t format, offset, control1, control3, imagerect; \
 offset = texture->offset; \
 format = (texture->location + 1) | (texture->cubemap << 2) | (border << 3) | (texture->dimension << 4) | (texture->format << 8) | (texture->mipmap << 16); \
 imagerect = texture->height | (texture->width << 16); \
 control1 = texture->remap; \
 control3 = texture->pitch | (texture->depth << 20); \
 (thisContext->current)[0] = (((2) << (18)) | ((0x00001a00) + ((index)) * 32)); \
 (thisContext->current)[1] = (offset); \
 (thisContext->current)[2] = (format); \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001a18) + ((index)) * 32)); \
 (thisContext->current)[1] = (imagerect); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001840) + ((index)) * 4)); \
 (thisContext->current)[1] = (control3); \
 (thisContext->current) += 2; \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001a10) + ((index)) * 32)); \
 (thisContext->current)[1] = (control1); \
 (thisContext->current) += 2;

#define rglGcmSetUserClipPlaneControl(thisContext, plane0, plane1, plane2, plane3, plane4, plane5) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001478))); \
 (thisContext->current)[1] = ((plane0) | ((plane1) << 4) | ((plane2) << 8) | ((plane3) << 12) | ((plane4) << 16) | ((plane5) << 20)); \
 (thisContext->current) += 2;

#define rglGcmSetInvalidateTextureCache(thisContext, value) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001fd8))); \
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
 (thisContext->current)[0] = (((2) << (18)) | ((0x00000a00))); \
 (thisContext->current)[1] = (((x)) | (((w)) << 16)); \
 (thisContext->current)[2] = (((y)) | (((h)) << 16)); \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((2) << (18)) | ((0x00000394))); \
 (thisContext->current)[1] = (d0.u); \
 (thisContext->current)[2] = (d1.u); \
 (thisContext->current) += 3; \
 (thisContext->current)[0] = (((8) << (18)) | ((0x00000a20))); \
 (thisContext->current)[1] = (o[0].u); \
 (thisContext->current)[2] = (o[1].u); \
 (thisContext->current)[3] = (o[2].u); \
 (thisContext->current)[4] = (o[3].u); \
 (thisContext->current)[5] = (s[0].u); \
 (thisContext->current)[6] = (s[1].u); \
 (thisContext->current)[7] = (s[2].u); \
 (thisContext->current)[8] = (s[3].u); \
 (thisContext->current) += 9; \
 (thisContext->current)[0] = (((8) << (18)) | ((0x00000a20))); \
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
 (thisContext->current)[0] = (((1) << (18)) | ((0x00000300))); \
 (thisContext->current)[1] = (enable); \
 (thisContext->current) += 2;

#define rglGcmSetReferenceCommand(thisContext, ref) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00000050))); \
 (thisContext->current)[1] = (ref); \
 (thisContext->current) += 2; 

#define rglGcmSetZMinMaxControl(thisContext, cullNearFarEnable, zclampEnable, cullIgnoreW) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001d78))); \
 (thisContext->current)[1] = ((cullNearFarEnable) | ((zclampEnable) << 4) | ((cullIgnoreW)<<8)); \
 (thisContext->current) += 2;

#define rglGcmSetVertexAttribOutputMask(thisContext, mask) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001ff4))); \
 (thisContext->current)[1] = (mask); \
 (thisContext->current) += 2;

#define rglGcmSetNopCommand(thisContext, i, count) \
 for(i=0;i<count;i++) \
  thisContext->current[i] = 0; \
 thisContext->current += count;

#define rglGcmSetAntiAliasingControl(thisContext, enable, alphaToCoverage, alphaToOne, sampleMask) \
 (thisContext->current)[0] = (((1) << (18)) | ((0x00001d7c))); \
 (thisContext->current)[1] = ((enable) | ((alphaToCoverage) << 4) | ((alphaToOne) << 8) | ((sampleMask) << 16)); \
 (thisContext->current) += 2; 

static inline void rglGcmSetDrawArrays(struct CellGcmContextData *thisContext, uint8_t mode,
      uint32_t first, uint32_t count)
{
   uint32_t lcount;

   --count;
   lcount = count & 0xff;
   count >>= 8;

   uint32_t loop, rest;
   loop = count / (2047);
   rest = count % (2047);

   (thisContext->current)[0] = (((3) << (18)) | ((0x00001714)) | (0x40000000));
   (thisContext->current)[1] = 0;
   (thisContext->current)[2] = 0;
   (thisContext->current)[3] = 0; ; (thisContext->current) += 4;

   (thisContext->current)[0] = (((1) << (18)) | ((0x00001808)));
   (thisContext->current)[1] = ((mode));
   (thisContext->current) += 2;
   
   (thisContext->current)[0] = (((1) << (18)) | ((0x00001814)));
   (thisContext->current)[1] = ((first) | ((lcount)<<24));
   (thisContext->current) += 2;
   first += lcount + 1;

   uint32_t i,j;

   for(i=0;i<loop;i++)
   {
      thisContext->current[0] = ((((2047)) << (18)) | ((0x00001814)) | (0x40000000));
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
      thisContext->current[0] = (((rest) << (18)) | ((0x00001814)) | (0x40000000));
      thisContext->current++;

      for(j=0;j<rest;j++)
      {
         thisContext->current[0] = ((first) | ((255U)<<24));
         thisContext->current++;
         first += 256;
      }
   }

   (thisContext->current)[0] = (((1) << (18)) | ((0x00001808)));
   (thisContext->current)[1] = (0);
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
   (thisContext->current)[0] = (((1) << (18)) | ((0x00006188)));
   (thisContext->current)[1] = 0xFEED0000; /* CELL_GCM_TRANSFER_LOCAL_TO_LOCAL */
   (thisContext->current) += 2;

   (thisContext->current)[0] = (((1) << (18)) | ((0x0000C184)));
   (thisContext->current)[1] = 0xFEED0000; /* CELL_GCM_TRANSFER_LOCAL_TO_LOCAL */

   (thisContext->current) += 2;

   (thisContext->current)[0] = (((1) << (18)) | ((0x0000C198)));
   (thisContext->current)[1] = ((0x313371C3));
   (thisContext->current) += 2;

   uint32_t srcFormat = 0;
   uint32_t dstFormat = 0;
   uint32_t x;
   uint32_t y;
   uint32_t finalDstX;
   uint32_t finalDstY;

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

         (thisContext->current)[0] = (((1) << (18)) | ((0x0000630C)));
         (thisContext->current)[1] = dstOffset + dstBlockOffset;
         (thisContext->current) += 2;

         (thisContext->current)[0] = (((2) << (18)) | ((0x00006300)));
         (thisContext->current)[1] = (dstFormat);
         (thisContext->current)[2] = ((dstPitch) | ((dstPitch) << 16));
         (thisContext->current) += 3;

         (thisContext->current)[0] = (((9) << (18)) | ((0x0000C2FC)));
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

         (thisContext->current)[0] = (((4) << (18)) | ((0x0000C400)));
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
inline static void RGLGCM_CALC_COLOR_LE_ARGB8( GLuint *color0, const GLfloat r, const GLfloat g, const GLfloat b, const GLfloat a )
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

   while( *(cellGcmGetLabelAddress( RGLGCM_UTIL_LABEL_INDEX)) != rglGcmState_i.labelValue)
      sys_timer_usleep(30);

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
      rglPrintIt((( uint32_t* )rglGcmState_i.fifo.current )[i] );
}

// prints the last numWords of the command fifo
static inline void rglPrintFifoFromGet( unsigned int numWords ) 
{
   for ( int i = -numWords; i <= -1; i++ )
      rglPrintIt((( uint32_t* )rglGcmState_i.fifo.lastGetRead )[i] );
}

// Determine whether a given location in a command buffer has been passed, by 
// using reference markers.
static inline GLboolean rglGcmFifoGlTestFenceRef (const GLuint ref)
{
   rglGcmFifo *fifo = &rglGcmState_i.fifo;
   return rglGcmFifoReferenceInUse( fifo, ref );
}

// Add a reference marker to the command buffer to determine whether a location 
// in the command buffer has been passed
static inline void rglGcmFifoGlIncFenceRef (GLuint *ref)
{
   rglGcmFifo *fifo = &rglGcmState_i.fifo;
   *ref = rglGcmFifoPutReference( fifo );
}

// Look up the memory location of a buffer object (VBO, PBO)
static inline GLuint rglGcmGetBufferObjectOrigin (GLuint buffer)
{
   rglBufferObject *bufferObject = (rglBufferObject*)_CurrentContext->bufferObjectNameSpace.data[buffer];
   rglGcmBufferObject *gcmBuffer = (rglGcmBufferObject *)bufferObject->platformBufferObject;
   return gcmBuffer->bufferId;
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
   struct CellGcmContextData *thisContext = gCellGcmCurrentContext;
   GLuint dstOffset = gmmIdToOffset(dstId) + dstIdOffset;
   GLuint srcOffset = gmmIdToOffset(srcId) + srcIdOffset;

   (thisContext->current)[0] = (((2) << (18)) | ((0x00002184)));
   (thisContext->current)[1] = 0xFEED0000; /* CELL_GCM_TRANSFER_LOCAL_TO_LOCAL */
   (thisContext->current)[2] = 0xFEED0000; /* CELL_GCM_TRANSFER_LOCAL_TO_LOCAL */
   (thisContext->current) += 3;

   uint32_t colCount;
   uint32_t rows;
   uint32_t cols;

   if ((srcPitch == bytesPerRow) && (dstPitch == bytesPerRow))
   {
      bytesPerRow *= rowCount;
      rowCount = 1;
      srcPitch = 0;
      dstPitch = 0;
   }

   if ((srcPitch < CL0039_MIN_PITCH) || (srcPitch > CL0039_MAX_PITCH) ||
         (dstPitch < CL0039_MIN_PITCH) || (dstPitch > CL0039_MAX_PITCH))
   {
      while(--rowCount >= 0)
      {
         for(colCount = bytesPerRow; colCount>0; colCount -= cols)
         {
            cols = (colCount > CL0039_MAX_LINES) ? CL0039_MAX_LINES : colCount;

            (thisContext->current)[0] = (((8) << (18)) | ((0x0000230C)));
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
   }
   else
   {
      for(;rowCount>0; rowCount -= rows)
      {
         rows = (rowCount > CL0039_MAX_ROWS) ? CL0039_MAX_ROWS : rowCount;

         for(colCount = bytesPerRow; colCount>0; colCount -= cols)
         {
            cols = (colCount > CL0039_MAX_LINES) ? CL0039_MAX_LINES : colCount;

            (thisContext->current)[0] = (((8) << (18)) | ((0x0000230C)));
            (thisContext->current)[1] = (srcOffset + (bytesPerRow - colCount));
            (thisContext->current)[2] = (dstOffset + (bytesPerRow - colCount));
            (thisContext->current)[3] = (srcPitch);
            (thisContext->current)[4] = (dstPitch);
            (thisContext->current)[5] = (cols);
            (thisContext->current)[6] = (rows);
            (thisContext->current)[7] = (((1) << 8) | (1));
            (thisContext->current)[8] = (0);
            (thisContext->current) += 9;
         }

         srcOffset += rows * srcPitch;
         dstOffset += rows * dstPitch;
      }
   }

   (thisContext->current)[0] = (((1) << (18)) | ((0x00002310)));
   (thisContext->current)[1] = (0);
   (thisContext->current) += 2;
}
