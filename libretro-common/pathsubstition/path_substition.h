#ifndef __LIBRETRO_PATH_SUBSTITUTION_H
#define __LIBRETRO_PATH_SUBSTITUTION_H

#include <stdlib.h>

#include <retroarch.h>
#include <lists/string_list.h>

char* get_substitute_path(const char* path);
void substitute_path(char* path);

#endif
