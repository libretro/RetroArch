#ifndef HUNTER_H
#define HUNTER_H

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include <GLFW/glfw3.h>

EXTERNC bool hunter_inited;
EXTERNC void hunter_init();
EXTERNC void hunter_draw();

#endif // HUNTER_H
