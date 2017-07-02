#ifndef HUNTER_H
#define HUNTER_H

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include <GLFW/glfw3.h>

EXTERNC void hunter_init();
EXTERNC void hunter_draw(bool* deinit);

#endif // HUNTER_H
