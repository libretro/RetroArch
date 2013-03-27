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

static inline void rglGcmFifoGlViewport(void *data, GLclampf zNear, GLclampf zFar)
{
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

   GCM_FUNC( cellGcmSetViewport, clipX0, clipY0, clipX1 - clipX0,
         clipY1 - clipY0, zNear, zFar, scale, offset );
}

static inline void rglGcmFifoGlTransferDataVidToVid
(
 GLuint dstVidId,   
 GLuint dstVidIdOffset,
 GLuint dstPitch,
 GLuint dstX,
 GLuint dstY,
 GLuint srcVidId, 
 GLuint srcVidIdOffset,
 GLuint srcPitch,
 GLuint srcX,
 GLuint srcY,
 GLuint width,            // size in pixel
 GLuint height,
 GLuint bytesPerPixel
 )
{
   GLuint dstOffset = gmmIdToOffset(dstVidId) + dstVidIdOffset;
   GLuint srcOffset = gmmIdToOffset(srcVidId) + srcVidIdOffset;

   GCM_FUNC( cellGcmSetTransferImage, CELL_GCM_TRANSFER_LOCAL_TO_LOCAL, dstOffset, dstPitch, dstX, dstY, srcOffset, srcPitch, srcX, srcY, width, height, bytesPerPixel );
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

static inline GLuint rglGcmMapMagTextureFilter( GLenum filter )
{
   switch ( filter )
   {
      case GL_NEAREST:
         return CELL_GCM_TEXTURE_NEAREST;
         break;
      case GL_LINEAR:
         return CELL_GCM_TEXTURE_LINEAR;
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

static inline void rglGcmMapTextureFormat( GLuint internalFormat, uint8_t *gcmFormat, uint32_t *remap )
{
   *gcmFormat = 0;

   switch (internalFormat)
   {
      case RGLGCM_ALPHA8:                 // in_rgba = xxAx, out_rgba = 000A
         {
            *gcmFormat =  CELL_GCM_TEXTURE_B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_ZERO,
                  CELL_GCM_TEXTURE_REMAP_ZERO,
                  CELL_GCM_TEXTURE_REMAP_ZERO );

         }
         break;
      case RGLGCM_ALPHA16:                // in_rgba = xAAx, out_rgba = 000A
         {
            *gcmFormat =  CELL_GCM_TEXTURE_X16;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_ZERO,
                  CELL_GCM_TEXTURE_REMAP_ZERO,
                  CELL_GCM_TEXTURE_REMAP_ZERO );

         }
         break;
      case RGLGCM_HILO8:                  // in_rgba = HLxx, out_rgba = HL11
         {
            *gcmFormat =  CELL_GCM_TEXTURE_COMPRESSED_HILO8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_ONE );

         }
         break;
      case RGLGCM_HILO16:                 // in_rgba = HLxx, out_rgba = HL11
         {
            *gcmFormat =  CELL_GCM_TEXTURE_Y16_X16;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_ONE );

         }
         break;
      case RGLGCM_ARGB8:                  // in_rgba = RGBA, out_rgba = RGBA
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_BGRA8:                  // in_rgba = GRAB, out_rgba = RGBA ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_RGBA8:                  // in_rgba = GBAR, out_rgba = RGBA ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );
         }
         break;
      case RGLGCM_ABGR8:                  // in_rgba = BGRA, out_rgba = RGBA  ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_RGBX8:                  // in_rgba = BGRA, out_rgba = RGB1  ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_XBGR8:                  // in_rgba = BGRA, out_rgba = RGB1  ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_FLOAT_R32:              // in_rgba = Rxxx, out_rgba = R001
         {
            *gcmFormat =  CELL_GCM_TEXTURE_X32_FLOAT;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_ZERO,
                  CELL_GCM_TEXTURE_REMAP_ZERO );

         }
         break;
      case RGLGCM_RGB5_A1_SCE:          // in_rgba = RGBA, out_rgba = RGBA
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A1R5G5B5;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XXXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_RGB565_SCE:          // in_rgba = RGBA, out_rgba = RGBA
         {
            *gcmFormat =  CELL_GCM_TEXTURE_R5G6B5;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XXXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      default:
         break;
   }
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
   // set write label command in push buffer, and wait
   // NOTE: this is for RSX to wailt
   GCM_FUNC( cellGcmSetWriteBackEndLabel, RGLGCM_UTIL_LABEL_INDEX, rglGcmState_i.labelValue );
   GCM_FUNC( cellGcmSetWaitLabel, RGLGCM_UTIL_LABEL_INDEX, rglGcmState_i.labelValue );

   // increment label value for next time. 
   rglGcmState_i.labelValue++; 

   // make sure the entire pipe in clear not just the front end 
   // Utility function that does GPU 'finish'.
   GCM_FUNC( cellGcmSetWriteBackEndLabel, RGLGCM_UTIL_LABEL_INDEX, 
         rglGcmState_i.labelValue );
   cellGcmFlush();

   while( *(cellGcmGetLabelAddress( RGLGCM_UTIL_LABEL_INDEX)) != rglGcmState_i.labelValue)
      sys_timer_usleep(30);

   rglGcmState_i.labelValue++;
}

// Sets the source and destination factor used for blending.
static inline void rglGcmFifoGlBlendFunc( rglGcmEnum sf, rglGcmEnum df, rglGcmEnum sfAlpha, rglGcmEnum dfAlpha )
{
   // syntax check
   switch ( sf )
   {
      case RGLGCM_ZERO:
      case RGLGCM_ONE:
      case RGLGCM_SRC_COLOR:
      case RGLGCM_ONE_MINUS_SRC_COLOR:
      case RGLGCM_SRC_ALPHA:
      case RGLGCM_ONE_MINUS_SRC_ALPHA:
      case RGLGCM_DST_ALPHA:
      case RGLGCM_ONE_MINUS_DST_ALPHA:
      case RGLGCM_DST_COLOR:
      case RGLGCM_ONE_MINUS_DST_COLOR:
      case RGLGCM_SRC_ALPHA_SATURATE:
      case RGLGCM_CONSTANT_COLOR:
      case RGLGCM_ONE_MINUS_CONSTANT_COLOR:
      case RGLGCM_CONSTANT_ALPHA:
      case RGLGCM_ONE_MINUS_CONSTANT_ALPHA:
         break;
      default:
         break;
   }
   switch ( sfAlpha )
   {
      case RGLGCM_ZERO:
      case RGLGCM_ONE:
      case RGLGCM_SRC_COLOR:
      case RGLGCM_ONE_MINUS_SRC_COLOR:
      case RGLGCM_SRC_ALPHA:
      case RGLGCM_ONE_MINUS_SRC_ALPHA:
      case RGLGCM_DST_ALPHA:
      case RGLGCM_ONE_MINUS_DST_ALPHA:
      case RGLGCM_DST_COLOR:
      case RGLGCM_ONE_MINUS_DST_COLOR:
      case RGLGCM_SRC_ALPHA_SATURATE:
      case RGLGCM_CONSTANT_COLOR:
      case RGLGCM_ONE_MINUS_CONSTANT_COLOR:
      case RGLGCM_CONSTANT_ALPHA:
      case RGLGCM_ONE_MINUS_CONSTANT_ALPHA:
         break;
      default:
         break;
   }

   switch ( df )
   {
      case RGLGCM_ZERO:
      case RGLGCM_ONE:
      case RGLGCM_SRC_COLOR:
      case RGLGCM_ONE_MINUS_SRC_COLOR:
      case RGLGCM_SRC_ALPHA:
      case RGLGCM_ONE_MINUS_SRC_ALPHA:
      case RGLGCM_DST_ALPHA:
      case RGLGCM_ONE_MINUS_DST_ALPHA:
      case RGLGCM_DST_COLOR:
      case RGLGCM_ONE_MINUS_DST_COLOR:
      case RGLGCM_SRC_ALPHA_SATURATE:
      case RGLGCM_CONSTANT_COLOR:
      case RGLGCM_ONE_MINUS_CONSTANT_COLOR:
      case RGLGCM_CONSTANT_ALPHA:
      case RGLGCM_ONE_MINUS_CONSTANT_ALPHA:
         break;
      default:
         break;
   }
   switch ( dfAlpha )
   {
      case RGLGCM_ZERO:
      case RGLGCM_ONE:
      case RGLGCM_SRC_COLOR:
      case RGLGCM_ONE_MINUS_SRC_COLOR:
      case RGLGCM_SRC_ALPHA:
      case RGLGCM_ONE_MINUS_SRC_ALPHA:
      case RGLGCM_DST_ALPHA:
      case RGLGCM_ONE_MINUS_DST_ALPHA:
      case RGLGCM_DST_COLOR:
      case RGLGCM_ONE_MINUS_DST_COLOR:
      case RGLGCM_SRC_ALPHA_SATURATE:
      case RGLGCM_CONSTANT_COLOR:
      case RGLGCM_ONE_MINUS_CONSTANT_COLOR:
      case RGLGCM_CONSTANT_ALPHA:
      case RGLGCM_ONE_MINUS_CONSTANT_ALPHA:
         break;
      default:
         break;
   }

   GCM_FUNC( cellGcmSetBlendFunc, sf, df, sfAlpha, dfAlpha );
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

// Flush the current FIFO.
static inline void rglGcmFifoGlFlush (void)
{
   GCM_FUNC_NO_ARGS( cellGcmSetInvalidateVertexCache );
   rglGcmFifoFlush( &rglGcmState_i.fifo );
}

// Set blending constant, used for certain blending modes.
static inline void rglGcmFifoGlBlendColor( GLfloat r, GLfloat g, GLfloat b, GLfloat a )
{
   rglGcmBlendState *blend = &rglGcmState_i.state.blend;
   GLuint hwColor;

   // syntax check
   blend->r = r;
   blend->g = g;
   blend->b = b;
   blend->a = a;

   // program hw
   switch ( rglGcmState_i.renderTarget.colorFormat )
   {
      case RGLGCM_ARGB8:
         RGLGCM_CALC_COLOR_LE_ARGB8( &hwColor, r, g, b, a );
         GCM_FUNC( cellGcmSetBlendColor, hwColor, hwColor );
         break;
      case RGLGCM_NONE:
      case RGLGCM_FLOAT_R32:
         // no native support support
         break;
      default:
         break;
   }
}

// Set the current blend equation.
static inline void rglGcmFifoGlBlendEquation( rglGcmEnum mode, rglGcmEnum modeAlpha )
{
   // syntax check
   switch ( mode )
   {
      case RGLGCM_FUNC_ADD:
      case RGLGCM_MIN:
      case RGLGCM_MAX:
      case RGLGCM_FUNC_SUBTRACT:
      case RGLGCM_FUNC_REVERSE_SUBTRACT:
         break;
      default:
         break;
   }
   switch ( modeAlpha )
   {
      case RGLGCM_FUNC_ADD:
      case RGLGCM_MIN:
      case RGLGCM_MAX:
      case RGLGCM_FUNC_SUBTRACT:
      case RGLGCM_FUNC_REVERSE_SUBTRACT:
         break;
      default:
         break;
   }

   GCM_FUNC( cellGcmSetBlendEquation, mode, modeAlpha );
}

static inline void rglGcmFifoGlVertexAttribPointer
(
 GLuint          index,
 GLint           size,
 rglGcmEnum       type,
 GLboolean       normalized,
 GLsizei         stride,
 GLushort        frequency,
 GLuint          offset
 )
{
   // syntax check
   switch ( size )
   {
      case 0: // disable
         stride = 0;
         normalized = 0;
         type = RGLGCM_FLOAT;
         offset = 0;
         break;
      case 1:
      case 2:
      case 3:
      case 4:
         // valid
         break;
      default:
         break;
   }

   // mapping to native types
   uint8_t gcmType = 0;
   switch ( type )
   {
      case RGLGCM_UNSIGNED_BYTE:
         if (normalized)
            gcmType = CELL_GCM_VERTEX_UB;
         else
            gcmType = CELL_GCM_VERTEX_UB256;
         break;

      case RGLGCM_SHORT:
         gcmType = normalized ? CELL_GCM_VERTEX_S1 : CELL_GCM_VERTEX_S32K;
         break;

      case RGLGCM_FLOAT:
         gcmType = CELL_GCM_VERTEX_F;
         break;

      case RGLGCM_HALF_FLOAT:
         gcmType = CELL_GCM_VERTEX_SF;
         break;

      case RGLGCM_CMP:
         size = 1;   // required for this format
         gcmType = CELL_GCM_VERTEX_CMP;
         break;

      default:
         break;
   }

   GCM_FUNC( cellGcmSetVertexDataArray, index, frequency, stride, size, gcmType, CELL_GCM_LOCATION_LOCAL, offset );
}

static inline void rglGcmFifoGlEnable( rglGcmEnum cap )
{
   switch (cap)
   {
      case RGLGCM_BLEND:
         GCM_FUNC( cellGcmSetBlendEnable,  RGLGCM_TRUE );
         GCM_FUNC( cellGcmSetBlendEnableMrt, RGLGCM_TRUE, RGLGCM_TRUE, RGLGCM_TRUE );
         break;
      case RGLGCM_DITHER:
         GCM_FUNC( cellGcmSetDitherEnable, RGLGCM_TRUE );
         break;
      case RGLGCM_PSHADER_SRGB_REMAPPING:
         GCM_FUNC( cellGcmSetFragmentProgramGammaEnable, RGLGCM_TRUE );
         break;
      default:
         break;
   }
}

static inline void rglGcmFifoGlDisable( rglGcmEnum cap )
{
   switch (cap)
   {
      case RGLGCM_BLEND:
         GCM_FUNC( cellGcmSetBlendEnable, RGLGCM_FALSE );
         GCM_FUNC( cellGcmSetBlendEnableMrt, RGLGCM_FALSE, RGLGCM_FALSE, RGLGCM_FALSE );
         break;
      case RGLGCM_DITHER:
         GCM_FUNC( cellGcmSetDitherEnable, RGLGCM_FALSE );
         break;
      case RGLGCM_PSHADER_SRGB_REMAPPING:
         GCM_FUNC( cellGcmSetFragmentProgramGammaEnable, RGLGCM_FALSE );
         break;
      default:
         break;
   }
}

// Look up the memory location of a buffer object (VBO, PBO)
static inline GLuint rglGcmGetBufferObjectOrigin (GLuint buffer)
{
   rglBufferObject *bufferObject = (rglBufferObject*)_CurrentContext->bufferObjectNameSpace.data[buffer];
   rglGcmBufferObject *gcmBuffer = (rglGcmBufferObject *)bufferObject->platformBufferObject;
   return gcmBuffer->bufferId;
}
