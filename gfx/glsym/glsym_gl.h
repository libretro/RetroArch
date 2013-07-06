#ifndef RGLGEN_DECL_H__
#define RGLGEN_DECL_H__
#ifdef GL_APIENTRY
typedef void (GL_APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#else
typedef void (APIENTRY *RGLGENGLDEBUGPROCARB)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
typedef void (APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);
#endif
#ifndef GL_OES_EGL_image
typedef void *GLeglImageOES;
#endif
typedef void (APIENTRYP RGLSYMGLBLENDCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONPROC) (GLenum mode);
typedef void (APIENTRYP RGLSYMGLDRAWRANGEELEMENTSPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (APIENTRYP RGLSYMGLTEXIMAGE3DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP RGLSYMGLTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLCOLORTABLEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table);
typedef void (APIENTRYP RGLSYMGLCOLORTABLEPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLCOLORTABLEPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLCOPYCOLORTABLEPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (APIENTRYP RGLSYMGLGETCOLORTABLEPROC) (GLenum target, GLenum format, GLenum type, GLvoid *table);
typedef void (APIENTRYP RGLSYMGLGETCOLORTABLEPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETCOLORTABLEPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLCOLORSUBTABLEPROC) (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOPYCOLORSUBTABLEPROC) (GLenum target, GLsizei start, GLint x, GLint y, GLsizei width);
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONFILTER1DPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *image);
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONFILTER2DPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image);
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat params);
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERIPROC) (GLenum target, GLenum pname, GLint params);
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLCOPYCONVOLUTIONFILTER1DPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (APIENTRYP RGLSYMGLCOPYCONVOLUTIONFILTER2DPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLGETCONVOLUTIONFILTERPROC) (GLenum target, GLenum format, GLenum type, GLvoid *image);
typedef void (APIENTRYP RGLSYMGLGETCONVOLUTIONPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETCONVOLUTIONPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETSEPARABLEFILTERPROC) (GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span);
typedef void (APIENTRYP RGLSYMGLSEPARABLEFILTER2DPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *row, const GLvoid *column);
typedef void (APIENTRYP RGLSYMGLGETHISTOGRAMPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
typedef void (APIENTRYP RGLSYMGLGETHISTOGRAMPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETHISTOGRAMPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETMINMAXPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
typedef void (APIENTRYP RGLSYMGLGETMINMAXPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETMINMAXPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLHISTOGRAMPROC) (GLenum target, GLsizei width, GLenum internalformat, GLboolean sink);
typedef void (APIENTRYP RGLSYMGLMINMAXPROC) (GLenum target, GLenum internalformat, GLboolean sink);
typedef void (APIENTRYP RGLSYMGLRESETHISTOGRAMPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLRESETMINMAXPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (APIENTRYP RGLSYMGLSAMPLECOVERAGEPROC) (GLfloat value, GLboolean invert);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE1DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLGETCOMPRESSEDTEXIMAGEPROC) (GLenum target, GLint level, GLvoid *img);
typedef void (APIENTRYP RGLSYMGLCLIENTACTIVETEXTUREPROC) (GLenum texture);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1DPROC) (GLenum target, GLdouble s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1DVPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1FPROC) (GLenum target, GLfloat s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1FVPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1IPROC) (GLenum target, GLint s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1IVPROC) (GLenum target, const GLint *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1SPROC) (GLenum target, GLshort s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1SVPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2DPROC) (GLenum target, GLdouble s, GLdouble t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2DVPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2FPROC) (GLenum target, GLfloat s, GLfloat t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2FVPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2IPROC) (GLenum target, GLint s, GLint t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2IVPROC) (GLenum target, const GLint *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2SPROC) (GLenum target, GLshort s, GLshort t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2SVPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3DPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3DVPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3FPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3FVPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3IPROC) (GLenum target, GLint s, GLint t, GLint r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3IVPROC) (GLenum target, const GLint *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3SPROC) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3SVPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4DPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4DVPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4FPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4FVPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4IPROC) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4IVPROC) (GLenum target, const GLint *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4SPROC) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4SVPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLLOADTRANSPOSEMATRIXFPROC) (const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLLOADTRANSPOSEMATRIXDPROC) (const GLdouble *m);
typedef void (APIENTRYP RGLSYMGLMULTTRANSPOSEMATRIXFPROC) (const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMULTTRANSPOSEMATRIXDPROC) (const GLdouble *m);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCSEPARATEPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWARRAYSPROC) (GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSPROC) (GLenum mode, const GLsizei *count, GLenum type, const GLvoid* const *indices, GLsizei drawcount);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERFPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERFVPROC) (GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERIPROC) (GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERIVPROC) (GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLFOGCOORDFPROC) (GLfloat coord);
typedef void (APIENTRYP RGLSYMGLFOGCOORDFVPROC) (const GLfloat *coord);
typedef void (APIENTRYP RGLSYMGLFOGCOORDDPROC) (GLdouble coord);
typedef void (APIENTRYP RGLSYMGLFOGCOORDDVPROC) (const GLdouble *coord);
typedef void (APIENTRYP RGLSYMGLFOGCOORDPOINTERPROC) (GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3BPROC) (GLbyte red, GLbyte green, GLbyte blue);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3BVPROC) (const GLbyte *v);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3DPROC) (GLdouble red, GLdouble green, GLdouble blue);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3DVPROC) (const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3FPROC) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3FVPROC) (const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3IPROC) (GLint red, GLint green, GLint blue);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3IVPROC) (const GLint *v);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3SPROC) (GLshort red, GLshort green, GLshort blue);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3SVPROC) (const GLshort *v);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3UBPROC) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3UBVPROC) (const GLubyte *v);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3UIPROC) (GLuint red, GLuint green, GLuint blue);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3UIVPROC) (const GLuint *v);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3USPROC) (GLushort red, GLushort green, GLushort blue);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLOR3USVPROC) (const GLushort *v);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLORPOINTERPROC) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2DPROC) (GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2DVPROC) (const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2FPROC) (GLfloat x, GLfloat y);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2FVPROC) (const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2IPROC) (GLint x, GLint y);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2IVPROC) (const GLint *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2SPROC) (GLshort x, GLshort y);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2SVPROC) (const GLshort *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3DPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3DVPROC) (const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3FPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3FVPROC) (const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3IPROC) (GLint x, GLint y, GLint z);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3IVPROC) (const GLint *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3SPROC) (GLshort x, GLshort y, GLshort z);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3SVPROC) (const GLshort *v);
typedef void (APIENTRYP RGLSYMGLGENQUERIESPROC) (GLsizei n, GLuint *ids);
typedef void (APIENTRYP RGLSYMGLDELETEQUERIESPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (APIENTRYP RGLSYMGLISQUERYPROC) (GLuint id);
typedef void (APIENTRYP RGLSYMGLBEGINQUERYPROC) (GLenum target, GLuint id);
typedef void (APIENTRYP RGLSYMGLENDQUERYPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLGETQUERYIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTIVPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTUIVPROC) (GLuint id, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLDELETEBUFFERSPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP RGLSYMGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef GLboolean (APIENTRYP RGLSYMGLISBUFFERPROC) (GLuint buffer);
typedef void (APIENTRYP RGLSYMGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef void (APIENTRYP RGLSYMGLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLGETBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, GLvoid *data);
typedef GLvoid* (APIENTRYP RGLSYMGLMAPBUFFERPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRYP RGLSYMGLUNMAPBUFFERPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLGETBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETBUFFERPOINTERVPROC) (GLenum target, GLenum pname, GLvoid* *params);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONSEPARATEPROC) (GLenum modeRGB, GLenum modeAlpha);
typedef void (APIENTRYP RGLSYMGLDRAWBUFFERSPROC) (GLsizei n, const GLenum *bufs);
typedef void (APIENTRYP RGLSYMGLSTENCILOPSEPARATEPROC) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
typedef void (APIENTRYP RGLSYMGLSTENCILFUNCSEPARATEPROC) (GLenum face, GLenum func, GLint ref, GLuint mask);
typedef void (APIENTRYP RGLSYMGLSTENCILMASKSEPARATEPROC) (GLenum face, GLuint mask);
typedef void (APIENTRYP RGLSYMGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRYP RGLSYMGLBINDATTRIBLOCATIONPROC) (GLuint program, GLuint index, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (APIENTRYP RGLSYMGLCREATEPROGRAMPROC) (void);
typedef GLuint (APIENTRYP RGLSYMGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRYP RGLSYMGLDELETEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP RGLSYMGLDELETESHADERPROC) (GLuint shader);
typedef void (APIENTRYP RGLSYMGLDETACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRYP RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (APIENTRYP RGLSYMGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (APIENTRYP RGLSYMGLGETACTIVEATTRIBPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETATTACHEDSHADERSPROC) (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *obj);
typedef GLint (APIENTRYP RGLSYMGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP RGLSYMGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP RGLSYMGLGETSHADERSOURCEPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
typedef GLint (APIENTRYP RGLSYMGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMFVPROC) (GLuint program, GLint location, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMIVPROC) (GLuint program, GLint location, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBDVPROC) (GLuint index, GLenum pname, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBFVPROC) (GLuint index, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBIVPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBPOINTERVPROC) (GLuint index, GLenum pname, GLvoid* *pointer);
typedef GLboolean (APIENTRYP RGLSYMGLISPROGRAMPROC) (GLuint program);
typedef GLboolean (APIENTRYP RGLSYMGLISSHADERPROC) (GLuint shader);
typedef void (APIENTRYP RGLSYMGLLINKPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP RGLSYMGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar* const *string, const GLint *length);
typedef void (APIENTRYP RGLSYMGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP RGLSYMGLUNIFORM1FPROC) (GLint location, GLfloat v0);
typedef void (APIENTRYP RGLSYMGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP RGLSYMGLUNIFORM3FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP RGLSYMGLUNIFORM4FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRYP RGLSYMGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (APIENTRYP RGLSYMGLUNIFORM2IPROC) (GLint location, GLint v0, GLint v1);
typedef void (APIENTRYP RGLSYMGLUNIFORM3IPROC) (GLint location, GLint v0, GLint v1, GLint v2);
typedef void (APIENTRYP RGLSYMGLUNIFORM4IPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (APIENTRYP RGLSYMGLUNIFORM1FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM1IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLVALIDATEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1DPROC) (GLuint index, GLdouble x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1FPROC) (GLuint index, GLfloat x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1FVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1SPROC) (GLuint index, GLshort x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2DPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2FPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2FVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2SPROC) (GLuint index, GLshort x, GLshort y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3FVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3SPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NBVPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NIVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NSVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUBPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUBVPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUSVPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4BVPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4FVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4SPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4UBVPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4USVPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLCOLORMASKIPROC) (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
typedef void (APIENTRYP RGLSYMGLGETBOOLEANI_VPROC) (GLenum target, GLuint index, GLboolean *data);
typedef void (APIENTRYP RGLSYMGLGETINTEGERI_VPROC) (GLenum target, GLuint index, GLint *data);
typedef void (APIENTRYP RGLSYMGLENABLEIPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP RGLSYMGLDISABLEIPROC) (GLenum target, GLuint index);
typedef GLboolean (APIENTRYP RGLSYMGLISENABLEDIPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP RGLSYMGLBEGINTRANSFORMFEEDBACKPROC) (GLenum primitiveMode);
typedef void (APIENTRYP RGLSYMGLENDTRANSFORMFEEDBACKPROC) (void);
typedef void (APIENTRYP RGLSYMGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLTRANSFORMFEEDBACKVARYINGSPROC) (GLuint program, GLsizei count, const GLchar* const *varyings, GLenum bufferMode);
typedef void (APIENTRYP RGLSYMGLGETTRANSFORMFEEDBACKVARYINGPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name);
typedef void (APIENTRYP RGLSYMGLCLAMPCOLORPROC) (GLenum target, GLenum clamp);
typedef void (APIENTRYP RGLSYMGLBEGINCONDITIONALRENDERPROC) (GLuint id, GLenum mode);
typedef void (APIENTRYP RGLSYMGLENDCONDITIONALRENDERPROC) (void);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBIPOINTERPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBIIVPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBIUIVPROC) (GLuint index, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI1IPROC) (GLuint index, GLint x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI2IPROC) (GLuint index, GLint x, GLint y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI3IPROC) (GLuint index, GLint x, GLint y, GLint z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4IPROC) (GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI1UIPROC) (GLuint index, GLuint x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI2UIPROC) (GLuint index, GLuint x, GLuint y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI3UIPROC) (GLuint index, GLuint x, GLuint y, GLuint z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4UIPROC) (GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI1IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI2IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI3IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4IVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI1UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI2UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI3UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4UIVPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4BVPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4SVPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4UBVPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBI4USVPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMUIVPROC) (GLuint program, GLint location, GLuint *params);
typedef void (APIENTRYP RGLSYMGLBINDFRAGDATALOCATIONPROC) (GLuint program, GLuint color, const GLchar *name);
typedef GLint (APIENTRYP RGLSYMGLGETFRAGDATALOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLUNIFORM1UIPROC) (GLint location, GLuint v0);
typedef void (APIENTRYP RGLSYMGLUNIFORM2UIPROC) (GLint location, GLuint v0, GLuint v1);
typedef void (APIENTRYP RGLSYMGLUNIFORM3UIPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (APIENTRYP RGLSYMGLUNIFORM4UIPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (APIENTRYP RGLSYMGLUNIFORM1UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4UIVPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERIIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERIUIVPROC) (GLenum target, GLenum pname, const GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXPARAMETERIIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETTEXPARAMETERIUIVPROC) (GLenum target, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERIVPROC) (GLenum buffer, GLint drawbuffer, const GLint *value);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERUIVPROC) (GLenum buffer, GLint drawbuffer, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERFVPROC) (GLenum buffer, GLint drawbuffer, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERFIPROC) (GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
typedef const GLubyte * (APIENTRYP RGLSYMGLGETSTRINGIPROC) (GLenum name, GLuint index);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSINSTANCEDPROC) (GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei instancecount);
typedef void (APIENTRYP RGLSYMGLTEXBUFFERPROC) (GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLPRIMITIVERESTARTINDEXPROC) (GLuint index);
typedef void (APIENTRYP RGLSYMGLGETINTEGER64I_VPROC) (GLenum target, GLuint index, GLint64 *data);
typedef void (APIENTRYP RGLSYMGLGETBUFFERPARAMETERI64VPROC) (GLenum target, GLenum pname, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBDIVISORPROC) (GLuint index, GLuint divisor);
typedef void (APIENTRYP RGLSYMGLMINSAMPLESHADINGPROC) (GLfloat value);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONIPROC) (GLuint buf, GLenum mode);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONSEPARATEIPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCIPROC) (GLuint buf, GLenum src, GLenum dst);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCSEPARATEIPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (APIENTRYP RGLSYMGLACTIVETEXTUREARBPROC) (GLenum texture);
typedef void (APIENTRYP RGLSYMGLCLIENTACTIVETEXTUREARBPROC) (GLenum texture);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1DARBPROC) (GLenum target, GLdouble s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1FARBPROC) (GLenum target, GLfloat s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1IARBPROC) (GLenum target, GLint s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1IVARBPROC) (GLenum target, const GLint *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1SARBPROC) (GLenum target, GLshort s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1SVARBPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2DARBPROC) (GLenum target, GLdouble s, GLdouble t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2FARBPROC) (GLenum target, GLfloat s, GLfloat t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2IARBPROC) (GLenum target, GLint s, GLint t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2IVARBPROC) (GLenum target, const GLint *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2SARBPROC) (GLenum target, GLshort s, GLshort t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2SVARBPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3DARBPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3FARBPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3IARBPROC) (GLenum target, GLint s, GLint t, GLint r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3IVARBPROC) (GLenum target, const GLint *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3SARBPROC) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3SVARBPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4DARBPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4FARBPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4IARBPROC) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4IVARBPROC) (GLenum target, const GLint *v);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4SARBPROC) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4SVARBPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLLOADTRANSPOSEMATRIXFARBPROC) (const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLLOADTRANSPOSEMATRIXDARBPROC) (const GLdouble *m);
typedef void (APIENTRYP RGLSYMGLMULTTRANSPOSEMATRIXFARBPROC) (const GLfloat *m);
typedef void (APIENTRYP RGLSYMGLMULTTRANSPOSEMATRIXDARBPROC) (const GLdouble *m);
typedef void (APIENTRYP RGLSYMGLSAMPLECOVERAGEARBPROC) (GLfloat value, GLboolean invert);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE3DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE2DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE1DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DARBPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DARBPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DARBPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLGETCOMPRESSEDTEXIMAGEARBPROC) (GLenum target, GLint level, GLvoid *img);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERFARBPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERFVARBPROC) (GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLWEIGHTBVARBPROC) (GLint size, const GLbyte *weights);
typedef void (APIENTRYP RGLSYMGLWEIGHTSVARBPROC) (GLint size, const GLshort *weights);
typedef void (APIENTRYP RGLSYMGLWEIGHTIVARBPROC) (GLint size, const GLint *weights);
typedef void (APIENTRYP RGLSYMGLWEIGHTFVARBPROC) (GLint size, const GLfloat *weights);
typedef void (APIENTRYP RGLSYMGLWEIGHTDVARBPROC) (GLint size, const GLdouble *weights);
typedef void (APIENTRYP RGLSYMGLWEIGHTUBVARBPROC) (GLint size, const GLubyte *weights);
typedef void (APIENTRYP RGLSYMGLWEIGHTUSVARBPROC) (GLint size, const GLushort *weights);
typedef void (APIENTRYP RGLSYMGLWEIGHTUIVARBPROC) (GLint size, const GLuint *weights);
typedef void (APIENTRYP RGLSYMGLWEIGHTPOINTERARBPROC) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP RGLSYMGLVERTEXBLENDARBPROC) (GLint count);
typedef void (APIENTRYP RGLSYMGLCURRENTPALETTEMATRIXARBPROC) (GLint index);
typedef void (APIENTRYP RGLSYMGLMATRIXINDEXUBVARBPROC) (GLint size, const GLubyte *indices);
typedef void (APIENTRYP RGLSYMGLMATRIXINDEXUSVARBPROC) (GLint size, const GLushort *indices);
typedef void (APIENTRYP RGLSYMGLMATRIXINDEXUIVARBPROC) (GLint size, const GLuint *indices);
typedef void (APIENTRYP RGLSYMGLMATRIXINDEXPOINTERARBPROC) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2DARBPROC) (GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2DVARBPROC) (const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2FARBPROC) (GLfloat x, GLfloat y);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2FVARBPROC) (const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2IARBPROC) (GLint x, GLint y);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2IVARBPROC) (const GLint *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2SARBPROC) (GLshort x, GLshort y);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS2SVARBPROC) (const GLshort *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3DARBPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3DVARBPROC) (const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3FARBPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3FVARBPROC) (const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3IARBPROC) (GLint x, GLint y, GLint z);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3IVARBPROC) (const GLint *v);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3SARBPROC) (GLshort x, GLshort y, GLshort z);
typedef void (APIENTRYP RGLSYMGLWINDOWPOS3SVARBPROC) (const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1DARBPROC) (GLuint index, GLdouble x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1DVARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1FARBPROC) (GLuint index, GLfloat x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1FVARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1SARBPROC) (GLuint index, GLshort x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB1SVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2DARBPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2DVARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2FARBPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2FVARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2SARBPROC) (GLuint index, GLshort x, GLshort y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB2SVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3DARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3DVARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3FARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3FVARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3SARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB3SVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NBVARBPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NIVARBPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NSVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUBARBPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUBVARBPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUIVARBPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4NUSVARBPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4BVARBPROC) (GLuint index, const GLbyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4DARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4DVARBPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4FARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4FVARBPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4IVARBPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4SARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4SVARBPROC) (GLuint index, const GLshort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4UBVARBPROC) (GLuint index, const GLubyte *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4UIVARBPROC) (GLuint index, const GLuint *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIB4USVARBPROC) (GLuint index, const GLushort *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBPOINTERARBPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP RGLSYMGLENABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (APIENTRYP RGLSYMGLDISABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (APIENTRYP RGLSYMGLPROGRAMSTRINGARBPROC) (GLenum target, GLenum format, GLsizei len, const GLvoid *string);
typedef void (APIENTRYP RGLSYMGLBINDPROGRAMARBPROC) (GLenum target, GLuint program);
typedef void (APIENTRYP RGLSYMGLDELETEPROGRAMSARBPROC) (GLsizei n, const GLuint *programs);
typedef void (APIENTRYP RGLSYMGLGENPROGRAMSARBPROC) (GLsizei n, GLuint *programs);
typedef void (APIENTRYP RGLSYMGLPROGRAMENVPARAMETER4DARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLPROGRAMENVPARAMETER4DVARBPROC) (GLenum target, GLuint index, const GLdouble *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMENVPARAMETER4FARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRYP RGLSYMGLPROGRAMENVPARAMETER4FVARBPROC) (GLenum target, GLuint index, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMLOCALPARAMETER4DARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLPROGRAMLOCALPARAMETER4DVARBPROC) (GLenum target, GLuint index, const GLdouble *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMLOCALPARAMETER4FARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRYP RGLSYMGLPROGRAMLOCALPARAMETER4FVARBPROC) (GLenum target, GLuint index, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMENVPARAMETERDVARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMENVPARAMETERFVARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMLOCALPARAMETERDVARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMLOCALPARAMETERFVARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMIVARBPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMSTRINGARBPROC) (GLenum target, GLenum pname, GLvoid *string);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBDVARBPROC) (GLuint index, GLenum pname, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBFVARBPROC) (GLuint index, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBIVARBPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBPOINTERVARBPROC) (GLuint index, GLenum pname, GLvoid* *pointer);
typedef GLboolean (APIENTRYP RGLSYMGLISPROGRAMARBPROC) (GLuint program);
typedef void (APIENTRYP RGLSYMGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP RGLSYMGLGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef GLboolean (APIENTRYP RGLSYMGLISBUFFERARBPROC) (GLuint buffer);
typedef void (APIENTRYP RGLSYMGLBUFFERDATAARBPROC) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
typedef void (APIENTRYP RGLSYMGLBUFFERSUBDATAARBPROC) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);
typedef void (APIENTRYP RGLSYMGLGETBUFFERSUBDATAARBPROC) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data);
typedef GLvoid* (APIENTRYP RGLSYMGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRYP RGLSYMGLUNMAPBUFFERARBPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLGETBUFFERPARAMETERIVARBPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETBUFFERPOINTERVARBPROC) (GLenum target, GLenum pname, GLvoid* *params);
typedef void (APIENTRYP RGLSYMGLGENQUERIESARBPROC) (GLsizei n, GLuint *ids);
typedef void (APIENTRYP RGLSYMGLDELETEQUERIESARBPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (APIENTRYP RGLSYMGLISQUERYARBPROC) (GLuint id);
typedef void (APIENTRYP RGLSYMGLBEGINQUERYARBPROC) (GLenum target, GLuint id);
typedef void (APIENTRYP RGLSYMGLENDQUERYARBPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLGETQUERYIVARBPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTIVARBPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTUIVARBPROC) (GLuint id, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLDELETEOBJECTARBPROC) (GLhandleARB obj);
typedef GLhandleARB (APIENTRYP RGLSYMGLGETHANDLEARBPROC) (GLenum pname);
typedef void (APIENTRYP RGLSYMGLDETACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB attachedObj);
typedef GLhandleARB (APIENTRYP RGLSYMGLCREATESHADEROBJECTARBPROC) (GLenum shaderType);
typedef void (APIENTRYP RGLSYMGLSHADERSOURCEARBPROC) (GLhandleARB shaderObj, GLsizei count, const GLcharARB* *string, const GLint *length);
typedef void (APIENTRYP RGLSYMGLCOMPILESHADERARBPROC) (GLhandleARB shaderObj);
typedef GLhandleARB (APIENTRYP RGLSYMGLCREATEPROGRAMOBJECTARBPROC) (void);
typedef void (APIENTRYP RGLSYMGLATTACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB obj);
typedef void (APIENTRYP RGLSYMGLLINKPROGRAMARBPROC) (GLhandleARB programObj);
typedef void (APIENTRYP RGLSYMGLUSEPROGRAMOBJECTARBPROC) (GLhandleARB programObj);
typedef void (APIENTRYP RGLSYMGLVALIDATEPROGRAMARBPROC) (GLhandleARB programObj);
typedef void (APIENTRYP RGLSYMGLUNIFORM1FARBPROC) (GLint location, GLfloat v0);
typedef void (APIENTRYP RGLSYMGLUNIFORM2FARBPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP RGLSYMGLUNIFORM3FARBPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP RGLSYMGLUNIFORM4FARBPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRYP RGLSYMGLUNIFORM1IARBPROC) (GLint location, GLint v0);
typedef void (APIENTRYP RGLSYMGLUNIFORM2IARBPROC) (GLint location, GLint v0, GLint v1);
typedef void (APIENTRYP RGLSYMGLUNIFORM3IARBPROC) (GLint location, GLint v0, GLint v1, GLint v2);
typedef void (APIENTRYP RGLSYMGLUNIFORM4IARBPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (APIENTRYP RGLSYMGLUNIFORM1FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM1IVARBPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2IVARBPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3IVARBPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4IVARBPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLGETOBJECTPARAMETERFVARBPROC) (GLhandleARB obj, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETOBJECTPARAMETERIVARBPROC) (GLhandleARB obj, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETINFOLOGARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
typedef void (APIENTRYP RGLSYMGLGETATTACHEDOBJECTSARBPROC) (GLhandleARB containerObj, GLsizei maxCount, GLsizei *count, GLhandleARB *obj);
typedef GLint (APIENTRYP RGLSYMGLGETUNIFORMLOCATIONARBPROC) (GLhandleARB programObj, const GLcharARB *name);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMARBPROC) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMFVARBPROC) (GLhandleARB programObj, GLint location, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMIVARBPROC) (GLhandleARB programObj, GLint location, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETSHADERSOURCEARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *source);
typedef void (APIENTRYP RGLSYMGLBINDATTRIBLOCATIONARBPROC) (GLhandleARB programObj, GLuint index, const GLcharARB *name);
typedef void (APIENTRYP RGLSYMGLGETACTIVEATTRIBARBPROC) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
typedef GLint (APIENTRYP RGLSYMGLGETATTRIBLOCATIONARBPROC) (GLhandleARB programObj, const GLcharARB *name);
typedef void (APIENTRYP RGLSYMGLDRAWBUFFERSARBPROC) (GLsizei n, const GLenum *bufs);
typedef void (APIENTRYP RGLSYMGLCLAMPCOLORARBPROC) (GLenum target, GLenum clamp);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSINSTANCEDARBPROC) (GLenum mode, GLint first, GLsizei count, GLsizei primcount);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDARBPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount);
typedef GLboolean (APIENTRYP RGLSYMGLISRENDERBUFFERPROC) (GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLBINDRENDERBUFFERPROC) (GLenum target, GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLDELETERENDERBUFFERSPROC) (GLsizei n, const GLuint *renderbuffers);
typedef void (APIENTRYP RGLSYMGLGENRENDERBUFFERSPROC) (GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRYP RGLSYMGLRENDERBUFFERSTORAGEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLGETRENDERBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef GLboolean (APIENTRYP RGLSYMGLISFRAMEBUFFERPROC) (GLuint framebuffer);
typedef void (APIENTRYP RGLSYMGLBINDFRAMEBUFFERPROC) (GLenum target, GLuint framebuffer);
typedef void (APIENTRYP RGLSYMGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRYP RGLSYMGLGENFRAMEBUFFERSPROC) (GLsizei n, GLuint *framebuffers);
typedef GLenum (APIENTRYP RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE1DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURE3DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRYP RGLSYMGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) (GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGENERATEMIPMAPPROC) (GLenum target);
typedef void (APIENTRYP RGLSYMGLBLITFRAMEBUFFERPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (APIENTRYP RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURELAYERPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (APIENTRYP RGLSYMGLPROGRAMPARAMETERIARBPROC) (GLuint program, GLenum pname, GLint value);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREARBPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTURELAYERARBPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERTEXTUREFACEARBPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLenum face);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBDIVISORARBPROC) (GLuint index, GLuint divisor);
typedef GLvoid* (APIENTRYP RGLSYMGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (APIENTRYP RGLSYMGLFLUSHMAPPEDBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length);
typedef void (APIENTRYP RGLSYMGLTEXBUFFERARBPROC) (GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRYP RGLSYMGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRYP RGLSYMGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint *arrays);
typedef void (APIENTRYP RGLSYMGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef GLboolean (APIENTRYP RGLSYMGLISVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMINDICESPROC) (GLuint program, GLsizei uniformCount, const GLchar* const *uniformNames, GLuint *uniformIndices);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMSIVPROC) (GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMNAMEPROC) (GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformName);
typedef GLuint (APIENTRYP RGLSYMGLGETUNIFORMBLOCKINDEXPROC) (GLuint program, const GLchar *uniformBlockName);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMBLOCKIVPROC) (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETACTIVEUNIFORMBLOCKNAMEPROC) (GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName);
typedef void (APIENTRYP RGLSYMGLUNIFORMBLOCKBINDINGPROC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef void (APIENTRYP RGLSYMGLCOPYBUFFERSUBDATAPROC) (GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSBASEVERTEXPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex);
typedef void (APIENTRYP RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei instancecount, GLint basevertex);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXPROC) (GLenum mode, const GLsizei *count, GLenum type, const GLvoid* const *indices, GLsizei drawcount, const GLint *basevertex);
typedef void (APIENTRYP RGLSYMGLPROVOKINGVERTEXPROC) (GLenum mode);
typedef GLsync (APIENTRYP RGLSYMGLFENCESYNCPROC) (GLenum condition, GLbitfield flags);
typedef GLboolean (APIENTRYP RGLSYMGLISSYNCPROC) (GLsync sync);
typedef void (APIENTRYP RGLSYMGLDELETESYNCPROC) (GLsync sync);
typedef GLenum (APIENTRYP RGLSYMGLCLIENTWAITSYNCPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (APIENTRYP RGLSYMGLWAITSYNCPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (APIENTRYP RGLSYMGLGETINTEGER64VPROC) (GLenum pname, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLGETSYNCIVPROC) (GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values);
typedef void (APIENTRYP RGLSYMGLTEXIMAGE2DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLTEXIMAGE3DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLGETMULTISAMPLEFVPROC) (GLenum pname, GLuint index, GLfloat *val);
typedef void (APIENTRYP RGLSYMGLSAMPLEMASKIPROC) (GLuint index, GLbitfield mask);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONIARBPROC) (GLuint buf, GLenum mode);
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONSEPARATEIARBPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCIARBPROC) (GLuint buf, GLenum src, GLenum dst);
typedef void (APIENTRYP RGLSYMGLBLENDFUNCSEPARATEIARBPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (APIENTRYP RGLSYMGLMINSAMPLESHADINGARBPROC) (GLfloat value);
typedef void (APIENTRYP RGLSYMGLNAMEDSTRINGARBPROC) (GLenum type, GLint namelen, const GLchar *name, GLint stringlen, const GLchar *string);
typedef void (APIENTRYP RGLSYMGLDELETENAMEDSTRINGARBPROC) (GLint namelen, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLCOMPILESHADERINCLUDEARBPROC) (GLuint shader, GLsizei count, const GLchar* *path, const GLint *length);
typedef GLboolean (APIENTRYP RGLSYMGLISNAMEDSTRINGARBPROC) (GLint namelen, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETNAMEDSTRINGARBPROC) (GLint namelen, const GLchar *name, GLsizei bufSize, GLint *stringlen, GLchar *string);
typedef void (APIENTRYP RGLSYMGLGETNAMEDSTRINGIVARBPROC) (GLint namelen, const GLchar *name, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLBINDFRAGDATALOCATIONINDEXEDPROC) (GLuint program, GLuint colorNumber, GLuint index, const GLchar *name);
typedef GLint (APIENTRYP RGLSYMGLGETFRAGDATAINDEXPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGENSAMPLERSPROC) (GLsizei count, GLuint *samplers);
typedef void (APIENTRYP RGLSYMGLDELETESAMPLERSPROC) (GLsizei count, const GLuint *samplers);
typedef GLboolean (APIENTRYP RGLSYMGLISSAMPLERPROC) (GLuint sampler);
typedef void (APIENTRYP RGLSYMGLBINDSAMPLERPROC) (GLuint unit, GLuint sampler);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERIPROC) (GLuint sampler, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, const GLint *param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERFPROC) (GLuint sampler, GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, const GLfloat *param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, const GLint *param);
typedef void (APIENTRYP RGLSYMGLSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, const GLuint *param);
typedef void (APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, GLuint *params);
typedef void (APIENTRYP RGLSYMGLQUERYCOUNTERPROC) (GLuint id, GLenum target);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTI64VPROC) (GLuint id, GLenum pname, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLGETQUERYOBJECTUI64VPROC) (GLuint id, GLenum pname, GLuint64 *params);
typedef void (APIENTRYP RGLSYMGLVERTEXP2UIPROC) (GLenum type, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXP2UIVPROC) (GLenum type, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLVERTEXP3UIPROC) (GLenum type, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXP3UIVPROC) (GLenum type, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLVERTEXP4UIPROC) (GLenum type, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXP4UIVPROC) (GLenum type, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLTEXCOORDP1UIPROC) (GLenum type, GLuint coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORDP1UIVPROC) (GLenum type, const GLuint *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORDP2UIPROC) (GLenum type, GLuint coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORDP2UIVPROC) (GLenum type, const GLuint *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORDP3UIPROC) (GLenum type, GLuint coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORDP3UIVPROC) (GLenum type, const GLuint *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORDP4UIPROC) (GLenum type, GLuint coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORDP4UIVPROC) (GLenum type, const GLuint *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORDP1UIPROC) (GLenum texture, GLenum type, GLuint coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORDP1UIVPROC) (GLenum texture, GLenum type, const GLuint *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORDP2UIPROC) (GLenum texture, GLenum type, GLuint coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORDP2UIVPROC) (GLenum texture, GLenum type, const GLuint *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORDP3UIPROC) (GLenum texture, GLenum type, GLuint coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORDP3UIVPROC) (GLenum texture, GLenum type, const GLuint *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORDP4UIPROC) (GLenum texture, GLenum type, GLuint coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORDP4UIVPROC) (GLenum texture, GLenum type, const GLuint *coords);
typedef void (APIENTRYP RGLSYMGLNORMALP3UIPROC) (GLenum type, GLuint coords);
typedef void (APIENTRYP RGLSYMGLNORMALP3UIVPROC) (GLenum type, const GLuint *coords);
typedef void (APIENTRYP RGLSYMGLCOLORP3UIPROC) (GLenum type, GLuint color);
typedef void (APIENTRYP RGLSYMGLCOLORP3UIVPROC) (GLenum type, const GLuint *color);
typedef void (APIENTRYP RGLSYMGLCOLORP4UIPROC) (GLenum type, GLuint color);
typedef void (APIENTRYP RGLSYMGLCOLORP4UIVPROC) (GLenum type, const GLuint *color);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLORP3UIPROC) (GLenum type, GLuint color);
typedef void (APIENTRYP RGLSYMGLSECONDARYCOLORP3UIVPROC) (GLenum type, const GLuint *color);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP1UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP1UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP2UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP2UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP3UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP3UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP4UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBP4UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSINDIRECTPROC) (GLenum mode, const GLvoid *indirect);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINDIRECTPROC) (GLenum mode, GLenum type, const GLvoid *indirect);
typedef void (APIENTRYP RGLSYMGLUNIFORM1DPROC) (GLint location, GLdouble x);
typedef void (APIENTRYP RGLSYMGLUNIFORM2DPROC) (GLint location, GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLUNIFORM3DPROC) (GLint location, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLUNIFORM4DPROC) (GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLUNIFORM1DVPROC) (GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM2DVPROC) (GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM3DVPROC) (GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORM4DVPROC) (GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2X3DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX2X4DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3X2DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX3X4DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4X2DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLUNIFORMMATRIX4X3DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMDVPROC) (GLuint program, GLint location, GLdouble *params);
typedef GLint (APIENTRYP RGLSYMGLGETSUBROUTINEUNIFORMLOCATIONPROC) (GLuint program, GLenum shadertype, const GLchar *name);
typedef GLuint (APIENTRYP RGLSYMGLGETSUBROUTINEINDEXPROC) (GLuint program, GLenum shadertype, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETACTIVESUBROUTINEUNIFORMIVPROC) (GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint *values);
typedef void (APIENTRYP RGLSYMGLGETACTIVESUBROUTINEUNIFORMNAMEPROC) (GLuint program, GLenum shadertype, GLuint index, GLsizei bufsize, GLsizei *length, GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETACTIVESUBROUTINENAMEPROC) (GLuint program, GLenum shadertype, GLuint index, GLsizei bufsize, GLsizei *length, GLchar *name);
typedef void (APIENTRYP RGLSYMGLUNIFORMSUBROUTINESUIVPROC) (GLenum shadertype, GLsizei count, const GLuint *indices);
typedef void (APIENTRYP RGLSYMGLGETUNIFORMSUBROUTINEUIVPROC) (GLenum shadertype, GLint location, GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMSTAGEIVPROC) (GLuint program, GLenum shadertype, GLenum pname, GLint *values);
typedef void (APIENTRYP RGLSYMGLPATCHPARAMETERIPROC) (GLenum pname, GLint value);
typedef void (APIENTRYP RGLSYMGLPATCHPARAMETERFVPROC) (GLenum pname, const GLfloat *values);
typedef void (APIENTRYP RGLSYMGLBINDTRANSFORMFEEDBACKPROC) (GLenum target, GLuint id);
typedef void (APIENTRYP RGLSYMGLDELETETRANSFORMFEEDBACKSPROC) (GLsizei n, const GLuint *ids);
typedef void (APIENTRYP RGLSYMGLGENTRANSFORMFEEDBACKSPROC) (GLsizei n, GLuint *ids);
typedef GLboolean (APIENTRYP RGLSYMGLISTRANSFORMFEEDBACKPROC) (GLuint id);
typedef void (APIENTRYP RGLSYMGLPAUSETRANSFORMFEEDBACKPROC) (void);
typedef void (APIENTRYP RGLSYMGLRESUMETRANSFORMFEEDBACKPROC) (void);
typedef void (APIENTRYP RGLSYMGLDRAWTRANSFORMFEEDBACKPROC) (GLenum mode, GLuint id);
typedef void (APIENTRYP RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMPROC) (GLenum mode, GLuint id, GLuint stream);
typedef void (APIENTRYP RGLSYMGLBEGINQUERYINDEXEDPROC) (GLenum target, GLuint index, GLuint id);
typedef void (APIENTRYP RGLSYMGLENDQUERYINDEXEDPROC) (GLenum target, GLuint index);
typedef void (APIENTRYP RGLSYMGLGETQUERYINDEXEDIVPROC) (GLenum target, GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLRELEASESHADERCOMPILERPROC) (void);
typedef void (APIENTRYP RGLSYMGLSHADERBINARYPROC) (GLsizei count, const GLuint *shaders, GLenum binaryformat, const GLvoid *binary, GLsizei length);
typedef void (APIENTRYP RGLSYMGLGETSHADERPRECISIONFORMATPROC) (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEFPROC) (GLfloat n, GLfloat f);
typedef void (APIENTRYP RGLSYMGLCLEARDEPTHFPROC) (GLfloat d);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMBINARYPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, GLvoid *binary);
typedef void (APIENTRYP RGLSYMGLPROGRAMBINARYPROC) (GLuint program, GLenum binaryFormat, const GLvoid *binary, GLsizei length);
typedef void (APIENTRYP RGLSYMGLPROGRAMPARAMETERIPROC) (GLuint program, GLenum pname, GLint value);
typedef void (APIENTRYP RGLSYMGLUSEPROGRAMSTAGESPROC) (GLuint pipeline, GLbitfield stages, GLuint program);
typedef void (APIENTRYP RGLSYMGLACTIVESHADERPROGRAMPROC) (GLuint pipeline, GLuint program);
typedef GLuint (APIENTRYP RGLSYMGLCREATESHADERPROGRAMVPROC) (GLenum type, GLsizei count, const GLchar* const *strings);
typedef void (APIENTRYP RGLSYMGLBINDPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (APIENTRYP RGLSYMGLDELETEPROGRAMPIPELINESPROC) (GLsizei n, const GLuint *pipelines);
typedef void (APIENTRYP RGLSYMGLGENPROGRAMPIPELINESPROC) (GLsizei n, GLuint *pipelines);
typedef GLboolean (APIENTRYP RGLSYMGLISPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMPIPELINEIVPROC) (GLuint pipeline, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1IPROC) (GLuint program, GLint location, GLint v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1FPROC) (GLuint program, GLint location, GLfloat v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1DPROC) (GLuint program, GLint location, GLdouble v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UIPROC) (GLuint program, GLint location, GLuint v0);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM1UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2IPROC) (GLuint program, GLint location, GLint v0, GLint v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2FPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2DPROC) (GLuint program, GLint location, GLdouble v0, GLdouble v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UIPROC) (GLuint program, GLint location, GLuint v0, GLuint v1);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM2UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3IPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3FPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3DPROC) (GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UIPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM3UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4IPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4IVPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4FPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4DPROC) (GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UIPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORM4UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X3DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X2DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX2X4DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X2DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX3X4DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLPROGRAMUNIFORMMATRIX4X3DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
typedef void (APIENTRYP RGLSYMGLVALIDATEPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMPIPELINEINFOLOGPROC) (GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1DPROC) (GLuint index, GLdouble x);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL2DPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL3DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL4DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL1DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL2DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL3DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBL4DVPROC) (GLuint index, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBLPOINTERPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP RGLSYMGLGETVERTEXATTRIBLDVPROC) (GLuint index, GLenum pname, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLVIEWPORTARRAYVPROC) (GLuint first, GLsizei count, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLVIEWPORTINDEXEDFPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
typedef void (APIENTRYP RGLSYMGLVIEWPORTINDEXEDFVPROC) (GLuint index, const GLfloat *v);
typedef void (APIENTRYP RGLSYMGLSCISSORARRAYVPROC) (GLuint first, GLsizei count, const GLint *v);
typedef void (APIENTRYP RGLSYMGLSCISSORINDEXEDPROC) (GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLSCISSORINDEXEDVPROC) (GLuint index, const GLint *v);
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEARRAYVPROC) (GLuint first, GLsizei count, const GLdouble *v);
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEINDEXEDPROC) (GLuint index, GLdouble n, GLdouble f);
typedef void (APIENTRYP RGLSYMGLGETFLOATI_VPROC) (GLenum target, GLuint index, GLfloat *data);
typedef void (APIENTRYP RGLSYMGLGETDOUBLEI_VPROC) (GLenum target, GLuint index, GLdouble *data);
typedef GLsync (APIENTRYP RGLSYMGLCREATESYNCFROMCLEVENTARBPROC) (struct _cl_context * context, struct _cl_event * event, GLbitfield flags);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGECONTROLARBPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGEINSERTARBPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGECALLBACKARBPROC) (RGLGENGLDEBUGPROCARB callback, const GLvoid *userParam);
typedef GLuint (APIENTRYP RGLSYMGLGETDEBUGMESSAGELOGARBPROC) (GLuint count, GLsizei bufsize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef GLenum (APIENTRYP RGLSYMGLGETGRAPHICSRESETSTATUSARBPROC) (void);
typedef void (APIENTRYP RGLSYMGLGETNMAPDVARBPROC) (GLenum target, GLenum query, GLsizei bufSize, GLdouble *v);
typedef void (APIENTRYP RGLSYMGLGETNMAPFVARBPROC) (GLenum target, GLenum query, GLsizei bufSize, GLfloat *v);
typedef void (APIENTRYP RGLSYMGLGETNMAPIVARBPROC) (GLenum target, GLenum query, GLsizei bufSize, GLint *v);
typedef void (APIENTRYP RGLSYMGLGETNPIXELMAPFVARBPROC) (GLenum map, GLsizei bufSize, GLfloat *values);
typedef void (APIENTRYP RGLSYMGLGETNPIXELMAPUIVARBPROC) (GLenum map, GLsizei bufSize, GLuint *values);
typedef void (APIENTRYP RGLSYMGLGETNPIXELMAPUSVARBPROC) (GLenum map, GLsizei bufSize, GLushort *values);
typedef void (APIENTRYP RGLSYMGLGETNPOLYGONSTIPPLEARBPROC) (GLsizei bufSize, GLubyte *pattern);
typedef void (APIENTRYP RGLSYMGLGETNCOLORTABLEARBPROC) (GLenum target, GLenum format, GLenum type, GLsizei bufSize, GLvoid *table);
typedef void (APIENTRYP RGLSYMGLGETNCONVOLUTIONFILTERARBPROC) (GLenum target, GLenum format, GLenum type, GLsizei bufSize, GLvoid *image);
typedef void (APIENTRYP RGLSYMGLGETNSEPARABLEFILTERARBPROC) (GLenum target, GLenum format, GLenum type, GLsizei rowBufSize, GLvoid *row, GLsizei columnBufSize, GLvoid *column, GLvoid *span);
typedef void (APIENTRYP RGLSYMGLGETNHISTOGRAMARBPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLsizei bufSize, GLvoid *values);
typedef void (APIENTRYP RGLSYMGLGETNMINMAXARBPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLsizei bufSize, GLvoid *values);
typedef void (APIENTRYP RGLSYMGLGETNTEXIMAGEARBPROC) (GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, GLvoid *img);
typedef void (APIENTRYP RGLSYMGLREADNPIXELSARBPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLvoid *data);
typedef void (APIENTRYP RGLSYMGLGETNCOMPRESSEDTEXIMAGEARBPROC) (GLenum target, GLint lod, GLsizei bufSize, GLvoid *img);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMFVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMIVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMUIVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLuint *params);
typedef void (APIENTRYP RGLSYMGLGETNUNIFORMDVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLdouble *params);
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC) (GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance);
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance);
typedef void (APIENTRYP RGLSYMGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC) (GLenum mode, GLuint id, GLsizei instancecount);
typedef void (APIENTRYP RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC) (GLenum mode, GLuint id, GLuint stream, GLsizei instancecount);
typedef void (APIENTRYP RGLSYMGLGETINTERNALFORMATIVPROC) (GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETACTIVEATOMICCOUNTERBUFFERIVPROC) (GLuint program, GLuint bufferIndex, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLBINDIMAGETEXTUREPROC) (GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
typedef void (APIENTRYP RGLSYMGLMEMORYBARRIERPROC) (GLbitfield barriers);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE1DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE2DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE3DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGECONTROLPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGEINSERTPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf);
typedef void (APIENTRYP RGLSYMGLDEBUGMESSAGECALLBACKPROC) (RGLGENGLDEBUGPROC callback, const void *userParam);
typedef GLuint (APIENTRYP RGLSYMGLGETDEBUGMESSAGELOGPROC) (GLuint count, GLsizei bufsize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog);
typedef void (APIENTRYP RGLSYMGLPUSHDEBUGGROUPPROC) (GLenum source, GLuint id, GLsizei length, const GLchar *message);
typedef void (APIENTRYP RGLSYMGLPOPDEBUGGROUPPROC) (void);
typedef void (APIENTRYP RGLSYMGLOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
typedef void (APIENTRYP RGLSYMGLGETOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (APIENTRYP RGLSYMGLOBJECTPTRLABELPROC) (const void *ptr, GLsizei length, const GLchar *label);
typedef void (APIENTRYP RGLSYMGLGETOBJECTPTRLABELPROC) (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERDATAPROC) (GLenum target, GLenum internalformat, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP RGLSYMGLCLEARBUFFERSUBDATAPROC) (GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data);
typedef void (APIENTRYP RGLSYMGLDISPATCHCOMPUTEPROC) (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (APIENTRYP RGLSYMGLDISPATCHCOMPUTEINDIRECTPROC) (GLintptr indirect);
typedef void (APIENTRYP RGLSYMGLCOPYIMAGESUBDATAPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
typedef void (APIENTRYP RGLSYMGLTEXTUREVIEWPROC) (GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);
typedef void (APIENTRYP RGLSYMGLBINDVERTEXBUFFERPROC) (GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBFORMATPROC) (GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBIFORMATPROC) (GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBLFORMATPROC) (GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
typedef void (APIENTRYP RGLSYMGLVERTEXATTRIBBINDINGPROC) (GLuint attribindex, GLuint bindingindex);
typedef void (APIENTRYP RGLSYMGLVERTEXBINDINGDIVISORPROC) (GLuint bindingindex, GLuint divisor);
typedef void (APIENTRYP RGLSYMGLFRAMEBUFFERPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLGETFRAMEBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETINTERNALFORMATI64VPROC) (GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint64 *params);
typedef void (APIENTRYP RGLSYMGLINVALIDATETEXSUBIMAGEPROC) (GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRYP RGLSYMGLINVALIDATETEXIMAGEPROC) (GLuint texture, GLint level);
typedef void (APIENTRYP RGLSYMGLINVALIDATEBUFFERSUBDATAPROC) (GLuint buffer, GLintptr offset, GLsizeiptr length);
typedef void (APIENTRYP RGLSYMGLINVALIDATEBUFFERDATAPROC) (GLuint buffer);
typedef void (APIENTRYP RGLSYMGLINVALIDATEFRAMEBUFFERPROC) (GLenum target, GLsizei numAttachments, const GLenum *attachments);
typedef void (APIENTRYP RGLSYMGLINVALIDATESUBFRAMEBUFFERPROC) (GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWARRAYSINDIRECTPROC) (GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLMULTIDRAWELEMENTSINDIRECTPROC) (GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMINTERFACEIVPROC) (GLuint program, GLenum programInterface, GLenum pname, GLint *params);
typedef GLuint (APIENTRYP RGLSYMGLGETPROGRAMRESOURCEINDEXPROC) (GLuint program, GLenum programInterface, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMRESOURCENAMEPROC) (GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name);
typedef void (APIENTRYP RGLSYMGLGETPROGRAMRESOURCEIVPROC) (GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params);
typedef GLint (APIENTRYP RGLSYMGLGETPROGRAMRESOURCELOCATIONPROC) (GLuint program, GLenum programInterface, const GLchar *name);
typedef GLint (APIENTRYP RGLSYMGLGETPROGRAMRESOURCELOCATIONINDEXPROC) (GLuint program, GLenum programInterface, const GLchar *name);
typedef void (APIENTRYP RGLSYMGLSHADERSTORAGEBLOCKBINDINGPROC) (GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding);
typedef void (APIENTRYP RGLSYMGLTEXBUFFERRANGEPROC) (GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE2DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLTEXSTORAGE3DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (APIENTRYP RGLSYMGLIMAGETRANSFORMPARAMETERIHPPROC) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP RGLSYMGLIMAGETRANSFORMPARAMETERFHPPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP RGLSYMGLIMAGETRANSFORMPARAMETERIVHPPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLIMAGETRANSFORMPARAMETERFVHPPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETIMAGETRANSFORMPARAMETERIVHPPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRYP RGLSYMGLGETIMAGETRANSFORMPARAMETERFVHPPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1BOESPROC) (GLenum texture, GLbyte s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1BVOESPROC) (GLenum texture, const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2BOESPROC) (GLenum texture, GLbyte s, GLbyte t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2BVOESPROC) (GLenum texture, const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3BOESPROC) (GLenum texture, GLbyte s, GLbyte t, GLbyte r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3BVOESPROC) (GLenum texture, const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4BOESPROC) (GLenum texture, GLbyte s, GLbyte t, GLbyte r, GLbyte q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4BVOESPROC) (GLenum texture, const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORD1BOESPROC) (GLbyte s);
typedef void (APIENTRYP RGLSYMGLTEXCOORD1BVOESPROC) (const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORD2BOESPROC) (GLbyte s, GLbyte t);
typedef void (APIENTRYP RGLSYMGLTEXCOORD2BVOESPROC) (const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORD3BOESPROC) (GLbyte s, GLbyte t, GLbyte r);
typedef void (APIENTRYP RGLSYMGLTEXCOORD3BVOESPROC) (const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORD4BOESPROC) (GLbyte s, GLbyte t, GLbyte r, GLbyte q);
typedef void (APIENTRYP RGLSYMGLTEXCOORD4BVOESPROC) (const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLVERTEX2BOESPROC) (GLbyte x);
typedef void (APIENTRYP RGLSYMGLVERTEX2BVOESPROC) (const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLVERTEX3BOESPROC) (GLbyte x, GLbyte y);
typedef void (APIENTRYP RGLSYMGLVERTEX3BVOESPROC) (const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLVERTEX4BOESPROC) (GLbyte x, GLbyte y, GLbyte z);
typedef void (APIENTRYP RGLSYMGLVERTEX4BVOESPROC) (const GLbyte *coords);
typedef void (APIENTRYP RGLSYMGLACCUMXOESPROC) (GLenum op, GLfixed value);
typedef void (APIENTRYP RGLSYMGLALPHAFUNCXOESPROC) (GLenum func, GLfixed ref);
typedef void (APIENTRYP RGLSYMGLBITMAPXOESPROC) (GLsizei width, GLsizei height, GLfixed xorig, GLfixed yorig, GLfixed xmove, GLfixed ymove, const GLubyte *bitmap);
typedef void (APIENTRYP RGLSYMGLBLENDCOLORXOESPROC) (GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
typedef void (APIENTRYP RGLSYMGLCLEARACCUMXOESPROC) (GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
typedef void (APIENTRYP RGLSYMGLCLEARCOLORXOESPROC) (GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
typedef void (APIENTRYP RGLSYMGLCLEARDEPTHXOESPROC) (GLfixed depth);
typedef void (APIENTRYP RGLSYMGLCLIPPLANEXOESPROC) (GLenum plane, const GLfixed *equation);
typedef void (APIENTRYP RGLSYMGLCOLOR3XOESPROC) (GLfixed red, GLfixed green, GLfixed blue);
typedef void (APIENTRYP RGLSYMGLCOLOR4XOESPROC) (GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
typedef void (APIENTRYP RGLSYMGLCOLOR3XVOESPROC) (const GLfixed *components);
typedef void (APIENTRYP RGLSYMGLCOLOR4XVOESPROC) (const GLfixed *components);
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERXOESPROC) (GLenum target, GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERXVOESPROC) (GLenum target, GLenum pname, const GLfixed *params);
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEXOESPROC) (GLfixed n, GLfixed f);
typedef void (APIENTRYP RGLSYMGLEVALCOORD1XOESPROC) (GLfixed u);
typedef void (APIENTRYP RGLSYMGLEVALCOORD2XOESPROC) (GLfixed u, GLfixed v);
typedef void (APIENTRYP RGLSYMGLEVALCOORD1XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLEVALCOORD2XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLFEEDBACKBUFFERXOESPROC) (GLsizei n, GLenum type, const GLfixed *buffer);
typedef void (APIENTRYP RGLSYMGLFOGXOESPROC) (GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLFOGXVOESPROC) (GLenum pname, const GLfixed *param);
typedef void (APIENTRYP RGLSYMGLFRUSTUMXOESPROC) (GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f);
typedef void (APIENTRYP RGLSYMGLGETCLIPPLANEXOESPROC) (GLenum plane, GLfixed *equation);
typedef void (APIENTRYP RGLSYMGLGETCONVOLUTIONPARAMETERXVOESPROC) (GLenum target, GLenum pname, GLfixed *params);
typedef void (APIENTRYP RGLSYMGLGETFIXEDVOESPROC) (GLenum pname, GLfixed *params);
typedef void (APIENTRYP RGLSYMGLGETHISTOGRAMPARAMETERXVOESPROC) (GLenum target, GLenum pname, GLfixed *params);
typedef void (APIENTRYP RGLSYMGLGETLIGHTXOESPROC) (GLenum light, GLenum pname, GLfixed *params);
typedef void (APIENTRYP RGLSYMGLGETMAPXVOESPROC) (GLenum target, GLenum query, GLfixed *v);
typedef void (APIENTRYP RGLSYMGLGETMATERIALXOESPROC) (GLenum face, GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLGETPIXELMAPXVPROC) (GLenum map, GLint size, GLfixed *values);
typedef void (APIENTRYP RGLSYMGLGETTEXENVXVOESPROC) (GLenum target, GLenum pname, GLfixed *params);
typedef void (APIENTRYP RGLSYMGLGETTEXGENXVOESPROC) (GLenum coord, GLenum pname, GLfixed *params);
typedef void (APIENTRYP RGLSYMGLGETTEXLEVELPARAMETERXVOESPROC) (GLenum target, GLint level, GLenum pname, GLfixed *params);
typedef void (APIENTRYP RGLSYMGLGETTEXPARAMETERXVOESPROC) (GLenum target, GLenum pname, GLfixed *params);
typedef void (APIENTRYP RGLSYMGLINDEXXOESPROC) (GLfixed component);
typedef void (APIENTRYP RGLSYMGLINDEXXVOESPROC) (const GLfixed *component);
typedef void (APIENTRYP RGLSYMGLLIGHTMODELXOESPROC) (GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLLIGHTMODELXVOESPROC) (GLenum pname, const GLfixed *param);
typedef void (APIENTRYP RGLSYMGLLIGHTXOESPROC) (GLenum light, GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLLIGHTXVOESPROC) (GLenum light, GLenum pname, const GLfixed *params);
typedef void (APIENTRYP RGLSYMGLLINEWIDTHXOESPROC) (GLfixed width);
typedef void (APIENTRYP RGLSYMGLLOADMATRIXXOESPROC) (const GLfixed *m);
typedef void (APIENTRYP RGLSYMGLLOADTRANSPOSEMATRIXXOESPROC) (const GLfixed *m);
typedef void (APIENTRYP RGLSYMGLMAP1XOESPROC) (GLenum target, GLfixed u1, GLfixed u2, GLint stride, GLint order, GLfixed points);
typedef void (APIENTRYP RGLSYMGLMAP2XOESPROC) (GLenum target, GLfixed u1, GLfixed u2, GLint ustride, GLint uorder, GLfixed v1, GLfixed v2, GLint vstride, GLint vorder, GLfixed points);
typedef void (APIENTRYP RGLSYMGLMAPGRID1XOESPROC) (GLint n, GLfixed u1, GLfixed u2);
typedef void (APIENTRYP RGLSYMGLMAPGRID2XOESPROC) (GLint n, GLfixed u1, GLfixed u2, GLfixed v1, GLfixed v2);
typedef void (APIENTRYP RGLSYMGLMATERIALXOESPROC) (GLenum face, GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLMATERIALXVOESPROC) (GLenum face, GLenum pname, const GLfixed *param);
typedef void (APIENTRYP RGLSYMGLMULTMATRIXXOESPROC) (const GLfixed *m);
typedef void (APIENTRYP RGLSYMGLMULTTRANSPOSEMATRIXXOESPROC) (const GLfixed *m);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1XOESPROC) (GLenum texture, GLfixed s);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2XOESPROC) (GLenum texture, GLfixed s, GLfixed t);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3XOESPROC) (GLenum texture, GLfixed s, GLfixed t, GLfixed r);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4XOESPROC) (GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1XVOESPROC) (GLenum texture, const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2XVOESPROC) (GLenum texture, const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3XVOESPROC) (GLenum texture, const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4XVOESPROC) (GLenum texture, const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLNORMAL3XOESPROC) (GLfixed nx, GLfixed ny, GLfixed nz);
typedef void (APIENTRYP RGLSYMGLNORMAL3XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLORTHOXOESPROC) (GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f);
typedef void (APIENTRYP RGLSYMGLPASSTHROUGHXOESPROC) (GLfixed token);
typedef void (APIENTRYP RGLSYMGLPIXELMAPXPROC) (GLenum map, GLint size, const GLfixed *values);
typedef void (APIENTRYP RGLSYMGLPIXELSTOREXPROC) (GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLPIXELTRANSFERXOESPROC) (GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLPIXELZOOMXOESPROC) (GLfixed xfactor, GLfixed yfactor);
typedef void (APIENTRYP RGLSYMGLPOINTPARAMETERXVOESPROC) (GLenum pname, const GLfixed *params);
typedef void (APIENTRYP RGLSYMGLPOINTSIZEXOESPROC) (GLfixed size);
typedef void (APIENTRYP RGLSYMGLPOLYGONOFFSETXOESPROC) (GLfixed factor, GLfixed units);
typedef void (APIENTRYP RGLSYMGLPRIORITIZETEXTURESXOESPROC) (GLsizei n, const GLuint *textures, const GLfixed *priorities);
typedef void (APIENTRYP RGLSYMGLRASTERPOS2XOESPROC) (GLfixed x, GLfixed y);
typedef void (APIENTRYP RGLSYMGLRASTERPOS3XOESPROC) (GLfixed x, GLfixed y, GLfixed z);
typedef void (APIENTRYP RGLSYMGLRASTERPOS4XOESPROC) (GLfixed x, GLfixed y, GLfixed z, GLfixed w);
typedef void (APIENTRYP RGLSYMGLRASTERPOS2XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLRASTERPOS3XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLRASTERPOS4XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLRECTXOESPROC) (GLfixed x1, GLfixed y1, GLfixed x2, GLfixed y2);
typedef void (APIENTRYP RGLSYMGLRECTXVOESPROC) (const GLfixed *v1, const GLfixed *v2);
typedef void (APIENTRYP RGLSYMGLROTATEXOESPROC) (GLfixed angle, GLfixed x, GLfixed y, GLfixed z);
typedef void (APIENTRYP RGLSYMGLSAMPLECOVERAGEOESPROC) (GLfixed value, GLboolean invert);
typedef void (APIENTRYP RGLSYMGLSCALEXOESPROC) (GLfixed x, GLfixed y, GLfixed z);
typedef void (APIENTRYP RGLSYMGLTEXCOORD1XOESPROC) (GLfixed s);
typedef void (APIENTRYP RGLSYMGLTEXCOORD2XOESPROC) (GLfixed s, GLfixed t);
typedef void (APIENTRYP RGLSYMGLTEXCOORD3XOESPROC) (GLfixed s, GLfixed t, GLfixed r);
typedef void (APIENTRYP RGLSYMGLTEXCOORD4XOESPROC) (GLfixed s, GLfixed t, GLfixed r, GLfixed q);
typedef void (APIENTRYP RGLSYMGLTEXCOORD1XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORD2XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORD3XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLTEXCOORD4XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLTEXENVXOESPROC) (GLenum target, GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLTEXENVXVOESPROC) (GLenum target, GLenum pname, const GLfixed *params);
typedef void (APIENTRYP RGLSYMGLTEXGENXOESPROC) (GLenum coord, GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLTEXGENXVOESPROC) (GLenum coord, GLenum pname, const GLfixed *params);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERXOESPROC) (GLenum target, GLenum pname, GLfixed param);
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERXVOESPROC) (GLenum target, GLenum pname, const GLfixed *params);
typedef void (APIENTRYP RGLSYMGLTRANSLATEXOESPROC) (GLfixed x, GLfixed y, GLfixed z);
typedef void (APIENTRYP RGLSYMGLVERTEX2XOESPROC) (GLfixed x);
typedef void (APIENTRYP RGLSYMGLVERTEX3XOESPROC) (GLfixed x, GLfixed y);
typedef void (APIENTRYP RGLSYMGLVERTEX4XOESPROC) (GLfixed x, GLfixed y, GLfixed z);
typedef void (APIENTRYP RGLSYMGLVERTEX2XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLVERTEX3XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLVERTEX4XVOESPROC) (const GLfixed *coords);
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEFOESPROC) (GLclampf n, GLclampf f);
typedef void (APIENTRYP RGLSYMGLFRUSTUMFOESPROC) (GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
typedef void (APIENTRYP RGLSYMGLORTHOFOESPROC) (GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
typedef void (APIENTRYP RGLSYMGLCLIPPLANEFOESPROC) (GLenum plane, const GLfloat *equation);
typedef void (APIENTRYP RGLSYMGLCLEARDEPTHFOESPROC) (GLclampf depth);
typedef void (APIENTRYP RGLSYMGLGETCLIPPLANEFOESPROC) (GLenum plane, GLfloat *equation);
typedef GLbitfield (APIENTRYP RGLSYMGLQUERYMATRIXXOESPROC) (GLfixed *mantissa, GLint *exponent);

#define glBlendColor __rglgen_glBlendColor
#define glBlendEquation __rglgen_glBlendEquation
#define glDrawRangeElements __rglgen_glDrawRangeElements
#define glTexImage3D __rglgen_glTexImage3D
#define glTexSubImage3D __rglgen_glTexSubImage3D
#define glCopyTexSubImage3D __rglgen_glCopyTexSubImage3D
#define glColorTable __rglgen_glColorTable
#define glColorTableParameterfv __rglgen_glColorTableParameterfv
#define glColorTableParameteriv __rglgen_glColorTableParameteriv
#define glCopyColorTable __rglgen_glCopyColorTable
#define glGetColorTable __rglgen_glGetColorTable
#define glGetColorTableParameterfv __rglgen_glGetColorTableParameterfv
#define glGetColorTableParameteriv __rglgen_glGetColorTableParameteriv
#define glColorSubTable __rglgen_glColorSubTable
#define glCopyColorSubTable __rglgen_glCopyColorSubTable
#define glConvolutionFilter1D __rglgen_glConvolutionFilter1D
#define glConvolutionFilter2D __rglgen_glConvolutionFilter2D
#define glConvolutionParameterf __rglgen_glConvolutionParameterf
#define glConvolutionParameterfv __rglgen_glConvolutionParameterfv
#define glConvolutionParameteri __rglgen_glConvolutionParameteri
#define glConvolutionParameteriv __rglgen_glConvolutionParameteriv
#define glCopyConvolutionFilter1D __rglgen_glCopyConvolutionFilter1D
#define glCopyConvolutionFilter2D __rglgen_glCopyConvolutionFilter2D
#define glGetConvolutionFilter __rglgen_glGetConvolutionFilter
#define glGetConvolutionParameterfv __rglgen_glGetConvolutionParameterfv
#define glGetConvolutionParameteriv __rglgen_glGetConvolutionParameteriv
#define glGetSeparableFilter __rglgen_glGetSeparableFilter
#define glSeparableFilter2D __rglgen_glSeparableFilter2D
#define glGetHistogram __rglgen_glGetHistogram
#define glGetHistogramParameterfv __rglgen_glGetHistogramParameterfv
#define glGetHistogramParameteriv __rglgen_glGetHistogramParameteriv
#define glGetMinmax __rglgen_glGetMinmax
#define glGetMinmaxParameterfv __rglgen_glGetMinmaxParameterfv
#define glGetMinmaxParameteriv __rglgen_glGetMinmaxParameteriv
#define glHistogram __rglgen_glHistogram
#define glMinmax __rglgen_glMinmax
#define glResetHistogram __rglgen_glResetHistogram
#define glResetMinmax __rglgen_glResetMinmax
#define glActiveTexture __rglgen_glActiveTexture
#define glSampleCoverage __rglgen_glSampleCoverage
#define glCompressedTexImage3D __rglgen_glCompressedTexImage3D
#define glCompressedTexImage2D __rglgen_glCompressedTexImage2D
#define glCompressedTexImage1D __rglgen_glCompressedTexImage1D
#define glCompressedTexSubImage3D __rglgen_glCompressedTexSubImage3D
#define glCompressedTexSubImage2D __rglgen_glCompressedTexSubImage2D
#define glCompressedTexSubImage1D __rglgen_glCompressedTexSubImage1D
#define glGetCompressedTexImage __rglgen_glGetCompressedTexImage
#define glClientActiveTexture __rglgen_glClientActiveTexture
#define glMultiTexCoord1d __rglgen_glMultiTexCoord1d
#define glMultiTexCoord1dv __rglgen_glMultiTexCoord1dv
#define glMultiTexCoord1f __rglgen_glMultiTexCoord1f
#define glMultiTexCoord1fv __rglgen_glMultiTexCoord1fv
#define glMultiTexCoord1i __rglgen_glMultiTexCoord1i
#define glMultiTexCoord1iv __rglgen_glMultiTexCoord1iv
#define glMultiTexCoord1s __rglgen_glMultiTexCoord1s
#define glMultiTexCoord1sv __rglgen_glMultiTexCoord1sv
#define glMultiTexCoord2d __rglgen_glMultiTexCoord2d
#define glMultiTexCoord2dv __rglgen_glMultiTexCoord2dv
#define glMultiTexCoord2f __rglgen_glMultiTexCoord2f
#define glMultiTexCoord2fv __rglgen_glMultiTexCoord2fv
#define glMultiTexCoord2i __rglgen_glMultiTexCoord2i
#define glMultiTexCoord2iv __rglgen_glMultiTexCoord2iv
#define glMultiTexCoord2s __rglgen_glMultiTexCoord2s
#define glMultiTexCoord2sv __rglgen_glMultiTexCoord2sv
#define glMultiTexCoord3d __rglgen_glMultiTexCoord3d
#define glMultiTexCoord3dv __rglgen_glMultiTexCoord3dv
#define glMultiTexCoord3f __rglgen_glMultiTexCoord3f
#define glMultiTexCoord3fv __rglgen_glMultiTexCoord3fv
#define glMultiTexCoord3i __rglgen_glMultiTexCoord3i
#define glMultiTexCoord3iv __rglgen_glMultiTexCoord3iv
#define glMultiTexCoord3s __rglgen_glMultiTexCoord3s
#define glMultiTexCoord3sv __rglgen_glMultiTexCoord3sv
#define glMultiTexCoord4d __rglgen_glMultiTexCoord4d
#define glMultiTexCoord4dv __rglgen_glMultiTexCoord4dv
#define glMultiTexCoord4f __rglgen_glMultiTexCoord4f
#define glMultiTexCoord4fv __rglgen_glMultiTexCoord4fv
#define glMultiTexCoord4i __rglgen_glMultiTexCoord4i
#define glMultiTexCoord4iv __rglgen_glMultiTexCoord4iv
#define glMultiTexCoord4s __rglgen_glMultiTexCoord4s
#define glMultiTexCoord4sv __rglgen_glMultiTexCoord4sv
#define glLoadTransposeMatrixf __rglgen_glLoadTransposeMatrixf
#define glLoadTransposeMatrixd __rglgen_glLoadTransposeMatrixd
#define glMultTransposeMatrixf __rglgen_glMultTransposeMatrixf
#define glMultTransposeMatrixd __rglgen_glMultTransposeMatrixd
#define glBlendFuncSeparate __rglgen_glBlendFuncSeparate
#define glMultiDrawArrays __rglgen_glMultiDrawArrays
#define glMultiDrawElements __rglgen_glMultiDrawElements
#define glPointParameterf __rglgen_glPointParameterf
#define glPointParameterfv __rglgen_glPointParameterfv
#define glPointParameteri __rglgen_glPointParameteri
#define glPointParameteriv __rglgen_glPointParameteriv
#define glFogCoordf __rglgen_glFogCoordf
#define glFogCoordfv __rglgen_glFogCoordfv
#define glFogCoordd __rglgen_glFogCoordd
#define glFogCoorddv __rglgen_glFogCoorddv
#define glFogCoordPointer __rglgen_glFogCoordPointer
#define glSecondaryColor3b __rglgen_glSecondaryColor3b
#define glSecondaryColor3bv __rglgen_glSecondaryColor3bv
#define glSecondaryColor3d __rglgen_glSecondaryColor3d
#define glSecondaryColor3dv __rglgen_glSecondaryColor3dv
#define glSecondaryColor3f __rglgen_glSecondaryColor3f
#define glSecondaryColor3fv __rglgen_glSecondaryColor3fv
#define glSecondaryColor3i __rglgen_glSecondaryColor3i
#define glSecondaryColor3iv __rglgen_glSecondaryColor3iv
#define glSecondaryColor3s __rglgen_glSecondaryColor3s
#define glSecondaryColor3sv __rglgen_glSecondaryColor3sv
#define glSecondaryColor3ub __rglgen_glSecondaryColor3ub
#define glSecondaryColor3ubv __rglgen_glSecondaryColor3ubv
#define glSecondaryColor3ui __rglgen_glSecondaryColor3ui
#define glSecondaryColor3uiv __rglgen_glSecondaryColor3uiv
#define glSecondaryColor3us __rglgen_glSecondaryColor3us
#define glSecondaryColor3usv __rglgen_glSecondaryColor3usv
#define glSecondaryColorPointer __rglgen_glSecondaryColorPointer
#define glWindowPos2d __rglgen_glWindowPos2d
#define glWindowPos2dv __rglgen_glWindowPos2dv
#define glWindowPos2f __rglgen_glWindowPos2f
#define glWindowPos2fv __rglgen_glWindowPos2fv
#define glWindowPos2i __rglgen_glWindowPos2i
#define glWindowPos2iv __rglgen_glWindowPos2iv
#define glWindowPos2s __rglgen_glWindowPos2s
#define glWindowPos2sv __rglgen_glWindowPos2sv
#define glWindowPos3d __rglgen_glWindowPos3d
#define glWindowPos3dv __rglgen_glWindowPos3dv
#define glWindowPos3f __rglgen_glWindowPos3f
#define glWindowPos3fv __rglgen_glWindowPos3fv
#define glWindowPos3i __rglgen_glWindowPos3i
#define glWindowPos3iv __rglgen_glWindowPos3iv
#define glWindowPos3s __rglgen_glWindowPos3s
#define glWindowPos3sv __rglgen_glWindowPos3sv
#define glGenQueries __rglgen_glGenQueries
#define glDeleteQueries __rglgen_glDeleteQueries
#define glIsQuery __rglgen_glIsQuery
#define glBeginQuery __rglgen_glBeginQuery
#define glEndQuery __rglgen_glEndQuery
#define glGetQueryiv __rglgen_glGetQueryiv
#define glGetQueryObjectiv __rglgen_glGetQueryObjectiv
#define glGetQueryObjectuiv __rglgen_glGetQueryObjectuiv
#define glBindBuffer __rglgen_glBindBuffer
#define glDeleteBuffers __rglgen_glDeleteBuffers
#define glGenBuffers __rglgen_glGenBuffers
#define glIsBuffer __rglgen_glIsBuffer
#define glBufferData __rglgen_glBufferData
#define glBufferSubData __rglgen_glBufferSubData
#define glGetBufferSubData __rglgen_glGetBufferSubData
#define glMapBuffer __rglgen_glMapBuffer
#define glUnmapBuffer __rglgen_glUnmapBuffer
#define glGetBufferParameteriv __rglgen_glGetBufferParameteriv
#define glGetBufferPointerv __rglgen_glGetBufferPointerv
#define glBlendEquationSeparate __rglgen_glBlendEquationSeparate
#define glDrawBuffers __rglgen_glDrawBuffers
#define glStencilOpSeparate __rglgen_glStencilOpSeparate
#define glStencilFuncSeparate __rglgen_glStencilFuncSeparate
#define glStencilMaskSeparate __rglgen_glStencilMaskSeparate
#define glAttachShader __rglgen_glAttachShader
#define glBindAttribLocation __rglgen_glBindAttribLocation
#define glCompileShader __rglgen_glCompileShader
#define glCreateProgram __rglgen_glCreateProgram
#define glCreateShader __rglgen_glCreateShader
#define glDeleteProgram __rglgen_glDeleteProgram
#define glDeleteShader __rglgen_glDeleteShader
#define glDetachShader __rglgen_glDetachShader
#define glDisableVertexAttribArray __rglgen_glDisableVertexAttribArray
#define glEnableVertexAttribArray __rglgen_glEnableVertexAttribArray
#define glGetActiveAttrib __rglgen_glGetActiveAttrib
#define glGetActiveUniform __rglgen_glGetActiveUniform
#define glGetAttachedShaders __rglgen_glGetAttachedShaders
#define glGetAttribLocation __rglgen_glGetAttribLocation
#define glGetProgramiv __rglgen_glGetProgramiv
#define glGetProgramInfoLog __rglgen_glGetProgramInfoLog
#define glGetShaderiv __rglgen_glGetShaderiv
#define glGetShaderInfoLog __rglgen_glGetShaderInfoLog
#define glGetShaderSource __rglgen_glGetShaderSource
#define glGetUniformLocation __rglgen_glGetUniformLocation
#define glGetUniformfv __rglgen_glGetUniformfv
#define glGetUniformiv __rglgen_glGetUniformiv
#define glGetVertexAttribdv __rglgen_glGetVertexAttribdv
#define glGetVertexAttribfv __rglgen_glGetVertexAttribfv
#define glGetVertexAttribiv __rglgen_glGetVertexAttribiv
#define glGetVertexAttribPointerv __rglgen_glGetVertexAttribPointerv
#define glIsProgram __rglgen_glIsProgram
#define glIsShader __rglgen_glIsShader
#define glLinkProgram __rglgen_glLinkProgram
#define glShaderSource __rglgen_glShaderSource
#define glUseProgram __rglgen_glUseProgram
#define glUniform1f __rglgen_glUniform1f
#define glUniform2f __rglgen_glUniform2f
#define glUniform3f __rglgen_glUniform3f
#define glUniform4f __rglgen_glUniform4f
#define glUniform1i __rglgen_glUniform1i
#define glUniform2i __rglgen_glUniform2i
#define glUniform3i __rglgen_glUniform3i
#define glUniform4i __rglgen_glUniform4i
#define glUniform1fv __rglgen_glUniform1fv
#define glUniform2fv __rglgen_glUniform2fv
#define glUniform3fv __rglgen_glUniform3fv
#define glUniform4fv __rglgen_glUniform4fv
#define glUniform1iv __rglgen_glUniform1iv
#define glUniform2iv __rglgen_glUniform2iv
#define glUniform3iv __rglgen_glUniform3iv
#define glUniform4iv __rglgen_glUniform4iv
#define glUniformMatrix2fv __rglgen_glUniformMatrix2fv
#define glUniformMatrix3fv __rglgen_glUniformMatrix3fv
#define glUniformMatrix4fv __rglgen_glUniformMatrix4fv
#define glValidateProgram __rglgen_glValidateProgram
#define glVertexAttrib1d __rglgen_glVertexAttrib1d
#define glVertexAttrib1dv __rglgen_glVertexAttrib1dv
#define glVertexAttrib1f __rglgen_glVertexAttrib1f
#define glVertexAttrib1fv __rglgen_glVertexAttrib1fv
#define glVertexAttrib1s __rglgen_glVertexAttrib1s
#define glVertexAttrib1sv __rglgen_glVertexAttrib1sv
#define glVertexAttrib2d __rglgen_glVertexAttrib2d
#define glVertexAttrib2dv __rglgen_glVertexAttrib2dv
#define glVertexAttrib2f __rglgen_glVertexAttrib2f
#define glVertexAttrib2fv __rglgen_glVertexAttrib2fv
#define glVertexAttrib2s __rglgen_glVertexAttrib2s
#define glVertexAttrib2sv __rglgen_glVertexAttrib2sv
#define glVertexAttrib3d __rglgen_glVertexAttrib3d
#define glVertexAttrib3dv __rglgen_glVertexAttrib3dv
#define glVertexAttrib3f __rglgen_glVertexAttrib3f
#define glVertexAttrib3fv __rglgen_glVertexAttrib3fv
#define glVertexAttrib3s __rglgen_glVertexAttrib3s
#define glVertexAttrib3sv __rglgen_glVertexAttrib3sv
#define glVertexAttrib4Nbv __rglgen_glVertexAttrib4Nbv
#define glVertexAttrib4Niv __rglgen_glVertexAttrib4Niv
#define glVertexAttrib4Nsv __rglgen_glVertexAttrib4Nsv
#define glVertexAttrib4Nub __rglgen_glVertexAttrib4Nub
#define glVertexAttrib4Nubv __rglgen_glVertexAttrib4Nubv
#define glVertexAttrib4Nuiv __rglgen_glVertexAttrib4Nuiv
#define glVertexAttrib4Nusv __rglgen_glVertexAttrib4Nusv
#define glVertexAttrib4bv __rglgen_glVertexAttrib4bv
#define glVertexAttrib4d __rglgen_glVertexAttrib4d
#define glVertexAttrib4dv __rglgen_glVertexAttrib4dv
#define glVertexAttrib4f __rglgen_glVertexAttrib4f
#define glVertexAttrib4fv __rglgen_glVertexAttrib4fv
#define glVertexAttrib4iv __rglgen_glVertexAttrib4iv
#define glVertexAttrib4s __rglgen_glVertexAttrib4s
#define glVertexAttrib4sv __rglgen_glVertexAttrib4sv
#define glVertexAttrib4ubv __rglgen_glVertexAttrib4ubv
#define glVertexAttrib4uiv __rglgen_glVertexAttrib4uiv
#define glVertexAttrib4usv __rglgen_glVertexAttrib4usv
#define glVertexAttribPointer __rglgen_glVertexAttribPointer
#define glUniformMatrix2x3fv __rglgen_glUniformMatrix2x3fv
#define glUniformMatrix3x2fv __rglgen_glUniformMatrix3x2fv
#define glUniformMatrix2x4fv __rglgen_glUniformMatrix2x4fv
#define glUniformMatrix4x2fv __rglgen_glUniformMatrix4x2fv
#define glUniformMatrix3x4fv __rglgen_glUniformMatrix3x4fv
#define glUniformMatrix4x3fv __rglgen_glUniformMatrix4x3fv
#define glColorMaski __rglgen_glColorMaski
#define glGetBooleani_v __rglgen_glGetBooleani_v
#define glGetIntegeri_v __rglgen_glGetIntegeri_v
#define glEnablei __rglgen_glEnablei
#define glDisablei __rglgen_glDisablei
#define glIsEnabledi __rglgen_glIsEnabledi
#define glBeginTransformFeedback __rglgen_glBeginTransformFeedback
#define glEndTransformFeedback __rglgen_glEndTransformFeedback
#define glBindBufferRange __rglgen_glBindBufferRange
#define glBindBufferBase __rglgen_glBindBufferBase
#define glTransformFeedbackVaryings __rglgen_glTransformFeedbackVaryings
#define glGetTransformFeedbackVarying __rglgen_glGetTransformFeedbackVarying
#define glClampColor __rglgen_glClampColor
#define glBeginConditionalRender __rglgen_glBeginConditionalRender
#define glEndConditionalRender __rglgen_glEndConditionalRender
#define glVertexAttribIPointer __rglgen_glVertexAttribIPointer
#define glGetVertexAttribIiv __rglgen_glGetVertexAttribIiv
#define glGetVertexAttribIuiv __rglgen_glGetVertexAttribIuiv
#define glVertexAttribI1i __rglgen_glVertexAttribI1i
#define glVertexAttribI2i __rglgen_glVertexAttribI2i
#define glVertexAttribI3i __rglgen_glVertexAttribI3i
#define glVertexAttribI4i __rglgen_glVertexAttribI4i
#define glVertexAttribI1ui __rglgen_glVertexAttribI1ui
#define glVertexAttribI2ui __rglgen_glVertexAttribI2ui
#define glVertexAttribI3ui __rglgen_glVertexAttribI3ui
#define glVertexAttribI4ui __rglgen_glVertexAttribI4ui
#define glVertexAttribI1iv __rglgen_glVertexAttribI1iv
#define glVertexAttribI2iv __rglgen_glVertexAttribI2iv
#define glVertexAttribI3iv __rglgen_glVertexAttribI3iv
#define glVertexAttribI4iv __rglgen_glVertexAttribI4iv
#define glVertexAttribI1uiv __rglgen_glVertexAttribI1uiv
#define glVertexAttribI2uiv __rglgen_glVertexAttribI2uiv
#define glVertexAttribI3uiv __rglgen_glVertexAttribI3uiv
#define glVertexAttribI4uiv __rglgen_glVertexAttribI4uiv
#define glVertexAttribI4bv __rglgen_glVertexAttribI4bv
#define glVertexAttribI4sv __rglgen_glVertexAttribI4sv
#define glVertexAttribI4ubv __rglgen_glVertexAttribI4ubv
#define glVertexAttribI4usv __rglgen_glVertexAttribI4usv
#define glGetUniformuiv __rglgen_glGetUniformuiv
#define glBindFragDataLocation __rglgen_glBindFragDataLocation
#define glGetFragDataLocation __rglgen_glGetFragDataLocation
#define glUniform1ui __rglgen_glUniform1ui
#define glUniform2ui __rglgen_glUniform2ui
#define glUniform3ui __rglgen_glUniform3ui
#define glUniform4ui __rglgen_glUniform4ui
#define glUniform1uiv __rglgen_glUniform1uiv
#define glUniform2uiv __rglgen_glUniform2uiv
#define glUniform3uiv __rglgen_glUniform3uiv
#define glUniform4uiv __rglgen_glUniform4uiv
#define glTexParameterIiv __rglgen_glTexParameterIiv
#define glTexParameterIuiv __rglgen_glTexParameterIuiv
#define glGetTexParameterIiv __rglgen_glGetTexParameterIiv
#define glGetTexParameterIuiv __rglgen_glGetTexParameterIuiv
#define glClearBufferiv __rglgen_glClearBufferiv
#define glClearBufferuiv __rglgen_glClearBufferuiv
#define glClearBufferfv __rglgen_glClearBufferfv
#define glClearBufferfi __rglgen_glClearBufferfi
#define glGetStringi __rglgen_glGetStringi
#define glDrawArraysInstanced __rglgen_glDrawArraysInstanced
#define glDrawElementsInstanced __rglgen_glDrawElementsInstanced
#define glTexBuffer __rglgen_glTexBuffer
#define glPrimitiveRestartIndex __rglgen_glPrimitiveRestartIndex
#define glGetInteger64i_v __rglgen_glGetInteger64i_v
#define glGetBufferParameteri64v __rglgen_glGetBufferParameteri64v
#define glFramebufferTexture __rglgen_glFramebufferTexture
#define glVertexAttribDivisor __rglgen_glVertexAttribDivisor
#define glMinSampleShading __rglgen_glMinSampleShading
#define glBlendEquationi __rglgen_glBlendEquationi
#define glBlendEquationSeparatei __rglgen_glBlendEquationSeparatei
#define glBlendFunci __rglgen_glBlendFunci
#define glBlendFuncSeparatei __rglgen_glBlendFuncSeparatei
#define glActiveTextureARB __rglgen_glActiveTextureARB
#define glClientActiveTextureARB __rglgen_glClientActiveTextureARB
#define glMultiTexCoord1dARB __rglgen_glMultiTexCoord1dARB
#define glMultiTexCoord1dvARB __rglgen_glMultiTexCoord1dvARB
#define glMultiTexCoord1fARB __rglgen_glMultiTexCoord1fARB
#define glMultiTexCoord1fvARB __rglgen_glMultiTexCoord1fvARB
#define glMultiTexCoord1iARB __rglgen_glMultiTexCoord1iARB
#define glMultiTexCoord1ivARB __rglgen_glMultiTexCoord1ivARB
#define glMultiTexCoord1sARB __rglgen_glMultiTexCoord1sARB
#define glMultiTexCoord1svARB __rglgen_glMultiTexCoord1svARB
#define glMultiTexCoord2dARB __rglgen_glMultiTexCoord2dARB
#define glMultiTexCoord2dvARB __rglgen_glMultiTexCoord2dvARB
#define glMultiTexCoord2fARB __rglgen_glMultiTexCoord2fARB
#define glMultiTexCoord2fvARB __rglgen_glMultiTexCoord2fvARB
#define glMultiTexCoord2iARB __rglgen_glMultiTexCoord2iARB
#define glMultiTexCoord2ivARB __rglgen_glMultiTexCoord2ivARB
#define glMultiTexCoord2sARB __rglgen_glMultiTexCoord2sARB
#define glMultiTexCoord2svARB __rglgen_glMultiTexCoord2svARB
#define glMultiTexCoord3dARB __rglgen_glMultiTexCoord3dARB
#define glMultiTexCoord3dvARB __rglgen_glMultiTexCoord3dvARB
#define glMultiTexCoord3fARB __rglgen_glMultiTexCoord3fARB
#define glMultiTexCoord3fvARB __rglgen_glMultiTexCoord3fvARB
#define glMultiTexCoord3iARB __rglgen_glMultiTexCoord3iARB
#define glMultiTexCoord3ivARB __rglgen_glMultiTexCoord3ivARB
#define glMultiTexCoord3sARB __rglgen_glMultiTexCoord3sARB
#define glMultiTexCoord3svARB __rglgen_glMultiTexCoord3svARB
#define glMultiTexCoord4dARB __rglgen_glMultiTexCoord4dARB
#define glMultiTexCoord4dvARB __rglgen_glMultiTexCoord4dvARB
#define glMultiTexCoord4fARB __rglgen_glMultiTexCoord4fARB
#define glMultiTexCoord4fvARB __rglgen_glMultiTexCoord4fvARB
#define glMultiTexCoord4iARB __rglgen_glMultiTexCoord4iARB
#define glMultiTexCoord4ivARB __rglgen_glMultiTexCoord4ivARB
#define glMultiTexCoord4sARB __rglgen_glMultiTexCoord4sARB
#define glMultiTexCoord4svARB __rglgen_glMultiTexCoord4svARB
#define glLoadTransposeMatrixfARB __rglgen_glLoadTransposeMatrixfARB
#define glLoadTransposeMatrixdARB __rglgen_glLoadTransposeMatrixdARB
#define glMultTransposeMatrixfARB __rglgen_glMultTransposeMatrixfARB
#define glMultTransposeMatrixdARB __rglgen_glMultTransposeMatrixdARB
#define glSampleCoverageARB __rglgen_glSampleCoverageARB
#define glCompressedTexImage3DARB __rglgen_glCompressedTexImage3DARB
#define glCompressedTexImage2DARB __rglgen_glCompressedTexImage2DARB
#define glCompressedTexImage1DARB __rglgen_glCompressedTexImage1DARB
#define glCompressedTexSubImage3DARB __rglgen_glCompressedTexSubImage3DARB
#define glCompressedTexSubImage2DARB __rglgen_glCompressedTexSubImage2DARB
#define glCompressedTexSubImage1DARB __rglgen_glCompressedTexSubImage1DARB
#define glGetCompressedTexImageARB __rglgen_glGetCompressedTexImageARB
#define glPointParameterfARB __rglgen_glPointParameterfARB
#define glPointParameterfvARB __rglgen_glPointParameterfvARB
#define glWeightbvARB __rglgen_glWeightbvARB
#define glWeightsvARB __rglgen_glWeightsvARB
#define glWeightivARB __rglgen_glWeightivARB
#define glWeightfvARB __rglgen_glWeightfvARB
#define glWeightdvARB __rglgen_glWeightdvARB
#define glWeightubvARB __rglgen_glWeightubvARB
#define glWeightusvARB __rglgen_glWeightusvARB
#define glWeightuivARB __rglgen_glWeightuivARB
#define glWeightPointerARB __rglgen_glWeightPointerARB
#define glVertexBlendARB __rglgen_glVertexBlendARB
#define glCurrentPaletteMatrixARB __rglgen_glCurrentPaletteMatrixARB
#define glMatrixIndexubvARB __rglgen_glMatrixIndexubvARB
#define glMatrixIndexusvARB __rglgen_glMatrixIndexusvARB
#define glMatrixIndexuivARB __rglgen_glMatrixIndexuivARB
#define glMatrixIndexPointerARB __rglgen_glMatrixIndexPointerARB
#define glWindowPos2dARB __rglgen_glWindowPos2dARB
#define glWindowPos2dvARB __rglgen_glWindowPos2dvARB
#define glWindowPos2fARB __rglgen_glWindowPos2fARB
#define glWindowPos2fvARB __rglgen_glWindowPos2fvARB
#define glWindowPos2iARB __rglgen_glWindowPos2iARB
#define glWindowPos2ivARB __rglgen_glWindowPos2ivARB
#define glWindowPos2sARB __rglgen_glWindowPos2sARB
#define glWindowPos2svARB __rglgen_glWindowPos2svARB
#define glWindowPos3dARB __rglgen_glWindowPos3dARB
#define glWindowPos3dvARB __rglgen_glWindowPos3dvARB
#define glWindowPos3fARB __rglgen_glWindowPos3fARB
#define glWindowPos3fvARB __rglgen_glWindowPos3fvARB
#define glWindowPos3iARB __rglgen_glWindowPos3iARB
#define glWindowPos3ivARB __rglgen_glWindowPos3ivARB
#define glWindowPos3sARB __rglgen_glWindowPos3sARB
#define glWindowPos3svARB __rglgen_glWindowPos3svARB
#define glVertexAttrib1dARB __rglgen_glVertexAttrib1dARB
#define glVertexAttrib1dvARB __rglgen_glVertexAttrib1dvARB
#define glVertexAttrib1fARB __rglgen_glVertexAttrib1fARB
#define glVertexAttrib1fvARB __rglgen_glVertexAttrib1fvARB
#define glVertexAttrib1sARB __rglgen_glVertexAttrib1sARB
#define glVertexAttrib1svARB __rglgen_glVertexAttrib1svARB
#define glVertexAttrib2dARB __rglgen_glVertexAttrib2dARB
#define glVertexAttrib2dvARB __rglgen_glVertexAttrib2dvARB
#define glVertexAttrib2fARB __rglgen_glVertexAttrib2fARB
#define glVertexAttrib2fvARB __rglgen_glVertexAttrib2fvARB
#define glVertexAttrib2sARB __rglgen_glVertexAttrib2sARB
#define glVertexAttrib2svARB __rglgen_glVertexAttrib2svARB
#define glVertexAttrib3dARB __rglgen_glVertexAttrib3dARB
#define glVertexAttrib3dvARB __rglgen_glVertexAttrib3dvARB
#define glVertexAttrib3fARB __rglgen_glVertexAttrib3fARB
#define glVertexAttrib3fvARB __rglgen_glVertexAttrib3fvARB
#define glVertexAttrib3sARB __rglgen_glVertexAttrib3sARB
#define glVertexAttrib3svARB __rglgen_glVertexAttrib3svARB
#define glVertexAttrib4NbvARB __rglgen_glVertexAttrib4NbvARB
#define glVertexAttrib4NivARB __rglgen_glVertexAttrib4NivARB
#define glVertexAttrib4NsvARB __rglgen_glVertexAttrib4NsvARB
#define glVertexAttrib4NubARB __rglgen_glVertexAttrib4NubARB
#define glVertexAttrib4NubvARB __rglgen_glVertexAttrib4NubvARB
#define glVertexAttrib4NuivARB __rglgen_glVertexAttrib4NuivARB
#define glVertexAttrib4NusvARB __rglgen_glVertexAttrib4NusvARB
#define glVertexAttrib4bvARB __rglgen_glVertexAttrib4bvARB
#define glVertexAttrib4dARB __rglgen_glVertexAttrib4dARB
#define glVertexAttrib4dvARB __rglgen_glVertexAttrib4dvARB
#define glVertexAttrib4fARB __rglgen_glVertexAttrib4fARB
#define glVertexAttrib4fvARB __rglgen_glVertexAttrib4fvARB
#define glVertexAttrib4ivARB __rglgen_glVertexAttrib4ivARB
#define glVertexAttrib4sARB __rglgen_glVertexAttrib4sARB
#define glVertexAttrib4svARB __rglgen_glVertexAttrib4svARB
#define glVertexAttrib4ubvARB __rglgen_glVertexAttrib4ubvARB
#define glVertexAttrib4uivARB __rglgen_glVertexAttrib4uivARB
#define glVertexAttrib4usvARB __rglgen_glVertexAttrib4usvARB
#define glVertexAttribPointerARB __rglgen_glVertexAttribPointerARB
#define glEnableVertexAttribArrayARB __rglgen_glEnableVertexAttribArrayARB
#define glDisableVertexAttribArrayARB __rglgen_glDisableVertexAttribArrayARB
#define glProgramStringARB __rglgen_glProgramStringARB
#define glBindProgramARB __rglgen_glBindProgramARB
#define glDeleteProgramsARB __rglgen_glDeleteProgramsARB
#define glGenProgramsARB __rglgen_glGenProgramsARB
#define glProgramEnvParameter4dARB __rglgen_glProgramEnvParameter4dARB
#define glProgramEnvParameter4dvARB __rglgen_glProgramEnvParameter4dvARB
#define glProgramEnvParameter4fARB __rglgen_glProgramEnvParameter4fARB
#define glProgramEnvParameter4fvARB __rglgen_glProgramEnvParameter4fvARB
#define glProgramLocalParameter4dARB __rglgen_glProgramLocalParameter4dARB
#define glProgramLocalParameter4dvARB __rglgen_glProgramLocalParameter4dvARB
#define glProgramLocalParameter4fARB __rglgen_glProgramLocalParameter4fARB
#define glProgramLocalParameter4fvARB __rglgen_glProgramLocalParameter4fvARB
#define glGetProgramEnvParameterdvARB __rglgen_glGetProgramEnvParameterdvARB
#define glGetProgramEnvParameterfvARB __rglgen_glGetProgramEnvParameterfvARB
#define glGetProgramLocalParameterdvARB __rglgen_glGetProgramLocalParameterdvARB
#define glGetProgramLocalParameterfvARB __rglgen_glGetProgramLocalParameterfvARB
#define glGetProgramivARB __rglgen_glGetProgramivARB
#define glGetProgramStringARB __rglgen_glGetProgramStringARB
#define glGetVertexAttribdvARB __rglgen_glGetVertexAttribdvARB
#define glGetVertexAttribfvARB __rglgen_glGetVertexAttribfvARB
#define glGetVertexAttribivARB __rglgen_glGetVertexAttribivARB
#define glGetVertexAttribPointervARB __rglgen_glGetVertexAttribPointervARB
#define glIsProgramARB __rglgen_glIsProgramARB
#define glBindBufferARB __rglgen_glBindBufferARB
#define glDeleteBuffersARB __rglgen_glDeleteBuffersARB
#define glGenBuffersARB __rglgen_glGenBuffersARB
#define glIsBufferARB __rglgen_glIsBufferARB
#define glBufferDataARB __rglgen_glBufferDataARB
#define glBufferSubDataARB __rglgen_glBufferSubDataARB
#define glGetBufferSubDataARB __rglgen_glGetBufferSubDataARB
#define glMapBufferARB __rglgen_glMapBufferARB
#define glUnmapBufferARB __rglgen_glUnmapBufferARB
#define glGetBufferParameterivARB __rglgen_glGetBufferParameterivARB
#define glGetBufferPointervARB __rglgen_glGetBufferPointervARB
#define glGenQueriesARB __rglgen_glGenQueriesARB
#define glDeleteQueriesARB __rglgen_glDeleteQueriesARB
#define glIsQueryARB __rglgen_glIsQueryARB
#define glBeginQueryARB __rglgen_glBeginQueryARB
#define glEndQueryARB __rglgen_glEndQueryARB
#define glGetQueryivARB __rglgen_glGetQueryivARB
#define glGetQueryObjectivARB __rglgen_glGetQueryObjectivARB
#define glGetQueryObjectuivARB __rglgen_glGetQueryObjectuivARB
#define glDeleteObjectARB __rglgen_glDeleteObjectARB
#define glGetHandleARB __rglgen_glGetHandleARB
#define glDetachObjectARB __rglgen_glDetachObjectARB
#define glCreateShaderObjectARB __rglgen_glCreateShaderObjectARB
#define glShaderSourceARB __rglgen_glShaderSourceARB
#define glCompileShaderARB __rglgen_glCompileShaderARB
#define glCreateProgramObjectARB __rglgen_glCreateProgramObjectARB
#define glAttachObjectARB __rglgen_glAttachObjectARB
#define glLinkProgramARB __rglgen_glLinkProgramARB
#define glUseProgramObjectARB __rglgen_glUseProgramObjectARB
#define glValidateProgramARB __rglgen_glValidateProgramARB
#define glUniform1fARB __rglgen_glUniform1fARB
#define glUniform2fARB __rglgen_glUniform2fARB
#define glUniform3fARB __rglgen_glUniform3fARB
#define glUniform4fARB __rglgen_glUniform4fARB
#define glUniform1iARB __rglgen_glUniform1iARB
#define glUniform2iARB __rglgen_glUniform2iARB
#define glUniform3iARB __rglgen_glUniform3iARB
#define glUniform4iARB __rglgen_glUniform4iARB
#define glUniform1fvARB __rglgen_glUniform1fvARB
#define glUniform2fvARB __rglgen_glUniform2fvARB
#define glUniform3fvARB __rglgen_glUniform3fvARB
#define glUniform4fvARB __rglgen_glUniform4fvARB
#define glUniform1ivARB __rglgen_glUniform1ivARB
#define glUniform2ivARB __rglgen_glUniform2ivARB
#define glUniform3ivARB __rglgen_glUniform3ivARB
#define glUniform4ivARB __rglgen_glUniform4ivARB
#define glUniformMatrix2fvARB __rglgen_glUniformMatrix2fvARB
#define glUniformMatrix3fvARB __rglgen_glUniformMatrix3fvARB
#define glUniformMatrix4fvARB __rglgen_glUniformMatrix4fvARB
#define glGetObjectParameterfvARB __rglgen_glGetObjectParameterfvARB
#define glGetObjectParameterivARB __rglgen_glGetObjectParameterivARB
#define glGetInfoLogARB __rglgen_glGetInfoLogARB
#define glGetAttachedObjectsARB __rglgen_glGetAttachedObjectsARB
#define glGetUniformLocationARB __rglgen_glGetUniformLocationARB
#define glGetActiveUniformARB __rglgen_glGetActiveUniformARB
#define glGetUniformfvARB __rglgen_glGetUniformfvARB
#define glGetUniformivARB __rglgen_glGetUniformivARB
#define glGetShaderSourceARB __rglgen_glGetShaderSourceARB
#define glBindAttribLocationARB __rglgen_glBindAttribLocationARB
#define glGetActiveAttribARB __rglgen_glGetActiveAttribARB
#define glGetAttribLocationARB __rglgen_glGetAttribLocationARB
#define glDrawBuffersARB __rglgen_glDrawBuffersARB
#define glClampColorARB __rglgen_glClampColorARB
#define glDrawArraysInstancedARB __rglgen_glDrawArraysInstancedARB
#define glDrawElementsInstancedARB __rglgen_glDrawElementsInstancedARB
#define glIsRenderbuffer __rglgen_glIsRenderbuffer
#define glBindRenderbuffer __rglgen_glBindRenderbuffer
#define glDeleteRenderbuffers __rglgen_glDeleteRenderbuffers
#define glGenRenderbuffers __rglgen_glGenRenderbuffers
#define glRenderbufferStorage __rglgen_glRenderbufferStorage
#define glGetRenderbufferParameteriv __rglgen_glGetRenderbufferParameteriv
#define glIsFramebuffer __rglgen_glIsFramebuffer
#define glBindFramebuffer __rglgen_glBindFramebuffer
#define glDeleteFramebuffers __rglgen_glDeleteFramebuffers
#define glGenFramebuffers __rglgen_glGenFramebuffers
#define glCheckFramebufferStatus __rglgen_glCheckFramebufferStatus
#define glFramebufferTexture1D __rglgen_glFramebufferTexture1D
#define glFramebufferTexture2D __rglgen_glFramebufferTexture2D
#define glFramebufferTexture3D __rglgen_glFramebufferTexture3D
#define glFramebufferRenderbuffer __rglgen_glFramebufferRenderbuffer
#define glGetFramebufferAttachmentParameteriv __rglgen_glGetFramebufferAttachmentParameteriv
#define glGenerateMipmap __rglgen_glGenerateMipmap
#define glBlitFramebuffer __rglgen_glBlitFramebuffer
#define glRenderbufferStorageMultisample __rglgen_glRenderbufferStorageMultisample
#define glFramebufferTextureLayer __rglgen_glFramebufferTextureLayer
#define glProgramParameteriARB __rglgen_glProgramParameteriARB
#define glFramebufferTextureARB __rglgen_glFramebufferTextureARB
#define glFramebufferTextureLayerARB __rglgen_glFramebufferTextureLayerARB
#define glFramebufferTextureFaceARB __rglgen_glFramebufferTextureFaceARB
#define glVertexAttribDivisorARB __rglgen_glVertexAttribDivisorARB
#define glMapBufferRange __rglgen_glMapBufferRange
#define glFlushMappedBufferRange __rglgen_glFlushMappedBufferRange
#define glTexBufferARB __rglgen_glTexBufferARB
#define glBindVertexArray __rglgen_glBindVertexArray
#define glDeleteVertexArrays __rglgen_glDeleteVertexArrays
#define glGenVertexArrays __rglgen_glGenVertexArrays
#define glIsVertexArray __rglgen_glIsVertexArray
#define glGetUniformIndices __rglgen_glGetUniformIndices
#define glGetActiveUniformsiv __rglgen_glGetActiveUniformsiv
#define glGetActiveUniformName __rglgen_glGetActiveUniformName
#define glGetUniformBlockIndex __rglgen_glGetUniformBlockIndex
#define glGetActiveUniformBlockiv __rglgen_glGetActiveUniformBlockiv
#define glGetActiveUniformBlockName __rglgen_glGetActiveUniformBlockName
#define glUniformBlockBinding __rglgen_glUniformBlockBinding
#define glCopyBufferSubData __rglgen_glCopyBufferSubData
#define glDrawElementsBaseVertex __rglgen_glDrawElementsBaseVertex
#define glDrawRangeElementsBaseVertex __rglgen_glDrawRangeElementsBaseVertex
#define glDrawElementsInstancedBaseVertex __rglgen_glDrawElementsInstancedBaseVertex
#define glMultiDrawElementsBaseVertex __rglgen_glMultiDrawElementsBaseVertex
#define glProvokingVertex __rglgen_glProvokingVertex
#define glFenceSync __rglgen_glFenceSync
#define glIsSync __rglgen_glIsSync
#define glDeleteSync __rglgen_glDeleteSync
#define glClientWaitSync __rglgen_glClientWaitSync
#define glWaitSync __rglgen_glWaitSync
#define glGetInteger64v __rglgen_glGetInteger64v
#define glGetSynciv __rglgen_glGetSynciv
#define glTexImage2DMultisample __rglgen_glTexImage2DMultisample
#define glTexImage3DMultisample __rglgen_glTexImage3DMultisample
#define glGetMultisamplefv __rglgen_glGetMultisamplefv
#define glSampleMaski __rglgen_glSampleMaski
#define glBlendEquationiARB __rglgen_glBlendEquationiARB
#define glBlendEquationSeparateiARB __rglgen_glBlendEquationSeparateiARB
#define glBlendFunciARB __rglgen_glBlendFunciARB
#define glBlendFuncSeparateiARB __rglgen_glBlendFuncSeparateiARB
#define glMinSampleShadingARB __rglgen_glMinSampleShadingARB
#define glNamedStringARB __rglgen_glNamedStringARB
#define glDeleteNamedStringARB __rglgen_glDeleteNamedStringARB
#define glCompileShaderIncludeARB __rglgen_glCompileShaderIncludeARB
#define glIsNamedStringARB __rglgen_glIsNamedStringARB
#define glGetNamedStringARB __rglgen_glGetNamedStringARB
#define glGetNamedStringivARB __rglgen_glGetNamedStringivARB
#define glBindFragDataLocationIndexed __rglgen_glBindFragDataLocationIndexed
#define glGetFragDataIndex __rglgen_glGetFragDataIndex
#define glGenSamplers __rglgen_glGenSamplers
#define glDeleteSamplers __rglgen_glDeleteSamplers
#define glIsSampler __rglgen_glIsSampler
#define glBindSampler __rglgen_glBindSampler
#define glSamplerParameteri __rglgen_glSamplerParameteri
#define glSamplerParameteriv __rglgen_glSamplerParameteriv
#define glSamplerParameterf __rglgen_glSamplerParameterf
#define glSamplerParameterfv __rglgen_glSamplerParameterfv
#define glSamplerParameterIiv __rglgen_glSamplerParameterIiv
#define glSamplerParameterIuiv __rglgen_glSamplerParameterIuiv
#define glGetSamplerParameteriv __rglgen_glGetSamplerParameteriv
#define glGetSamplerParameterIiv __rglgen_glGetSamplerParameterIiv
#define glGetSamplerParameterfv __rglgen_glGetSamplerParameterfv
#define glGetSamplerParameterIuiv __rglgen_glGetSamplerParameterIuiv
#define glQueryCounter __rglgen_glQueryCounter
#define glGetQueryObjecti64v __rglgen_glGetQueryObjecti64v
#define glGetQueryObjectui64v __rglgen_glGetQueryObjectui64v
#define glVertexP2ui __rglgen_glVertexP2ui
#define glVertexP2uiv __rglgen_glVertexP2uiv
#define glVertexP3ui __rglgen_glVertexP3ui
#define glVertexP3uiv __rglgen_glVertexP3uiv
#define glVertexP4ui __rglgen_glVertexP4ui
#define glVertexP4uiv __rglgen_glVertexP4uiv
#define glTexCoordP1ui __rglgen_glTexCoordP1ui
#define glTexCoordP1uiv __rglgen_glTexCoordP1uiv
#define glTexCoordP2ui __rglgen_glTexCoordP2ui
#define glTexCoordP2uiv __rglgen_glTexCoordP2uiv
#define glTexCoordP3ui __rglgen_glTexCoordP3ui
#define glTexCoordP3uiv __rglgen_glTexCoordP3uiv
#define glTexCoordP4ui __rglgen_glTexCoordP4ui
#define glTexCoordP4uiv __rglgen_glTexCoordP4uiv
#define glMultiTexCoordP1ui __rglgen_glMultiTexCoordP1ui
#define glMultiTexCoordP1uiv __rglgen_glMultiTexCoordP1uiv
#define glMultiTexCoordP2ui __rglgen_glMultiTexCoordP2ui
#define glMultiTexCoordP2uiv __rglgen_glMultiTexCoordP2uiv
#define glMultiTexCoordP3ui __rglgen_glMultiTexCoordP3ui
#define glMultiTexCoordP3uiv __rglgen_glMultiTexCoordP3uiv
#define glMultiTexCoordP4ui __rglgen_glMultiTexCoordP4ui
#define glMultiTexCoordP4uiv __rglgen_glMultiTexCoordP4uiv
#define glNormalP3ui __rglgen_glNormalP3ui
#define glNormalP3uiv __rglgen_glNormalP3uiv
#define glColorP3ui __rglgen_glColorP3ui
#define glColorP3uiv __rglgen_glColorP3uiv
#define glColorP4ui __rglgen_glColorP4ui
#define glColorP4uiv __rglgen_glColorP4uiv
#define glSecondaryColorP3ui __rglgen_glSecondaryColorP3ui
#define glSecondaryColorP3uiv __rglgen_glSecondaryColorP3uiv
#define glVertexAttribP1ui __rglgen_glVertexAttribP1ui
#define glVertexAttribP1uiv __rglgen_glVertexAttribP1uiv
#define glVertexAttribP2ui __rglgen_glVertexAttribP2ui
#define glVertexAttribP2uiv __rglgen_glVertexAttribP2uiv
#define glVertexAttribP3ui __rglgen_glVertexAttribP3ui
#define glVertexAttribP3uiv __rglgen_glVertexAttribP3uiv
#define glVertexAttribP4ui __rglgen_glVertexAttribP4ui
#define glVertexAttribP4uiv __rglgen_glVertexAttribP4uiv
#define glDrawArraysIndirect __rglgen_glDrawArraysIndirect
#define glDrawElementsIndirect __rglgen_glDrawElementsIndirect
#define glUniform1d __rglgen_glUniform1d
#define glUniform2d __rglgen_glUniform2d
#define glUniform3d __rglgen_glUniform3d
#define glUniform4d __rglgen_glUniform4d
#define glUniform1dv __rglgen_glUniform1dv
#define glUniform2dv __rglgen_glUniform2dv
#define glUniform3dv __rglgen_glUniform3dv
#define glUniform4dv __rglgen_glUniform4dv
#define glUniformMatrix2dv __rglgen_glUniformMatrix2dv
#define glUniformMatrix3dv __rglgen_glUniformMatrix3dv
#define glUniformMatrix4dv __rglgen_glUniformMatrix4dv
#define glUniformMatrix2x3dv __rglgen_glUniformMatrix2x3dv
#define glUniformMatrix2x4dv __rglgen_glUniformMatrix2x4dv
#define glUniformMatrix3x2dv __rglgen_glUniformMatrix3x2dv
#define glUniformMatrix3x4dv __rglgen_glUniformMatrix3x4dv
#define glUniformMatrix4x2dv __rglgen_glUniformMatrix4x2dv
#define glUniformMatrix4x3dv __rglgen_glUniformMatrix4x3dv
#define glGetUniformdv __rglgen_glGetUniformdv
#define glGetSubroutineUniformLocation __rglgen_glGetSubroutineUniformLocation
#define glGetSubroutineIndex __rglgen_glGetSubroutineIndex
#define glGetActiveSubroutineUniformiv __rglgen_glGetActiveSubroutineUniformiv
#define glGetActiveSubroutineUniformName __rglgen_glGetActiveSubroutineUniformName
#define glGetActiveSubroutineName __rglgen_glGetActiveSubroutineName
#define glUniformSubroutinesuiv __rglgen_glUniformSubroutinesuiv
#define glGetUniformSubroutineuiv __rglgen_glGetUniformSubroutineuiv
#define glGetProgramStageiv __rglgen_glGetProgramStageiv
#define glPatchParameteri __rglgen_glPatchParameteri
#define glPatchParameterfv __rglgen_glPatchParameterfv
#define glBindTransformFeedback __rglgen_glBindTransformFeedback
#define glDeleteTransformFeedbacks __rglgen_glDeleteTransformFeedbacks
#define glGenTransformFeedbacks __rglgen_glGenTransformFeedbacks
#define glIsTransformFeedback __rglgen_glIsTransformFeedback
#define glPauseTransformFeedback __rglgen_glPauseTransformFeedback
#define glResumeTransformFeedback __rglgen_glResumeTransformFeedback
#define glDrawTransformFeedback __rglgen_glDrawTransformFeedback
#define glDrawTransformFeedbackStream __rglgen_glDrawTransformFeedbackStream
#define glBeginQueryIndexed __rglgen_glBeginQueryIndexed
#define glEndQueryIndexed __rglgen_glEndQueryIndexed
#define glGetQueryIndexediv __rglgen_glGetQueryIndexediv
#define glReleaseShaderCompiler __rglgen_glReleaseShaderCompiler
#define glShaderBinary __rglgen_glShaderBinary
#define glGetShaderPrecisionFormat __rglgen_glGetShaderPrecisionFormat
#define glDepthRangef __rglgen_glDepthRangef
#define glClearDepthf __rglgen_glClearDepthf
#define glGetProgramBinary __rglgen_glGetProgramBinary
#define glProgramBinary __rglgen_glProgramBinary
#define glProgramParameteri __rglgen_glProgramParameteri
#define glUseProgramStages __rglgen_glUseProgramStages
#define glActiveShaderProgram __rglgen_glActiveShaderProgram
#define glCreateShaderProgramv __rglgen_glCreateShaderProgramv
#define glBindProgramPipeline __rglgen_glBindProgramPipeline
#define glDeleteProgramPipelines __rglgen_glDeleteProgramPipelines
#define glGenProgramPipelines __rglgen_glGenProgramPipelines
#define glIsProgramPipeline __rglgen_glIsProgramPipeline
#define glGetProgramPipelineiv __rglgen_glGetProgramPipelineiv
#define glProgramUniform1i __rglgen_glProgramUniform1i
#define glProgramUniform1iv __rglgen_glProgramUniform1iv
#define glProgramUniform1f __rglgen_glProgramUniform1f
#define glProgramUniform1fv __rglgen_glProgramUniform1fv
#define glProgramUniform1d __rglgen_glProgramUniform1d
#define glProgramUniform1dv __rglgen_glProgramUniform1dv
#define glProgramUniform1ui __rglgen_glProgramUniform1ui
#define glProgramUniform1uiv __rglgen_glProgramUniform1uiv
#define glProgramUniform2i __rglgen_glProgramUniform2i
#define glProgramUniform2iv __rglgen_glProgramUniform2iv
#define glProgramUniform2f __rglgen_glProgramUniform2f
#define glProgramUniform2fv __rglgen_glProgramUniform2fv
#define glProgramUniform2d __rglgen_glProgramUniform2d
#define glProgramUniform2dv __rglgen_glProgramUniform2dv
#define glProgramUniform2ui __rglgen_glProgramUniform2ui
#define glProgramUniform2uiv __rglgen_glProgramUniform2uiv
#define glProgramUniform3i __rglgen_glProgramUniform3i
#define glProgramUniform3iv __rglgen_glProgramUniform3iv
#define glProgramUniform3f __rglgen_glProgramUniform3f
#define glProgramUniform3fv __rglgen_glProgramUniform3fv
#define glProgramUniform3d __rglgen_glProgramUniform3d
#define glProgramUniform3dv __rglgen_glProgramUniform3dv
#define glProgramUniform3ui __rglgen_glProgramUniform3ui
#define glProgramUniform3uiv __rglgen_glProgramUniform3uiv
#define glProgramUniform4i __rglgen_glProgramUniform4i
#define glProgramUniform4iv __rglgen_glProgramUniform4iv
#define glProgramUniform4f __rglgen_glProgramUniform4f
#define glProgramUniform4fv __rglgen_glProgramUniform4fv
#define glProgramUniform4d __rglgen_glProgramUniform4d
#define glProgramUniform4dv __rglgen_glProgramUniform4dv
#define glProgramUniform4ui __rglgen_glProgramUniform4ui
#define glProgramUniform4uiv __rglgen_glProgramUniform4uiv
#define glProgramUniformMatrix2fv __rglgen_glProgramUniformMatrix2fv
#define glProgramUniformMatrix3fv __rglgen_glProgramUniformMatrix3fv
#define glProgramUniformMatrix4fv __rglgen_glProgramUniformMatrix4fv
#define glProgramUniformMatrix2dv __rglgen_glProgramUniformMatrix2dv
#define glProgramUniformMatrix3dv __rglgen_glProgramUniformMatrix3dv
#define glProgramUniformMatrix4dv __rglgen_glProgramUniformMatrix4dv
#define glProgramUniformMatrix2x3fv __rglgen_glProgramUniformMatrix2x3fv
#define glProgramUniformMatrix3x2fv __rglgen_glProgramUniformMatrix3x2fv
#define glProgramUniformMatrix2x4fv __rglgen_glProgramUniformMatrix2x4fv
#define glProgramUniformMatrix4x2fv __rglgen_glProgramUniformMatrix4x2fv
#define glProgramUniformMatrix3x4fv __rglgen_glProgramUniformMatrix3x4fv
#define glProgramUniformMatrix4x3fv __rglgen_glProgramUniformMatrix4x3fv
#define glProgramUniformMatrix2x3dv __rglgen_glProgramUniformMatrix2x3dv
#define glProgramUniformMatrix3x2dv __rglgen_glProgramUniformMatrix3x2dv
#define glProgramUniformMatrix2x4dv __rglgen_glProgramUniformMatrix2x4dv
#define glProgramUniformMatrix4x2dv __rglgen_glProgramUniformMatrix4x2dv
#define glProgramUniformMatrix3x4dv __rglgen_glProgramUniformMatrix3x4dv
#define glProgramUniformMatrix4x3dv __rglgen_glProgramUniformMatrix4x3dv
#define glValidateProgramPipeline __rglgen_glValidateProgramPipeline
#define glGetProgramPipelineInfoLog __rglgen_glGetProgramPipelineInfoLog
#define glVertexAttribL1d __rglgen_glVertexAttribL1d
#define glVertexAttribL2d __rglgen_glVertexAttribL2d
#define glVertexAttribL3d __rglgen_glVertexAttribL3d
#define glVertexAttribL4d __rglgen_glVertexAttribL4d
#define glVertexAttribL1dv __rglgen_glVertexAttribL1dv
#define glVertexAttribL2dv __rglgen_glVertexAttribL2dv
#define glVertexAttribL3dv __rglgen_glVertexAttribL3dv
#define glVertexAttribL4dv __rglgen_glVertexAttribL4dv
#define glVertexAttribLPointer __rglgen_glVertexAttribLPointer
#define glGetVertexAttribLdv __rglgen_glGetVertexAttribLdv
#define glViewportArrayv __rglgen_glViewportArrayv
#define glViewportIndexedf __rglgen_glViewportIndexedf
#define glViewportIndexedfv __rglgen_glViewportIndexedfv
#define glScissorArrayv __rglgen_glScissorArrayv
#define glScissorIndexed __rglgen_glScissorIndexed
#define glScissorIndexedv __rglgen_glScissorIndexedv
#define glDepthRangeArrayv __rglgen_glDepthRangeArrayv
#define glDepthRangeIndexed __rglgen_glDepthRangeIndexed
#define glGetFloati_v __rglgen_glGetFloati_v
#define glGetDoublei_v __rglgen_glGetDoublei_v
#define glCreateSyncFromCLeventARB __rglgen_glCreateSyncFromCLeventARB
#define glDebugMessageControlARB __rglgen_glDebugMessageControlARB
#define glDebugMessageInsertARB __rglgen_glDebugMessageInsertARB
#define glDebugMessageCallbackARB __rglgen_glDebugMessageCallbackARB
#define glGetDebugMessageLogARB __rglgen_glGetDebugMessageLogARB
#define glGetGraphicsResetStatusARB __rglgen_glGetGraphicsResetStatusARB
#define glGetnMapdvARB __rglgen_glGetnMapdvARB
#define glGetnMapfvARB __rglgen_glGetnMapfvARB
#define glGetnMapivARB __rglgen_glGetnMapivARB
#define glGetnPixelMapfvARB __rglgen_glGetnPixelMapfvARB
#define glGetnPixelMapuivARB __rglgen_glGetnPixelMapuivARB
#define glGetnPixelMapusvARB __rglgen_glGetnPixelMapusvARB
#define glGetnPolygonStippleARB __rglgen_glGetnPolygonStippleARB
#define glGetnColorTableARB __rglgen_glGetnColorTableARB
#define glGetnConvolutionFilterARB __rglgen_glGetnConvolutionFilterARB
#define glGetnSeparableFilterARB __rglgen_glGetnSeparableFilterARB
#define glGetnHistogramARB __rglgen_glGetnHistogramARB
#define glGetnMinmaxARB __rglgen_glGetnMinmaxARB
#define glGetnTexImageARB __rglgen_glGetnTexImageARB
#define glReadnPixelsARB __rglgen_glReadnPixelsARB
#define glGetnCompressedTexImageARB __rglgen_glGetnCompressedTexImageARB
#define glGetnUniformfvARB __rglgen_glGetnUniformfvARB
#define glGetnUniformivARB __rglgen_glGetnUniformivARB
#define glGetnUniformuivARB __rglgen_glGetnUniformuivARB
#define glGetnUniformdvARB __rglgen_glGetnUniformdvARB
#define glDrawArraysInstancedBaseInstance __rglgen_glDrawArraysInstancedBaseInstance
#define glDrawElementsInstancedBaseInstance __rglgen_glDrawElementsInstancedBaseInstance
#define glDrawElementsInstancedBaseVertexBaseInstance __rglgen_glDrawElementsInstancedBaseVertexBaseInstance
#define glDrawTransformFeedbackInstanced __rglgen_glDrawTransformFeedbackInstanced
#define glDrawTransformFeedbackStreamInstanced __rglgen_glDrawTransformFeedbackStreamInstanced
#define glGetInternalformativ __rglgen_glGetInternalformativ
#define glGetActiveAtomicCounterBufferiv __rglgen_glGetActiveAtomicCounterBufferiv
#define glBindImageTexture __rglgen_glBindImageTexture
#define glMemoryBarrier __rglgen_glMemoryBarrier
#define glTexStorage1D __rglgen_glTexStorage1D
#define glTexStorage2D __rglgen_glTexStorage2D
#define glTexStorage3D __rglgen_glTexStorage3D
#define glDebugMessageControl __rglgen_glDebugMessageControl
#define glDebugMessageInsert __rglgen_glDebugMessageInsert
#define glDebugMessageCallback __rglgen_glDebugMessageCallback
#define glGetDebugMessageLog __rglgen_glGetDebugMessageLog
#define glPushDebugGroup __rglgen_glPushDebugGroup
#define glPopDebugGroup __rglgen_glPopDebugGroup
#define glObjectLabel __rglgen_glObjectLabel
#define glGetObjectLabel __rglgen_glGetObjectLabel
#define glObjectPtrLabel __rglgen_glObjectPtrLabel
#define glGetObjectPtrLabel __rglgen_glGetObjectPtrLabel
#define glClearBufferData __rglgen_glClearBufferData
#define glClearBufferSubData __rglgen_glClearBufferSubData
#define glDispatchCompute __rglgen_glDispatchCompute
#define glDispatchComputeIndirect __rglgen_glDispatchComputeIndirect
#define glCopyImageSubData __rglgen_glCopyImageSubData
#define glTextureView __rglgen_glTextureView
#define glBindVertexBuffer __rglgen_glBindVertexBuffer
#define glVertexAttribFormat __rglgen_glVertexAttribFormat
#define glVertexAttribIFormat __rglgen_glVertexAttribIFormat
#define glVertexAttribLFormat __rglgen_glVertexAttribLFormat
#define glVertexAttribBinding __rglgen_glVertexAttribBinding
#define glVertexBindingDivisor __rglgen_glVertexBindingDivisor
#define glFramebufferParameteri __rglgen_glFramebufferParameteri
#define glGetFramebufferParameteriv __rglgen_glGetFramebufferParameteriv
#define glGetInternalformati64v __rglgen_glGetInternalformati64v
#define glInvalidateTexSubImage __rglgen_glInvalidateTexSubImage
#define glInvalidateTexImage __rglgen_glInvalidateTexImage
#define glInvalidateBufferSubData __rglgen_glInvalidateBufferSubData
#define glInvalidateBufferData __rglgen_glInvalidateBufferData
#define glInvalidateFramebuffer __rglgen_glInvalidateFramebuffer
#define glInvalidateSubFramebuffer __rglgen_glInvalidateSubFramebuffer
#define glMultiDrawArraysIndirect __rglgen_glMultiDrawArraysIndirect
#define glMultiDrawElementsIndirect __rglgen_glMultiDrawElementsIndirect
#define glGetProgramInterfaceiv __rglgen_glGetProgramInterfaceiv
#define glGetProgramResourceIndex __rglgen_glGetProgramResourceIndex
#define glGetProgramResourceName __rglgen_glGetProgramResourceName
#define glGetProgramResourceiv __rglgen_glGetProgramResourceiv
#define glGetProgramResourceLocation __rglgen_glGetProgramResourceLocation
#define glGetProgramResourceLocationIndex __rglgen_glGetProgramResourceLocationIndex
#define glShaderStorageBlockBinding __rglgen_glShaderStorageBlockBinding
#define glTexBufferRange __rglgen_glTexBufferRange
#define glTexStorage2DMultisample __rglgen_glTexStorage2DMultisample
#define glTexStorage3DMultisample __rglgen_glTexStorage3DMultisample
#define glImageTransformParameteriHP __rglgen_glImageTransformParameteriHP
#define glImageTransformParameterfHP __rglgen_glImageTransformParameterfHP
#define glImageTransformParameterivHP __rglgen_glImageTransformParameterivHP
#define glImageTransformParameterfvHP __rglgen_glImageTransformParameterfvHP
#define glGetImageTransformParameterivHP __rglgen_glGetImageTransformParameterivHP
#define glGetImageTransformParameterfvHP __rglgen_glGetImageTransformParameterfvHP
#define glMultiTexCoord1bOES __rglgen_glMultiTexCoord1bOES
#define glMultiTexCoord1bvOES __rglgen_glMultiTexCoord1bvOES
#define glMultiTexCoord2bOES __rglgen_glMultiTexCoord2bOES
#define glMultiTexCoord2bvOES __rglgen_glMultiTexCoord2bvOES
#define glMultiTexCoord3bOES __rglgen_glMultiTexCoord3bOES
#define glMultiTexCoord3bvOES __rglgen_glMultiTexCoord3bvOES
#define glMultiTexCoord4bOES __rglgen_glMultiTexCoord4bOES
#define glMultiTexCoord4bvOES __rglgen_glMultiTexCoord4bvOES
#define glTexCoord1bOES __rglgen_glTexCoord1bOES
#define glTexCoord1bvOES __rglgen_glTexCoord1bvOES
#define glTexCoord2bOES __rglgen_glTexCoord2bOES
#define glTexCoord2bvOES __rglgen_glTexCoord2bvOES
#define glTexCoord3bOES __rglgen_glTexCoord3bOES
#define glTexCoord3bvOES __rglgen_glTexCoord3bvOES
#define glTexCoord4bOES __rglgen_glTexCoord4bOES
#define glTexCoord4bvOES __rglgen_glTexCoord4bvOES
#define glVertex2bOES __rglgen_glVertex2bOES
#define glVertex2bvOES __rglgen_glVertex2bvOES
#define glVertex3bOES __rglgen_glVertex3bOES
#define glVertex3bvOES __rglgen_glVertex3bvOES
#define glVertex4bOES __rglgen_glVertex4bOES
#define glVertex4bvOES __rglgen_glVertex4bvOES
#define glAccumxOES __rglgen_glAccumxOES
#define glAlphaFuncxOES __rglgen_glAlphaFuncxOES
#define glBitmapxOES __rglgen_glBitmapxOES
#define glBlendColorxOES __rglgen_glBlendColorxOES
#define glClearAccumxOES __rglgen_glClearAccumxOES
#define glClearColorxOES __rglgen_glClearColorxOES
#define glClearDepthxOES __rglgen_glClearDepthxOES
#define glClipPlanexOES __rglgen_glClipPlanexOES
#define glColor3xOES __rglgen_glColor3xOES
#define glColor4xOES __rglgen_glColor4xOES
#define glColor3xvOES __rglgen_glColor3xvOES
#define glColor4xvOES __rglgen_glColor4xvOES
#define glConvolutionParameterxOES __rglgen_glConvolutionParameterxOES
#define glConvolutionParameterxvOES __rglgen_glConvolutionParameterxvOES
#define glDepthRangexOES __rglgen_glDepthRangexOES
#define glEvalCoord1xOES __rglgen_glEvalCoord1xOES
#define glEvalCoord2xOES __rglgen_glEvalCoord2xOES
#define glEvalCoord1xvOES __rglgen_glEvalCoord1xvOES
#define glEvalCoord2xvOES __rglgen_glEvalCoord2xvOES
#define glFeedbackBufferxOES __rglgen_glFeedbackBufferxOES
#define glFogxOES __rglgen_glFogxOES
#define glFogxvOES __rglgen_glFogxvOES
#define glFrustumxOES __rglgen_glFrustumxOES
#define glGetClipPlanexOES __rglgen_glGetClipPlanexOES
#define glGetConvolutionParameterxvOES __rglgen_glGetConvolutionParameterxvOES
#define glGetFixedvOES __rglgen_glGetFixedvOES
#define glGetHistogramParameterxvOES __rglgen_glGetHistogramParameterxvOES
#define glGetLightxOES __rglgen_glGetLightxOES
#define glGetMapxvOES __rglgen_glGetMapxvOES
#define glGetMaterialxOES __rglgen_glGetMaterialxOES
#define glGetPixelMapxv __rglgen_glGetPixelMapxv
#define glGetTexEnvxvOES __rglgen_glGetTexEnvxvOES
#define glGetTexGenxvOES __rglgen_glGetTexGenxvOES
#define glGetTexLevelParameterxvOES __rglgen_glGetTexLevelParameterxvOES
#define glGetTexParameterxvOES __rglgen_glGetTexParameterxvOES
#define glIndexxOES __rglgen_glIndexxOES
#define glIndexxvOES __rglgen_glIndexxvOES
#define glLightModelxOES __rglgen_glLightModelxOES
#define glLightModelxvOES __rglgen_glLightModelxvOES
#define glLightxOES __rglgen_glLightxOES
#define glLightxvOES __rglgen_glLightxvOES
#define glLineWidthxOES __rglgen_glLineWidthxOES
#define glLoadMatrixxOES __rglgen_glLoadMatrixxOES
#define glLoadTransposeMatrixxOES __rglgen_glLoadTransposeMatrixxOES
#define glMap1xOES __rglgen_glMap1xOES
#define glMap2xOES __rglgen_glMap2xOES
#define glMapGrid1xOES __rglgen_glMapGrid1xOES
#define glMapGrid2xOES __rglgen_glMapGrid2xOES
#define glMaterialxOES __rglgen_glMaterialxOES
#define glMaterialxvOES __rglgen_glMaterialxvOES
#define glMultMatrixxOES __rglgen_glMultMatrixxOES
#define glMultTransposeMatrixxOES __rglgen_glMultTransposeMatrixxOES
#define glMultiTexCoord1xOES __rglgen_glMultiTexCoord1xOES
#define glMultiTexCoord2xOES __rglgen_glMultiTexCoord2xOES
#define glMultiTexCoord3xOES __rglgen_glMultiTexCoord3xOES
#define glMultiTexCoord4xOES __rglgen_glMultiTexCoord4xOES
#define glMultiTexCoord1xvOES __rglgen_glMultiTexCoord1xvOES
#define glMultiTexCoord2xvOES __rglgen_glMultiTexCoord2xvOES
#define glMultiTexCoord3xvOES __rglgen_glMultiTexCoord3xvOES
#define glMultiTexCoord4xvOES __rglgen_glMultiTexCoord4xvOES
#define glNormal3xOES __rglgen_glNormal3xOES
#define glNormal3xvOES __rglgen_glNormal3xvOES
#define glOrthoxOES __rglgen_glOrthoxOES
#define glPassThroughxOES __rglgen_glPassThroughxOES
#define glPixelMapx __rglgen_glPixelMapx
#define glPixelStorex __rglgen_glPixelStorex
#define glPixelTransferxOES __rglgen_glPixelTransferxOES
#define glPixelZoomxOES __rglgen_glPixelZoomxOES
#define glPointParameterxvOES __rglgen_glPointParameterxvOES
#define glPointSizexOES __rglgen_glPointSizexOES
#define glPolygonOffsetxOES __rglgen_glPolygonOffsetxOES
#define glPrioritizeTexturesxOES __rglgen_glPrioritizeTexturesxOES
#define glRasterPos2xOES __rglgen_glRasterPos2xOES
#define glRasterPos3xOES __rglgen_glRasterPos3xOES
#define glRasterPos4xOES __rglgen_glRasterPos4xOES
#define glRasterPos2xvOES __rglgen_glRasterPos2xvOES
#define glRasterPos3xvOES __rglgen_glRasterPos3xvOES
#define glRasterPos4xvOES __rglgen_glRasterPos4xvOES
#define glRectxOES __rglgen_glRectxOES
#define glRectxvOES __rglgen_glRectxvOES
#define glRotatexOES __rglgen_glRotatexOES
#define glSampleCoverageOES __rglgen_glSampleCoverageOES
#define glScalexOES __rglgen_glScalexOES
#define glTexCoord1xOES __rglgen_glTexCoord1xOES
#define glTexCoord2xOES __rglgen_glTexCoord2xOES
#define glTexCoord3xOES __rglgen_glTexCoord3xOES
#define glTexCoord4xOES __rglgen_glTexCoord4xOES
#define glTexCoord1xvOES __rglgen_glTexCoord1xvOES
#define glTexCoord2xvOES __rglgen_glTexCoord2xvOES
#define glTexCoord3xvOES __rglgen_glTexCoord3xvOES
#define glTexCoord4xvOES __rglgen_glTexCoord4xvOES
#define glTexEnvxOES __rglgen_glTexEnvxOES
#define glTexEnvxvOES __rglgen_glTexEnvxvOES
#define glTexGenxOES __rglgen_glTexGenxOES
#define glTexGenxvOES __rglgen_glTexGenxvOES
#define glTexParameterxOES __rglgen_glTexParameterxOES
#define glTexParameterxvOES __rglgen_glTexParameterxvOES
#define glTranslatexOES __rglgen_glTranslatexOES
#define glVertex2xOES __rglgen_glVertex2xOES
#define glVertex3xOES __rglgen_glVertex3xOES
#define glVertex4xOES __rglgen_glVertex4xOES
#define glVertex2xvOES __rglgen_glVertex2xvOES
#define glVertex3xvOES __rglgen_glVertex3xvOES
#define glVertex4xvOES __rglgen_glVertex4xvOES
#define glDepthRangefOES __rglgen_glDepthRangefOES
#define glFrustumfOES __rglgen_glFrustumfOES
#define glOrthofOES __rglgen_glOrthofOES
#define glClipPlanefOES __rglgen_glClipPlanefOES
#define glClearDepthfOES __rglgen_glClearDepthfOES
#define glGetClipPlanefOES __rglgen_glGetClipPlanefOES
#define glQueryMatrixxOES __rglgen_glQueryMatrixxOES

extern RGLSYMGLBLENDCOLORPROC __rglgen_glBlendColor;
extern RGLSYMGLBLENDEQUATIONPROC __rglgen_glBlendEquation;
extern RGLSYMGLDRAWRANGEELEMENTSPROC __rglgen_glDrawRangeElements;
extern RGLSYMGLTEXIMAGE3DPROC __rglgen_glTexImage3D;
extern RGLSYMGLTEXSUBIMAGE3DPROC __rglgen_glTexSubImage3D;
extern RGLSYMGLCOPYTEXSUBIMAGE3DPROC __rglgen_glCopyTexSubImage3D;
extern RGLSYMGLCOLORTABLEPROC __rglgen_glColorTable;
extern RGLSYMGLCOLORTABLEPARAMETERFVPROC __rglgen_glColorTableParameterfv;
extern RGLSYMGLCOLORTABLEPARAMETERIVPROC __rglgen_glColorTableParameteriv;
extern RGLSYMGLCOPYCOLORTABLEPROC __rglgen_glCopyColorTable;
extern RGLSYMGLGETCOLORTABLEPROC __rglgen_glGetColorTable;
extern RGLSYMGLGETCOLORTABLEPARAMETERFVPROC __rglgen_glGetColorTableParameterfv;
extern RGLSYMGLGETCOLORTABLEPARAMETERIVPROC __rglgen_glGetColorTableParameteriv;
extern RGLSYMGLCOLORSUBTABLEPROC __rglgen_glColorSubTable;
extern RGLSYMGLCOPYCOLORSUBTABLEPROC __rglgen_glCopyColorSubTable;
extern RGLSYMGLCONVOLUTIONFILTER1DPROC __rglgen_glConvolutionFilter1D;
extern RGLSYMGLCONVOLUTIONFILTER2DPROC __rglgen_glConvolutionFilter2D;
extern RGLSYMGLCONVOLUTIONPARAMETERFPROC __rglgen_glConvolutionParameterf;
extern RGLSYMGLCONVOLUTIONPARAMETERFVPROC __rglgen_glConvolutionParameterfv;
extern RGLSYMGLCONVOLUTIONPARAMETERIPROC __rglgen_glConvolutionParameteri;
extern RGLSYMGLCONVOLUTIONPARAMETERIVPROC __rglgen_glConvolutionParameteriv;
extern RGLSYMGLCOPYCONVOLUTIONFILTER1DPROC __rglgen_glCopyConvolutionFilter1D;
extern RGLSYMGLCOPYCONVOLUTIONFILTER2DPROC __rglgen_glCopyConvolutionFilter2D;
extern RGLSYMGLGETCONVOLUTIONFILTERPROC __rglgen_glGetConvolutionFilter;
extern RGLSYMGLGETCONVOLUTIONPARAMETERFVPROC __rglgen_glGetConvolutionParameterfv;
extern RGLSYMGLGETCONVOLUTIONPARAMETERIVPROC __rglgen_glGetConvolutionParameteriv;
extern RGLSYMGLGETSEPARABLEFILTERPROC __rglgen_glGetSeparableFilter;
extern RGLSYMGLSEPARABLEFILTER2DPROC __rglgen_glSeparableFilter2D;
extern RGLSYMGLGETHISTOGRAMPROC __rglgen_glGetHistogram;
extern RGLSYMGLGETHISTOGRAMPARAMETERFVPROC __rglgen_glGetHistogramParameterfv;
extern RGLSYMGLGETHISTOGRAMPARAMETERIVPROC __rglgen_glGetHistogramParameteriv;
extern RGLSYMGLGETMINMAXPROC __rglgen_glGetMinmax;
extern RGLSYMGLGETMINMAXPARAMETERFVPROC __rglgen_glGetMinmaxParameterfv;
extern RGLSYMGLGETMINMAXPARAMETERIVPROC __rglgen_glGetMinmaxParameteriv;
extern RGLSYMGLHISTOGRAMPROC __rglgen_glHistogram;
extern RGLSYMGLMINMAXPROC __rglgen_glMinmax;
extern RGLSYMGLRESETHISTOGRAMPROC __rglgen_glResetHistogram;
extern RGLSYMGLRESETMINMAXPROC __rglgen_glResetMinmax;
extern RGLSYMGLACTIVETEXTUREPROC __rglgen_glActiveTexture;
extern RGLSYMGLSAMPLECOVERAGEPROC __rglgen_glSampleCoverage;
extern RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC __rglgen_glCompressedTexImage3D;
extern RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC __rglgen_glCompressedTexImage2D;
extern RGLSYMGLCOMPRESSEDTEXIMAGE1DPROC __rglgen_glCompressedTexImage1D;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC __rglgen_glCompressedTexSubImage3D;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC __rglgen_glCompressedTexSubImage2D;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DPROC __rglgen_glCompressedTexSubImage1D;
extern RGLSYMGLGETCOMPRESSEDTEXIMAGEPROC __rglgen_glGetCompressedTexImage;
extern RGLSYMGLCLIENTACTIVETEXTUREPROC __rglgen_glClientActiveTexture;
extern RGLSYMGLMULTITEXCOORD1DPROC __rglgen_glMultiTexCoord1d;
extern RGLSYMGLMULTITEXCOORD1DVPROC __rglgen_glMultiTexCoord1dv;
extern RGLSYMGLMULTITEXCOORD1FPROC __rglgen_glMultiTexCoord1f;
extern RGLSYMGLMULTITEXCOORD1FVPROC __rglgen_glMultiTexCoord1fv;
extern RGLSYMGLMULTITEXCOORD1IPROC __rglgen_glMultiTexCoord1i;
extern RGLSYMGLMULTITEXCOORD1IVPROC __rglgen_glMultiTexCoord1iv;
extern RGLSYMGLMULTITEXCOORD1SPROC __rglgen_glMultiTexCoord1s;
extern RGLSYMGLMULTITEXCOORD1SVPROC __rglgen_glMultiTexCoord1sv;
extern RGLSYMGLMULTITEXCOORD2DPROC __rglgen_glMultiTexCoord2d;
extern RGLSYMGLMULTITEXCOORD2DVPROC __rglgen_glMultiTexCoord2dv;
extern RGLSYMGLMULTITEXCOORD2FPROC __rglgen_glMultiTexCoord2f;
extern RGLSYMGLMULTITEXCOORD2FVPROC __rglgen_glMultiTexCoord2fv;
extern RGLSYMGLMULTITEXCOORD2IPROC __rglgen_glMultiTexCoord2i;
extern RGLSYMGLMULTITEXCOORD2IVPROC __rglgen_glMultiTexCoord2iv;
extern RGLSYMGLMULTITEXCOORD2SPROC __rglgen_glMultiTexCoord2s;
extern RGLSYMGLMULTITEXCOORD2SVPROC __rglgen_glMultiTexCoord2sv;
extern RGLSYMGLMULTITEXCOORD3DPROC __rglgen_glMultiTexCoord3d;
extern RGLSYMGLMULTITEXCOORD3DVPROC __rglgen_glMultiTexCoord3dv;
extern RGLSYMGLMULTITEXCOORD3FPROC __rglgen_glMultiTexCoord3f;
extern RGLSYMGLMULTITEXCOORD3FVPROC __rglgen_glMultiTexCoord3fv;
extern RGLSYMGLMULTITEXCOORD3IPROC __rglgen_glMultiTexCoord3i;
extern RGLSYMGLMULTITEXCOORD3IVPROC __rglgen_glMultiTexCoord3iv;
extern RGLSYMGLMULTITEXCOORD3SPROC __rglgen_glMultiTexCoord3s;
extern RGLSYMGLMULTITEXCOORD3SVPROC __rglgen_glMultiTexCoord3sv;
extern RGLSYMGLMULTITEXCOORD4DPROC __rglgen_glMultiTexCoord4d;
extern RGLSYMGLMULTITEXCOORD4DVPROC __rglgen_glMultiTexCoord4dv;
extern RGLSYMGLMULTITEXCOORD4FPROC __rglgen_glMultiTexCoord4f;
extern RGLSYMGLMULTITEXCOORD4FVPROC __rglgen_glMultiTexCoord4fv;
extern RGLSYMGLMULTITEXCOORD4IPROC __rglgen_glMultiTexCoord4i;
extern RGLSYMGLMULTITEXCOORD4IVPROC __rglgen_glMultiTexCoord4iv;
extern RGLSYMGLMULTITEXCOORD4SPROC __rglgen_glMultiTexCoord4s;
extern RGLSYMGLMULTITEXCOORD4SVPROC __rglgen_glMultiTexCoord4sv;
extern RGLSYMGLLOADTRANSPOSEMATRIXFPROC __rglgen_glLoadTransposeMatrixf;
extern RGLSYMGLLOADTRANSPOSEMATRIXDPROC __rglgen_glLoadTransposeMatrixd;
extern RGLSYMGLMULTTRANSPOSEMATRIXFPROC __rglgen_glMultTransposeMatrixf;
extern RGLSYMGLMULTTRANSPOSEMATRIXDPROC __rglgen_glMultTransposeMatrixd;
extern RGLSYMGLBLENDFUNCSEPARATEPROC __rglgen_glBlendFuncSeparate;
extern RGLSYMGLMULTIDRAWARRAYSPROC __rglgen_glMultiDrawArrays;
extern RGLSYMGLMULTIDRAWELEMENTSPROC __rglgen_glMultiDrawElements;
extern RGLSYMGLPOINTPARAMETERFPROC __rglgen_glPointParameterf;
extern RGLSYMGLPOINTPARAMETERFVPROC __rglgen_glPointParameterfv;
extern RGLSYMGLPOINTPARAMETERIPROC __rglgen_glPointParameteri;
extern RGLSYMGLPOINTPARAMETERIVPROC __rglgen_glPointParameteriv;
extern RGLSYMGLFOGCOORDFPROC __rglgen_glFogCoordf;
extern RGLSYMGLFOGCOORDFVPROC __rglgen_glFogCoordfv;
extern RGLSYMGLFOGCOORDDPROC __rglgen_glFogCoordd;
extern RGLSYMGLFOGCOORDDVPROC __rglgen_glFogCoorddv;
extern RGLSYMGLFOGCOORDPOINTERPROC __rglgen_glFogCoordPointer;
extern RGLSYMGLSECONDARYCOLOR3BPROC __rglgen_glSecondaryColor3b;
extern RGLSYMGLSECONDARYCOLOR3BVPROC __rglgen_glSecondaryColor3bv;
extern RGLSYMGLSECONDARYCOLOR3DPROC __rglgen_glSecondaryColor3d;
extern RGLSYMGLSECONDARYCOLOR3DVPROC __rglgen_glSecondaryColor3dv;
extern RGLSYMGLSECONDARYCOLOR3FPROC __rglgen_glSecondaryColor3f;
extern RGLSYMGLSECONDARYCOLOR3FVPROC __rglgen_glSecondaryColor3fv;
extern RGLSYMGLSECONDARYCOLOR3IPROC __rglgen_glSecondaryColor3i;
extern RGLSYMGLSECONDARYCOLOR3IVPROC __rglgen_glSecondaryColor3iv;
extern RGLSYMGLSECONDARYCOLOR3SPROC __rglgen_glSecondaryColor3s;
extern RGLSYMGLSECONDARYCOLOR3SVPROC __rglgen_glSecondaryColor3sv;
extern RGLSYMGLSECONDARYCOLOR3UBPROC __rglgen_glSecondaryColor3ub;
extern RGLSYMGLSECONDARYCOLOR3UBVPROC __rglgen_glSecondaryColor3ubv;
extern RGLSYMGLSECONDARYCOLOR3UIPROC __rglgen_glSecondaryColor3ui;
extern RGLSYMGLSECONDARYCOLOR3UIVPROC __rglgen_glSecondaryColor3uiv;
extern RGLSYMGLSECONDARYCOLOR3USPROC __rglgen_glSecondaryColor3us;
extern RGLSYMGLSECONDARYCOLOR3USVPROC __rglgen_glSecondaryColor3usv;
extern RGLSYMGLSECONDARYCOLORPOINTERPROC __rglgen_glSecondaryColorPointer;
extern RGLSYMGLWINDOWPOS2DPROC __rglgen_glWindowPos2d;
extern RGLSYMGLWINDOWPOS2DVPROC __rglgen_glWindowPos2dv;
extern RGLSYMGLWINDOWPOS2FPROC __rglgen_glWindowPos2f;
extern RGLSYMGLWINDOWPOS2FVPROC __rglgen_glWindowPos2fv;
extern RGLSYMGLWINDOWPOS2IPROC __rglgen_glWindowPos2i;
extern RGLSYMGLWINDOWPOS2IVPROC __rglgen_glWindowPos2iv;
extern RGLSYMGLWINDOWPOS2SPROC __rglgen_glWindowPos2s;
extern RGLSYMGLWINDOWPOS2SVPROC __rglgen_glWindowPos2sv;
extern RGLSYMGLWINDOWPOS3DPROC __rglgen_glWindowPos3d;
extern RGLSYMGLWINDOWPOS3DVPROC __rglgen_glWindowPos3dv;
extern RGLSYMGLWINDOWPOS3FPROC __rglgen_glWindowPos3f;
extern RGLSYMGLWINDOWPOS3FVPROC __rglgen_glWindowPos3fv;
extern RGLSYMGLWINDOWPOS3IPROC __rglgen_glWindowPos3i;
extern RGLSYMGLWINDOWPOS3IVPROC __rglgen_glWindowPos3iv;
extern RGLSYMGLWINDOWPOS3SPROC __rglgen_glWindowPos3s;
extern RGLSYMGLWINDOWPOS3SVPROC __rglgen_glWindowPos3sv;
extern RGLSYMGLGENQUERIESPROC __rglgen_glGenQueries;
extern RGLSYMGLDELETEQUERIESPROC __rglgen_glDeleteQueries;
extern RGLSYMGLISQUERYPROC __rglgen_glIsQuery;
extern RGLSYMGLBEGINQUERYPROC __rglgen_glBeginQuery;
extern RGLSYMGLENDQUERYPROC __rglgen_glEndQuery;
extern RGLSYMGLGETQUERYIVPROC __rglgen_glGetQueryiv;
extern RGLSYMGLGETQUERYOBJECTIVPROC __rglgen_glGetQueryObjectiv;
extern RGLSYMGLGETQUERYOBJECTUIVPROC __rglgen_glGetQueryObjectuiv;
extern RGLSYMGLBINDBUFFERPROC __rglgen_glBindBuffer;
extern RGLSYMGLDELETEBUFFERSPROC __rglgen_glDeleteBuffers;
extern RGLSYMGLGENBUFFERSPROC __rglgen_glGenBuffers;
extern RGLSYMGLISBUFFERPROC __rglgen_glIsBuffer;
extern RGLSYMGLBUFFERDATAPROC __rglgen_glBufferData;
extern RGLSYMGLBUFFERSUBDATAPROC __rglgen_glBufferSubData;
extern RGLSYMGLGETBUFFERSUBDATAPROC __rglgen_glGetBufferSubData;
extern RGLSYMGLMAPBUFFERPROC __rglgen_glMapBuffer;
extern RGLSYMGLUNMAPBUFFERPROC __rglgen_glUnmapBuffer;
extern RGLSYMGLGETBUFFERPARAMETERIVPROC __rglgen_glGetBufferParameteriv;
extern RGLSYMGLGETBUFFERPOINTERVPROC __rglgen_glGetBufferPointerv;
extern RGLSYMGLBLENDEQUATIONSEPARATEPROC __rglgen_glBlendEquationSeparate;
extern RGLSYMGLDRAWBUFFERSPROC __rglgen_glDrawBuffers;
extern RGLSYMGLSTENCILOPSEPARATEPROC __rglgen_glStencilOpSeparate;
extern RGLSYMGLSTENCILFUNCSEPARATEPROC __rglgen_glStencilFuncSeparate;
extern RGLSYMGLSTENCILMASKSEPARATEPROC __rglgen_glStencilMaskSeparate;
extern RGLSYMGLATTACHSHADERPROC __rglgen_glAttachShader;
extern RGLSYMGLBINDATTRIBLOCATIONPROC __rglgen_glBindAttribLocation;
extern RGLSYMGLCOMPILESHADERPROC __rglgen_glCompileShader;
extern RGLSYMGLCREATEPROGRAMPROC __rglgen_glCreateProgram;
extern RGLSYMGLCREATESHADERPROC __rglgen_glCreateShader;
extern RGLSYMGLDELETEPROGRAMPROC __rglgen_glDeleteProgram;
extern RGLSYMGLDELETESHADERPROC __rglgen_glDeleteShader;
extern RGLSYMGLDETACHSHADERPROC __rglgen_glDetachShader;
extern RGLSYMGLDISABLEVERTEXATTRIBARRAYPROC __rglgen_glDisableVertexAttribArray;
extern RGLSYMGLENABLEVERTEXATTRIBARRAYPROC __rglgen_glEnableVertexAttribArray;
extern RGLSYMGLGETACTIVEATTRIBPROC __rglgen_glGetActiveAttrib;
extern RGLSYMGLGETACTIVEUNIFORMPROC __rglgen_glGetActiveUniform;
extern RGLSYMGLGETATTACHEDSHADERSPROC __rglgen_glGetAttachedShaders;
extern RGLSYMGLGETATTRIBLOCATIONPROC __rglgen_glGetAttribLocation;
extern RGLSYMGLGETPROGRAMIVPROC __rglgen_glGetProgramiv;
extern RGLSYMGLGETPROGRAMINFOLOGPROC __rglgen_glGetProgramInfoLog;
extern RGLSYMGLGETSHADERIVPROC __rglgen_glGetShaderiv;
extern RGLSYMGLGETSHADERINFOLOGPROC __rglgen_glGetShaderInfoLog;
extern RGLSYMGLGETSHADERSOURCEPROC __rglgen_glGetShaderSource;
extern RGLSYMGLGETUNIFORMLOCATIONPROC __rglgen_glGetUniformLocation;
extern RGLSYMGLGETUNIFORMFVPROC __rglgen_glGetUniformfv;
extern RGLSYMGLGETUNIFORMIVPROC __rglgen_glGetUniformiv;
extern RGLSYMGLGETVERTEXATTRIBDVPROC __rglgen_glGetVertexAttribdv;
extern RGLSYMGLGETVERTEXATTRIBFVPROC __rglgen_glGetVertexAttribfv;
extern RGLSYMGLGETVERTEXATTRIBIVPROC __rglgen_glGetVertexAttribiv;
extern RGLSYMGLGETVERTEXATTRIBPOINTERVPROC __rglgen_glGetVertexAttribPointerv;
extern RGLSYMGLISPROGRAMPROC __rglgen_glIsProgram;
extern RGLSYMGLISSHADERPROC __rglgen_glIsShader;
extern RGLSYMGLLINKPROGRAMPROC __rglgen_glLinkProgram;
extern RGLSYMGLSHADERSOURCEPROC __rglgen_glShaderSource;
extern RGLSYMGLUSEPROGRAMPROC __rglgen_glUseProgram;
extern RGLSYMGLUNIFORM1FPROC __rglgen_glUniform1f;
extern RGLSYMGLUNIFORM2FPROC __rglgen_glUniform2f;
extern RGLSYMGLUNIFORM3FPROC __rglgen_glUniform3f;
extern RGLSYMGLUNIFORM4FPROC __rglgen_glUniform4f;
extern RGLSYMGLUNIFORM1IPROC __rglgen_glUniform1i;
extern RGLSYMGLUNIFORM2IPROC __rglgen_glUniform2i;
extern RGLSYMGLUNIFORM3IPROC __rglgen_glUniform3i;
extern RGLSYMGLUNIFORM4IPROC __rglgen_glUniform4i;
extern RGLSYMGLUNIFORM1FVPROC __rglgen_glUniform1fv;
extern RGLSYMGLUNIFORM2FVPROC __rglgen_glUniform2fv;
extern RGLSYMGLUNIFORM3FVPROC __rglgen_glUniform3fv;
extern RGLSYMGLUNIFORM4FVPROC __rglgen_glUniform4fv;
extern RGLSYMGLUNIFORM1IVPROC __rglgen_glUniform1iv;
extern RGLSYMGLUNIFORM2IVPROC __rglgen_glUniform2iv;
extern RGLSYMGLUNIFORM3IVPROC __rglgen_glUniform3iv;
extern RGLSYMGLUNIFORM4IVPROC __rglgen_glUniform4iv;
extern RGLSYMGLUNIFORMMATRIX2FVPROC __rglgen_glUniformMatrix2fv;
extern RGLSYMGLUNIFORMMATRIX3FVPROC __rglgen_glUniformMatrix3fv;
extern RGLSYMGLUNIFORMMATRIX4FVPROC __rglgen_glUniformMatrix4fv;
extern RGLSYMGLVALIDATEPROGRAMPROC __rglgen_glValidateProgram;
extern RGLSYMGLVERTEXATTRIB1DPROC __rglgen_glVertexAttrib1d;
extern RGLSYMGLVERTEXATTRIB1DVPROC __rglgen_glVertexAttrib1dv;
extern RGLSYMGLVERTEXATTRIB1FPROC __rglgen_glVertexAttrib1f;
extern RGLSYMGLVERTEXATTRIB1FVPROC __rglgen_glVertexAttrib1fv;
extern RGLSYMGLVERTEXATTRIB1SPROC __rglgen_glVertexAttrib1s;
extern RGLSYMGLVERTEXATTRIB1SVPROC __rglgen_glVertexAttrib1sv;
extern RGLSYMGLVERTEXATTRIB2DPROC __rglgen_glVertexAttrib2d;
extern RGLSYMGLVERTEXATTRIB2DVPROC __rglgen_glVertexAttrib2dv;
extern RGLSYMGLVERTEXATTRIB2FPROC __rglgen_glVertexAttrib2f;
extern RGLSYMGLVERTEXATTRIB2FVPROC __rglgen_glVertexAttrib2fv;
extern RGLSYMGLVERTEXATTRIB2SPROC __rglgen_glVertexAttrib2s;
extern RGLSYMGLVERTEXATTRIB2SVPROC __rglgen_glVertexAttrib2sv;
extern RGLSYMGLVERTEXATTRIB3DPROC __rglgen_glVertexAttrib3d;
extern RGLSYMGLVERTEXATTRIB3DVPROC __rglgen_glVertexAttrib3dv;
extern RGLSYMGLVERTEXATTRIB3FPROC __rglgen_glVertexAttrib3f;
extern RGLSYMGLVERTEXATTRIB3FVPROC __rglgen_glVertexAttrib3fv;
extern RGLSYMGLVERTEXATTRIB3SPROC __rglgen_glVertexAttrib3s;
extern RGLSYMGLVERTEXATTRIB3SVPROC __rglgen_glVertexAttrib3sv;
extern RGLSYMGLVERTEXATTRIB4NBVPROC __rglgen_glVertexAttrib4Nbv;
extern RGLSYMGLVERTEXATTRIB4NIVPROC __rglgen_glVertexAttrib4Niv;
extern RGLSYMGLVERTEXATTRIB4NSVPROC __rglgen_glVertexAttrib4Nsv;
extern RGLSYMGLVERTEXATTRIB4NUBPROC __rglgen_glVertexAttrib4Nub;
extern RGLSYMGLVERTEXATTRIB4NUBVPROC __rglgen_glVertexAttrib4Nubv;
extern RGLSYMGLVERTEXATTRIB4NUIVPROC __rglgen_glVertexAttrib4Nuiv;
extern RGLSYMGLVERTEXATTRIB4NUSVPROC __rglgen_glVertexAttrib4Nusv;
extern RGLSYMGLVERTEXATTRIB4BVPROC __rglgen_glVertexAttrib4bv;
extern RGLSYMGLVERTEXATTRIB4DPROC __rglgen_glVertexAttrib4d;
extern RGLSYMGLVERTEXATTRIB4DVPROC __rglgen_glVertexAttrib4dv;
extern RGLSYMGLVERTEXATTRIB4FPROC __rglgen_glVertexAttrib4f;
extern RGLSYMGLVERTEXATTRIB4FVPROC __rglgen_glVertexAttrib4fv;
extern RGLSYMGLVERTEXATTRIB4IVPROC __rglgen_glVertexAttrib4iv;
extern RGLSYMGLVERTEXATTRIB4SPROC __rglgen_glVertexAttrib4s;
extern RGLSYMGLVERTEXATTRIB4SVPROC __rglgen_glVertexAttrib4sv;
extern RGLSYMGLVERTEXATTRIB4UBVPROC __rglgen_glVertexAttrib4ubv;
extern RGLSYMGLVERTEXATTRIB4UIVPROC __rglgen_glVertexAttrib4uiv;
extern RGLSYMGLVERTEXATTRIB4USVPROC __rglgen_glVertexAttrib4usv;
extern RGLSYMGLVERTEXATTRIBPOINTERPROC __rglgen_glVertexAttribPointer;
extern RGLSYMGLUNIFORMMATRIX2X3FVPROC __rglgen_glUniformMatrix2x3fv;
extern RGLSYMGLUNIFORMMATRIX3X2FVPROC __rglgen_glUniformMatrix3x2fv;
extern RGLSYMGLUNIFORMMATRIX2X4FVPROC __rglgen_glUniformMatrix2x4fv;
extern RGLSYMGLUNIFORMMATRIX4X2FVPROC __rglgen_glUniformMatrix4x2fv;
extern RGLSYMGLUNIFORMMATRIX3X4FVPROC __rglgen_glUniformMatrix3x4fv;
extern RGLSYMGLUNIFORMMATRIX4X3FVPROC __rglgen_glUniformMatrix4x3fv;
extern RGLSYMGLCOLORMASKIPROC __rglgen_glColorMaski;
extern RGLSYMGLGETBOOLEANI_VPROC __rglgen_glGetBooleani_v;
extern RGLSYMGLGETINTEGERI_VPROC __rglgen_glGetIntegeri_v;
extern RGLSYMGLENABLEIPROC __rglgen_glEnablei;
extern RGLSYMGLDISABLEIPROC __rglgen_glDisablei;
extern RGLSYMGLISENABLEDIPROC __rglgen_glIsEnabledi;
extern RGLSYMGLBEGINTRANSFORMFEEDBACKPROC __rglgen_glBeginTransformFeedback;
extern RGLSYMGLENDTRANSFORMFEEDBACKPROC __rglgen_glEndTransformFeedback;
extern RGLSYMGLBINDBUFFERRANGEPROC __rglgen_glBindBufferRange;
extern RGLSYMGLBINDBUFFERBASEPROC __rglgen_glBindBufferBase;
extern RGLSYMGLTRANSFORMFEEDBACKVARYINGSPROC __rglgen_glTransformFeedbackVaryings;
extern RGLSYMGLGETTRANSFORMFEEDBACKVARYINGPROC __rglgen_glGetTransformFeedbackVarying;
extern RGLSYMGLCLAMPCOLORPROC __rglgen_glClampColor;
extern RGLSYMGLBEGINCONDITIONALRENDERPROC __rglgen_glBeginConditionalRender;
extern RGLSYMGLENDCONDITIONALRENDERPROC __rglgen_glEndConditionalRender;
extern RGLSYMGLVERTEXATTRIBIPOINTERPROC __rglgen_glVertexAttribIPointer;
extern RGLSYMGLGETVERTEXATTRIBIIVPROC __rglgen_glGetVertexAttribIiv;
extern RGLSYMGLGETVERTEXATTRIBIUIVPROC __rglgen_glGetVertexAttribIuiv;
extern RGLSYMGLVERTEXATTRIBI1IPROC __rglgen_glVertexAttribI1i;
extern RGLSYMGLVERTEXATTRIBI2IPROC __rglgen_glVertexAttribI2i;
extern RGLSYMGLVERTEXATTRIBI3IPROC __rglgen_glVertexAttribI3i;
extern RGLSYMGLVERTEXATTRIBI4IPROC __rglgen_glVertexAttribI4i;
extern RGLSYMGLVERTEXATTRIBI1UIPROC __rglgen_glVertexAttribI1ui;
extern RGLSYMGLVERTEXATTRIBI2UIPROC __rglgen_glVertexAttribI2ui;
extern RGLSYMGLVERTEXATTRIBI3UIPROC __rglgen_glVertexAttribI3ui;
extern RGLSYMGLVERTEXATTRIBI4UIPROC __rglgen_glVertexAttribI4ui;
extern RGLSYMGLVERTEXATTRIBI1IVPROC __rglgen_glVertexAttribI1iv;
extern RGLSYMGLVERTEXATTRIBI2IVPROC __rglgen_glVertexAttribI2iv;
extern RGLSYMGLVERTEXATTRIBI3IVPROC __rglgen_glVertexAttribI3iv;
extern RGLSYMGLVERTEXATTRIBI4IVPROC __rglgen_glVertexAttribI4iv;
extern RGLSYMGLVERTEXATTRIBI1UIVPROC __rglgen_glVertexAttribI1uiv;
extern RGLSYMGLVERTEXATTRIBI2UIVPROC __rglgen_glVertexAttribI2uiv;
extern RGLSYMGLVERTEXATTRIBI3UIVPROC __rglgen_glVertexAttribI3uiv;
extern RGLSYMGLVERTEXATTRIBI4UIVPROC __rglgen_glVertexAttribI4uiv;
extern RGLSYMGLVERTEXATTRIBI4BVPROC __rglgen_glVertexAttribI4bv;
extern RGLSYMGLVERTEXATTRIBI4SVPROC __rglgen_glVertexAttribI4sv;
extern RGLSYMGLVERTEXATTRIBI4UBVPROC __rglgen_glVertexAttribI4ubv;
extern RGLSYMGLVERTEXATTRIBI4USVPROC __rglgen_glVertexAttribI4usv;
extern RGLSYMGLGETUNIFORMUIVPROC __rglgen_glGetUniformuiv;
extern RGLSYMGLBINDFRAGDATALOCATIONPROC __rglgen_glBindFragDataLocation;
extern RGLSYMGLGETFRAGDATALOCATIONPROC __rglgen_glGetFragDataLocation;
extern RGLSYMGLUNIFORM1UIPROC __rglgen_glUniform1ui;
extern RGLSYMGLUNIFORM2UIPROC __rglgen_glUniform2ui;
extern RGLSYMGLUNIFORM3UIPROC __rglgen_glUniform3ui;
extern RGLSYMGLUNIFORM4UIPROC __rglgen_glUniform4ui;
extern RGLSYMGLUNIFORM1UIVPROC __rglgen_glUniform1uiv;
extern RGLSYMGLUNIFORM2UIVPROC __rglgen_glUniform2uiv;
extern RGLSYMGLUNIFORM3UIVPROC __rglgen_glUniform3uiv;
extern RGLSYMGLUNIFORM4UIVPROC __rglgen_glUniform4uiv;
extern RGLSYMGLTEXPARAMETERIIVPROC __rglgen_glTexParameterIiv;
extern RGLSYMGLTEXPARAMETERIUIVPROC __rglgen_glTexParameterIuiv;
extern RGLSYMGLGETTEXPARAMETERIIVPROC __rglgen_glGetTexParameterIiv;
extern RGLSYMGLGETTEXPARAMETERIUIVPROC __rglgen_glGetTexParameterIuiv;
extern RGLSYMGLCLEARBUFFERIVPROC __rglgen_glClearBufferiv;
extern RGLSYMGLCLEARBUFFERUIVPROC __rglgen_glClearBufferuiv;
extern RGLSYMGLCLEARBUFFERFVPROC __rglgen_glClearBufferfv;
extern RGLSYMGLCLEARBUFFERFIPROC __rglgen_glClearBufferfi;
extern RGLSYMGLGETSTRINGIPROC __rglgen_glGetStringi;
extern RGLSYMGLDRAWARRAYSINSTANCEDPROC __rglgen_glDrawArraysInstanced;
extern RGLSYMGLDRAWELEMENTSINSTANCEDPROC __rglgen_glDrawElementsInstanced;
extern RGLSYMGLTEXBUFFERPROC __rglgen_glTexBuffer;
extern RGLSYMGLPRIMITIVERESTARTINDEXPROC __rglgen_glPrimitiveRestartIndex;
extern RGLSYMGLGETINTEGER64I_VPROC __rglgen_glGetInteger64i_v;
extern RGLSYMGLGETBUFFERPARAMETERI64VPROC __rglgen_glGetBufferParameteri64v;
extern RGLSYMGLFRAMEBUFFERTEXTUREPROC __rglgen_glFramebufferTexture;
extern RGLSYMGLVERTEXATTRIBDIVISORPROC __rglgen_glVertexAttribDivisor;
extern RGLSYMGLMINSAMPLESHADINGPROC __rglgen_glMinSampleShading;
extern RGLSYMGLBLENDEQUATIONIPROC __rglgen_glBlendEquationi;
extern RGLSYMGLBLENDEQUATIONSEPARATEIPROC __rglgen_glBlendEquationSeparatei;
extern RGLSYMGLBLENDFUNCIPROC __rglgen_glBlendFunci;
extern RGLSYMGLBLENDFUNCSEPARATEIPROC __rglgen_glBlendFuncSeparatei;
extern RGLSYMGLACTIVETEXTUREARBPROC __rglgen_glActiveTextureARB;
extern RGLSYMGLCLIENTACTIVETEXTUREARBPROC __rglgen_glClientActiveTextureARB;
extern RGLSYMGLMULTITEXCOORD1DARBPROC __rglgen_glMultiTexCoord1dARB;
extern RGLSYMGLMULTITEXCOORD1DVARBPROC __rglgen_glMultiTexCoord1dvARB;
extern RGLSYMGLMULTITEXCOORD1FARBPROC __rglgen_glMultiTexCoord1fARB;
extern RGLSYMGLMULTITEXCOORD1FVARBPROC __rglgen_glMultiTexCoord1fvARB;
extern RGLSYMGLMULTITEXCOORD1IARBPROC __rglgen_glMultiTexCoord1iARB;
extern RGLSYMGLMULTITEXCOORD1IVARBPROC __rglgen_glMultiTexCoord1ivARB;
extern RGLSYMGLMULTITEXCOORD1SARBPROC __rglgen_glMultiTexCoord1sARB;
extern RGLSYMGLMULTITEXCOORD1SVARBPROC __rglgen_glMultiTexCoord1svARB;
extern RGLSYMGLMULTITEXCOORD2DARBPROC __rglgen_glMultiTexCoord2dARB;
extern RGLSYMGLMULTITEXCOORD2DVARBPROC __rglgen_glMultiTexCoord2dvARB;
extern RGLSYMGLMULTITEXCOORD2FARBPROC __rglgen_glMultiTexCoord2fARB;
extern RGLSYMGLMULTITEXCOORD2FVARBPROC __rglgen_glMultiTexCoord2fvARB;
extern RGLSYMGLMULTITEXCOORD2IARBPROC __rglgen_glMultiTexCoord2iARB;
extern RGLSYMGLMULTITEXCOORD2IVARBPROC __rglgen_glMultiTexCoord2ivARB;
extern RGLSYMGLMULTITEXCOORD2SARBPROC __rglgen_glMultiTexCoord2sARB;
extern RGLSYMGLMULTITEXCOORD2SVARBPROC __rglgen_glMultiTexCoord2svARB;
extern RGLSYMGLMULTITEXCOORD3DARBPROC __rglgen_glMultiTexCoord3dARB;
extern RGLSYMGLMULTITEXCOORD3DVARBPROC __rglgen_glMultiTexCoord3dvARB;
extern RGLSYMGLMULTITEXCOORD3FARBPROC __rglgen_glMultiTexCoord3fARB;
extern RGLSYMGLMULTITEXCOORD3FVARBPROC __rglgen_glMultiTexCoord3fvARB;
extern RGLSYMGLMULTITEXCOORD3IARBPROC __rglgen_glMultiTexCoord3iARB;
extern RGLSYMGLMULTITEXCOORD3IVARBPROC __rglgen_glMultiTexCoord3ivARB;
extern RGLSYMGLMULTITEXCOORD3SARBPROC __rglgen_glMultiTexCoord3sARB;
extern RGLSYMGLMULTITEXCOORD3SVARBPROC __rglgen_glMultiTexCoord3svARB;
extern RGLSYMGLMULTITEXCOORD4DARBPROC __rglgen_glMultiTexCoord4dARB;
extern RGLSYMGLMULTITEXCOORD4DVARBPROC __rglgen_glMultiTexCoord4dvARB;
extern RGLSYMGLMULTITEXCOORD4FARBPROC __rglgen_glMultiTexCoord4fARB;
extern RGLSYMGLMULTITEXCOORD4FVARBPROC __rglgen_glMultiTexCoord4fvARB;
extern RGLSYMGLMULTITEXCOORD4IARBPROC __rglgen_glMultiTexCoord4iARB;
extern RGLSYMGLMULTITEXCOORD4IVARBPROC __rglgen_glMultiTexCoord4ivARB;
extern RGLSYMGLMULTITEXCOORD4SARBPROC __rglgen_glMultiTexCoord4sARB;
extern RGLSYMGLMULTITEXCOORD4SVARBPROC __rglgen_glMultiTexCoord4svARB;
extern RGLSYMGLLOADTRANSPOSEMATRIXFARBPROC __rglgen_glLoadTransposeMatrixfARB;
extern RGLSYMGLLOADTRANSPOSEMATRIXDARBPROC __rglgen_glLoadTransposeMatrixdARB;
extern RGLSYMGLMULTTRANSPOSEMATRIXFARBPROC __rglgen_glMultTransposeMatrixfARB;
extern RGLSYMGLMULTTRANSPOSEMATRIXDARBPROC __rglgen_glMultTransposeMatrixdARB;
extern RGLSYMGLSAMPLECOVERAGEARBPROC __rglgen_glSampleCoverageARB;
extern RGLSYMGLCOMPRESSEDTEXIMAGE3DARBPROC __rglgen_glCompressedTexImage3DARB;
extern RGLSYMGLCOMPRESSEDTEXIMAGE2DARBPROC __rglgen_glCompressedTexImage2DARB;
extern RGLSYMGLCOMPRESSEDTEXIMAGE1DARBPROC __rglgen_glCompressedTexImage1DARB;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DARBPROC __rglgen_glCompressedTexSubImage3DARB;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DARBPROC __rglgen_glCompressedTexSubImage2DARB;
extern RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DARBPROC __rglgen_glCompressedTexSubImage1DARB;
extern RGLSYMGLGETCOMPRESSEDTEXIMAGEARBPROC __rglgen_glGetCompressedTexImageARB;
extern RGLSYMGLPOINTPARAMETERFARBPROC __rglgen_glPointParameterfARB;
extern RGLSYMGLPOINTPARAMETERFVARBPROC __rglgen_glPointParameterfvARB;
extern RGLSYMGLWEIGHTBVARBPROC __rglgen_glWeightbvARB;
extern RGLSYMGLWEIGHTSVARBPROC __rglgen_glWeightsvARB;
extern RGLSYMGLWEIGHTIVARBPROC __rglgen_glWeightivARB;
extern RGLSYMGLWEIGHTFVARBPROC __rglgen_glWeightfvARB;
extern RGLSYMGLWEIGHTDVARBPROC __rglgen_glWeightdvARB;
extern RGLSYMGLWEIGHTUBVARBPROC __rglgen_glWeightubvARB;
extern RGLSYMGLWEIGHTUSVARBPROC __rglgen_glWeightusvARB;
extern RGLSYMGLWEIGHTUIVARBPROC __rglgen_glWeightuivARB;
extern RGLSYMGLWEIGHTPOINTERARBPROC __rglgen_glWeightPointerARB;
extern RGLSYMGLVERTEXBLENDARBPROC __rglgen_glVertexBlendARB;
extern RGLSYMGLCURRENTPALETTEMATRIXARBPROC __rglgen_glCurrentPaletteMatrixARB;
extern RGLSYMGLMATRIXINDEXUBVARBPROC __rglgen_glMatrixIndexubvARB;
extern RGLSYMGLMATRIXINDEXUSVARBPROC __rglgen_glMatrixIndexusvARB;
extern RGLSYMGLMATRIXINDEXUIVARBPROC __rglgen_glMatrixIndexuivARB;
extern RGLSYMGLMATRIXINDEXPOINTERARBPROC __rglgen_glMatrixIndexPointerARB;
extern RGLSYMGLWINDOWPOS2DARBPROC __rglgen_glWindowPos2dARB;
extern RGLSYMGLWINDOWPOS2DVARBPROC __rglgen_glWindowPos2dvARB;
extern RGLSYMGLWINDOWPOS2FARBPROC __rglgen_glWindowPos2fARB;
extern RGLSYMGLWINDOWPOS2FVARBPROC __rglgen_glWindowPos2fvARB;
extern RGLSYMGLWINDOWPOS2IARBPROC __rglgen_glWindowPos2iARB;
extern RGLSYMGLWINDOWPOS2IVARBPROC __rglgen_glWindowPos2ivARB;
extern RGLSYMGLWINDOWPOS2SARBPROC __rglgen_glWindowPos2sARB;
extern RGLSYMGLWINDOWPOS2SVARBPROC __rglgen_glWindowPos2svARB;
extern RGLSYMGLWINDOWPOS3DARBPROC __rglgen_glWindowPos3dARB;
extern RGLSYMGLWINDOWPOS3DVARBPROC __rglgen_glWindowPos3dvARB;
extern RGLSYMGLWINDOWPOS3FARBPROC __rglgen_glWindowPos3fARB;
extern RGLSYMGLWINDOWPOS3FVARBPROC __rglgen_glWindowPos3fvARB;
extern RGLSYMGLWINDOWPOS3IARBPROC __rglgen_glWindowPos3iARB;
extern RGLSYMGLWINDOWPOS3IVARBPROC __rglgen_glWindowPos3ivARB;
extern RGLSYMGLWINDOWPOS3SARBPROC __rglgen_glWindowPos3sARB;
extern RGLSYMGLWINDOWPOS3SVARBPROC __rglgen_glWindowPos3svARB;
extern RGLSYMGLVERTEXATTRIB1DARBPROC __rglgen_glVertexAttrib1dARB;
extern RGLSYMGLVERTEXATTRIB1DVARBPROC __rglgen_glVertexAttrib1dvARB;
extern RGLSYMGLVERTEXATTRIB1FARBPROC __rglgen_glVertexAttrib1fARB;
extern RGLSYMGLVERTEXATTRIB1FVARBPROC __rglgen_glVertexAttrib1fvARB;
extern RGLSYMGLVERTEXATTRIB1SARBPROC __rglgen_glVertexAttrib1sARB;
extern RGLSYMGLVERTEXATTRIB1SVARBPROC __rglgen_glVertexAttrib1svARB;
extern RGLSYMGLVERTEXATTRIB2DARBPROC __rglgen_glVertexAttrib2dARB;
extern RGLSYMGLVERTEXATTRIB2DVARBPROC __rglgen_glVertexAttrib2dvARB;
extern RGLSYMGLVERTEXATTRIB2FARBPROC __rglgen_glVertexAttrib2fARB;
extern RGLSYMGLVERTEXATTRIB2FVARBPROC __rglgen_glVertexAttrib2fvARB;
extern RGLSYMGLVERTEXATTRIB2SARBPROC __rglgen_glVertexAttrib2sARB;
extern RGLSYMGLVERTEXATTRIB2SVARBPROC __rglgen_glVertexAttrib2svARB;
extern RGLSYMGLVERTEXATTRIB3DARBPROC __rglgen_glVertexAttrib3dARB;
extern RGLSYMGLVERTEXATTRIB3DVARBPROC __rglgen_glVertexAttrib3dvARB;
extern RGLSYMGLVERTEXATTRIB3FARBPROC __rglgen_glVertexAttrib3fARB;
extern RGLSYMGLVERTEXATTRIB3FVARBPROC __rglgen_glVertexAttrib3fvARB;
extern RGLSYMGLVERTEXATTRIB3SARBPROC __rglgen_glVertexAttrib3sARB;
extern RGLSYMGLVERTEXATTRIB3SVARBPROC __rglgen_glVertexAttrib3svARB;
extern RGLSYMGLVERTEXATTRIB4NBVARBPROC __rglgen_glVertexAttrib4NbvARB;
extern RGLSYMGLVERTEXATTRIB4NIVARBPROC __rglgen_glVertexAttrib4NivARB;
extern RGLSYMGLVERTEXATTRIB4NSVARBPROC __rglgen_glVertexAttrib4NsvARB;
extern RGLSYMGLVERTEXATTRIB4NUBARBPROC __rglgen_glVertexAttrib4NubARB;
extern RGLSYMGLVERTEXATTRIB4NUBVARBPROC __rglgen_glVertexAttrib4NubvARB;
extern RGLSYMGLVERTEXATTRIB4NUIVARBPROC __rglgen_glVertexAttrib4NuivARB;
extern RGLSYMGLVERTEXATTRIB4NUSVARBPROC __rglgen_glVertexAttrib4NusvARB;
extern RGLSYMGLVERTEXATTRIB4BVARBPROC __rglgen_glVertexAttrib4bvARB;
extern RGLSYMGLVERTEXATTRIB4DARBPROC __rglgen_glVertexAttrib4dARB;
extern RGLSYMGLVERTEXATTRIB4DVARBPROC __rglgen_glVertexAttrib4dvARB;
extern RGLSYMGLVERTEXATTRIB4FARBPROC __rglgen_glVertexAttrib4fARB;
extern RGLSYMGLVERTEXATTRIB4FVARBPROC __rglgen_glVertexAttrib4fvARB;
extern RGLSYMGLVERTEXATTRIB4IVARBPROC __rglgen_glVertexAttrib4ivARB;
extern RGLSYMGLVERTEXATTRIB4SARBPROC __rglgen_glVertexAttrib4sARB;
extern RGLSYMGLVERTEXATTRIB4SVARBPROC __rglgen_glVertexAttrib4svARB;
extern RGLSYMGLVERTEXATTRIB4UBVARBPROC __rglgen_glVertexAttrib4ubvARB;
extern RGLSYMGLVERTEXATTRIB4UIVARBPROC __rglgen_glVertexAttrib4uivARB;
extern RGLSYMGLVERTEXATTRIB4USVARBPROC __rglgen_glVertexAttrib4usvARB;
extern RGLSYMGLVERTEXATTRIBPOINTERARBPROC __rglgen_glVertexAttribPointerARB;
extern RGLSYMGLENABLEVERTEXATTRIBARRAYARBPROC __rglgen_glEnableVertexAttribArrayARB;
extern RGLSYMGLDISABLEVERTEXATTRIBARRAYARBPROC __rglgen_glDisableVertexAttribArrayARB;
extern RGLSYMGLPROGRAMSTRINGARBPROC __rglgen_glProgramStringARB;
extern RGLSYMGLBINDPROGRAMARBPROC __rglgen_glBindProgramARB;
extern RGLSYMGLDELETEPROGRAMSARBPROC __rglgen_glDeleteProgramsARB;
extern RGLSYMGLGENPROGRAMSARBPROC __rglgen_glGenProgramsARB;
extern RGLSYMGLPROGRAMENVPARAMETER4DARBPROC __rglgen_glProgramEnvParameter4dARB;
extern RGLSYMGLPROGRAMENVPARAMETER4DVARBPROC __rglgen_glProgramEnvParameter4dvARB;
extern RGLSYMGLPROGRAMENVPARAMETER4FARBPROC __rglgen_glProgramEnvParameter4fARB;
extern RGLSYMGLPROGRAMENVPARAMETER4FVARBPROC __rglgen_glProgramEnvParameter4fvARB;
extern RGLSYMGLPROGRAMLOCALPARAMETER4DARBPROC __rglgen_glProgramLocalParameter4dARB;
extern RGLSYMGLPROGRAMLOCALPARAMETER4DVARBPROC __rglgen_glProgramLocalParameter4dvARB;
extern RGLSYMGLPROGRAMLOCALPARAMETER4FARBPROC __rglgen_glProgramLocalParameter4fARB;
extern RGLSYMGLPROGRAMLOCALPARAMETER4FVARBPROC __rglgen_glProgramLocalParameter4fvARB;
extern RGLSYMGLGETPROGRAMENVPARAMETERDVARBPROC __rglgen_glGetProgramEnvParameterdvARB;
extern RGLSYMGLGETPROGRAMENVPARAMETERFVARBPROC __rglgen_glGetProgramEnvParameterfvARB;
extern RGLSYMGLGETPROGRAMLOCALPARAMETERDVARBPROC __rglgen_glGetProgramLocalParameterdvARB;
extern RGLSYMGLGETPROGRAMLOCALPARAMETERFVARBPROC __rglgen_glGetProgramLocalParameterfvARB;
extern RGLSYMGLGETPROGRAMIVARBPROC __rglgen_glGetProgramivARB;
extern RGLSYMGLGETPROGRAMSTRINGARBPROC __rglgen_glGetProgramStringARB;
extern RGLSYMGLGETVERTEXATTRIBDVARBPROC __rglgen_glGetVertexAttribdvARB;
extern RGLSYMGLGETVERTEXATTRIBFVARBPROC __rglgen_glGetVertexAttribfvARB;
extern RGLSYMGLGETVERTEXATTRIBIVARBPROC __rglgen_glGetVertexAttribivARB;
extern RGLSYMGLGETVERTEXATTRIBPOINTERVARBPROC __rglgen_glGetVertexAttribPointervARB;
extern RGLSYMGLISPROGRAMARBPROC __rglgen_glIsProgramARB;
extern RGLSYMGLBINDBUFFERARBPROC __rglgen_glBindBufferARB;
extern RGLSYMGLDELETEBUFFERSARBPROC __rglgen_glDeleteBuffersARB;
extern RGLSYMGLGENBUFFERSARBPROC __rglgen_glGenBuffersARB;
extern RGLSYMGLISBUFFERARBPROC __rglgen_glIsBufferARB;
extern RGLSYMGLBUFFERDATAARBPROC __rglgen_glBufferDataARB;
extern RGLSYMGLBUFFERSUBDATAARBPROC __rglgen_glBufferSubDataARB;
extern RGLSYMGLGETBUFFERSUBDATAARBPROC __rglgen_glGetBufferSubDataARB;
extern RGLSYMGLMAPBUFFERARBPROC __rglgen_glMapBufferARB;
extern RGLSYMGLUNMAPBUFFERARBPROC __rglgen_glUnmapBufferARB;
extern RGLSYMGLGETBUFFERPARAMETERIVARBPROC __rglgen_glGetBufferParameterivARB;
extern RGLSYMGLGETBUFFERPOINTERVARBPROC __rglgen_glGetBufferPointervARB;
extern RGLSYMGLGENQUERIESARBPROC __rglgen_glGenQueriesARB;
extern RGLSYMGLDELETEQUERIESARBPROC __rglgen_glDeleteQueriesARB;
extern RGLSYMGLISQUERYARBPROC __rglgen_glIsQueryARB;
extern RGLSYMGLBEGINQUERYARBPROC __rglgen_glBeginQueryARB;
extern RGLSYMGLENDQUERYARBPROC __rglgen_glEndQueryARB;
extern RGLSYMGLGETQUERYIVARBPROC __rglgen_glGetQueryivARB;
extern RGLSYMGLGETQUERYOBJECTIVARBPROC __rglgen_glGetQueryObjectivARB;
extern RGLSYMGLGETQUERYOBJECTUIVARBPROC __rglgen_glGetQueryObjectuivARB;
extern RGLSYMGLDELETEOBJECTARBPROC __rglgen_glDeleteObjectARB;
extern RGLSYMGLGETHANDLEARBPROC __rglgen_glGetHandleARB;
extern RGLSYMGLDETACHOBJECTARBPROC __rglgen_glDetachObjectARB;
extern RGLSYMGLCREATESHADEROBJECTARBPROC __rglgen_glCreateShaderObjectARB;
extern RGLSYMGLSHADERSOURCEARBPROC __rglgen_glShaderSourceARB;
extern RGLSYMGLCOMPILESHADERARBPROC __rglgen_glCompileShaderARB;
extern RGLSYMGLCREATEPROGRAMOBJECTARBPROC __rglgen_glCreateProgramObjectARB;
extern RGLSYMGLATTACHOBJECTARBPROC __rglgen_glAttachObjectARB;
extern RGLSYMGLLINKPROGRAMARBPROC __rglgen_glLinkProgramARB;
extern RGLSYMGLUSEPROGRAMOBJECTARBPROC __rglgen_glUseProgramObjectARB;
extern RGLSYMGLVALIDATEPROGRAMARBPROC __rglgen_glValidateProgramARB;
extern RGLSYMGLUNIFORM1FARBPROC __rglgen_glUniform1fARB;
extern RGLSYMGLUNIFORM2FARBPROC __rglgen_glUniform2fARB;
extern RGLSYMGLUNIFORM3FARBPROC __rglgen_glUniform3fARB;
extern RGLSYMGLUNIFORM4FARBPROC __rglgen_glUniform4fARB;
extern RGLSYMGLUNIFORM1IARBPROC __rglgen_glUniform1iARB;
extern RGLSYMGLUNIFORM2IARBPROC __rglgen_glUniform2iARB;
extern RGLSYMGLUNIFORM3IARBPROC __rglgen_glUniform3iARB;
extern RGLSYMGLUNIFORM4IARBPROC __rglgen_glUniform4iARB;
extern RGLSYMGLUNIFORM1FVARBPROC __rglgen_glUniform1fvARB;
extern RGLSYMGLUNIFORM2FVARBPROC __rglgen_glUniform2fvARB;
extern RGLSYMGLUNIFORM3FVARBPROC __rglgen_glUniform3fvARB;
extern RGLSYMGLUNIFORM4FVARBPROC __rglgen_glUniform4fvARB;
extern RGLSYMGLUNIFORM1IVARBPROC __rglgen_glUniform1ivARB;
extern RGLSYMGLUNIFORM2IVARBPROC __rglgen_glUniform2ivARB;
extern RGLSYMGLUNIFORM3IVARBPROC __rglgen_glUniform3ivARB;
extern RGLSYMGLUNIFORM4IVARBPROC __rglgen_glUniform4ivARB;
extern RGLSYMGLUNIFORMMATRIX2FVARBPROC __rglgen_glUniformMatrix2fvARB;
extern RGLSYMGLUNIFORMMATRIX3FVARBPROC __rglgen_glUniformMatrix3fvARB;
extern RGLSYMGLUNIFORMMATRIX4FVARBPROC __rglgen_glUniformMatrix4fvARB;
extern RGLSYMGLGETOBJECTPARAMETERFVARBPROC __rglgen_glGetObjectParameterfvARB;
extern RGLSYMGLGETOBJECTPARAMETERIVARBPROC __rglgen_glGetObjectParameterivARB;
extern RGLSYMGLGETINFOLOGARBPROC __rglgen_glGetInfoLogARB;
extern RGLSYMGLGETATTACHEDOBJECTSARBPROC __rglgen_glGetAttachedObjectsARB;
extern RGLSYMGLGETUNIFORMLOCATIONARBPROC __rglgen_glGetUniformLocationARB;
extern RGLSYMGLGETACTIVEUNIFORMARBPROC __rglgen_glGetActiveUniformARB;
extern RGLSYMGLGETUNIFORMFVARBPROC __rglgen_glGetUniformfvARB;
extern RGLSYMGLGETUNIFORMIVARBPROC __rglgen_glGetUniformivARB;
extern RGLSYMGLGETSHADERSOURCEARBPROC __rglgen_glGetShaderSourceARB;
extern RGLSYMGLBINDATTRIBLOCATIONARBPROC __rglgen_glBindAttribLocationARB;
extern RGLSYMGLGETACTIVEATTRIBARBPROC __rglgen_glGetActiveAttribARB;
extern RGLSYMGLGETATTRIBLOCATIONARBPROC __rglgen_glGetAttribLocationARB;
extern RGLSYMGLDRAWBUFFERSARBPROC __rglgen_glDrawBuffersARB;
extern RGLSYMGLCLAMPCOLORARBPROC __rglgen_glClampColorARB;
extern RGLSYMGLDRAWARRAYSINSTANCEDARBPROC __rglgen_glDrawArraysInstancedARB;
extern RGLSYMGLDRAWELEMENTSINSTANCEDARBPROC __rglgen_glDrawElementsInstancedARB;
extern RGLSYMGLISRENDERBUFFERPROC __rglgen_glIsRenderbuffer;
extern RGLSYMGLBINDRENDERBUFFERPROC __rglgen_glBindRenderbuffer;
extern RGLSYMGLDELETERENDERBUFFERSPROC __rglgen_glDeleteRenderbuffers;
extern RGLSYMGLGENRENDERBUFFERSPROC __rglgen_glGenRenderbuffers;
extern RGLSYMGLRENDERBUFFERSTORAGEPROC __rglgen_glRenderbufferStorage;
extern RGLSYMGLGETRENDERBUFFERPARAMETERIVPROC __rglgen_glGetRenderbufferParameteriv;
extern RGLSYMGLISFRAMEBUFFERPROC __rglgen_glIsFramebuffer;
extern RGLSYMGLBINDFRAMEBUFFERPROC __rglgen_glBindFramebuffer;
extern RGLSYMGLDELETEFRAMEBUFFERSPROC __rglgen_glDeleteFramebuffers;
extern RGLSYMGLGENFRAMEBUFFERSPROC __rglgen_glGenFramebuffers;
extern RGLSYMGLCHECKFRAMEBUFFERSTATUSPROC __rglgen_glCheckFramebufferStatus;
extern RGLSYMGLFRAMEBUFFERTEXTURE1DPROC __rglgen_glFramebufferTexture1D;
extern RGLSYMGLFRAMEBUFFERTEXTURE2DPROC __rglgen_glFramebufferTexture2D;
extern RGLSYMGLFRAMEBUFFERTEXTURE3DPROC __rglgen_glFramebufferTexture3D;
extern RGLSYMGLFRAMEBUFFERRENDERBUFFERPROC __rglgen_glFramebufferRenderbuffer;
extern RGLSYMGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC __rglgen_glGetFramebufferAttachmentParameteriv;
extern RGLSYMGLGENERATEMIPMAPPROC __rglgen_glGenerateMipmap;
extern RGLSYMGLBLITFRAMEBUFFERPROC __rglgen_glBlitFramebuffer;
extern RGLSYMGLRENDERBUFFERSTORAGEMULTISAMPLEPROC __rglgen_glRenderbufferStorageMultisample;
extern RGLSYMGLFRAMEBUFFERTEXTURELAYERPROC __rglgen_glFramebufferTextureLayer;
extern RGLSYMGLPROGRAMPARAMETERIARBPROC __rglgen_glProgramParameteriARB;
extern RGLSYMGLFRAMEBUFFERTEXTUREARBPROC __rglgen_glFramebufferTextureARB;
extern RGLSYMGLFRAMEBUFFERTEXTURELAYERARBPROC __rglgen_glFramebufferTextureLayerARB;
extern RGLSYMGLFRAMEBUFFERTEXTUREFACEARBPROC __rglgen_glFramebufferTextureFaceARB;
extern RGLSYMGLVERTEXATTRIBDIVISORARBPROC __rglgen_glVertexAttribDivisorARB;
extern RGLSYMGLMAPBUFFERRANGEPROC __rglgen_glMapBufferRange;
extern RGLSYMGLFLUSHMAPPEDBUFFERRANGEPROC __rglgen_glFlushMappedBufferRange;
extern RGLSYMGLTEXBUFFERARBPROC __rglgen_glTexBufferARB;
extern RGLSYMGLBINDVERTEXARRAYPROC __rglgen_glBindVertexArray;
extern RGLSYMGLDELETEVERTEXARRAYSPROC __rglgen_glDeleteVertexArrays;
extern RGLSYMGLGENVERTEXARRAYSPROC __rglgen_glGenVertexArrays;
extern RGLSYMGLISVERTEXARRAYPROC __rglgen_glIsVertexArray;
extern RGLSYMGLGETUNIFORMINDICESPROC __rglgen_glGetUniformIndices;
extern RGLSYMGLGETACTIVEUNIFORMSIVPROC __rglgen_glGetActiveUniformsiv;
extern RGLSYMGLGETACTIVEUNIFORMNAMEPROC __rglgen_glGetActiveUniformName;
extern RGLSYMGLGETUNIFORMBLOCKINDEXPROC __rglgen_glGetUniformBlockIndex;
extern RGLSYMGLGETACTIVEUNIFORMBLOCKIVPROC __rglgen_glGetActiveUniformBlockiv;
extern RGLSYMGLGETACTIVEUNIFORMBLOCKNAMEPROC __rglgen_glGetActiveUniformBlockName;
extern RGLSYMGLUNIFORMBLOCKBINDINGPROC __rglgen_glUniformBlockBinding;
extern RGLSYMGLCOPYBUFFERSUBDATAPROC __rglgen_glCopyBufferSubData;
extern RGLSYMGLDRAWELEMENTSBASEVERTEXPROC __rglgen_glDrawElementsBaseVertex;
extern RGLSYMGLDRAWRANGEELEMENTSBASEVERTEXPROC __rglgen_glDrawRangeElementsBaseVertex;
extern RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC __rglgen_glDrawElementsInstancedBaseVertex;
extern RGLSYMGLMULTIDRAWELEMENTSBASEVERTEXPROC __rglgen_glMultiDrawElementsBaseVertex;
extern RGLSYMGLPROVOKINGVERTEXPROC __rglgen_glProvokingVertex;
extern RGLSYMGLFENCESYNCPROC __rglgen_glFenceSync;
extern RGLSYMGLISSYNCPROC __rglgen_glIsSync;
extern RGLSYMGLDELETESYNCPROC __rglgen_glDeleteSync;
extern RGLSYMGLCLIENTWAITSYNCPROC __rglgen_glClientWaitSync;
extern RGLSYMGLWAITSYNCPROC __rglgen_glWaitSync;
extern RGLSYMGLGETINTEGER64VPROC __rglgen_glGetInteger64v;
extern RGLSYMGLGETSYNCIVPROC __rglgen_glGetSynciv;
extern RGLSYMGLTEXIMAGE2DMULTISAMPLEPROC __rglgen_glTexImage2DMultisample;
extern RGLSYMGLTEXIMAGE3DMULTISAMPLEPROC __rglgen_glTexImage3DMultisample;
extern RGLSYMGLGETMULTISAMPLEFVPROC __rglgen_glGetMultisamplefv;
extern RGLSYMGLSAMPLEMASKIPROC __rglgen_glSampleMaski;
extern RGLSYMGLBLENDEQUATIONIARBPROC __rglgen_glBlendEquationiARB;
extern RGLSYMGLBLENDEQUATIONSEPARATEIARBPROC __rglgen_glBlendEquationSeparateiARB;
extern RGLSYMGLBLENDFUNCIARBPROC __rglgen_glBlendFunciARB;
extern RGLSYMGLBLENDFUNCSEPARATEIARBPROC __rglgen_glBlendFuncSeparateiARB;
extern RGLSYMGLMINSAMPLESHADINGARBPROC __rglgen_glMinSampleShadingARB;
extern RGLSYMGLNAMEDSTRINGARBPROC __rglgen_glNamedStringARB;
extern RGLSYMGLDELETENAMEDSTRINGARBPROC __rglgen_glDeleteNamedStringARB;
extern RGLSYMGLCOMPILESHADERINCLUDEARBPROC __rglgen_glCompileShaderIncludeARB;
extern RGLSYMGLISNAMEDSTRINGARBPROC __rglgen_glIsNamedStringARB;
extern RGLSYMGLGETNAMEDSTRINGARBPROC __rglgen_glGetNamedStringARB;
extern RGLSYMGLGETNAMEDSTRINGIVARBPROC __rglgen_glGetNamedStringivARB;
extern RGLSYMGLBINDFRAGDATALOCATIONINDEXEDPROC __rglgen_glBindFragDataLocationIndexed;
extern RGLSYMGLGETFRAGDATAINDEXPROC __rglgen_glGetFragDataIndex;
extern RGLSYMGLGENSAMPLERSPROC __rglgen_glGenSamplers;
extern RGLSYMGLDELETESAMPLERSPROC __rglgen_glDeleteSamplers;
extern RGLSYMGLISSAMPLERPROC __rglgen_glIsSampler;
extern RGLSYMGLBINDSAMPLERPROC __rglgen_glBindSampler;
extern RGLSYMGLSAMPLERPARAMETERIPROC __rglgen_glSamplerParameteri;
extern RGLSYMGLSAMPLERPARAMETERIVPROC __rglgen_glSamplerParameteriv;
extern RGLSYMGLSAMPLERPARAMETERFPROC __rglgen_glSamplerParameterf;
extern RGLSYMGLSAMPLERPARAMETERFVPROC __rglgen_glSamplerParameterfv;
extern RGLSYMGLSAMPLERPARAMETERIIVPROC __rglgen_glSamplerParameterIiv;
extern RGLSYMGLSAMPLERPARAMETERIUIVPROC __rglgen_glSamplerParameterIuiv;
extern RGLSYMGLGETSAMPLERPARAMETERIVPROC __rglgen_glGetSamplerParameteriv;
extern RGLSYMGLGETSAMPLERPARAMETERIIVPROC __rglgen_glGetSamplerParameterIiv;
extern RGLSYMGLGETSAMPLERPARAMETERFVPROC __rglgen_glGetSamplerParameterfv;
extern RGLSYMGLGETSAMPLERPARAMETERIUIVPROC __rglgen_glGetSamplerParameterIuiv;
extern RGLSYMGLQUERYCOUNTERPROC __rglgen_glQueryCounter;
extern RGLSYMGLGETQUERYOBJECTI64VPROC __rglgen_glGetQueryObjecti64v;
extern RGLSYMGLGETQUERYOBJECTUI64VPROC __rglgen_glGetQueryObjectui64v;
extern RGLSYMGLVERTEXP2UIPROC __rglgen_glVertexP2ui;
extern RGLSYMGLVERTEXP2UIVPROC __rglgen_glVertexP2uiv;
extern RGLSYMGLVERTEXP3UIPROC __rglgen_glVertexP3ui;
extern RGLSYMGLVERTEXP3UIVPROC __rglgen_glVertexP3uiv;
extern RGLSYMGLVERTEXP4UIPROC __rglgen_glVertexP4ui;
extern RGLSYMGLVERTEXP4UIVPROC __rglgen_glVertexP4uiv;
extern RGLSYMGLTEXCOORDP1UIPROC __rglgen_glTexCoordP1ui;
extern RGLSYMGLTEXCOORDP1UIVPROC __rglgen_glTexCoordP1uiv;
extern RGLSYMGLTEXCOORDP2UIPROC __rglgen_glTexCoordP2ui;
extern RGLSYMGLTEXCOORDP2UIVPROC __rglgen_glTexCoordP2uiv;
extern RGLSYMGLTEXCOORDP3UIPROC __rglgen_glTexCoordP3ui;
extern RGLSYMGLTEXCOORDP3UIVPROC __rglgen_glTexCoordP3uiv;
extern RGLSYMGLTEXCOORDP4UIPROC __rglgen_glTexCoordP4ui;
extern RGLSYMGLTEXCOORDP4UIVPROC __rglgen_glTexCoordP4uiv;
extern RGLSYMGLMULTITEXCOORDP1UIPROC __rglgen_glMultiTexCoordP1ui;
extern RGLSYMGLMULTITEXCOORDP1UIVPROC __rglgen_glMultiTexCoordP1uiv;
extern RGLSYMGLMULTITEXCOORDP2UIPROC __rglgen_glMultiTexCoordP2ui;
extern RGLSYMGLMULTITEXCOORDP2UIVPROC __rglgen_glMultiTexCoordP2uiv;
extern RGLSYMGLMULTITEXCOORDP3UIPROC __rglgen_glMultiTexCoordP3ui;
extern RGLSYMGLMULTITEXCOORDP3UIVPROC __rglgen_glMultiTexCoordP3uiv;
extern RGLSYMGLMULTITEXCOORDP4UIPROC __rglgen_glMultiTexCoordP4ui;
extern RGLSYMGLMULTITEXCOORDP4UIVPROC __rglgen_glMultiTexCoordP4uiv;
extern RGLSYMGLNORMALP3UIPROC __rglgen_glNormalP3ui;
extern RGLSYMGLNORMALP3UIVPROC __rglgen_glNormalP3uiv;
extern RGLSYMGLCOLORP3UIPROC __rglgen_glColorP3ui;
extern RGLSYMGLCOLORP3UIVPROC __rglgen_glColorP3uiv;
extern RGLSYMGLCOLORP4UIPROC __rglgen_glColorP4ui;
extern RGLSYMGLCOLORP4UIVPROC __rglgen_glColorP4uiv;
extern RGLSYMGLSECONDARYCOLORP3UIPROC __rglgen_glSecondaryColorP3ui;
extern RGLSYMGLSECONDARYCOLORP3UIVPROC __rglgen_glSecondaryColorP3uiv;
extern RGLSYMGLVERTEXATTRIBP1UIPROC __rglgen_glVertexAttribP1ui;
extern RGLSYMGLVERTEXATTRIBP1UIVPROC __rglgen_glVertexAttribP1uiv;
extern RGLSYMGLVERTEXATTRIBP2UIPROC __rglgen_glVertexAttribP2ui;
extern RGLSYMGLVERTEXATTRIBP2UIVPROC __rglgen_glVertexAttribP2uiv;
extern RGLSYMGLVERTEXATTRIBP3UIPROC __rglgen_glVertexAttribP3ui;
extern RGLSYMGLVERTEXATTRIBP3UIVPROC __rglgen_glVertexAttribP3uiv;
extern RGLSYMGLVERTEXATTRIBP4UIPROC __rglgen_glVertexAttribP4ui;
extern RGLSYMGLVERTEXATTRIBP4UIVPROC __rglgen_glVertexAttribP4uiv;
extern RGLSYMGLDRAWARRAYSINDIRECTPROC __rglgen_glDrawArraysIndirect;
extern RGLSYMGLDRAWELEMENTSINDIRECTPROC __rglgen_glDrawElementsIndirect;
extern RGLSYMGLUNIFORM1DPROC __rglgen_glUniform1d;
extern RGLSYMGLUNIFORM2DPROC __rglgen_glUniform2d;
extern RGLSYMGLUNIFORM3DPROC __rglgen_glUniform3d;
extern RGLSYMGLUNIFORM4DPROC __rglgen_glUniform4d;
extern RGLSYMGLUNIFORM1DVPROC __rglgen_glUniform1dv;
extern RGLSYMGLUNIFORM2DVPROC __rglgen_glUniform2dv;
extern RGLSYMGLUNIFORM3DVPROC __rglgen_glUniform3dv;
extern RGLSYMGLUNIFORM4DVPROC __rglgen_glUniform4dv;
extern RGLSYMGLUNIFORMMATRIX2DVPROC __rglgen_glUniformMatrix2dv;
extern RGLSYMGLUNIFORMMATRIX3DVPROC __rglgen_glUniformMatrix3dv;
extern RGLSYMGLUNIFORMMATRIX4DVPROC __rglgen_glUniformMatrix4dv;
extern RGLSYMGLUNIFORMMATRIX2X3DVPROC __rglgen_glUniformMatrix2x3dv;
extern RGLSYMGLUNIFORMMATRIX2X4DVPROC __rglgen_glUniformMatrix2x4dv;
extern RGLSYMGLUNIFORMMATRIX3X2DVPROC __rglgen_glUniformMatrix3x2dv;
extern RGLSYMGLUNIFORMMATRIX3X4DVPROC __rglgen_glUniformMatrix3x4dv;
extern RGLSYMGLUNIFORMMATRIX4X2DVPROC __rglgen_glUniformMatrix4x2dv;
extern RGLSYMGLUNIFORMMATRIX4X3DVPROC __rglgen_glUniformMatrix4x3dv;
extern RGLSYMGLGETUNIFORMDVPROC __rglgen_glGetUniformdv;
extern RGLSYMGLGETSUBROUTINEUNIFORMLOCATIONPROC __rglgen_glGetSubroutineUniformLocation;
extern RGLSYMGLGETSUBROUTINEINDEXPROC __rglgen_glGetSubroutineIndex;
extern RGLSYMGLGETACTIVESUBROUTINEUNIFORMIVPROC __rglgen_glGetActiveSubroutineUniformiv;
extern RGLSYMGLGETACTIVESUBROUTINEUNIFORMNAMEPROC __rglgen_glGetActiveSubroutineUniformName;
extern RGLSYMGLGETACTIVESUBROUTINENAMEPROC __rglgen_glGetActiveSubroutineName;
extern RGLSYMGLUNIFORMSUBROUTINESUIVPROC __rglgen_glUniformSubroutinesuiv;
extern RGLSYMGLGETUNIFORMSUBROUTINEUIVPROC __rglgen_glGetUniformSubroutineuiv;
extern RGLSYMGLGETPROGRAMSTAGEIVPROC __rglgen_glGetProgramStageiv;
extern RGLSYMGLPATCHPARAMETERIPROC __rglgen_glPatchParameteri;
extern RGLSYMGLPATCHPARAMETERFVPROC __rglgen_glPatchParameterfv;
extern RGLSYMGLBINDTRANSFORMFEEDBACKPROC __rglgen_glBindTransformFeedback;
extern RGLSYMGLDELETETRANSFORMFEEDBACKSPROC __rglgen_glDeleteTransformFeedbacks;
extern RGLSYMGLGENTRANSFORMFEEDBACKSPROC __rglgen_glGenTransformFeedbacks;
extern RGLSYMGLISTRANSFORMFEEDBACKPROC __rglgen_glIsTransformFeedback;
extern RGLSYMGLPAUSETRANSFORMFEEDBACKPROC __rglgen_glPauseTransformFeedback;
extern RGLSYMGLRESUMETRANSFORMFEEDBACKPROC __rglgen_glResumeTransformFeedback;
extern RGLSYMGLDRAWTRANSFORMFEEDBACKPROC __rglgen_glDrawTransformFeedback;
extern RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMPROC __rglgen_glDrawTransformFeedbackStream;
extern RGLSYMGLBEGINQUERYINDEXEDPROC __rglgen_glBeginQueryIndexed;
extern RGLSYMGLENDQUERYINDEXEDPROC __rglgen_glEndQueryIndexed;
extern RGLSYMGLGETQUERYINDEXEDIVPROC __rglgen_glGetQueryIndexediv;
extern RGLSYMGLRELEASESHADERCOMPILERPROC __rglgen_glReleaseShaderCompiler;
extern RGLSYMGLSHADERBINARYPROC __rglgen_glShaderBinary;
extern RGLSYMGLGETSHADERPRECISIONFORMATPROC __rglgen_glGetShaderPrecisionFormat;
extern RGLSYMGLDEPTHRANGEFPROC __rglgen_glDepthRangef;
extern RGLSYMGLCLEARDEPTHFPROC __rglgen_glClearDepthf;
extern RGLSYMGLGETPROGRAMBINARYPROC __rglgen_glGetProgramBinary;
extern RGLSYMGLPROGRAMBINARYPROC __rglgen_glProgramBinary;
extern RGLSYMGLPROGRAMPARAMETERIPROC __rglgen_glProgramParameteri;
extern RGLSYMGLUSEPROGRAMSTAGESPROC __rglgen_glUseProgramStages;
extern RGLSYMGLACTIVESHADERPROGRAMPROC __rglgen_glActiveShaderProgram;
extern RGLSYMGLCREATESHADERPROGRAMVPROC __rglgen_glCreateShaderProgramv;
extern RGLSYMGLBINDPROGRAMPIPELINEPROC __rglgen_glBindProgramPipeline;
extern RGLSYMGLDELETEPROGRAMPIPELINESPROC __rglgen_glDeleteProgramPipelines;
extern RGLSYMGLGENPROGRAMPIPELINESPROC __rglgen_glGenProgramPipelines;
extern RGLSYMGLISPROGRAMPIPELINEPROC __rglgen_glIsProgramPipeline;
extern RGLSYMGLGETPROGRAMPIPELINEIVPROC __rglgen_glGetProgramPipelineiv;
extern RGLSYMGLPROGRAMUNIFORM1IPROC __rglgen_glProgramUniform1i;
extern RGLSYMGLPROGRAMUNIFORM1IVPROC __rglgen_glProgramUniform1iv;
extern RGLSYMGLPROGRAMUNIFORM1FPROC __rglgen_glProgramUniform1f;
extern RGLSYMGLPROGRAMUNIFORM1FVPROC __rglgen_glProgramUniform1fv;
extern RGLSYMGLPROGRAMUNIFORM1DPROC __rglgen_glProgramUniform1d;
extern RGLSYMGLPROGRAMUNIFORM1DVPROC __rglgen_glProgramUniform1dv;
extern RGLSYMGLPROGRAMUNIFORM1UIPROC __rglgen_glProgramUniform1ui;
extern RGLSYMGLPROGRAMUNIFORM1UIVPROC __rglgen_glProgramUniform1uiv;
extern RGLSYMGLPROGRAMUNIFORM2IPROC __rglgen_glProgramUniform2i;
extern RGLSYMGLPROGRAMUNIFORM2IVPROC __rglgen_glProgramUniform2iv;
extern RGLSYMGLPROGRAMUNIFORM2FPROC __rglgen_glProgramUniform2f;
extern RGLSYMGLPROGRAMUNIFORM2FVPROC __rglgen_glProgramUniform2fv;
extern RGLSYMGLPROGRAMUNIFORM2DPROC __rglgen_glProgramUniform2d;
extern RGLSYMGLPROGRAMUNIFORM2DVPROC __rglgen_glProgramUniform2dv;
extern RGLSYMGLPROGRAMUNIFORM2UIPROC __rglgen_glProgramUniform2ui;
extern RGLSYMGLPROGRAMUNIFORM2UIVPROC __rglgen_glProgramUniform2uiv;
extern RGLSYMGLPROGRAMUNIFORM3IPROC __rglgen_glProgramUniform3i;
extern RGLSYMGLPROGRAMUNIFORM3IVPROC __rglgen_glProgramUniform3iv;
extern RGLSYMGLPROGRAMUNIFORM3FPROC __rglgen_glProgramUniform3f;
extern RGLSYMGLPROGRAMUNIFORM3FVPROC __rglgen_glProgramUniform3fv;
extern RGLSYMGLPROGRAMUNIFORM3DPROC __rglgen_glProgramUniform3d;
extern RGLSYMGLPROGRAMUNIFORM3DVPROC __rglgen_glProgramUniform3dv;
extern RGLSYMGLPROGRAMUNIFORM3UIPROC __rglgen_glProgramUniform3ui;
extern RGLSYMGLPROGRAMUNIFORM3UIVPROC __rglgen_glProgramUniform3uiv;
extern RGLSYMGLPROGRAMUNIFORM4IPROC __rglgen_glProgramUniform4i;
extern RGLSYMGLPROGRAMUNIFORM4IVPROC __rglgen_glProgramUniform4iv;
extern RGLSYMGLPROGRAMUNIFORM4FPROC __rglgen_glProgramUniform4f;
extern RGLSYMGLPROGRAMUNIFORM4FVPROC __rglgen_glProgramUniform4fv;
extern RGLSYMGLPROGRAMUNIFORM4DPROC __rglgen_glProgramUniform4d;
extern RGLSYMGLPROGRAMUNIFORM4DVPROC __rglgen_glProgramUniform4dv;
extern RGLSYMGLPROGRAMUNIFORM4UIPROC __rglgen_glProgramUniform4ui;
extern RGLSYMGLPROGRAMUNIFORM4UIVPROC __rglgen_glProgramUniform4uiv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2FVPROC __rglgen_glProgramUniformMatrix2fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3FVPROC __rglgen_glProgramUniformMatrix3fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4FVPROC __rglgen_glProgramUniformMatrix4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2DVPROC __rglgen_glProgramUniformMatrix2dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3DVPROC __rglgen_glProgramUniformMatrix3dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4DVPROC __rglgen_glProgramUniformMatrix4dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X3FVPROC __rglgen_glProgramUniformMatrix2x3fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X2FVPROC __rglgen_glProgramUniformMatrix3x2fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X4FVPROC __rglgen_glProgramUniformMatrix2x4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X2FVPROC __rglgen_glProgramUniformMatrix4x2fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X4FVPROC __rglgen_glProgramUniformMatrix3x4fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X3FVPROC __rglgen_glProgramUniformMatrix4x3fv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X3DVPROC __rglgen_glProgramUniformMatrix2x3dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X2DVPROC __rglgen_glProgramUniformMatrix3x2dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX2X4DVPROC __rglgen_glProgramUniformMatrix2x4dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X2DVPROC __rglgen_glProgramUniformMatrix4x2dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX3X4DVPROC __rglgen_glProgramUniformMatrix3x4dv;
extern RGLSYMGLPROGRAMUNIFORMMATRIX4X3DVPROC __rglgen_glProgramUniformMatrix4x3dv;
extern RGLSYMGLVALIDATEPROGRAMPIPELINEPROC __rglgen_glValidateProgramPipeline;
extern RGLSYMGLGETPROGRAMPIPELINEINFOLOGPROC __rglgen_glGetProgramPipelineInfoLog;
extern RGLSYMGLVERTEXATTRIBL1DPROC __rglgen_glVertexAttribL1d;
extern RGLSYMGLVERTEXATTRIBL2DPROC __rglgen_glVertexAttribL2d;
extern RGLSYMGLVERTEXATTRIBL3DPROC __rglgen_glVertexAttribL3d;
extern RGLSYMGLVERTEXATTRIBL4DPROC __rglgen_glVertexAttribL4d;
extern RGLSYMGLVERTEXATTRIBL1DVPROC __rglgen_glVertexAttribL1dv;
extern RGLSYMGLVERTEXATTRIBL2DVPROC __rglgen_glVertexAttribL2dv;
extern RGLSYMGLVERTEXATTRIBL3DVPROC __rglgen_glVertexAttribL3dv;
extern RGLSYMGLVERTEXATTRIBL4DVPROC __rglgen_glVertexAttribL4dv;
extern RGLSYMGLVERTEXATTRIBLPOINTERPROC __rglgen_glVertexAttribLPointer;
extern RGLSYMGLGETVERTEXATTRIBLDVPROC __rglgen_glGetVertexAttribLdv;
extern RGLSYMGLVIEWPORTARRAYVPROC __rglgen_glViewportArrayv;
extern RGLSYMGLVIEWPORTINDEXEDFPROC __rglgen_glViewportIndexedf;
extern RGLSYMGLVIEWPORTINDEXEDFVPROC __rglgen_glViewportIndexedfv;
extern RGLSYMGLSCISSORARRAYVPROC __rglgen_glScissorArrayv;
extern RGLSYMGLSCISSORINDEXEDPROC __rglgen_glScissorIndexed;
extern RGLSYMGLSCISSORINDEXEDVPROC __rglgen_glScissorIndexedv;
extern RGLSYMGLDEPTHRANGEARRAYVPROC __rglgen_glDepthRangeArrayv;
extern RGLSYMGLDEPTHRANGEINDEXEDPROC __rglgen_glDepthRangeIndexed;
extern RGLSYMGLGETFLOATI_VPROC __rglgen_glGetFloati_v;
extern RGLSYMGLGETDOUBLEI_VPROC __rglgen_glGetDoublei_v;
extern RGLSYMGLCREATESYNCFROMCLEVENTARBPROC __rglgen_glCreateSyncFromCLeventARB;
extern RGLSYMGLDEBUGMESSAGECONTROLARBPROC __rglgen_glDebugMessageControlARB;
extern RGLSYMGLDEBUGMESSAGEINSERTARBPROC __rglgen_glDebugMessageInsertARB;
extern RGLSYMGLDEBUGMESSAGECALLBACKARBPROC __rglgen_glDebugMessageCallbackARB;
extern RGLSYMGLGETDEBUGMESSAGELOGARBPROC __rglgen_glGetDebugMessageLogARB;
extern RGLSYMGLGETGRAPHICSRESETSTATUSARBPROC __rglgen_glGetGraphicsResetStatusARB;
extern RGLSYMGLGETNMAPDVARBPROC __rglgen_glGetnMapdvARB;
extern RGLSYMGLGETNMAPFVARBPROC __rglgen_glGetnMapfvARB;
extern RGLSYMGLGETNMAPIVARBPROC __rglgen_glGetnMapivARB;
extern RGLSYMGLGETNPIXELMAPFVARBPROC __rglgen_glGetnPixelMapfvARB;
extern RGLSYMGLGETNPIXELMAPUIVARBPROC __rglgen_glGetnPixelMapuivARB;
extern RGLSYMGLGETNPIXELMAPUSVARBPROC __rglgen_glGetnPixelMapusvARB;
extern RGLSYMGLGETNPOLYGONSTIPPLEARBPROC __rglgen_glGetnPolygonStippleARB;
extern RGLSYMGLGETNCOLORTABLEARBPROC __rglgen_glGetnColorTableARB;
extern RGLSYMGLGETNCONVOLUTIONFILTERARBPROC __rglgen_glGetnConvolutionFilterARB;
extern RGLSYMGLGETNSEPARABLEFILTERARBPROC __rglgen_glGetnSeparableFilterARB;
extern RGLSYMGLGETNHISTOGRAMARBPROC __rglgen_glGetnHistogramARB;
extern RGLSYMGLGETNMINMAXARBPROC __rglgen_glGetnMinmaxARB;
extern RGLSYMGLGETNTEXIMAGEARBPROC __rglgen_glGetnTexImageARB;
extern RGLSYMGLREADNPIXELSARBPROC __rglgen_glReadnPixelsARB;
extern RGLSYMGLGETNCOMPRESSEDTEXIMAGEARBPROC __rglgen_glGetnCompressedTexImageARB;
extern RGLSYMGLGETNUNIFORMFVARBPROC __rglgen_glGetnUniformfvARB;
extern RGLSYMGLGETNUNIFORMIVARBPROC __rglgen_glGetnUniformivARB;
extern RGLSYMGLGETNUNIFORMUIVARBPROC __rglgen_glGetnUniformuivARB;
extern RGLSYMGLGETNUNIFORMDVARBPROC __rglgen_glGetnUniformdvARB;
extern RGLSYMGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC __rglgen_glDrawArraysInstancedBaseInstance;
extern RGLSYMGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC __rglgen_glDrawElementsInstancedBaseInstance;
extern RGLSYMGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC __rglgen_glDrawElementsInstancedBaseVertexBaseInstance;
extern RGLSYMGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC __rglgen_glDrawTransformFeedbackInstanced;
extern RGLSYMGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC __rglgen_glDrawTransformFeedbackStreamInstanced;
extern RGLSYMGLGETINTERNALFORMATIVPROC __rglgen_glGetInternalformativ;
extern RGLSYMGLGETACTIVEATOMICCOUNTERBUFFERIVPROC __rglgen_glGetActiveAtomicCounterBufferiv;
extern RGLSYMGLBINDIMAGETEXTUREPROC __rglgen_glBindImageTexture;
extern RGLSYMGLMEMORYBARRIERPROC __rglgen_glMemoryBarrier;
extern RGLSYMGLTEXSTORAGE1DPROC __rglgen_glTexStorage1D;
extern RGLSYMGLTEXSTORAGE2DPROC __rglgen_glTexStorage2D;
extern RGLSYMGLTEXSTORAGE3DPROC __rglgen_glTexStorage3D;
extern RGLSYMGLDEBUGMESSAGECONTROLPROC __rglgen_glDebugMessageControl;
extern RGLSYMGLDEBUGMESSAGEINSERTPROC __rglgen_glDebugMessageInsert;
extern RGLSYMGLDEBUGMESSAGECALLBACKPROC __rglgen_glDebugMessageCallback;
extern RGLSYMGLGETDEBUGMESSAGELOGPROC __rglgen_glGetDebugMessageLog;
extern RGLSYMGLPUSHDEBUGGROUPPROC __rglgen_glPushDebugGroup;
extern RGLSYMGLPOPDEBUGGROUPPROC __rglgen_glPopDebugGroup;
extern RGLSYMGLOBJECTLABELPROC __rglgen_glObjectLabel;
extern RGLSYMGLGETOBJECTLABELPROC __rglgen_glGetObjectLabel;
extern RGLSYMGLOBJECTPTRLABELPROC __rglgen_glObjectPtrLabel;
extern RGLSYMGLGETOBJECTPTRLABELPROC __rglgen_glGetObjectPtrLabel;
extern RGLSYMGLCLEARBUFFERDATAPROC __rglgen_glClearBufferData;
extern RGLSYMGLCLEARBUFFERSUBDATAPROC __rglgen_glClearBufferSubData;
extern RGLSYMGLDISPATCHCOMPUTEPROC __rglgen_glDispatchCompute;
extern RGLSYMGLDISPATCHCOMPUTEINDIRECTPROC __rglgen_glDispatchComputeIndirect;
extern RGLSYMGLCOPYIMAGESUBDATAPROC __rglgen_glCopyImageSubData;
extern RGLSYMGLTEXTUREVIEWPROC __rglgen_glTextureView;
extern RGLSYMGLBINDVERTEXBUFFERPROC __rglgen_glBindVertexBuffer;
extern RGLSYMGLVERTEXATTRIBFORMATPROC __rglgen_glVertexAttribFormat;
extern RGLSYMGLVERTEXATTRIBIFORMATPROC __rglgen_glVertexAttribIFormat;
extern RGLSYMGLVERTEXATTRIBLFORMATPROC __rglgen_glVertexAttribLFormat;
extern RGLSYMGLVERTEXATTRIBBINDINGPROC __rglgen_glVertexAttribBinding;
extern RGLSYMGLVERTEXBINDINGDIVISORPROC __rglgen_glVertexBindingDivisor;
extern RGLSYMGLFRAMEBUFFERPARAMETERIPROC __rglgen_glFramebufferParameteri;
extern RGLSYMGLGETFRAMEBUFFERPARAMETERIVPROC __rglgen_glGetFramebufferParameteriv;
extern RGLSYMGLGETINTERNALFORMATI64VPROC __rglgen_glGetInternalformati64v;
extern RGLSYMGLINVALIDATETEXSUBIMAGEPROC __rglgen_glInvalidateTexSubImage;
extern RGLSYMGLINVALIDATETEXIMAGEPROC __rglgen_glInvalidateTexImage;
extern RGLSYMGLINVALIDATEBUFFERSUBDATAPROC __rglgen_glInvalidateBufferSubData;
extern RGLSYMGLINVALIDATEBUFFERDATAPROC __rglgen_glInvalidateBufferData;
extern RGLSYMGLINVALIDATEFRAMEBUFFERPROC __rglgen_glInvalidateFramebuffer;
extern RGLSYMGLINVALIDATESUBFRAMEBUFFERPROC __rglgen_glInvalidateSubFramebuffer;
extern RGLSYMGLMULTIDRAWARRAYSINDIRECTPROC __rglgen_glMultiDrawArraysIndirect;
extern RGLSYMGLMULTIDRAWELEMENTSINDIRECTPROC __rglgen_glMultiDrawElementsIndirect;
extern RGLSYMGLGETPROGRAMINTERFACEIVPROC __rglgen_glGetProgramInterfaceiv;
extern RGLSYMGLGETPROGRAMRESOURCEINDEXPROC __rglgen_glGetProgramResourceIndex;
extern RGLSYMGLGETPROGRAMRESOURCENAMEPROC __rglgen_glGetProgramResourceName;
extern RGLSYMGLGETPROGRAMRESOURCEIVPROC __rglgen_glGetProgramResourceiv;
extern RGLSYMGLGETPROGRAMRESOURCELOCATIONPROC __rglgen_glGetProgramResourceLocation;
extern RGLSYMGLGETPROGRAMRESOURCELOCATIONINDEXPROC __rglgen_glGetProgramResourceLocationIndex;
extern RGLSYMGLSHADERSTORAGEBLOCKBINDINGPROC __rglgen_glShaderStorageBlockBinding;
extern RGLSYMGLTEXBUFFERRANGEPROC __rglgen_glTexBufferRange;
extern RGLSYMGLTEXSTORAGE2DMULTISAMPLEPROC __rglgen_glTexStorage2DMultisample;
extern RGLSYMGLTEXSTORAGE3DMULTISAMPLEPROC __rglgen_glTexStorage3DMultisample;
extern RGLSYMGLIMAGETRANSFORMPARAMETERIHPPROC __rglgen_glImageTransformParameteriHP;
extern RGLSYMGLIMAGETRANSFORMPARAMETERFHPPROC __rglgen_glImageTransformParameterfHP;
extern RGLSYMGLIMAGETRANSFORMPARAMETERIVHPPROC __rglgen_glImageTransformParameterivHP;
extern RGLSYMGLIMAGETRANSFORMPARAMETERFVHPPROC __rglgen_glImageTransformParameterfvHP;
extern RGLSYMGLGETIMAGETRANSFORMPARAMETERIVHPPROC __rglgen_glGetImageTransformParameterivHP;
extern RGLSYMGLGETIMAGETRANSFORMPARAMETERFVHPPROC __rglgen_glGetImageTransformParameterfvHP;
extern RGLSYMGLMULTITEXCOORD1BOESPROC __rglgen_glMultiTexCoord1bOES;
extern RGLSYMGLMULTITEXCOORD1BVOESPROC __rglgen_glMultiTexCoord1bvOES;
extern RGLSYMGLMULTITEXCOORD2BOESPROC __rglgen_glMultiTexCoord2bOES;
extern RGLSYMGLMULTITEXCOORD2BVOESPROC __rglgen_glMultiTexCoord2bvOES;
extern RGLSYMGLMULTITEXCOORD3BOESPROC __rglgen_glMultiTexCoord3bOES;
extern RGLSYMGLMULTITEXCOORD3BVOESPROC __rglgen_glMultiTexCoord3bvOES;
extern RGLSYMGLMULTITEXCOORD4BOESPROC __rglgen_glMultiTexCoord4bOES;
extern RGLSYMGLMULTITEXCOORD4BVOESPROC __rglgen_glMultiTexCoord4bvOES;
extern RGLSYMGLTEXCOORD1BOESPROC __rglgen_glTexCoord1bOES;
extern RGLSYMGLTEXCOORD1BVOESPROC __rglgen_glTexCoord1bvOES;
extern RGLSYMGLTEXCOORD2BOESPROC __rglgen_glTexCoord2bOES;
extern RGLSYMGLTEXCOORD2BVOESPROC __rglgen_glTexCoord2bvOES;
extern RGLSYMGLTEXCOORD3BOESPROC __rglgen_glTexCoord3bOES;
extern RGLSYMGLTEXCOORD3BVOESPROC __rglgen_glTexCoord3bvOES;
extern RGLSYMGLTEXCOORD4BOESPROC __rglgen_glTexCoord4bOES;
extern RGLSYMGLTEXCOORD4BVOESPROC __rglgen_glTexCoord4bvOES;
extern RGLSYMGLVERTEX2BOESPROC __rglgen_glVertex2bOES;
extern RGLSYMGLVERTEX2BVOESPROC __rglgen_glVertex2bvOES;
extern RGLSYMGLVERTEX3BOESPROC __rglgen_glVertex3bOES;
extern RGLSYMGLVERTEX3BVOESPROC __rglgen_glVertex3bvOES;
extern RGLSYMGLVERTEX4BOESPROC __rglgen_glVertex4bOES;
extern RGLSYMGLVERTEX4BVOESPROC __rglgen_glVertex4bvOES;
extern RGLSYMGLACCUMXOESPROC __rglgen_glAccumxOES;
extern RGLSYMGLALPHAFUNCXOESPROC __rglgen_glAlphaFuncxOES;
extern RGLSYMGLBITMAPXOESPROC __rglgen_glBitmapxOES;
extern RGLSYMGLBLENDCOLORXOESPROC __rglgen_glBlendColorxOES;
extern RGLSYMGLCLEARACCUMXOESPROC __rglgen_glClearAccumxOES;
extern RGLSYMGLCLEARCOLORXOESPROC __rglgen_glClearColorxOES;
extern RGLSYMGLCLEARDEPTHXOESPROC __rglgen_glClearDepthxOES;
extern RGLSYMGLCLIPPLANEXOESPROC __rglgen_glClipPlanexOES;
extern RGLSYMGLCOLOR3XOESPROC __rglgen_glColor3xOES;
extern RGLSYMGLCOLOR4XOESPROC __rglgen_glColor4xOES;
extern RGLSYMGLCOLOR3XVOESPROC __rglgen_glColor3xvOES;
extern RGLSYMGLCOLOR4XVOESPROC __rglgen_glColor4xvOES;
extern RGLSYMGLCONVOLUTIONPARAMETERXOESPROC __rglgen_glConvolutionParameterxOES;
extern RGLSYMGLCONVOLUTIONPARAMETERXVOESPROC __rglgen_glConvolutionParameterxvOES;
extern RGLSYMGLDEPTHRANGEXOESPROC __rglgen_glDepthRangexOES;
extern RGLSYMGLEVALCOORD1XOESPROC __rglgen_glEvalCoord1xOES;
extern RGLSYMGLEVALCOORD2XOESPROC __rglgen_glEvalCoord2xOES;
extern RGLSYMGLEVALCOORD1XVOESPROC __rglgen_glEvalCoord1xvOES;
extern RGLSYMGLEVALCOORD2XVOESPROC __rglgen_glEvalCoord2xvOES;
extern RGLSYMGLFEEDBACKBUFFERXOESPROC __rglgen_glFeedbackBufferxOES;
extern RGLSYMGLFOGXOESPROC __rglgen_glFogxOES;
extern RGLSYMGLFOGXVOESPROC __rglgen_glFogxvOES;
extern RGLSYMGLFRUSTUMXOESPROC __rglgen_glFrustumxOES;
extern RGLSYMGLGETCLIPPLANEXOESPROC __rglgen_glGetClipPlanexOES;
extern RGLSYMGLGETCONVOLUTIONPARAMETERXVOESPROC __rglgen_glGetConvolutionParameterxvOES;
extern RGLSYMGLGETFIXEDVOESPROC __rglgen_glGetFixedvOES;
extern RGLSYMGLGETHISTOGRAMPARAMETERXVOESPROC __rglgen_glGetHistogramParameterxvOES;
extern RGLSYMGLGETLIGHTXOESPROC __rglgen_glGetLightxOES;
extern RGLSYMGLGETMAPXVOESPROC __rglgen_glGetMapxvOES;
extern RGLSYMGLGETMATERIALXOESPROC __rglgen_glGetMaterialxOES;
extern RGLSYMGLGETPIXELMAPXVPROC __rglgen_glGetPixelMapxv;
extern RGLSYMGLGETTEXENVXVOESPROC __rglgen_glGetTexEnvxvOES;
extern RGLSYMGLGETTEXGENXVOESPROC __rglgen_glGetTexGenxvOES;
extern RGLSYMGLGETTEXLEVELPARAMETERXVOESPROC __rglgen_glGetTexLevelParameterxvOES;
extern RGLSYMGLGETTEXPARAMETERXVOESPROC __rglgen_glGetTexParameterxvOES;
extern RGLSYMGLINDEXXOESPROC __rglgen_glIndexxOES;
extern RGLSYMGLINDEXXVOESPROC __rglgen_glIndexxvOES;
extern RGLSYMGLLIGHTMODELXOESPROC __rglgen_glLightModelxOES;
extern RGLSYMGLLIGHTMODELXVOESPROC __rglgen_glLightModelxvOES;
extern RGLSYMGLLIGHTXOESPROC __rglgen_glLightxOES;
extern RGLSYMGLLIGHTXVOESPROC __rglgen_glLightxvOES;
extern RGLSYMGLLINEWIDTHXOESPROC __rglgen_glLineWidthxOES;
extern RGLSYMGLLOADMATRIXXOESPROC __rglgen_glLoadMatrixxOES;
extern RGLSYMGLLOADTRANSPOSEMATRIXXOESPROC __rglgen_glLoadTransposeMatrixxOES;
extern RGLSYMGLMAP1XOESPROC __rglgen_glMap1xOES;
extern RGLSYMGLMAP2XOESPROC __rglgen_glMap2xOES;
extern RGLSYMGLMAPGRID1XOESPROC __rglgen_glMapGrid1xOES;
extern RGLSYMGLMAPGRID2XOESPROC __rglgen_glMapGrid2xOES;
extern RGLSYMGLMATERIALXOESPROC __rglgen_glMaterialxOES;
extern RGLSYMGLMATERIALXVOESPROC __rglgen_glMaterialxvOES;
extern RGLSYMGLMULTMATRIXXOESPROC __rglgen_glMultMatrixxOES;
extern RGLSYMGLMULTTRANSPOSEMATRIXXOESPROC __rglgen_glMultTransposeMatrixxOES;
extern RGLSYMGLMULTITEXCOORD1XOESPROC __rglgen_glMultiTexCoord1xOES;
extern RGLSYMGLMULTITEXCOORD2XOESPROC __rglgen_glMultiTexCoord2xOES;
extern RGLSYMGLMULTITEXCOORD3XOESPROC __rglgen_glMultiTexCoord3xOES;
extern RGLSYMGLMULTITEXCOORD4XOESPROC __rglgen_glMultiTexCoord4xOES;
extern RGLSYMGLMULTITEXCOORD1XVOESPROC __rglgen_glMultiTexCoord1xvOES;
extern RGLSYMGLMULTITEXCOORD2XVOESPROC __rglgen_glMultiTexCoord2xvOES;
extern RGLSYMGLMULTITEXCOORD3XVOESPROC __rglgen_glMultiTexCoord3xvOES;
extern RGLSYMGLMULTITEXCOORD4XVOESPROC __rglgen_glMultiTexCoord4xvOES;
extern RGLSYMGLNORMAL3XOESPROC __rglgen_glNormal3xOES;
extern RGLSYMGLNORMAL3XVOESPROC __rglgen_glNormal3xvOES;
extern RGLSYMGLORTHOXOESPROC __rglgen_glOrthoxOES;
extern RGLSYMGLPASSTHROUGHXOESPROC __rglgen_glPassThroughxOES;
extern RGLSYMGLPIXELMAPXPROC __rglgen_glPixelMapx;
extern RGLSYMGLPIXELSTOREXPROC __rglgen_glPixelStorex;
extern RGLSYMGLPIXELTRANSFERXOESPROC __rglgen_glPixelTransferxOES;
extern RGLSYMGLPIXELZOOMXOESPROC __rglgen_glPixelZoomxOES;
extern RGLSYMGLPOINTPARAMETERXVOESPROC __rglgen_glPointParameterxvOES;
extern RGLSYMGLPOINTSIZEXOESPROC __rglgen_glPointSizexOES;
extern RGLSYMGLPOLYGONOFFSETXOESPROC __rglgen_glPolygonOffsetxOES;
extern RGLSYMGLPRIORITIZETEXTURESXOESPROC __rglgen_glPrioritizeTexturesxOES;
extern RGLSYMGLRASTERPOS2XOESPROC __rglgen_glRasterPos2xOES;
extern RGLSYMGLRASTERPOS3XOESPROC __rglgen_glRasterPos3xOES;
extern RGLSYMGLRASTERPOS4XOESPROC __rglgen_glRasterPos4xOES;
extern RGLSYMGLRASTERPOS2XVOESPROC __rglgen_glRasterPos2xvOES;
extern RGLSYMGLRASTERPOS3XVOESPROC __rglgen_glRasterPos3xvOES;
extern RGLSYMGLRASTERPOS4XVOESPROC __rglgen_glRasterPos4xvOES;
extern RGLSYMGLRECTXOESPROC __rglgen_glRectxOES;
extern RGLSYMGLRECTXVOESPROC __rglgen_glRectxvOES;
extern RGLSYMGLROTATEXOESPROC __rglgen_glRotatexOES;
extern RGLSYMGLSAMPLECOVERAGEOESPROC __rglgen_glSampleCoverageOES;
extern RGLSYMGLSCALEXOESPROC __rglgen_glScalexOES;
extern RGLSYMGLTEXCOORD1XOESPROC __rglgen_glTexCoord1xOES;
extern RGLSYMGLTEXCOORD2XOESPROC __rglgen_glTexCoord2xOES;
extern RGLSYMGLTEXCOORD3XOESPROC __rglgen_glTexCoord3xOES;
extern RGLSYMGLTEXCOORD4XOESPROC __rglgen_glTexCoord4xOES;
extern RGLSYMGLTEXCOORD1XVOESPROC __rglgen_glTexCoord1xvOES;
extern RGLSYMGLTEXCOORD2XVOESPROC __rglgen_glTexCoord2xvOES;
extern RGLSYMGLTEXCOORD3XVOESPROC __rglgen_glTexCoord3xvOES;
extern RGLSYMGLTEXCOORD4XVOESPROC __rglgen_glTexCoord4xvOES;
extern RGLSYMGLTEXENVXOESPROC __rglgen_glTexEnvxOES;
extern RGLSYMGLTEXENVXVOESPROC __rglgen_glTexEnvxvOES;
extern RGLSYMGLTEXGENXOESPROC __rglgen_glTexGenxOES;
extern RGLSYMGLTEXGENXVOESPROC __rglgen_glTexGenxvOES;
extern RGLSYMGLTEXPARAMETERXOESPROC __rglgen_glTexParameterxOES;
extern RGLSYMGLTEXPARAMETERXVOESPROC __rglgen_glTexParameterxvOES;
extern RGLSYMGLTRANSLATEXOESPROC __rglgen_glTranslatexOES;
extern RGLSYMGLVERTEX2XOESPROC __rglgen_glVertex2xOES;
extern RGLSYMGLVERTEX3XOESPROC __rglgen_glVertex3xOES;
extern RGLSYMGLVERTEX4XOESPROC __rglgen_glVertex4xOES;
extern RGLSYMGLVERTEX2XVOESPROC __rglgen_glVertex2xvOES;
extern RGLSYMGLVERTEX3XVOESPROC __rglgen_glVertex3xvOES;
extern RGLSYMGLVERTEX4XVOESPROC __rglgen_glVertex4xvOES;
extern RGLSYMGLDEPTHRANGEFOESPROC __rglgen_glDepthRangefOES;
extern RGLSYMGLFRUSTUMFOESPROC __rglgen_glFrustumfOES;
extern RGLSYMGLORTHOFOESPROC __rglgen_glOrthofOES;
extern RGLSYMGLCLIPPLANEFOESPROC __rglgen_glClipPlanefOES;
extern RGLSYMGLCLEARDEPTHFOESPROC __rglgen_glClearDepthfOES;
extern RGLSYMGLGETCLIPPLANEFOESPROC __rglgen_glGetClipPlanefOES;
extern RGLSYMGLQUERYMATRIXXOESPROC __rglgen_glQueryMatrixxOES;

struct rglgen_sym_map { const char *sym; void *ptr; };
extern const struct rglgen_sym_map rglgen_symbol_map[];
#endif
