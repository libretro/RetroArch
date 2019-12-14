#ifndef SHARED_H
#define SHARED_H

/* Shared with other .c */
extern float _vita2d_ortho_matrix[4*4];
extern SceGxmContext *_vita2d_context;
extern SceGxmVertexProgram *_vita2d_colorVertexProgram;
extern SceGxmFragmentProgram *_vita2d_colorFragmentProgram;
extern SceGxmVertexProgram *_vita2d_textureVertexProgram;
extern SceGxmFragmentProgram *_vita2d_textureFragmentProgram;
extern SceGxmVertexProgram *_vita2d_textureTintVertexProgram;
extern SceGxmFragmentProgram *_vita2d_textureTintFragmentProgram;
extern const SceGxmProgramParameter *_vita2d_colorWvpParam;
extern const SceGxmProgramParameter *_vita2d_textureWvpParam;
extern const SceGxmProgramParameter *_vita2d_textureTintWvpParam;


#endif
