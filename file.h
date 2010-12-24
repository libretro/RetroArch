#ifndef __SSNES_FILE_H
#define __SSNES_FILE_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

ssize_t read_file(FILE *file, void **buf);

void load_state(const char* path, uint8_t* data, size_t size);
void write_file(const char* path, uint8_t* data, size_t size);
void load_save_file(const char* path, int type);
void save_file(const char* path, int type);

#endif
