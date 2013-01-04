#ifndef _RGL_REPORT_INTERNAL_H
#define _RGL_REPORT_INTERNAL_H

#include "../export/RGL/rgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   GLenum e;
   const char *s;
} RGLenumMap;

const char *rglMapLookupEnum( const RGLenumMap* map, unsigned int count, GLenum e );
GLenum rglMapLookupString( const RGLenumMap* map, unsigned int count, const char *s );

#define _RGL_MAP_LOOKUP_ENUM(MAP,ENUM) rglMapLookupEnum(MAP,sizeof(MAP)/sizeof(MAP[0]),ENUM)
#define _RGL_MAP_LOOKUP_STRING(MAP,STRING) rglMapLookupString(MAP,sizeof(MAP)/sizeof(MAP[0]),STRING)

const char *rglGetGLEnumName( GLenum e );
const char *rglGetGLErrorName( GLenum e );

#ifdef __cplusplus
}
#endif

#endif
