/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2018 - Francisco Javier Trujillo Mata - fjtrujy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PS2_HASHTABLE_H
#define PS2_HASHTABLE_H

#include <stdint.h>
#include <stdbool.h>

struct table_entry {
   char               *key;
   void               *value;
   uint32_t            hash;
   struct table_entry *next;
};

struct hash_table {
   uint32_t       capacity;
   uint32_t       size;
   struct table_entry **table;
};

typedef struct hash_table HashTable;

HashTable *crb_hashtable_create(uint32_t _size);
void       crb_hashtable_destroy(HashTable **_tbl);

void       crb_hashtable_grow(HashTable *_tbl);
void       crb_hashtable_hash(const char *_key, uint32_t *hash);
bool       crb_hashtable_insert(HashTable *_tbl, const char *_key, void *_data);
void      *crb_hashtable_find(HashTable *_tbl, const char *_key);
void      *crb_hashtable_remove(HashTable *_tbl, const char *_key);

#endif
