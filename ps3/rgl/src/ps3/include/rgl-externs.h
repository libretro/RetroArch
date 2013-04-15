GLboolean rglGcmFifoReferenceInUse (void *data, GLuint reference);
GLuint rglGcmFifoPutReference (void *data);
void rglGcmGetTileRegionInfo (void *data, GLuint *address, GLuint *size);
GLboolean rglGcmTryResizeTileRegion( GLuint address, GLuint size, void *data);

int32_t rglOutOfSpaceCallback (void *data, uint32_t spaceInWords);
void rglGcmFifoGlSetRenderTarget (const void *args);
void rglCreatePushBuffer (void *data);
void rglGcmSend( unsigned int dstId, unsigned dstOffset, unsigned int pitch, const char *src, unsigned int size );
void rglGcmFreeTiledSurface (GLuint bufferId);
