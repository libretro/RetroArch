/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __CONFIG_FILE_H
#define __CONFIG_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../boolean.h"
#include <stdio.h>
#include <stddef.h>

typedef struct config_file config_file_t;

/////
// Config file format
// - # are treated as comments. Rest of the line is ignored.
// - Format is: key = value. There can be as many spaces as you like in-between.
// - Value can be wrapped inside "" for multiword strings. (foo = "hai u")
//
// - #include includes a config file in-place.
// Path is relative to where config file was loaded unless an absolute path is chosen.
// Key/value pairs from an #include are read-only, and cannot be modified.

// Loads a config file. Returns NULL if file doesn't exist.
// NULL path will create an empty config file.
config_file_t *config_file_new(const char *path);
// Load a config file from a string.
config_file_t *config_file_new_from_string(const char *from_string);
// Frees config file.
void config_file_free(config_file_t *conf);

// Loads a new config, and appends its data to conf.
// The key-value pairs of the new config file takes priority over the old.
bool config_append_file(config_file_t *conf, const char *path);

// All extract functions return true when value is valid and exists.
// Returns false otherwise.

bool config_entry_exists(config_file_t *conf, const char *entry);

struct config_entry_list;
struct config_file_entry
{
   const char *key;
   const char *value;
   const struct config_entry_list *next; // Used internally. Opaque here.
};

bool config_get_entry_list_head(config_file_t *conf, struct config_file_entry *entry);
bool config_get_entry_list_next(struct config_file_entry *entry);

// Extracts a double from config file.
bool config_get_double(config_file_t *conf, const char *entry, double *in);
// Extracts a float from config file.
bool config_get_float(config_file_t *conf, const char *entry, float *in);
// Extracts an int from config file.
bool config_get_int(config_file_t *conf, const char *entry, int *in);
// Extracts an uint from config file.
bool config_get_uint(config_file_t *conf, const char *entry, unsigned *in);
// Extracts an uint64 from config file.
bool config_get_uint64(config_file_t *conf, const char *entry, uint64_t *in);
// Extracts an unsigned int from config file treating input as hex.
bool config_get_hex(config_file_t *conf, const char *entry, unsigned *in);
// Extracts a single char. If value consists of several chars, this is an error.
bool config_get_char(config_file_t *conf, const char *entry, char *in);
// Extracts an allocated string in *in. This must be free()-d if this function succeeds.
bool config_get_string(config_file_t *conf, const char *entry, char **in);
// Extracts a string to a preallocated buffer. Avoid memory allocation.
bool config_get_array(config_file_t *conf, const char *entry, char *in, size_t size);
// Extracts a string to a preallocated buffer. Avoid memory allocation. Recognized magic like ~/. Similar to config_get_array() otherwise.
bool config_get_path(config_file_t *conf, const char *entry, char *in, size_t size);
// Extracts a boolean from config. Valid boolean true are "true" and "1". Valid false are "false" and "0". Other values will be treated as an error.
bool config_get_bool(config_file_t *conf, const char *entry, bool *in);

// Setters. Similar to the getters. Will not write to entry if the entry
// was obtained from an #include.
void config_set_double(config_file_t *conf, const char *entry, double value);
void config_set_float(config_file_t *conf, const char *entry, float value);
void config_set_int(config_file_t *conf, const char *entry, int val);
void config_set_hex(config_file_t *conf, const char *entry, unsigned val);
void config_set_uint64(config_file_t *conf, const char *entry, uint64_t val);
void config_set_char(config_file_t *conf, const char *entry, char val);
void config_set_string(config_file_t *conf, const char *entry, const char *val);
void config_set_path(config_file_t *conf, const char *entry, const char *val);
void config_set_bool(config_file_t *conf, const char *entry, bool val);

// Write the current config to a file.
bool config_file_write(config_file_t *conf, const char *path);

// Dump the current config to an already opened file. Does not close the file.
void config_file_dump(config_file_t *conf, FILE *file);
// Also dumps inherited values, useful for logging.
void config_file_dump_all(config_file_t *conf, FILE *file);

#ifdef __cplusplus
}
#endif

#endif

