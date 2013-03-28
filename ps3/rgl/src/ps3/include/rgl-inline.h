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

// Look up the memory location of a buffer object (VBO, PBO)
static inline GLuint rglGcmGetBufferObjectOrigin (GLuint buffer)
{
   rglBufferObject *bufferObject = (rglBufferObject*)_CurrentContext->bufferObjectNameSpace.data[buffer];
   rglGcmBufferObject *gcmBuffer = (rglGcmBufferObject *)bufferObject->platformBufferObject;
   return gcmBuffer->bufferId;
}
