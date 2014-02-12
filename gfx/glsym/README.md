# Autogenerate GL extension loaders

## OpenGL desktop

Use Khronos recent [header](www.opengl.org/registry/api/glext.h).

    ./glgen.py /usr/include/GL/glext.h glsym\_gl.h glsym\_gl.c

## OpenGL ES

    ./glgen.py /usr/include/GLES/gl2ext.h glsym\_es2.h glsym\_es2.c

