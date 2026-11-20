# OpenGL 4 GLSL shaders

GLSL requiring OpenGL 4.x belongs here; effects belong in effects/.
GLSL 1.10/1.20 and GLSL 1.30+ draw shaders live in gl2_glsl_shaders and
gl3_glsl_shaders respectively. Cg sources live in gl_cg_shaders.
OpenGL 4 contexts reuse those lower-version draw shaders. PyroWave's
GLSL 4.30 compute kernels remain with the reusable codec in libretro-common.
