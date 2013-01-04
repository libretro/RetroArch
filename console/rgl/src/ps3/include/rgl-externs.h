void rglGcmFifoFinish (void *data);
GLboolean rglGcmFifoReferenceInUse (void *data, GLuint reference);
GLuint rglGcmFifoPutReference (void *data);
void rglGcmFifoFlush (void *data);
uint32_t *rglGcmFifoWaitForFreeSpace (void *data, GLuint spaceInWords);
void rglGcmGetTileRegionInfo (void *data, GLuint *address, GLuint *size);
GLboolean rglGcmTryResizeTileRegion( GLuint address, GLuint size, void *data);

void rglGcmTransferData (GLuint dstId, GLuint dstIdOffset, 
 GLint dstPitch, GLuint srcId, GLuint srcIdOffset,
 GLint srcPitch, GLint bytesPerRow, GLint rowCount);

int32_t rglOutOfSpaceCallback (struct CellGcmContextData* fifoContext, uint32_t spaceInWords);
void rglGcmFifoGlSetRenderTarget (rglGcmRenderTargetEx const * const args);
void rglpFifoGlFinish (void);
void rglCreatePushBuffer (void *data);
void rglSetDefaultValuesFP (void *data);
void rglSetDefaultValuesVP (void *data);
void rglGcmSend( unsigned int dstId, unsigned dstOffset, unsigned int pitch, const char *src, unsigned int size );
void rglGcmMemcpy( const GLuint dstId, unsigned dstOffset, unsigned int pitch, const GLuint srcId, GLuint srcOffset, unsigned int size );
void rglPlatformValidateTextureResources (void *data);
void rglSetNativeCgFragmentProgram(const void *data);
void rglGcmFreeTiledSurface (GLuint bufferId);

void rglGcmCopySurface(const void *data, GLuint srcX, GLuint srcY,
      const void *data_dst, GLuint dstX, GLuint dstY,
      GLuint width, GLuint height, GLboolean writeSync);
void rglSetNativeCgVertexProgram (const void *data);
