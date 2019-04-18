# Autogenerate GL extension loaders

## OpenGL desktop

Use Khronos' recent [header](www.opengl.org/registry/api/glext.h).

    ./glgen.py /usr/include/GL/glext.h glsym_gl.h glsym_gl.c

## OpenGL ES

    ./glgen.py /usr/include/GLES2/gl2ext.h glsym_es2.h glsym_es2.c
