#ifndef __NX_GLSYM_H__
#define __NX_GLSYM_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void *GLeglImageOES;
typedef void (APIENTRYP RGLSYMGLCLEARINDEXPROC) ( GLfloat c );
typedef void (APIENTRYP RGLSYMGLCLEARCOLORPROC) ( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
typedef void (APIENTRYP RGLSYMGLCLEARPROC) ( GLbitfield mask );
typedef void (APIENTRYP RGLSYMGLINDEXMASKPROC) ( GLuint mask );
typedef void (APIENTRYP RGLSYMGLCOLORMASKPROC) ( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha );
typedef void (APIENTRYP RGLSYMGLALPHAFUNCPROC) ( GLenum func, GLclampf ref );
typedef void (APIENTRYP RGLSYMGLBLENDFUNCPROC) ( GLenum sfactor, GLenum dfactor );
typedef void (APIENTRYP RGLSYMGLLOGICOPPROC) ( GLenum opcode );
typedef void (APIENTRYP RGLSYMGLCULLFACEPROC) ( GLenum mode );
typedef void (APIENTRYP RGLSYMGLFRONTFACEPROC) ( GLenum mode );
typedef void (APIENTRYP RGLSYMGLPOINTSIZEPROC) ( GLfloat size );
typedef void (APIENTRYP RGLSYMGLLINEWIDTHPROC) ( GLfloat width );
typedef void (APIENTRYP RGLSYMGLLINESTIPPLEPROC) ( GLint factor, GLushort pattern );
typedef void (APIENTRYP RGLSYMGLPOLYGONMODEPROC) ( GLenum face, GLenum mode );
typedef void (APIENTRYP RGLSYMGLPOLYGONOFFSETPROC) ( GLfloat factor, GLfloat units );
typedef void (APIENTRYP RGLSYMGLPOLYGONSTIPPLEPROC) ( const GLubyte *mask );
typedef void (APIENTRYP RGLSYMGLGETPOLYGONSTIPPLEPROC) ( GLubyte *mask );
typedef void (APIENTRYP RGLSYMGLEDGEFLAGPROC) ( GLboolean flag );
typedef void (APIENTRYP RGLSYMGLEDGEFLAGVPROC) ( const GLboolean *flag );
typedef void (APIENTRYP RGLSYMGLSCISSORPROC) ( GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP RGLSYMGLCLIPPLANEPROC) ( GLenum plane, const GLdouble *equation );
typedef void (APIENTRYP RGLSYMGLGETCLIPPLANEPROC) ( GLenum plane, GLdouble *equation );
typedef void (APIENTRYP RGLSYMGLDRAWBUFFERPROC) ( GLenum mode );
typedef void (APIENTRYP RGLSYMGLREADBUFFERPROC) ( GLenum mode );
typedef void (APIENTRYP RGLSYMGLENABLEPROC) ( GLenum cap );
typedef void (APIENTRYP RGLSYMGLDISABLEPROC) ( GLenum cap );
typedef GLboolean (APIENTRYP RGLSYMGLISENABLEDPROC) ( GLenum cap );
typedef void (APIENTRYP RGLSYMGLENABLECLIENTSTATEPROC) ( GLenum cap );
typedef void (APIENTRYP RGLSYMGLDISABLECLIENTSTATEPROC) ( GLenum cap );
typedef void (APIENTRYP RGLSYMGLGETBOOLEANVPROC) ( GLenum pname, GLboolean *params );
typedef void (APIENTRYP RGLSYMGLGETDOUBLEVPROC) ( GLenum pname, GLdouble *params );
typedef void (APIENTRYP RGLSYMGLGETFLOATVPROC) ( GLenum pname, GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETINTEGERVPROC) ( GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLPUSHATTRIBPROC) ( GLbitfield mask );
typedef void (APIENTRYP RGLSYMGLPOPATTRIBPROC) ( void );
typedef void (APIENTRYP RGLSYMGLPUSHCLIENTATTRIBPROC) ( GLbitfield mask );
typedef void (APIENTRYP RGLSYMGLPOPCLIENTATTRIBPROC) ( void );
typedef GLint (APIENTRYP RGLSYMGLRENDERMODEPROC) ( GLenum mode );
typedef GLenum (APIENTRYP RGLSYMGLGETERRORPROC) ( void );
typedef const GLubyte * (APIENTRYP RGLSYMGLGETSTRINGPROC) ( GLenum name );
typedef void (APIENTRYP RGLSYMGLFINISHPROC) ( void );
typedef void (APIENTRYP RGLSYMGLFLUSHPROC) ( void );
typedef void (APIENTRYP RGLSYMGLHINTPROC) ( GLenum target, GLenum mode );
typedef void (APIENTRYP RGLSYMGLCLEARDEPTHPROC) ( GLclampd depth );
typedef void (APIENTRYP RGLSYMGLDEPTHFUNCPROC) ( GLenum func );
typedef void (APIENTRYP RGLSYMGLDEPTHMASKPROC) ( GLboolean flag );
typedef void (APIENTRYP RGLSYMGLDEPTHRANGEPROC) ( GLclampd near_val, GLclampd far_val );
typedef void (APIENTRYP RGLSYMGLCLEARACCUMPROC) ( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
typedef void (APIENTRYP RGLSYMGLACCUMPROC) ( GLenum op, GLfloat value );
typedef void (APIENTRYP RGLSYMGLMATRIXMODEPROC) ( GLenum mode );
typedef void (APIENTRYP RGLSYMGLORTHOPROC) ( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
typedef void (APIENTRYP RGLSYMGLFRUSTUMPROC) ( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
typedef void (APIENTRYP RGLSYMGLVIEWPORTPROC) ( GLint x, GLint y, GLsizei width, GLsizei height );
typedef void (APIENTRYP RGLSYMGLPUSHMATRIXPROC) ( void );
typedef void (APIENTRYP RGLSYMGLPOPMATRIXPROC) ( void );
typedef void (APIENTRYP RGLSYMGLLOADIDENTITYPROC) ( void );
typedef void (APIENTRYP RGLSYMGLLOADMATRIXDPROC) ( const GLdouble *m );
typedef void (APIENTRYP RGLSYMGLLOADMATRIXFPROC) ( const GLfloat *m );
typedef void (APIENTRYP RGLSYMGLMULTMATRIXDPROC) ( const GLdouble *m );
typedef void (APIENTRYP RGLSYMGLMULTMATRIXFPROC) ( const GLfloat *m );
typedef void (APIENTRYP RGLSYMGLROTATEDPROC) ( GLdouble angle, GLdouble x, GLdouble y, GLdouble z );
typedef void (APIENTRYP RGLSYMGLROTATEFPROC) ( GLfloat angle, GLfloat x, GLfloat y, GLfloat z );
typedef void (APIENTRYP RGLSYMGLSCALEDPROC) ( GLdouble x, GLdouble y, GLdouble z );
typedef void (APIENTRYP RGLSYMGLSCALEFPROC) ( GLfloat x, GLfloat y, GLfloat z );
typedef void (APIENTRYP RGLSYMGLTRANSLATEDPROC) ( GLdouble x, GLdouble y, GLdouble z );
typedef void (APIENTRYP RGLSYMGLTRANSLATEFPROC) ( GLfloat x, GLfloat y, GLfloat z );
typedef GLboolean (APIENTRYP RGLSYMGLISLISTPROC) ( GLuint list );
typedef void (APIENTRYP RGLSYMGLDELETELISTSPROC) ( GLuint list, GLsizei range );
typedef GLuint (APIENTRYP RGLSYMGLGENLISTSPROC) ( GLsizei range );
typedef void (APIENTRYP RGLSYMGLNEWLISTPROC) ( GLuint list, GLenum mode );
typedef void (APIENTRYP RGLSYMGLENDLISTPROC) ( void );
typedef void (APIENTRYP RGLSYMGLCALLLISTPROC) ( GLuint list );
typedef void (APIENTRYP RGLSYMGLCALLLISTSPROC) ( GLsizei n, GLenum type, const GLvoid *lists );
typedef void (APIENTRYP RGLSYMGLLISTBASEPROC) ( GLuint base );
typedef void (APIENTRYP RGLSYMGLBEGINPROC) ( GLenum mode );
typedef void (APIENTRYP RGLSYMGLENDPROC) ( void );
typedef void (APIENTRYP RGLSYMGLVERTEX2DPROC) ( GLdouble x, GLdouble y );
typedef void (APIENTRYP RGLSYMGLVERTEX2FPROC) ( GLfloat x, GLfloat y );
typedef void (APIENTRYP RGLSYMGLVERTEX2IPROC) ( GLint x, GLint y );
typedef void (APIENTRYP RGLSYMGLVERTEX2SPROC) ( GLshort x, GLshort y );
typedef void (APIENTRYP RGLSYMGLVERTEX3DPROC) ( GLdouble x, GLdouble y, GLdouble z );
typedef void (APIENTRYP RGLSYMGLVERTEX3FPROC) ( GLfloat x, GLfloat y, GLfloat z );
typedef void (APIENTRYP RGLSYMGLVERTEX3IPROC) ( GLint x, GLint y, GLint z );
typedef void (APIENTRYP RGLSYMGLVERTEX3SPROC) ( GLshort x, GLshort y, GLshort z );
typedef void (APIENTRYP RGLSYMGLVERTEX4DPROC) ( GLdouble x, GLdouble y, GLdouble z, GLdouble w );
typedef void (APIENTRYP RGLSYMGLVERTEX4FPROC) ( GLfloat x, GLfloat y, GLfloat z, GLfloat w );
typedef void (APIENTRYP RGLSYMGLVERTEX4IPROC) ( GLint x, GLint y, GLint z, GLint w );
typedef void (APIENTRYP RGLSYMGLVERTEX4SPROC) ( GLshort x, GLshort y, GLshort z, GLshort w );
typedef void (APIENTRYP RGLSYMGLVERTEX2DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLVERTEX2FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLVERTEX2IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLVERTEX2SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLVERTEX3DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLVERTEX3FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLVERTEX3IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLVERTEX3SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLVERTEX4DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLVERTEX4FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLVERTEX4IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLVERTEX4SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLNORMAL3BPROC) ( GLbyte nx, GLbyte ny, GLbyte nz );
typedef void (APIENTRYP RGLSYMGLNORMAL3DPROC) ( GLdouble nx, GLdouble ny, GLdouble nz );
typedef void (APIENTRYP RGLSYMGLNORMAL3FPROC) ( GLfloat nx, GLfloat ny, GLfloat nz );
typedef void (APIENTRYP RGLSYMGLNORMAL3IPROC) ( GLint nx, GLint ny, GLint nz );
typedef void (APIENTRYP RGLSYMGLNORMAL3SPROC) ( GLshort nx, GLshort ny, GLshort nz );
typedef void (APIENTRYP RGLSYMGLNORMAL3BVPROC) ( const GLbyte *v );
typedef void (APIENTRYP RGLSYMGLNORMAL3DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLNORMAL3FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLNORMAL3IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLNORMAL3SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLINDEXDPROC) ( GLdouble c );
typedef void (APIENTRYP RGLSYMGLINDEXFPROC) ( GLfloat c );
typedef void (APIENTRYP RGLSYMGLINDEXIPROC) ( GLint c );
typedef void (APIENTRYP RGLSYMGLINDEXSPROC) ( GLshort c );
typedef void (APIENTRYP RGLSYMGLINDEXUBPROC) ( GLubyte c );
typedef void (APIENTRYP RGLSYMGLINDEXDVPROC) ( const GLdouble *c );
typedef void (APIENTRYP RGLSYMGLINDEXFVPROC) ( const GLfloat *c );
typedef void (APIENTRYP RGLSYMGLINDEXIVPROC) ( const GLint *c );
typedef void (APIENTRYP RGLSYMGLINDEXSVPROC) ( const GLshort *c );
typedef void (APIENTRYP RGLSYMGLINDEXUBVPROC) ( const GLubyte *c );
typedef void (APIENTRYP RGLSYMGLCOLOR3BPROC) ( GLbyte red, GLbyte green, GLbyte blue );
typedef void (APIENTRYP RGLSYMGLCOLOR3DPROC) ( GLdouble red, GLdouble green, GLdouble blue );
typedef void (APIENTRYP RGLSYMGLCOLOR3FPROC) ( GLfloat red, GLfloat green, GLfloat blue );
typedef void (APIENTRYP RGLSYMGLCOLOR3IPROC) ( GLint red, GLint green, GLint blue );
typedef void (APIENTRYP RGLSYMGLCOLOR3SPROC) ( GLshort red, GLshort green, GLshort blue );
typedef void (APIENTRYP RGLSYMGLCOLOR3UBPROC) ( GLubyte red, GLubyte green, GLubyte blue );
typedef void (APIENTRYP RGLSYMGLCOLOR3UIPROC) ( GLuint red, GLuint green, GLuint blue );
typedef void (APIENTRYP RGLSYMGLCOLOR3USPROC) ( GLushort red, GLushort green, GLushort blue );
typedef void (APIENTRYP RGLSYMGLCOLOR4BPROC) ( GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha );
typedef void (APIENTRYP RGLSYMGLCOLOR4DPROC) ( GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha );
typedef void (APIENTRYP RGLSYMGLCOLOR4FPROC) ( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
typedef void (APIENTRYP RGLSYMGLCOLOR4IPROC) ( GLint red, GLint green, GLint blue, GLint alpha );
typedef void (APIENTRYP RGLSYMGLCOLOR4SPROC) ( GLshort red, GLshort green, GLshort blue, GLshort alpha );
typedef void (APIENTRYP RGLSYMGLCOLOR4UBPROC) ( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
typedef void (APIENTRYP RGLSYMGLCOLOR4UIPROC) ( GLuint red, GLuint green, GLuint blue, GLuint alpha );
typedef void (APIENTRYP RGLSYMGLCOLOR4USPROC) ( GLushort red, GLushort green, GLushort blue, GLushort alpha );
typedef void (APIENTRYP RGLSYMGLCOLOR3BVPROC) ( const GLbyte *v );
typedef void (APIENTRYP RGLSYMGLCOLOR3DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLCOLOR3FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLCOLOR3IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLCOLOR3SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLCOLOR3UBVPROC) ( const GLubyte *v );
typedef void (APIENTRYP RGLSYMGLCOLOR3UIVPROC) ( const GLuint *v );
typedef void (APIENTRYP RGLSYMGLCOLOR3USVPROC) ( const GLushort *v );
typedef void (APIENTRYP RGLSYMGLCOLOR4BVPROC) ( const GLbyte *v );
typedef void (APIENTRYP RGLSYMGLCOLOR4DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLCOLOR4FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLCOLOR4IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLCOLOR4SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLCOLOR4UBVPROC) ( const GLubyte *v );
typedef void (APIENTRYP RGLSYMGLCOLOR4UIVPROC) ( const GLuint *v );
typedef void (APIENTRYP RGLSYMGLCOLOR4USVPROC) ( const GLushort *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD1DPROC) ( GLdouble s );
typedef void (APIENTRYP RGLSYMGLTEXCOORD1FPROC) ( GLfloat s );
typedef void (APIENTRYP RGLSYMGLTEXCOORD1IPROC) ( GLint s );
typedef void (APIENTRYP RGLSYMGLTEXCOORD1SPROC) ( GLshort s );
typedef void (APIENTRYP RGLSYMGLTEXCOORD2DPROC) ( GLdouble s, GLdouble t );
typedef void (APIENTRYP RGLSYMGLTEXCOORD2FPROC) ( GLfloat s, GLfloat t );
typedef void (APIENTRYP RGLSYMGLTEXCOORD2IPROC) ( GLint s, GLint t );
typedef void (APIENTRYP RGLSYMGLTEXCOORD2SPROC) ( GLshort s, GLshort t );
typedef void (APIENTRYP RGLSYMGLTEXCOORD3DPROC) ( GLdouble s, GLdouble t, GLdouble r );
typedef void (APIENTRYP RGLSYMGLTEXCOORD3FPROC) ( GLfloat s, GLfloat t, GLfloat r );
typedef void (APIENTRYP RGLSYMGLTEXCOORD3IPROC) ( GLint s, GLint t, GLint r );
typedef void (APIENTRYP RGLSYMGLTEXCOORD3SPROC) ( GLshort s, GLshort t, GLshort r );
typedef void (APIENTRYP RGLSYMGLTEXCOORD4DPROC) ( GLdouble s, GLdouble t, GLdouble r, GLdouble q );
typedef void (APIENTRYP RGLSYMGLTEXCOORD4FPROC) ( GLfloat s, GLfloat t, GLfloat r, GLfloat q );
typedef void (APIENTRYP RGLSYMGLTEXCOORD4IPROC) ( GLint s, GLint t, GLint r, GLint q );
typedef void (APIENTRYP RGLSYMGLTEXCOORD4SPROC) ( GLshort s, GLshort t, GLshort r, GLshort q );
typedef void (APIENTRYP RGLSYMGLTEXCOORD1DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD1FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD1IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD1SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD2DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD2FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD2IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD2SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD3DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD3FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD3IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD3SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD4DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD4FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD4IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLTEXCOORD4SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS2DPROC) ( GLdouble x, GLdouble y );
typedef void (APIENTRYP RGLSYMGLRASTERPOS2FPROC) ( GLfloat x, GLfloat y );
typedef void (APIENTRYP RGLSYMGLRASTERPOS2IPROC) ( GLint x, GLint y );
typedef void (APIENTRYP RGLSYMGLRASTERPOS2SPROC) ( GLshort x, GLshort y );
typedef void (APIENTRYP RGLSYMGLRASTERPOS3DPROC) ( GLdouble x, GLdouble y, GLdouble z );
typedef void (APIENTRYP RGLSYMGLRASTERPOS3FPROC) ( GLfloat x, GLfloat y, GLfloat z );
typedef void (APIENTRYP RGLSYMGLRASTERPOS3IPROC) ( GLint x, GLint y, GLint z );
typedef void (APIENTRYP RGLSYMGLRASTERPOS3SPROC) ( GLshort x, GLshort y, GLshort z );
typedef void (APIENTRYP RGLSYMGLRASTERPOS4DPROC) ( GLdouble x, GLdouble y, GLdouble z, GLdouble w );
typedef void (APIENTRYP RGLSYMGLRASTERPOS4FPROC) ( GLfloat x, GLfloat y, GLfloat z, GLfloat w );
typedef void (APIENTRYP RGLSYMGLRASTERPOS4IPROC) ( GLint x, GLint y, GLint z, GLint w );
typedef void (APIENTRYP RGLSYMGLRASTERPOS4SPROC) ( GLshort x, GLshort y, GLshort z, GLshort w );
typedef void (APIENTRYP RGLSYMGLRASTERPOS2DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS2FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS2IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS2SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS3DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS3FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS3IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS3SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS4DVPROC) ( const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS4FVPROC) ( const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS4IVPROC) ( const GLint *v );
typedef void (APIENTRYP RGLSYMGLRASTERPOS4SVPROC) ( const GLshort *v );
typedef void (APIENTRYP RGLSYMGLRECTDPROC) ( GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2 );
typedef void (APIENTRYP RGLSYMGLRECTFPROC) ( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 );
typedef void (APIENTRYP RGLSYMGLRECTIPROC) ( GLint x1, GLint y1, GLint x2, GLint y2 );
typedef void (APIENTRYP RGLSYMGLRECTSPROC) ( GLshort x1, GLshort y1, GLshort x2, GLshort y2 );
typedef void (APIENTRYP RGLSYMGLRECTDVPROC) ( const GLdouble *v1, const GLdouble *v2 );
typedef void (APIENTRYP RGLSYMGLRECTFVPROC) ( const GLfloat *v1, const GLfloat *v2 );
typedef void (APIENTRYP RGLSYMGLRECTIVPROC) ( const GLint *v1, const GLint *v2 );
typedef void (APIENTRYP RGLSYMGLRECTSVPROC) ( const GLshort *v1, const GLshort *v2 );
typedef void (APIENTRYP RGLSYMGLVERTEXPOINTERPROC) ( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (APIENTRYP RGLSYMGLNORMALPOINTERPROC) ( GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (APIENTRYP RGLSYMGLCOLORPOINTERPROC) ( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (APIENTRYP RGLSYMGLINDEXPOINTERPROC) ( GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (APIENTRYP RGLSYMGLTEXCOORDPOINTERPROC) ( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr );
typedef void (APIENTRYP RGLSYMGLEDGEFLAGPOINTERPROC) ( GLsizei stride, const GLvoid *ptr );
typedef void (APIENTRYP RGLSYMGLGETPOINTERVPROC) ( GLenum pname, GLvoid **params );
typedef void (APIENTRYP RGLSYMGLARRAYELEMENTPROC) ( GLint i );
typedef void (APIENTRYP RGLSYMGLDRAWARRAYSPROC) ( GLenum mode, GLint first, GLsizei count );
typedef void (APIENTRYP RGLSYMGLDRAWELEMENTSPROC) ( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices );
typedef void (APIENTRYP RGLSYMGLINTERLEAVEDARRAYSPROC) ( GLenum format, GLsizei stride, const GLvoid *pointer );
typedef void (APIENTRYP RGLSYMGLSHADEMODELPROC) ( GLenum mode );
typedef void (APIENTRYP RGLSYMGLLIGHTFPROC) ( GLenum light, GLenum pname, GLfloat param );
typedef void (APIENTRYP RGLSYMGLLIGHTIPROC) ( GLenum light, GLenum pname, GLint param );
typedef void (APIENTRYP RGLSYMGLLIGHTFVPROC) ( GLenum light, GLenum pname, const GLfloat *params );
typedef void (APIENTRYP RGLSYMGLLIGHTIVPROC) ( GLenum light, GLenum pname, const GLint *params );
typedef void (APIENTRYP RGLSYMGLGETLIGHTFVPROC) ( GLenum light, GLenum pname, GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETLIGHTIVPROC) ( GLenum light, GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLLIGHTMODELFPROC) ( GLenum pname, GLfloat param );
typedef void (APIENTRYP RGLSYMGLLIGHTMODELIPROC) ( GLenum pname, GLint param );
typedef void (APIENTRYP RGLSYMGLLIGHTMODELFVPROC) ( GLenum pname, const GLfloat *params );
typedef void (APIENTRYP RGLSYMGLLIGHTMODELIVPROC) ( GLenum pname, const GLint *params );
typedef void (APIENTRYP RGLSYMGLMATERIALFPROC) ( GLenum face, GLenum pname, GLfloat param );
typedef void (APIENTRYP RGLSYMGLMATERIALIPROC) ( GLenum face, GLenum pname, GLint param );
typedef void (APIENTRYP RGLSYMGLMATERIALFVPROC) ( GLenum face, GLenum pname, const GLfloat *params );
typedef void (APIENTRYP RGLSYMGLMATERIALIVPROC) ( GLenum face, GLenum pname, const GLint *params );
typedef void (APIENTRYP RGLSYMGLGETMATERIALFVPROC) ( GLenum face, GLenum pname, GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETMATERIALIVPROC) ( GLenum face, GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLCOLORMATERIALPROC) ( GLenum face, GLenum mode );
typedef void (APIENTRYP RGLSYMGLPIXELZOOMPROC) ( GLfloat xfactor, GLfloat yfactor );
typedef void (APIENTRYP RGLSYMGLPIXELSTOREFPROC) ( GLenum pname, GLfloat param );
typedef void (APIENTRYP RGLSYMGLPIXELSTOREIPROC) ( GLenum pname, GLint param );
typedef void (APIENTRYP RGLSYMGLPIXELTRANSFERFPROC) ( GLenum pname, GLfloat param );
typedef void (APIENTRYP RGLSYMGLPIXELTRANSFERIPROC) ( GLenum pname, GLint param );
typedef void (APIENTRYP RGLSYMGLPIXELMAPFVPROC) ( GLenum map, GLsizei mapsize, const GLfloat *values );
typedef void (APIENTRYP RGLSYMGLPIXELMAPUIVPROC) ( GLenum map, GLsizei mapsize, const GLuint *values );
typedef void (APIENTRYP RGLSYMGLPIXELMAPUSVPROC) ( GLenum map, GLsizei mapsize, const GLushort *values );
typedef void (APIENTRYP RGLSYMGLGETPIXELMAPFVPROC) ( GLenum map, GLfloat *values );
typedef void (APIENTRYP RGLSYMGLGETPIXELMAPUIVPROC) ( GLenum map, GLuint *values );
typedef void (APIENTRYP RGLSYMGLGETPIXELMAPUSVPROC) ( GLenum map, GLushort *values );
typedef void (APIENTRYP RGLSYMGLBITMAPPROC) ( GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap );
typedef void (APIENTRYP RGLSYMGLREADPIXELSPROC) ( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels );
typedef void (APIENTRYP RGLSYMGLDRAWPIXELSPROC) ( GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (APIENTRYP RGLSYMGLCOPYPIXELSPROC) ( GLint x, GLint y, GLsizei width, GLsizei height, GLenum type );
typedef void (APIENTRYP RGLSYMGLSTENCILFUNCPROC) ( GLenum func, GLint ref, GLuint mask );
typedef void (APIENTRYP RGLSYMGLSTENCILMASKPROC) ( GLuint mask );
typedef void (APIENTRYP RGLSYMGLSTENCILOPPROC) ( GLenum fail, GLenum zfail, GLenum zpass );
typedef void (APIENTRYP RGLSYMGLCLEARSTENCILPROC) ( GLint s );
typedef void (APIENTRYP RGLSYMGLTEXGENDPROC) ( GLenum coord, GLenum pname, GLdouble param );
typedef void (APIENTRYP RGLSYMGLTEXGENFPROC) ( GLenum coord, GLenum pname, GLfloat param );
typedef void (APIENTRYP RGLSYMGLTEXGENIPROC) ( GLenum coord, GLenum pname, GLint param );
typedef void (APIENTRYP RGLSYMGLTEXGENDVPROC) ( GLenum coord, GLenum pname, const GLdouble *params );
typedef void (APIENTRYP RGLSYMGLTEXGENFVPROC) ( GLenum coord, GLenum pname, const GLfloat *params );
typedef void (APIENTRYP RGLSYMGLTEXGENIVPROC) ( GLenum coord, GLenum pname, const GLint *params );
typedef void (APIENTRYP RGLSYMGLGETTEXGENDVPROC) ( GLenum coord, GLenum pname, GLdouble *params );
typedef void (APIENTRYP RGLSYMGLGETTEXGENFVPROC) ( GLenum coord, GLenum pname, GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETTEXGENIVPROC) ( GLenum coord, GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLTEXENVFPROC) ( GLenum target, GLenum pname, GLfloat param );
typedef void (APIENTRYP RGLSYMGLTEXENVIPROC) ( GLenum target, GLenum pname, GLint param );
typedef void (APIENTRYP RGLSYMGLTEXENVFVPROC) ( GLenum target, GLenum pname, const GLfloat *params );
typedef void (APIENTRYP RGLSYMGLTEXENVIVPROC) ( GLenum target, GLenum pname, const GLint *params );
typedef void (APIENTRYP RGLSYMGLGETTEXENVFVPROC) ( GLenum target, GLenum pname, GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETTEXENVIVPROC) ( GLenum target, GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERFPROC) ( GLenum target, GLenum pname, GLfloat param );
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERIPROC) ( GLenum target, GLenum pname, GLint param );
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERFVPROC) ( GLenum target, GLenum pname, const GLfloat *params );
typedef void (APIENTRYP RGLSYMGLTEXPARAMETERIVPROC) ( GLenum target, GLenum pname, const GLint *params );
typedef void (APIENTRYP RGLSYMGLGETTEXPARAMETERFVPROC) ( GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRYP RGLSYMGLGETTEXPARAMETERIVPROC) ( GLenum target, GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLGETTEXLEVELPARAMETERFVPROC) ( GLenum target, GLint level, GLenum pname, GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETTEXLEVELPARAMETERIVPROC) ( GLenum target, GLint level, GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLTEXIMAGE1DPROC) ( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (APIENTRYP RGLSYMGLTEXIMAGE2DPROC) ( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (APIENTRYP RGLSYMGLGETTEXIMAGEPROC) ( GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels );
typedef void (APIENTRYP RGLSYMGLGENTEXTURESPROC) ( GLsizei n, GLuint *textures );
typedef void (APIENTRYP RGLSYMGLDELETETEXTURESPROC) ( GLsizei n, const GLuint *textures);
typedef void (APIENTRYP RGLSYMGLBINDTEXTUREPROC) ( GLenum target, GLuint texture );
typedef void (APIENTRYP RGLSYMGLPRIORITIZETEXTURESPROC) ( GLsizei n, const GLuint *textures, const GLclampf *priorities );
typedef GLboolean (APIENTRYP RGLSYMGLARETEXTURESRESIDENTPROC) ( GLsizei n, const GLuint *textures, GLboolean *residences );
typedef GLboolean (APIENTRYP RGLSYMGLISTEXTUREPROC) ( GLuint texture );
typedef void (APIENTRYP RGLSYMGLTEXSUBIMAGE1DPROC) ( GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (APIENTRYP RGLSYMGLTEXSUBIMAGE2DPROC) ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (APIENTRYP RGLSYMGLCOPYTEXIMAGE1DPROC) ( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border );
typedef void (APIENTRYP RGLSYMGLCOPYTEXIMAGE2DPROC) ( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border );
typedef void (APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE1DPROC) ( GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width );
typedef void (APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE2DPROC) ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height );
typedef void (APIENTRYP RGLSYMGLMAP1DPROC) ( GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points );
typedef void (APIENTRYP RGLSYMGLMAP1FPROC) ( GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points );
typedef void (APIENTRYP RGLSYMGLMAP2DPROC) ( GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points );
typedef void (APIENTRYP RGLSYMGLMAP2FPROC) ( GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points );
typedef void (APIENTRYP RGLSYMGLGETMAPDVPROC) ( GLenum target, GLenum query, GLdouble *v );
typedef void (APIENTRYP RGLSYMGLGETMAPFVPROC) ( GLenum target, GLenum query, GLfloat *v );
typedef void (APIENTRYP RGLSYMGLGETMAPIVPROC) ( GLenum target, GLenum query, GLint *v );
typedef void (APIENTRYP RGLSYMGLEVALCOORD1DPROC) ( GLdouble u );
typedef void (APIENTRYP RGLSYMGLEVALCOORD1FPROC) ( GLfloat u );
typedef void (APIENTRYP RGLSYMGLEVALCOORD1DVPROC) ( const GLdouble *u );
typedef void (APIENTRYP RGLSYMGLEVALCOORD1FVPROC) ( const GLfloat *u );
typedef void (APIENTRYP RGLSYMGLEVALCOORD2DPROC) ( GLdouble u, GLdouble v );
typedef void (APIENTRYP RGLSYMGLEVALCOORD2FPROC) ( GLfloat u, GLfloat v );
typedef void (APIENTRYP RGLSYMGLEVALCOORD2DVPROC) ( const GLdouble *u );
typedef void (APIENTRYP RGLSYMGLEVALCOORD2FVPROC) ( const GLfloat *u );
typedef void (APIENTRYP RGLSYMGLMAPGRID1DPROC) ( GLint un, GLdouble u1, GLdouble u2 );
typedef void (APIENTRYP RGLSYMGLMAPGRID1FPROC) ( GLint un, GLfloat u1, GLfloat u2 );
typedef void (APIENTRYP RGLSYMGLMAPGRID2DPROC) ( GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2 );
typedef void (APIENTRYP RGLSYMGLMAPGRID2FPROC) ( GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2 );
typedef void (APIENTRYP RGLSYMGLEVALPOINT1PROC) ( GLint i );
typedef void (APIENTRYP RGLSYMGLEVALPOINT2PROC) ( GLint i, GLint j );
typedef void (APIENTRYP RGLSYMGLEVALMESH1PROC) ( GLenum mode, GLint i1, GLint i2 );
typedef void (APIENTRYP RGLSYMGLEVALMESH2PROC) ( GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2 );
typedef void (APIENTRYP RGLSYMGLFOGFPROC) ( GLenum pname, GLfloat param );
typedef void (APIENTRYP RGLSYMGLFOGIPROC) ( GLenum pname, GLint param );
typedef void (APIENTRYP RGLSYMGLFOGFVPROC) ( GLenum pname, const GLfloat *params );
typedef void (APIENTRYP RGLSYMGLFOGIVPROC) ( GLenum pname, const GLint *params );
typedef void (APIENTRYP RGLSYMGLFEEDBACKBUFFERPROC) ( GLsizei size, GLenum type, GLfloat *buffer );
typedef void (APIENTRYP RGLSYMGLPASSTHROUGHPROC) ( GLfloat token );
typedef void (APIENTRYP RGLSYMGLSELECTBUFFERPROC) ( GLsizei size, GLuint *buffer );
typedef void (APIENTRYP RGLSYMGLINITNAMESPROC) ( void );
typedef void (APIENTRYP RGLSYMGLLOADNAMEPROC) ( GLuint name );
typedef void (APIENTRYP RGLSYMGLPUSHNAMEPROC) ( GLuint name );
typedef void (APIENTRYP RGLSYMGLPOPNAMEPROC) ( void );
typedef void (APIENTRYP RGLSYMGLDRAWRANGEELEMENTSPROC) ( GLenum mode, GLuint start,GLuint end, GLsizei count, GLenum type, const GLvoid *indices );
typedef void (APIENTRYP RGLSYMGLTEXIMAGE3DPROC) ( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
typedef void (APIENTRYP RGLSYMGLTEXSUBIMAGE3DPROC) ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP RGLSYMGLCOPYTEXSUBIMAGE3DPROC) ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height );
typedef void (APIENTRYP RGLSYMGLCOLORTABLEPROC) ( GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table );
typedef void (APIENTRYP RGLSYMGLCOLORSUBTABLEPROC) ( GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data );
typedef void (APIENTRYP RGLSYMGLCOLORTABLEPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRYP RGLSYMGLCOLORTABLEPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP RGLSYMGLCOPYCOLORSUBTABLEPROC) ( GLenum target, GLsizei start, GLint x, GLint y, GLsizei width );
typedef void (APIENTRYP RGLSYMGLCOPYCOLORTABLEPROC) ( GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width );
typedef void (APIENTRYP RGLSYMGLGETCOLORTABLEPROC) ( GLenum target, GLenum format, GLenum type, GLvoid *table );
typedef void (APIENTRYP RGLSYMGLGETCOLORTABLEPARAMETERFVPROC) ( GLenum target, GLenum pname, GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETCOLORTABLEPARAMETERIVPROC) ( GLenum target, GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLBLENDEQUATIONPROC) ( GLenum mode );
typedef void (APIENTRYP RGLSYMGLBLENDCOLORPROC) ( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
typedef void (APIENTRYP RGLSYMGLHISTOGRAMPROC) ( GLenum target, GLsizei width, GLenum internalformat, GLboolean sink );
typedef void (APIENTRYP RGLSYMGLRESETHISTOGRAMPROC) ( GLenum target );
typedef void (APIENTRYP RGLSYMGLGETHISTOGRAMPROC) ( GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values );
typedef void (APIENTRYP RGLSYMGLGETHISTOGRAMPARAMETERFVPROC) ( GLenum target, GLenum pname, GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETHISTOGRAMPARAMETERIVPROC) ( GLenum target, GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLMINMAXPROC) ( GLenum target, GLenum internalformat,GLboolean sink );
typedef void (APIENTRYP RGLSYMGLRESETMINMAXPROC) ( GLenum target );
typedef void (APIENTRYP RGLSYMGLGETMINMAXPROC) ( GLenum target, GLboolean reset, GLenum format, GLenum types, GLvoid *values );
typedef void (APIENTRYP RGLSYMGLGETMINMAXPARAMETERFVPROC) ( GLenum target, GLenum pname, GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETMINMAXPARAMETERIVPROC) ( GLenum target, GLenum pname, GLint *params );
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONFILTER1DPROC) ( GLenum target,GLenum internalformat, GLsizei width, GLenum format, GLenum type,const GLvoid *image );
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONFILTER2DPROC) ( GLenum target,GLenum internalformat, GLsizei width, GLsizei height, GLenum format,GLenum type, const GLvoid *image );
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERFPROC) ( GLenum target, GLenum pname,GLfloat params );
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERFVPROC) ( GLenum target, GLenum pname,const GLfloat *params );
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERIPROC) ( GLenum target, GLenum pname,GLint params );
typedef void (APIENTRYP RGLSYMGLCONVOLUTIONPARAMETERIVPROC) ( GLenum target, GLenum pname,const GLint *params );
typedef void (APIENTRYP RGLSYMGLCOPYCONVOLUTIONFILTER1DPROC) ( GLenum target,GLenum internalformat, GLint x, GLint y, GLsizei width );
typedef void (APIENTRYP RGLSYMGLCOPYCONVOLUTIONFILTER2DPROC) ( GLenum target,GLenum internalformat, GLint x, GLint y, GLsizei width,GLsizei height);
typedef void (APIENTRYP RGLSYMGLGETCONVOLUTIONFILTERPROC) ( GLenum target, GLenum format,GLenum type, GLvoid *image );
typedef void (APIENTRYP RGLSYMGLGETCONVOLUTIONPARAMETERFVPROC) ( GLenum target, GLenum pname,GLfloat *params );
typedef void (APIENTRYP RGLSYMGLGETCONVOLUTIONPARAMETERIVPROC) ( GLenum target, GLenum pname,GLint *params );
typedef void (APIENTRYP RGLSYMGLSEPARABLEFILTER2DPROC) ( GLenum target,GLenum internalformat, GLsizei width, GLsizei height, GLenum format,GLenum type, const GLvoid *row, const GLvoid *column );
typedef void (APIENTRYP RGLSYMGLGETSEPARABLEFILTERPROC) ( GLenum target, GLenum format,GLenum type, GLvoid *row, GLvoid *column, GLvoid *span );
typedef void (APIENTRYP RGLSYMGLACTIVETEXTUREPROC) ( GLenum texture );
typedef void (APIENTRYP RGLSYMGLCLIENTACTIVETEXTUREPROC) ( GLenum texture );
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE1DPROC) ( GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data );
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC) ( GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data );
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC) ( GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data );
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DPROC) ( GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data );
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC) ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data );
typedef void (APIENTRYP RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC) ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data );
typedef void (APIENTRYP RGLSYMGLGETCOMPRESSEDTEXIMAGEPROC) ( GLenum target, GLint lod, GLvoid *img );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1DPROC) ( GLenum target, GLdouble s );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1DVPROC) ( GLenum target, const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1FPROC) ( GLenum target, GLfloat s );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1FVPROC) ( GLenum target, const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1IPROC) ( GLenum target, GLint s );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1IVPROC) ( GLenum target, const GLint *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1SPROC) ( GLenum target, GLshort s );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD1SVPROC) ( GLenum target, const GLshort *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2DPROC) ( GLenum target, GLdouble s, GLdouble t );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2DVPROC) ( GLenum target, const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2FPROC) ( GLenum target, GLfloat s, GLfloat t );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2FVPROC) ( GLenum target, const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2IPROC) ( GLenum target, GLint s, GLint t );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2IVPROC) ( GLenum target, const GLint *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2SPROC) ( GLenum target, GLshort s, GLshort t );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD2SVPROC) ( GLenum target, const GLshort *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3DPROC) ( GLenum target, GLdouble s, GLdouble t, GLdouble r );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3DVPROC) ( GLenum target, const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3FPROC) ( GLenum target, GLfloat s, GLfloat t, GLfloat r );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3FVPROC) ( GLenum target, const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3IPROC) ( GLenum target, GLint s, GLint t, GLint r );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3IVPROC) ( GLenum target, const GLint *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3SPROC) ( GLenum target, GLshort s, GLshort t, GLshort r );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD3SVPROC) ( GLenum target, const GLshort *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4DPROC) ( GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4DVPROC) ( GLenum target, const GLdouble *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4FPROC) ( GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4FVPROC) ( GLenum target, const GLfloat *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4IPROC) ( GLenum target, GLint s, GLint t, GLint r, GLint q );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4IVPROC) ( GLenum target, const GLint *v );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4SPROC) ( GLenum target, GLshort s, GLshort t, GLshort r, GLshort q );
typedef void (APIENTRYP RGLSYMGLMULTITEXCOORD4SVPROC) ( GLenum target, const GLshort *v );
typedef void (APIENTRYP RGLSYMGLLOADTRANSPOSEMATRIXDPROC) ( const GLdouble m[16] );
typedef void (APIENTRYP RGLSYMGLLOADTRANSPOSEMATRIXFPROC) ( const GLfloat m[16] );
typedef void (APIENTRYP RGLSYMGLMULTTRANSPOSEMATRIXDPROC) ( const GLdouble m[16] );
typedef void (APIENTRYP RGLSYMGLMULTTRANSPOSEMATRIXFPROC) ( const GLfloat m[16] );
typedef void (APIENTRYP RGLSYMGLSAMPLECOVERAGEPROC) ( GLclampf value, GLboolean invert );
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
typedef void (APIENTRYP RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
typedef void (APIENTRYP RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLeglImageOES image);
typedef void (APIENTRYP RGLSYMGLBINDTEXTURESPROC) (GLuint first, GLsizei count, const GLuint *textures);

#ifdef __SWITCH2__
#define glClearIndex rsw2_glClearIndex
#define glClearColor rsw2_glClearColor
#define glClear rsw2_glClear
#define glIndexMask rsw2_glIndexMask
#define glColorMask rsw2_glColorMask
#define glAlphaFunc rsw2_glAlphaFunc
#define glBlendFunc rsw2_glBlendFunc
#define glLogicOp rsw2_glLogicOp
#define glCullFace rsw2_glCullFace
#define glFrontFace rsw2_glFrontFace
#define glPointSize rsw2_glPointSize
#define glLineWidth rsw2_glLineWidth
#define glLineStipple rsw2_glLineStipple
#define glPolygonMode rsw2_glPolygonMode
#define glPolygonOffset rsw2_glPolygonOffset
#define glPolygonStipple rsw2_glPolygonStipple
#define glGetPolygonStipple rsw2_glGetPolygonStipple
#define glEdgeFlag rsw2_glEdgeFlag
#define glEdgeFlagv rsw2_glEdgeFlagv
#define glScissor rsw2_glScissor
#define glClipPlane rsw2_glClipPlane
#define glGetClipPlane rsw2_glGetClipPlane
#define glDrawBuffer rsw2_glDrawBuffer
#define glReadBuffer rsw2_glReadBuffer
#define glEnable rsw2_glEnable
#define glDisable rsw2_glDisable
#define glIsEnabled rsw2_glIsEnabled
#define glEnableClientState rsw2_glEnableClientState
#define glDisableClientState rsw2_glDisableClientState
#define glGetBooleanv rsw2_glGetBooleanv
#define glGetDoublev rsw2_glGetDoublev
#define glGetFloatv rsw2_glGetFloatv
#define glGetIntegerv rsw2_glGetIntegerv
#define glPushAttrib rsw2_glPushAttrib
#define glPopAttrib rsw2_glPopAttrib
#define glPushClientAttrib rsw2_glPushClientAttrib
#define glPopClientAttrib rsw2_glPopClientAttrib
#define glRenderMode rsw2_glRenderMode
#define glGetError rsw2_glGetError
#define glGetString rsw2_glGetString
#define glFinish rsw2_glFinish
#define glFlush rsw2_glFlush
#define glHint rsw2_glHint
#define glClearDepth rsw2_glClearDepth
#define glDepthFunc rsw2_glDepthFunc
#define glDepthMask rsw2_glDepthMask
#define glDepthRange rsw2_glDepthRange
#define glClearAccum rsw2_glClearAccum
#define glAccum rsw2_glAccum
#define glMatrixMode rsw2_glMatrixMode
#define glOrtho rsw2_glOrtho
#define glFrustum rsw2_glFrustum
#define glViewport rsw2_glViewport
#define glPushMatrix rsw2_glPushMatrix
#define glPopMatrix rsw2_glPopMatrix
#define glLoadIdentity rsw2_glLoadIdentity
#define glLoadMatrixd rsw2_glLoadMatrixd
#define glLoadMatrixf rsw2_glLoadMatrixf
#define glMultMatrixd rsw2_glMultMatrixd
#define glMultMatrixf rsw2_glMultMatrixf
#define glRotated rsw2_glRotated
#define glRotatef rsw2_glRotatef
#define glScaled rsw2_glScaled
#define glScalef rsw2_glScalef
#define glTranslated rsw2_glTranslated
#define glTranslatef rsw2_glTranslatef
#define glIsList rsw2_glIsList
#define glDeleteLists rsw2_glDeleteLists
#define glGenLists rsw2_glGenLists
#define glNewList rsw2_glNewList
#define glEndList rsw2_glEndList
#define glCallList rsw2_glCallList
#define glCallLists rsw2_glCallLists
#define glListBase rsw2_glListBase
#define glBegin rsw2_glBegin
#define glEnd rsw2_glEnd
#define glVertex2d rsw2_glVertex2d
#define glVertex2f rsw2_glVertex2f
#define glVertex2i rsw2_glVertex2i
#define glVertex2s rsw2_glVertex2s
#define glVertex3d rsw2_glVertex3d
#define glVertex3f rsw2_glVertex3f
#define glVertex3i rsw2_glVertex3i
#define glVertex3s rsw2_glVertex3s
#define glVertex4d rsw2_glVertex4d
#define glVertex4f rsw2_glVertex4f
#define glVertex4i rsw2_glVertex4i
#define glVertex4s rsw2_glVertex4s
#define glVertex2dv rsw2_glVertex2dv
#define glVertex2fv rsw2_glVertex2fv
#define glVertex2iv rsw2_glVertex2iv
#define glVertex2sv rsw2_glVertex2sv
#define glVertex3dv rsw2_glVertex3dv
#define glVertex3fv rsw2_glVertex3fv
#define glVertex3iv rsw2_glVertex3iv
#define glVertex3sv rsw2_glVertex3sv
#define glVertex4dv rsw2_glVertex4dv
#define glVertex4fv rsw2_glVertex4fv
#define glVertex4iv rsw2_glVertex4iv
#define glVertex4sv rsw2_glVertex4sv
#define glNormal3b rsw2_glNormal3b
#define glNormal3d rsw2_glNormal3d
#define glNormal3f rsw2_glNormal3f
#define glNormal3i rsw2_glNormal3i
#define glNormal3s rsw2_glNormal3s
#define glNormal3bv rsw2_glNormal3bv
#define glNormal3dv rsw2_glNormal3dv
#define glNormal3fv rsw2_glNormal3fv
#define glNormal3iv rsw2_glNormal3iv
#define glNormal3sv rsw2_glNormal3sv
#define glIndexd rsw2_glIndexd
#define glIndexf rsw2_glIndexf
#define glIndexi rsw2_glIndexi
#define glIndexs rsw2_glIndexs
#define glIndexub rsw2_glIndexub
#define glIndexdv rsw2_glIndexdv
#define glIndexfv rsw2_glIndexfv
#define glIndexiv rsw2_glIndexiv
#define glIndexsv rsw2_glIndexsv
#define glIndexubv rsw2_glIndexubv
#define glColor3b rsw2_glColor3b
#define glColor3d rsw2_glColor3d
#define glColor3f rsw2_glColor3f
#define glColor3i rsw2_glColor3i
#define glColor3s rsw2_glColor3s
#define glColor3ub rsw2_glColor3ub
#define glColor3ui rsw2_glColor3ui
#define glColor3us rsw2_glColor3us
#define glColor4b rsw2_glColor4b
#define glColor4d rsw2_glColor4d
#define glColor4f rsw2_glColor4f
#define glColor4i rsw2_glColor4i
#define glColor4s rsw2_glColor4s
#define glColor4ub rsw2_glColor4ub
#define glColor4ui rsw2_glColor4ui
#define glColor4us rsw2_glColor4us
#define glColor3bv rsw2_glColor3bv
#define glColor3dv rsw2_glColor3dv
#define glColor3fv rsw2_glColor3fv
#define glColor3iv rsw2_glColor3iv
#define glColor3sv rsw2_glColor3sv
#define glColor3ubv rsw2_glColor3ubv
#define glColor3uiv rsw2_glColor3uiv
#define glColor3usv rsw2_glColor3usv
#define glColor4bv rsw2_glColor4bv
#define glColor4dv rsw2_glColor4dv
#define glColor4fv rsw2_glColor4fv
#define glColor4iv rsw2_glColor4iv
#define glColor4sv rsw2_glColor4sv
#define glColor4ubv rsw2_glColor4ubv
#define glColor4uiv rsw2_glColor4uiv
#define glColor4usv rsw2_glColor4usv
#define glTexCoord1d rsw2_glTexCoord1d
#define glTexCoord1f rsw2_glTexCoord1f
#define glTexCoord1i rsw2_glTexCoord1i
#define glTexCoord1s rsw2_glTexCoord1s
#define glTexCoord2d rsw2_glTexCoord2d
#define glTexCoord2f rsw2_glTexCoord2f
#define glTexCoord2i rsw2_glTexCoord2i
#define glTexCoord2s rsw2_glTexCoord2s
#define glTexCoord3d rsw2_glTexCoord3d
#define glTexCoord3f rsw2_glTexCoord3f
#define glTexCoord3i rsw2_glTexCoord3i
#define glTexCoord3s rsw2_glTexCoord3s
#define glTexCoord4d rsw2_glTexCoord4d
#define glTexCoord4f rsw2_glTexCoord4f
#define glTexCoord4i rsw2_glTexCoord4i
#define glTexCoord4s rsw2_glTexCoord4s
#define glTexCoord1dv rsw2_glTexCoord1dv
#define glTexCoord1fv rsw2_glTexCoord1fv
#define glTexCoord1iv rsw2_glTexCoord1iv
#define glTexCoord1sv rsw2_glTexCoord1sv
#define glTexCoord2dv rsw2_glTexCoord2dv
#define glTexCoord2fv rsw2_glTexCoord2fv
#define glTexCoord2iv rsw2_glTexCoord2iv
#define glTexCoord2sv rsw2_glTexCoord2sv
#define glTexCoord3dv rsw2_glTexCoord3dv
#define glTexCoord3fv rsw2_glTexCoord3fv
#define glTexCoord3iv rsw2_glTexCoord3iv
#define glTexCoord3sv rsw2_glTexCoord3sv
#define glTexCoord4dv rsw2_glTexCoord4dv
#define glTexCoord4fv rsw2_glTexCoord4fv
#define glTexCoord4iv rsw2_glTexCoord4iv
#define glTexCoord4sv rsw2_glTexCoord4sv
#define glRasterPos2d rsw2_glRasterPos2d
#define glRasterPos2f rsw2_glRasterPos2f
#define glRasterPos2i rsw2_glRasterPos2i
#define glRasterPos2s rsw2_glRasterPos2s
#define glRasterPos3d rsw2_glRasterPos3d
#define glRasterPos3f rsw2_glRasterPos3f
#define glRasterPos3i rsw2_glRasterPos3i
#define glRasterPos3s rsw2_glRasterPos3s
#define glRasterPos4d rsw2_glRasterPos4d
#define glRasterPos4f rsw2_glRasterPos4f
#define glRasterPos4i rsw2_glRasterPos4i
#define glRasterPos4s rsw2_glRasterPos4s
#define glRasterPos2dv rsw2_glRasterPos2dv
#define glRasterPos2fv rsw2_glRasterPos2fv
#define glRasterPos2iv rsw2_glRasterPos2iv
#define glRasterPos2sv rsw2_glRasterPos2sv
#define glRasterPos3dv rsw2_glRasterPos3dv
#define glRasterPos3fv rsw2_glRasterPos3fv
#define glRasterPos3iv rsw2_glRasterPos3iv
#define glRasterPos3sv rsw2_glRasterPos3sv
#define glRasterPos4dv rsw2_glRasterPos4dv
#define glRasterPos4fv rsw2_glRasterPos4fv
#define glRasterPos4iv rsw2_glRasterPos4iv
#define glRasterPos4sv rsw2_glRasterPos4sv
#define glRectd rsw2_glRectd
#define glRectf rsw2_glRectf
#define glRecti rsw2_glRecti
#define glRects rsw2_glRects
#define glRectdv rsw2_glRectdv
#define glRectfv rsw2_glRectfv
#define glRectiv rsw2_glRectiv
#define glRectsv rsw2_glRectsv
#define glVertexPointer rsw2_glVertexPointer
#define glNormalPointer rsw2_glNormalPointer
#define glColorPointer rsw2_glColorPointer
#define glIndexPointer rsw2_glIndexPointer
#define glTexCoordPointer rsw2_glTexCoordPointer
#define glEdgeFlagPointer rsw2_glEdgeFlagPointer
#define glGetPointerv rsw2_glGetPointerv
#define glArrayElement rsw2_glArrayElement
#define glDrawArrays rsw2_glDrawArrays
#define glDrawElements rsw2_glDrawElements
#define glInterleavedArrays rsw2_glInterleavedArrays
#define glShadeModel rsw2_glShadeModel
#define glLightf rsw2_glLightf
#define glLighti rsw2_glLighti
#define glLightfv rsw2_glLightfv
#define glLightiv rsw2_glLightiv
#define glGetLightfv rsw2_glGetLightfv
#define glGetLightiv rsw2_glGetLightiv
#define glLightModelf rsw2_glLightModelf
#define glLightModeli rsw2_glLightModeli
#define glLightModelfv rsw2_glLightModelfv
#define glLightModeliv rsw2_glLightModeliv
#define glMaterialf rsw2_glMaterialf
#define glMateriali rsw2_glMateriali
#define glMaterialfv rsw2_glMaterialfv
#define glMaterialiv rsw2_glMaterialiv
#define glGetMaterialfv rsw2_glGetMaterialfv
#define glGetMaterialiv rsw2_glGetMaterialiv
#define glColorMaterial rsw2_glColorMaterial
#define glPixelZoom rsw2_glPixelZoom
#define glPixelStoref rsw2_glPixelStoref
#define glPixelStorei rsw2_glPixelStorei
#define glPixelTransferf rsw2_glPixelTransferf
#define glPixelTransferi rsw2_glPixelTransferi
#define glPixelMapfv rsw2_glPixelMapfv
#define glPixelMapuiv rsw2_glPixelMapuiv
#define glPixelMapusv rsw2_glPixelMapusv
#define glGetPixelMapfv rsw2_glGetPixelMapfv
#define glGetPixelMapuiv rsw2_glGetPixelMapuiv
#define glGetPixelMapusv rsw2_glGetPixelMapusv
#define glBitmap rsw2_glBitmap
#define glReadPixels rsw2_glReadPixels
#define glDrawPixels rsw2_glDrawPixels
#define glCopyPixels rsw2_glCopyPixels
#define glStencilFunc rsw2_glStencilFunc
#define glStencilMask rsw2_glStencilMask
#define glStencilOp rsw2_glStencilOp
#define glClearStencil rsw2_glClearStencil
#define glTexGend rsw2_glTexGend
#define glTexGenf rsw2_glTexGenf
#define glTexGeni rsw2_glTexGeni
#define glTexGendv rsw2_glTexGendv
#define glTexGenfv rsw2_glTexGenfv
#define glTexGeniv rsw2_glTexGeniv
#define glGetTexGendv rsw2_glGetTexGendv
#define glGetTexGenfv rsw2_glGetTexGenfv
#define glGetTexGeniv rsw2_glGetTexGeniv
#define glTexEnvf rsw2_glTexEnvf
#define glTexEnvi rsw2_glTexEnvi
#define glTexEnvfv rsw2_glTexEnvfv
#define glTexEnviv rsw2_glTexEnviv
#define glGetTexEnvfv rsw2_glGetTexEnvfv
#define glGetTexEnviv rsw2_glGetTexEnviv
#define glTexParameterf rsw2_glTexParameterf
#define glTexParameteri rsw2_glTexParameteri
#define glTexParameterfv rsw2_glTexParameterfv
#define glTexParameteriv rsw2_glTexParameteriv
#define glGetTexParameterfv rsw2_glGetTexParameterfv
#define glGetTexParameteriv rsw2_glGetTexParameteriv
#define glGetTexLevelParameterfv rsw2_glGetTexLevelParameterfv
#define glGetTexLevelParameteriv rsw2_glGetTexLevelParameteriv
#define glTexImage1D rsw2_glTexImage1D
#define glTexImage2D rsw2_glTexImage2D
#define glGetTexImage rsw2_glGetTexImage
#define glGenTextures rsw2_glGenTextures
#define glDeleteTextures rsw2_glDeleteTextures
#define glBindTexture rsw2_glBindTexture
#define glPrioritizeTextures rsw2_glPrioritizeTextures
#define glAreTexturesResident rsw2_glAreTexturesResident
#define glIsTexture rsw2_glIsTexture
#define glTexSubImage1D rsw2_glTexSubImage1D
#define glTexSubImage2D rsw2_glTexSubImage2D
#define glCopyTexImage1D rsw2_glCopyTexImage1D
#define glCopyTexImage2D rsw2_glCopyTexImage2D
#define glCopyTexSubImage1D rsw2_glCopyTexSubImage1D
#define glCopyTexSubImage2D rsw2_glCopyTexSubImage2D
#define glMap1d rsw2_glMap1d
#define glMap1f rsw2_glMap1f
#define glMap2d rsw2_glMap2d
#define glMap2f rsw2_glMap2f
#define glGetMapdv rsw2_glGetMapdv
#define glGetMapfv rsw2_glGetMapfv
#define glGetMapiv rsw2_glGetMapiv
#define glEvalCoord1d rsw2_glEvalCoord1d
#define glEvalCoord1f rsw2_glEvalCoord1f
#define glEvalCoord1dv rsw2_glEvalCoord1dv
#define glEvalCoord1fv rsw2_glEvalCoord1fv
#define glEvalCoord2d rsw2_glEvalCoord2d
#define glEvalCoord2f rsw2_glEvalCoord2f
#define glEvalCoord2dv rsw2_glEvalCoord2dv
#define glEvalCoord2fv rsw2_glEvalCoord2fv
#define glMapGrid1d rsw2_glMapGrid1d
#define glMapGrid1f rsw2_glMapGrid1f
#define glMapGrid2d rsw2_glMapGrid2d
#define glMapGrid2f rsw2_glMapGrid2f
#define glEvalPoint1 rsw2_glEvalPoint1
#define glEvalPoint2 rsw2_glEvalPoint2
#define glEvalMesh1 rsw2_glEvalMesh1
#define glEvalMesh2 rsw2_glEvalMesh2
#define glFogf rsw2_glFogf
#define glFogi rsw2_glFogi
#define glFogfv rsw2_glFogfv
#define glFogiv rsw2_glFogiv
#define glFeedbackBuffer rsw2_glFeedbackBuffer
#define glPassThrough rsw2_glPassThrough
#define glSelectBuffer rsw2_glSelectBuffer
#define glInitNames rsw2_glInitNames
#define glLoadName rsw2_glLoadName
#define glPushName rsw2_glPushName
#define glPopName rsw2_glPopName
#define glDrawRangeElements rsw2_glDrawRangeElements
#define glTexImage3D rsw2_glTexImage3D
#define glTexSubImage3D rsw2_glTexSubImage3D
#define glCopyTexSubImage3D rsw2_glCopyTexSubImage3D
#define glColorTable rsw2_glColorTable
#define glColorSubTable rsw2_glColorSubTable
#define glColorTableParameteriv rsw2_glColorTableParameteriv
#define glColorTableParameterfv rsw2_glColorTableParameterfv
#define glCopyColorSubTable rsw2_glCopyColorSubTable
#define glCopyColorTable rsw2_glCopyColorTable
#define glGetColorTable rsw2_glGetColorTable
#define glGetColorTableParameterfv rsw2_glGetColorTableParameterfv
#define glGetColorTableParameteriv rsw2_glGetColorTableParameteriv
#define glBlendEquation rsw2_glBlendEquation
#define glBlendColor rsw2_glBlendColor
#define glHistogram rsw2_glHistogram
#define glResetHistogram rsw2_glResetHistogram
#define glGetHistogram rsw2_glGetHistogram
#define glGetHistogramParameterfv rsw2_glGetHistogramParameterfv
#define glGetHistogramParameteriv rsw2_glGetHistogramParameteriv
#define glMinmax rsw2_glMinmax
#define glResetMinmax rsw2_glResetMinmax
#define glGetMinmax rsw2_glGetMinmax
#define glGetMinmaxParameterfv rsw2_glGetMinmaxParameterfv
#define glGetMinmaxParameteriv rsw2_glGetMinmaxParameteriv
#define glConvolutionFilter1D rsw2_glConvolutionFilter1D
#define glConvolutionFilter2D rsw2_glConvolutionFilter2D
#define glConvolutionParameterf rsw2_glConvolutionParameterf
#define glConvolutionParameterfv rsw2_glConvolutionParameterfv
#define glConvolutionParameteri rsw2_glConvolutionParameteri
#define glConvolutionParameteriv rsw2_glConvolutionParameteriv
#define glCopyConvolutionFilter1D rsw2_glCopyConvolutionFilter1D
#define glCopyConvolutionFilter2D rsw2_glCopyConvolutionFilter2D
#define glGetConvolutionFilter rsw2_glGetConvolutionFilter
#define glGetConvolutionParameterfv rsw2_glGetConvolutionParameterfv
#define glGetConvolutionParameteriv rsw2_glGetConvolutionParameteriv
#define glSeparableFilter2D rsw2_glSeparableFilter2D
#define glGetSeparableFilter rsw2_glGetSeparableFilter
#define glActiveTexture rsw2_glActiveTexture
#define glClientActiveTexture rsw2_glClientActiveTexture
#define glCompressedTexImage1D rsw2_glCompressedTexImage1D
#define glCompressedTexImage2D rsw2_glCompressedTexImage2D
#define glCompressedTexImage3D rsw2_glCompressedTexImage3D
#define glCompressedTexSubImage1D rsw2_glCompressedTexSubImage1D
#define glCompressedTexSubImage2D rsw2_glCompressedTexSubImage2D
#define glCompressedTexSubImage3D rsw2_glCompressedTexSubImage3D
#define glGetCompressedTexImage rsw2_glGetCompressedTexImage
#define glMultiTexCoord1d rsw2_glMultiTexCoord1d
#define glMultiTexCoord1dv rsw2_glMultiTexCoord1dv
#define glMultiTexCoord1f rsw2_glMultiTexCoord1f
#define glMultiTexCoord1fv rsw2_glMultiTexCoord1fv
#define glMultiTexCoord1i rsw2_glMultiTexCoord1i
#define glMultiTexCoord1iv rsw2_glMultiTexCoord1iv
#define glMultiTexCoord1s rsw2_glMultiTexCoord1s
#define glMultiTexCoord1sv rsw2_glMultiTexCoord1sv
#define glMultiTexCoord2d rsw2_glMultiTexCoord2d
#define glMultiTexCoord2dv rsw2_glMultiTexCoord2dv
#define glMultiTexCoord2f rsw2_glMultiTexCoord2f
#define glMultiTexCoord2fv rsw2_glMultiTexCoord2fv
#define glMultiTexCoord2i rsw2_glMultiTexCoord2i
#define glMultiTexCoord2iv rsw2_glMultiTexCoord2iv
#define glMultiTexCoord2s rsw2_glMultiTexCoord2s
#define glMultiTexCoord2sv rsw2_glMultiTexCoord2sv
#define glMultiTexCoord3d rsw2_glMultiTexCoord3d
#define glMultiTexCoord3dv rsw2_glMultiTexCoord3dv
#define glMultiTexCoord3f rsw2_glMultiTexCoord3f
#define glMultiTexCoord3fv rsw2_glMultiTexCoord3fv
#define glMultiTexCoord3i rsw2_glMultiTexCoord3i
#define glMultiTexCoord3iv rsw2_glMultiTexCoord3iv
#define glMultiTexCoord3s rsw2_glMultiTexCoord3s
#define glMultiTexCoord3sv rsw2_glMultiTexCoord3sv
#define glMultiTexCoord4d rsw2_glMultiTexCoord4d
#define glMultiTexCoord4dv rsw2_glMultiTexCoord4dv
#define glMultiTexCoord4f rsw2_glMultiTexCoord4f
#define glMultiTexCoord4fv rsw2_glMultiTexCoord4fv
#define glMultiTexCoord4i rsw2_glMultiTexCoord4i
#define glMultiTexCoord4iv rsw2_glMultiTexCoord4iv
#define glMultiTexCoord4s rsw2_glMultiTexCoord4s
#define glMultiTexCoord4sv rsw2_glMultiTexCoord4sv
#define glLoadTransposeMatrixd rsw2_glLoadTransposeMatrixd
#define glLoadTransposeMatrixf rsw2_glLoadTransposeMatrixf
#define glMultTransposeMatrixd rsw2_glMultTransposeMatrixd
#define glMultTransposeMatrixf rsw2_glMultTransposeMatrixf
#define glSampleCoverage rsw2_glSampleCoverage
#define glActiveTextureARB rsw2_glActiveTextureARB
#define glClientActiveTextureARB rsw2_glClientActiveTextureARB
#define glMultiTexCoord1dARB rsw2_glMultiTexCoord1dARB
#define glMultiTexCoord1dvARB rsw2_glMultiTexCoord1dvARB
#define glMultiTexCoord1fARB rsw2_glMultiTexCoord1fARB
#define glMultiTexCoord1fvARB rsw2_glMultiTexCoord1fvARB
#define glMultiTexCoord1iARB rsw2_glMultiTexCoord1iARB
#define glMultiTexCoord1ivARB rsw2_glMultiTexCoord1ivARB
#define glMultiTexCoord1sARB rsw2_glMultiTexCoord1sARB
#define glMultiTexCoord1svARB rsw2_glMultiTexCoord1svARB
#define glMultiTexCoord2dARB rsw2_glMultiTexCoord2dARB
#define glMultiTexCoord2dvARB rsw2_glMultiTexCoord2dvARB
#define glMultiTexCoord2fARB rsw2_glMultiTexCoord2fARB
#define glMultiTexCoord2fvARB rsw2_glMultiTexCoord2fvARB
#define glMultiTexCoord2iARB rsw2_glMultiTexCoord2iARB
#define glMultiTexCoord2ivARB rsw2_glMultiTexCoord2ivARB
#define glMultiTexCoord2sARB rsw2_glMultiTexCoord2sARB
#define glMultiTexCoord2svARB rsw2_glMultiTexCoord2svARB
#define glMultiTexCoord3dARB rsw2_glMultiTexCoord3dARB
#define glMultiTexCoord3dvARB rsw2_glMultiTexCoord3dvARB
#define glMultiTexCoord3fARB rsw2_glMultiTexCoord3fARB
#define glMultiTexCoord3fvARB rsw2_glMultiTexCoord3fvARB
#define glMultiTexCoord3iARB rsw2_glMultiTexCoord3iARB
#define glMultiTexCoord3ivARB rsw2_glMultiTexCoord3ivARB
#define glMultiTexCoord3sARB rsw2_glMultiTexCoord3sARB
#define glMultiTexCoord3svARB rsw2_glMultiTexCoord3svARB
#define glMultiTexCoord4dARB rsw2_glMultiTexCoord4dARB
#define glMultiTexCoord4dvARB rsw2_glMultiTexCoord4dvARB
#define glMultiTexCoord4fARB rsw2_glMultiTexCoord4fARB
#define glMultiTexCoord4fvARB rsw2_glMultiTexCoord4fvARB
#define glMultiTexCoord4iARB rsw2_glMultiTexCoord4iARB
#define glMultiTexCoord4ivARB rsw2_glMultiTexCoord4ivARB
#define glMultiTexCoord4sARB rsw2_glMultiTexCoord4sARB
#define glMultiTexCoord4svARB rsw2_glMultiTexCoord4svARB
#define glEGLImageTargetTexture2DOES rsw2_glEGLImageTargetTexture2DOES
#define glEGLImageTargetRenderbufferStorageOES rsw2_glEGLImageTargetRenderbufferStorageOES
#define glBindTextures rsw2_glBindTextures
#endif


RGLSYMGLCLEARINDEXPROC glClearIndex;
RGLSYMGLCLEARCOLORPROC glClearColor;
RGLSYMGLCLEARPROC glClear;
RGLSYMGLINDEXMASKPROC glIndexMask;
RGLSYMGLCOLORMASKPROC glColorMask;
RGLSYMGLALPHAFUNCPROC glAlphaFunc;
RGLSYMGLBLENDFUNCPROC glBlendFunc;
RGLSYMGLLOGICOPPROC glLogicOp;
RGLSYMGLCULLFACEPROC glCullFace;
RGLSYMGLFRONTFACEPROC glFrontFace;
RGLSYMGLPOINTSIZEPROC glPointSize;
RGLSYMGLLINEWIDTHPROC glLineWidth;
RGLSYMGLLINESTIPPLEPROC glLineStipple;
RGLSYMGLPOLYGONMODEPROC glPolygonMode;
RGLSYMGLPOLYGONOFFSETPROC glPolygonOffset;
RGLSYMGLPOLYGONSTIPPLEPROC glPolygonStipple;
RGLSYMGLGETPOLYGONSTIPPLEPROC glGetPolygonStipple;
RGLSYMGLEDGEFLAGPROC glEdgeFlag;
RGLSYMGLEDGEFLAGVPROC glEdgeFlagv;
RGLSYMGLSCISSORPROC glScissor;
RGLSYMGLCLIPPLANEPROC glClipPlane;
RGLSYMGLGETCLIPPLANEPROC glGetClipPlane;
RGLSYMGLDRAWBUFFERPROC glDrawBuffer;
RGLSYMGLREADBUFFERPROC glReadBuffer;
RGLSYMGLENABLEPROC glEnable;
RGLSYMGLDISABLEPROC glDisable;
RGLSYMGLISENABLEDPROC glIsEnabled;
RGLSYMGLENABLECLIENTSTATEPROC glEnableClientState;
RGLSYMGLDISABLECLIENTSTATEPROC glDisableClientState;
RGLSYMGLGETBOOLEANVPROC glGetBooleanv;
RGLSYMGLGETDOUBLEVPROC glGetDoublev;
RGLSYMGLGETFLOATVPROC glGetFloatv;
RGLSYMGLGETINTEGERVPROC glGetIntegerv;
RGLSYMGLPUSHATTRIBPROC glPushAttrib;
RGLSYMGLPOPATTRIBPROC glPopAttrib;
RGLSYMGLPUSHCLIENTATTRIBPROC glPushClientAttrib;
RGLSYMGLPOPCLIENTATTRIBPROC glPopClientAttrib;
RGLSYMGLRENDERMODEPROC glRenderMode;
RGLSYMGLGETERRORPROC glGetError;
RGLSYMGLGETSTRINGPROC glGetString;
RGLSYMGLFINISHPROC glFinish;
RGLSYMGLFLUSHPROC glFlush;
RGLSYMGLHINTPROC glHint;
RGLSYMGLCLEARDEPTHPROC glClearDepth;
RGLSYMGLDEPTHFUNCPROC glDepthFunc;
RGLSYMGLDEPTHMASKPROC glDepthMask;
RGLSYMGLDEPTHRANGEPROC glDepthRange;
RGLSYMGLCLEARACCUMPROC glClearAccum;
RGLSYMGLACCUMPROC glAccum;
RGLSYMGLMATRIXMODEPROC glMatrixMode;
RGLSYMGLORTHOPROC glOrtho;
RGLSYMGLFRUSTUMPROC glFrustum;
RGLSYMGLVIEWPORTPROC glViewport;
RGLSYMGLPUSHMATRIXPROC glPushMatrix;
RGLSYMGLPOPMATRIXPROC glPopMatrix;
RGLSYMGLLOADIDENTITYPROC glLoadIdentity;
RGLSYMGLLOADMATRIXDPROC glLoadMatrixd;
RGLSYMGLLOADMATRIXFPROC glLoadMatrixf;
RGLSYMGLMULTMATRIXDPROC glMultMatrixd;
RGLSYMGLMULTMATRIXFPROC glMultMatrixf;
RGLSYMGLROTATEDPROC glRotated;
RGLSYMGLROTATEFPROC glRotatef;
RGLSYMGLSCALEDPROC glScaled;
RGLSYMGLSCALEFPROC glScalef;
RGLSYMGLTRANSLATEDPROC glTranslated;
RGLSYMGLTRANSLATEFPROC glTranslatef;
RGLSYMGLISLISTPROC glIsList;
RGLSYMGLDELETELISTSPROC glDeleteLists;
RGLSYMGLGENLISTSPROC glGenLists;
RGLSYMGLNEWLISTPROC glNewList;
RGLSYMGLENDLISTPROC glEndList;
RGLSYMGLCALLLISTPROC glCallList;
RGLSYMGLCALLLISTSPROC glCallLists;
RGLSYMGLLISTBASEPROC glListBase;
RGLSYMGLBEGINPROC glBegin;
RGLSYMGLENDPROC glEnd;
RGLSYMGLVERTEX2DPROC glVertex2d;
RGLSYMGLVERTEX2FPROC glVertex2f;
RGLSYMGLVERTEX2IPROC glVertex2i;
RGLSYMGLVERTEX2SPROC glVertex2s;
RGLSYMGLVERTEX3DPROC glVertex3d;
RGLSYMGLVERTEX3FPROC glVertex3f;
RGLSYMGLVERTEX3IPROC glVertex3i;
RGLSYMGLVERTEX3SPROC glVertex3s;
RGLSYMGLVERTEX4DPROC glVertex4d;
RGLSYMGLVERTEX4FPROC glVertex4f;
RGLSYMGLVERTEX4IPROC glVertex4i;
RGLSYMGLVERTEX4SPROC glVertex4s;
RGLSYMGLVERTEX2DVPROC glVertex2dv;
RGLSYMGLVERTEX2FVPROC glVertex2fv;
RGLSYMGLVERTEX2IVPROC glVertex2iv;
RGLSYMGLVERTEX2SVPROC glVertex2sv;
RGLSYMGLVERTEX3DVPROC glVertex3dv;
RGLSYMGLVERTEX3FVPROC glVertex3fv;
RGLSYMGLVERTEX3IVPROC glVertex3iv;
RGLSYMGLVERTEX3SVPROC glVertex3sv;
RGLSYMGLVERTEX4DVPROC glVertex4dv;
RGLSYMGLVERTEX4FVPROC glVertex4fv;
RGLSYMGLVERTEX4IVPROC glVertex4iv;
RGLSYMGLVERTEX4SVPROC glVertex4sv;
RGLSYMGLNORMAL3BPROC glNormal3b;
RGLSYMGLNORMAL3DPROC glNormal3d;
RGLSYMGLNORMAL3FPROC glNormal3f;
RGLSYMGLNORMAL3IPROC glNormal3i;
RGLSYMGLNORMAL3SPROC glNormal3s;
RGLSYMGLNORMAL3BVPROC glNormal3bv;
RGLSYMGLNORMAL3DVPROC glNormal3dv;
RGLSYMGLNORMAL3FVPROC glNormal3fv;
RGLSYMGLNORMAL3IVPROC glNormal3iv;
RGLSYMGLNORMAL3SVPROC glNormal3sv;
RGLSYMGLINDEXDPROC glIndexd;
RGLSYMGLINDEXFPROC glIndexf;
RGLSYMGLINDEXIPROC glIndexi;
RGLSYMGLINDEXSPROC glIndexs;
RGLSYMGLINDEXUBPROC glIndexub;
RGLSYMGLINDEXDVPROC glIndexdv;
RGLSYMGLINDEXFVPROC glIndexfv;
RGLSYMGLINDEXIVPROC glIndexiv;
RGLSYMGLINDEXSVPROC glIndexsv;
RGLSYMGLINDEXUBVPROC glIndexubv;
RGLSYMGLCOLOR3BPROC glColor3b;
RGLSYMGLCOLOR3DPROC glColor3d;
RGLSYMGLCOLOR3FPROC glColor3f;
RGLSYMGLCOLOR3IPROC glColor3i;
RGLSYMGLCOLOR3SPROC glColor3s;
RGLSYMGLCOLOR3UBPROC glColor3ub;
RGLSYMGLCOLOR3UIPROC glColor3ui;
RGLSYMGLCOLOR3USPROC glColor3us;
RGLSYMGLCOLOR4BPROC glColor4b;
RGLSYMGLCOLOR4DPROC glColor4d;
RGLSYMGLCOLOR4FPROC glColor4f;
RGLSYMGLCOLOR4IPROC glColor4i;
RGLSYMGLCOLOR4SPROC glColor4s;
RGLSYMGLCOLOR4UBPROC glColor4ub;
RGLSYMGLCOLOR4UIPROC glColor4ui;
RGLSYMGLCOLOR4USPROC glColor4us;
RGLSYMGLCOLOR3BVPROC glColor3bv;
RGLSYMGLCOLOR3DVPROC glColor3dv;
RGLSYMGLCOLOR3FVPROC glColor3fv;
RGLSYMGLCOLOR3IVPROC glColor3iv;
RGLSYMGLCOLOR3SVPROC glColor3sv;
RGLSYMGLCOLOR3UBVPROC glColor3ubv;
RGLSYMGLCOLOR3UIVPROC glColor3uiv;
RGLSYMGLCOLOR3USVPROC glColor3usv;
RGLSYMGLCOLOR4BVPROC glColor4bv;
RGLSYMGLCOLOR4DVPROC glColor4dv;
RGLSYMGLCOLOR4FVPROC glColor4fv;
RGLSYMGLCOLOR4IVPROC glColor4iv;
RGLSYMGLCOLOR4SVPROC glColor4sv;
RGLSYMGLCOLOR4UBVPROC glColor4ubv;
RGLSYMGLCOLOR4UIVPROC glColor4uiv;
RGLSYMGLCOLOR4USVPROC glColor4usv;
RGLSYMGLTEXCOORD1DPROC glTexCoord1d;
RGLSYMGLTEXCOORD1FPROC glTexCoord1f;
RGLSYMGLTEXCOORD1IPROC glTexCoord1i;
RGLSYMGLTEXCOORD1SPROC glTexCoord1s;
RGLSYMGLTEXCOORD2DPROC glTexCoord2d;
RGLSYMGLTEXCOORD2FPROC glTexCoord2f;
RGLSYMGLTEXCOORD2IPROC glTexCoord2i;
RGLSYMGLTEXCOORD2SPROC glTexCoord2s;
RGLSYMGLTEXCOORD3DPROC glTexCoord3d;
RGLSYMGLTEXCOORD3FPROC glTexCoord3f;
RGLSYMGLTEXCOORD3IPROC glTexCoord3i;
RGLSYMGLTEXCOORD3SPROC glTexCoord3s;
RGLSYMGLTEXCOORD4DPROC glTexCoord4d;
RGLSYMGLTEXCOORD4FPROC glTexCoord4f;
RGLSYMGLTEXCOORD4IPROC glTexCoord4i;
RGLSYMGLTEXCOORD4SPROC glTexCoord4s;
RGLSYMGLTEXCOORD1DVPROC glTexCoord1dv;
RGLSYMGLTEXCOORD1FVPROC glTexCoord1fv;
RGLSYMGLTEXCOORD1IVPROC glTexCoord1iv;
RGLSYMGLTEXCOORD1SVPROC glTexCoord1sv;
RGLSYMGLTEXCOORD2DVPROC glTexCoord2dv;
RGLSYMGLTEXCOORD2FVPROC glTexCoord2fv;
RGLSYMGLTEXCOORD2IVPROC glTexCoord2iv;
RGLSYMGLTEXCOORD2SVPROC glTexCoord2sv;
RGLSYMGLTEXCOORD3DVPROC glTexCoord3dv;
RGLSYMGLTEXCOORD3FVPROC glTexCoord3fv;
RGLSYMGLTEXCOORD3IVPROC glTexCoord3iv;
RGLSYMGLTEXCOORD3SVPROC glTexCoord3sv;
RGLSYMGLTEXCOORD4DVPROC glTexCoord4dv;
RGLSYMGLTEXCOORD4FVPROC glTexCoord4fv;
RGLSYMGLTEXCOORD4IVPROC glTexCoord4iv;
RGLSYMGLTEXCOORD4SVPROC glTexCoord4sv;
RGLSYMGLRASTERPOS2DPROC glRasterPos2d;
RGLSYMGLRASTERPOS2FPROC glRasterPos2f;
RGLSYMGLRASTERPOS2IPROC glRasterPos2i;
RGLSYMGLRASTERPOS2SPROC glRasterPos2s;
RGLSYMGLRASTERPOS3DPROC glRasterPos3d;
RGLSYMGLRASTERPOS3FPROC glRasterPos3f;
RGLSYMGLRASTERPOS3IPROC glRasterPos3i;
RGLSYMGLRASTERPOS3SPROC glRasterPos3s;
RGLSYMGLRASTERPOS4DPROC glRasterPos4d;
RGLSYMGLRASTERPOS4FPROC glRasterPos4f;
RGLSYMGLRASTERPOS4IPROC glRasterPos4i;
RGLSYMGLRASTERPOS4SPROC glRasterPos4s;
RGLSYMGLRASTERPOS2DVPROC glRasterPos2dv;
RGLSYMGLRASTERPOS2FVPROC glRasterPos2fv;
RGLSYMGLRASTERPOS2IVPROC glRasterPos2iv;
RGLSYMGLRASTERPOS2SVPROC glRasterPos2sv;
RGLSYMGLRASTERPOS3DVPROC glRasterPos3dv;
RGLSYMGLRASTERPOS3FVPROC glRasterPos3fv;
RGLSYMGLRASTERPOS3IVPROC glRasterPos3iv;
RGLSYMGLRASTERPOS3SVPROC glRasterPos3sv;
RGLSYMGLRASTERPOS4DVPROC glRasterPos4dv;
RGLSYMGLRASTERPOS4FVPROC glRasterPos4fv;
RGLSYMGLRASTERPOS4IVPROC glRasterPos4iv;
RGLSYMGLRASTERPOS4SVPROC glRasterPos4sv;
RGLSYMGLRECTDPROC glRectd;
RGLSYMGLRECTFPROC glRectf;
RGLSYMGLRECTIPROC glRecti;
RGLSYMGLRECTSPROC glRects;
RGLSYMGLRECTDVPROC glRectdv;
RGLSYMGLRECTFVPROC glRectfv;
RGLSYMGLRECTIVPROC glRectiv;
RGLSYMGLRECTSVPROC glRectsv;
RGLSYMGLVERTEXPOINTERPROC glVertexPointer;
RGLSYMGLNORMALPOINTERPROC glNormalPointer;
RGLSYMGLCOLORPOINTERPROC glColorPointer;
RGLSYMGLINDEXPOINTERPROC glIndexPointer;
RGLSYMGLTEXCOORDPOINTERPROC glTexCoordPointer;
RGLSYMGLEDGEFLAGPOINTERPROC glEdgeFlagPointer;
RGLSYMGLGETPOINTERVPROC glGetPointerv;
RGLSYMGLARRAYELEMENTPROC glArrayElement;
RGLSYMGLDRAWARRAYSPROC glDrawArrays;
RGLSYMGLDRAWELEMENTSPROC glDrawElements;
RGLSYMGLINTERLEAVEDARRAYSPROC glInterleavedArrays;
RGLSYMGLSHADEMODELPROC glShadeModel;
RGLSYMGLLIGHTFPROC glLightf;
RGLSYMGLLIGHTIPROC glLighti;
RGLSYMGLLIGHTFVPROC glLightfv;
RGLSYMGLLIGHTIVPROC glLightiv;
RGLSYMGLGETLIGHTFVPROC glGetLightfv;
RGLSYMGLGETLIGHTIVPROC glGetLightiv;
RGLSYMGLLIGHTMODELFPROC glLightModelf;
RGLSYMGLLIGHTMODELIPROC glLightModeli;
RGLSYMGLLIGHTMODELFVPROC glLightModelfv;
RGLSYMGLLIGHTMODELIVPROC glLightModeliv;
RGLSYMGLMATERIALFPROC glMaterialf;
RGLSYMGLMATERIALIPROC glMateriali;
RGLSYMGLMATERIALFVPROC glMaterialfv;
RGLSYMGLMATERIALIVPROC glMaterialiv;
RGLSYMGLGETMATERIALFVPROC glGetMaterialfv;
RGLSYMGLGETMATERIALIVPROC glGetMaterialiv;
RGLSYMGLCOLORMATERIALPROC glColorMaterial;
RGLSYMGLPIXELZOOMPROC glPixelZoom;
RGLSYMGLPIXELSTOREFPROC glPixelStoref;
RGLSYMGLPIXELSTOREIPROC glPixelStorei;
RGLSYMGLPIXELTRANSFERFPROC glPixelTransferf;
RGLSYMGLPIXELTRANSFERIPROC glPixelTransferi;
RGLSYMGLPIXELMAPFVPROC glPixelMapfv;
RGLSYMGLPIXELMAPUIVPROC glPixelMapuiv;
RGLSYMGLPIXELMAPUSVPROC glPixelMapusv;
RGLSYMGLGETPIXELMAPFVPROC glGetPixelMapfv;
RGLSYMGLGETPIXELMAPUIVPROC glGetPixelMapuiv;
RGLSYMGLGETPIXELMAPUSVPROC glGetPixelMapusv;
RGLSYMGLBITMAPPROC glBitmap;
RGLSYMGLREADPIXELSPROC glReadPixels;
RGLSYMGLDRAWPIXELSPROC glDrawPixels;
RGLSYMGLCOPYPIXELSPROC glCopyPixels;
RGLSYMGLSTENCILFUNCPROC glStencilFunc;
RGLSYMGLSTENCILMASKPROC glStencilMask;
RGLSYMGLSTENCILOPPROC glStencilOp;
RGLSYMGLCLEARSTENCILPROC glClearStencil;
RGLSYMGLTEXGENDPROC glTexGend;
RGLSYMGLTEXGENFPROC glTexGenf;
RGLSYMGLTEXGENIPROC glTexGeni;
RGLSYMGLTEXGENDVPROC glTexGendv;
RGLSYMGLTEXGENFVPROC glTexGenfv;
RGLSYMGLTEXGENIVPROC glTexGeniv;
RGLSYMGLGETTEXGENDVPROC glGetTexGendv;
RGLSYMGLGETTEXGENFVPROC glGetTexGenfv;
RGLSYMGLGETTEXGENIVPROC glGetTexGeniv;
RGLSYMGLTEXENVFPROC glTexEnvf;
RGLSYMGLTEXENVIPROC glTexEnvi;
RGLSYMGLTEXENVFVPROC glTexEnvfv;
RGLSYMGLTEXENVIVPROC glTexEnviv;
RGLSYMGLGETTEXENVFVPROC glGetTexEnvfv;
RGLSYMGLGETTEXENVIVPROC glGetTexEnviv;
RGLSYMGLTEXPARAMETERFPROC glTexParameterf;
RGLSYMGLTEXPARAMETERIPROC glTexParameteri;
RGLSYMGLTEXPARAMETERFVPROC glTexParameterfv;
RGLSYMGLTEXPARAMETERIVPROC glTexParameteriv;
RGLSYMGLGETTEXPARAMETERFVPROC glGetTexParameterfv;
RGLSYMGLGETTEXPARAMETERIVPROC glGetTexParameteriv;
RGLSYMGLGETTEXLEVELPARAMETERFVPROC glGetTexLevelParameterfv;
RGLSYMGLGETTEXLEVELPARAMETERIVPROC glGetTexLevelParameteriv;
RGLSYMGLTEXIMAGE1DPROC glTexImage1D;
RGLSYMGLTEXIMAGE2DPROC glTexImage2D;
RGLSYMGLGETTEXIMAGEPROC glGetTexImage;
RGLSYMGLGENTEXTURESPROC glGenTextures;
RGLSYMGLDELETETEXTURESPROC glDeleteTextures;
RGLSYMGLBINDTEXTUREPROC glBindTexture;
RGLSYMGLPRIORITIZETEXTURESPROC glPrioritizeTextures;
RGLSYMGLARETEXTURESRESIDENTPROC glAreTexturesResident;
RGLSYMGLISTEXTUREPROC glIsTexture;
RGLSYMGLTEXSUBIMAGE1DPROC glTexSubImage1D;
RGLSYMGLTEXSUBIMAGE2DPROC glTexSubImage2D;
RGLSYMGLCOPYTEXIMAGE1DPROC glCopyTexImage1D;
RGLSYMGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
RGLSYMGLCOPYTEXSUBIMAGE1DPROC glCopyTexSubImage1D;
RGLSYMGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;
RGLSYMGLMAP1DPROC glMap1d;
RGLSYMGLMAP1FPROC glMap1f;
RGLSYMGLMAP2DPROC glMap2d;
RGLSYMGLMAP2FPROC glMap2f;
RGLSYMGLGETMAPDVPROC glGetMapdv;
RGLSYMGLGETMAPFVPROC glGetMapfv;
RGLSYMGLGETMAPIVPROC glGetMapiv;
RGLSYMGLEVALCOORD1DPROC glEvalCoord1d;
RGLSYMGLEVALCOORD1FPROC glEvalCoord1f;
RGLSYMGLEVALCOORD1DVPROC glEvalCoord1dv;
RGLSYMGLEVALCOORD1FVPROC glEvalCoord1fv;
RGLSYMGLEVALCOORD2DPROC glEvalCoord2d;
RGLSYMGLEVALCOORD2FPROC glEvalCoord2f;
RGLSYMGLEVALCOORD2DVPROC glEvalCoord2dv;
RGLSYMGLEVALCOORD2FVPROC glEvalCoord2fv;
RGLSYMGLMAPGRID1DPROC glMapGrid1d;
RGLSYMGLMAPGRID1FPROC glMapGrid1f;
RGLSYMGLMAPGRID2DPROC glMapGrid2d;
RGLSYMGLMAPGRID2FPROC glMapGrid2f;
RGLSYMGLEVALPOINT1PROC glEvalPoint1;
RGLSYMGLEVALPOINT2PROC glEvalPoint2;
RGLSYMGLEVALMESH1PROC glEvalMesh1;
RGLSYMGLEVALMESH2PROC glEvalMesh2;
RGLSYMGLFOGFPROC glFogf;
RGLSYMGLFOGIPROC glFogi;
RGLSYMGLFOGFVPROC glFogfv;
RGLSYMGLFOGIVPROC glFogiv;
RGLSYMGLFEEDBACKBUFFERPROC glFeedbackBuffer;
RGLSYMGLPASSTHROUGHPROC glPassThrough;
RGLSYMGLSELECTBUFFERPROC glSelectBuffer;
RGLSYMGLINITNAMESPROC glInitNames;
RGLSYMGLLOADNAMEPROC glLoadName;
RGLSYMGLPUSHNAMEPROC glPushName;
RGLSYMGLPOPNAMEPROC glPopName;
RGLSYMGLDRAWRANGEELEMENTSPROC glDrawRangeElements;
RGLSYMGLTEXIMAGE3DPROC glTexImage3D;
RGLSYMGLTEXSUBIMAGE3DPROC glTexSubImage3D;
RGLSYMGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D;
RGLSYMGLCOLORTABLEPROC glColorTable;
RGLSYMGLCOLORSUBTABLEPROC glColorSubTable;
RGLSYMGLCOLORTABLEPARAMETERIVPROC glColorTableParameteriv;
RGLSYMGLCOLORTABLEPARAMETERFVPROC glColorTableParameterfv;
RGLSYMGLCOPYCOLORSUBTABLEPROC glCopyColorSubTable;
RGLSYMGLCOPYCOLORTABLEPROC glCopyColorTable;
RGLSYMGLGETCOLORTABLEPROC glGetColorTable;
RGLSYMGLGETCOLORTABLEPARAMETERFVPROC glGetColorTableParameterfv;
RGLSYMGLGETCOLORTABLEPARAMETERIVPROC glGetColorTableParameteriv;
RGLSYMGLBLENDEQUATIONPROC glBlendEquation;
RGLSYMGLBLENDCOLORPROC glBlendColor;
RGLSYMGLHISTOGRAMPROC glHistogram;
RGLSYMGLRESETHISTOGRAMPROC glResetHistogram;
RGLSYMGLGETHISTOGRAMPROC glGetHistogram;
RGLSYMGLGETHISTOGRAMPARAMETERFVPROC glGetHistogramParameterfv;
RGLSYMGLGETHISTOGRAMPARAMETERIVPROC glGetHistogramParameteriv;
RGLSYMGLMINMAXPROC glMinmax;
RGLSYMGLRESETMINMAXPROC glResetMinmax;
RGLSYMGLGETMINMAXPROC glGetMinmax;
RGLSYMGLGETMINMAXPARAMETERFVPROC glGetMinmaxParameterfv;
RGLSYMGLGETMINMAXPARAMETERIVPROC glGetMinmaxParameteriv;
RGLSYMGLCONVOLUTIONFILTER1DPROC glConvolutionFilter1D;
RGLSYMGLCONVOLUTIONFILTER2DPROC glConvolutionFilter2D;
RGLSYMGLCONVOLUTIONPARAMETERFPROC glConvolutionParameterf;
RGLSYMGLCONVOLUTIONPARAMETERFVPROC glConvolutionParameterfv;
RGLSYMGLCONVOLUTIONPARAMETERIPROC glConvolutionParameteri;
RGLSYMGLCONVOLUTIONPARAMETERIVPROC glConvolutionParameteriv;
RGLSYMGLCOPYCONVOLUTIONFILTER1DPROC glCopyConvolutionFilter1D;
RGLSYMGLCOPYCONVOLUTIONFILTER2DPROC glCopyConvolutionFilter2D;
RGLSYMGLGETCONVOLUTIONFILTERPROC glGetConvolutionFilter;
RGLSYMGLGETCONVOLUTIONPARAMETERFVPROC glGetConvolutionParameterfv;
RGLSYMGLGETCONVOLUTIONPARAMETERIVPROC glGetConvolutionParameteriv;
RGLSYMGLSEPARABLEFILTER2DPROC glSeparableFilter2D;
RGLSYMGLGETSEPARABLEFILTERPROC glGetSeparableFilter;
RGLSYMGLACTIVETEXTUREPROC glActiveTexture;
RGLSYMGLCLIENTACTIVETEXTUREPROC glClientActiveTexture;
RGLSYMGLCOMPRESSEDTEXIMAGE1DPROC glCompressedTexImage1D;
RGLSYMGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;
RGLSYMGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;
RGLSYMGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D;
RGLSYMGLGETCOMPRESSEDTEXIMAGEPROC glGetCompressedTexImage;
RGLSYMGLMULTITEXCOORD1DPROC glMultiTexCoord1d;
RGLSYMGLMULTITEXCOORD1DVPROC glMultiTexCoord1dv;
RGLSYMGLMULTITEXCOORD1FPROC glMultiTexCoord1f;
RGLSYMGLMULTITEXCOORD1FVPROC glMultiTexCoord1fv;
RGLSYMGLMULTITEXCOORD1IPROC glMultiTexCoord1i;
RGLSYMGLMULTITEXCOORD1IVPROC glMultiTexCoord1iv;
RGLSYMGLMULTITEXCOORD1SPROC glMultiTexCoord1s;
RGLSYMGLMULTITEXCOORD1SVPROC glMultiTexCoord1sv;
RGLSYMGLMULTITEXCOORD2DPROC glMultiTexCoord2d;
RGLSYMGLMULTITEXCOORD2DVPROC glMultiTexCoord2dv;
RGLSYMGLMULTITEXCOORD2FPROC glMultiTexCoord2f;
RGLSYMGLMULTITEXCOORD2FVPROC glMultiTexCoord2fv;
RGLSYMGLMULTITEXCOORD2IPROC glMultiTexCoord2i;
RGLSYMGLMULTITEXCOORD2IVPROC glMultiTexCoord2iv;
RGLSYMGLMULTITEXCOORD2SPROC glMultiTexCoord2s;
RGLSYMGLMULTITEXCOORD2SVPROC glMultiTexCoord2sv;
RGLSYMGLMULTITEXCOORD3DPROC glMultiTexCoord3d;
RGLSYMGLMULTITEXCOORD3DVPROC glMultiTexCoord3dv;
RGLSYMGLMULTITEXCOORD3FPROC glMultiTexCoord3f;
RGLSYMGLMULTITEXCOORD3FVPROC glMultiTexCoord3fv;
RGLSYMGLMULTITEXCOORD3IPROC glMultiTexCoord3i;
RGLSYMGLMULTITEXCOORD3IVPROC glMultiTexCoord3iv;
RGLSYMGLMULTITEXCOORD3SPROC glMultiTexCoord3s;
RGLSYMGLMULTITEXCOORD3SVPROC glMultiTexCoord3sv;
RGLSYMGLMULTITEXCOORD4DPROC glMultiTexCoord4d;
RGLSYMGLMULTITEXCOORD4DVPROC glMultiTexCoord4dv;
RGLSYMGLMULTITEXCOORD4FPROC glMultiTexCoord4f;
RGLSYMGLMULTITEXCOORD4FVPROC glMultiTexCoord4fv;
RGLSYMGLMULTITEXCOORD4IPROC glMultiTexCoord4i;
RGLSYMGLMULTITEXCOORD4IVPROC glMultiTexCoord4iv;
RGLSYMGLMULTITEXCOORD4SPROC glMultiTexCoord4s;
RGLSYMGLMULTITEXCOORD4SVPROC glMultiTexCoord4sv;
RGLSYMGLLOADTRANSPOSEMATRIXDPROC glLoadTransposeMatrixd;
RGLSYMGLLOADTRANSPOSEMATRIXFPROC glLoadTransposeMatrixf;
RGLSYMGLMULTTRANSPOSEMATRIXDPROC glMultTransposeMatrixd;
RGLSYMGLMULTTRANSPOSEMATRIXFPROC glMultTransposeMatrixf;
RGLSYMGLSAMPLECOVERAGEPROC glSampleCoverage;
RGLSYMGLACTIVETEXTUREARBPROC glActiveTextureARB;
RGLSYMGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
RGLSYMGLMULTITEXCOORD1DARBPROC glMultiTexCoord1dARB;
RGLSYMGLMULTITEXCOORD1DVARBPROC glMultiTexCoord1dvARB;
RGLSYMGLMULTITEXCOORD1FARBPROC glMultiTexCoord1fARB;
RGLSYMGLMULTITEXCOORD1FVARBPROC glMultiTexCoord1fvARB;
RGLSYMGLMULTITEXCOORD1IARBPROC glMultiTexCoord1iARB;
RGLSYMGLMULTITEXCOORD1IVARBPROC glMultiTexCoord1ivARB;
RGLSYMGLMULTITEXCOORD1SARBPROC glMultiTexCoord1sARB;
RGLSYMGLMULTITEXCOORD1SVARBPROC glMultiTexCoord1svARB;
RGLSYMGLMULTITEXCOORD2DARBPROC glMultiTexCoord2dARB;
RGLSYMGLMULTITEXCOORD2DVARBPROC glMultiTexCoord2dvARB;
RGLSYMGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
RGLSYMGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB;
RGLSYMGLMULTITEXCOORD2IARBPROC glMultiTexCoord2iARB;
RGLSYMGLMULTITEXCOORD2IVARBPROC glMultiTexCoord2ivARB;
RGLSYMGLMULTITEXCOORD2SARBPROC glMultiTexCoord2sARB;
RGLSYMGLMULTITEXCOORD2SVARBPROC glMultiTexCoord2svARB;
RGLSYMGLMULTITEXCOORD3DARBPROC glMultiTexCoord3dARB;
RGLSYMGLMULTITEXCOORD3DVARBPROC glMultiTexCoord3dvARB;
RGLSYMGLMULTITEXCOORD3FARBPROC glMultiTexCoord3fARB;
RGLSYMGLMULTITEXCOORD3FVARBPROC glMultiTexCoord3fvARB;
RGLSYMGLMULTITEXCOORD3IARBPROC glMultiTexCoord3iARB;
RGLSYMGLMULTITEXCOORD3IVARBPROC glMultiTexCoord3ivARB;
RGLSYMGLMULTITEXCOORD3SARBPROC glMultiTexCoord3sARB;
RGLSYMGLMULTITEXCOORD3SVARBPROC glMultiTexCoord3svARB;
RGLSYMGLMULTITEXCOORD4DARBPROC glMultiTexCoord4dARB;
RGLSYMGLMULTITEXCOORD4DVARBPROC glMultiTexCoord4dvARB;
RGLSYMGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB;
RGLSYMGLMULTITEXCOORD4FVARBPROC glMultiTexCoord4fvARB;
RGLSYMGLMULTITEXCOORD4IARBPROC glMultiTexCoord4iARB;
RGLSYMGLMULTITEXCOORD4IVARBPROC glMultiTexCoord4ivARB;
RGLSYMGLMULTITEXCOORD4SARBPROC glMultiTexCoord4sARB;
RGLSYMGLMULTITEXCOORD4SVARBPROC glMultiTexCoord4svARB;
RGLSYMGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
RGLSYMGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES;
RGLSYMGLBINDTEXTURESPROC glBindTextures;

#ifdef __cplusplus
}
#endif
#endif /* __NX_GLSYM_H__ */
