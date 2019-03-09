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

#include <ps2_hashtable.h>

#include <stdio.h>
#include <string.h>
#include <murmur3.h>

HashTable *crb_hashtable_create(uint32_t _size) {
   HashTable *tbl = malloc(sizeof(HashTable));
   tbl->size = 0;
   tbl->capacity = 0;
   tbl->table = NULL;
   return tbl;
}

void crb_hashtable_destroy(HashTable **_tbl) {
   if (_tbl == NULL || *_tbl == NULL) return;

   if ((*_tbl)->table != NULL) {
      free((*_tbl)->table);
      (*_tbl)->table = NULL;
   }

   free(*_tbl);

   *_tbl = NULL;
}

void crb_hashtable_grow(HashTable *_tbl) {
   unsigned int oldCapacity;
   struct table_entry **oldTable;
   unsigned int i;

   if (_tbl == NULL) return;

   oldCapacity = _tbl->capacity;
   if (_tbl->capacity > 0) {
      _tbl->capacity *= 2;
   } else {
      _tbl->capacity = 32;
   }
   oldTable = _tbl->table;
   _tbl->table = calloc(_tbl->capacity, sizeof(struct table_entry*));
   for (i = 0; i < oldCapacity; i++) {
      if (oldTable[i] != NULL) {
         crb_hashtable_insert(_tbl, oldTable[i]->key, oldTable[i]->value);
         free(oldTable[i]);
      }
   }
   free(oldTable);
}

void crb_hashtable_hash(const char *_key, uint32_t *hash) {
   MurmurHash3_x86_32(_key, strlen(_key), 0, hash);
}

bool crb_hashtable_insert(HashTable *_tbl, const char *_key, void *_data) {
   struct table_entry *entry, *next_entry;
   uint32_t index;

   if (_tbl == NULL || _key == NULL || _data == NULL) return false;

   if (_tbl->size == _tbl->capacity) {
      crb_hashtable_grow(_tbl);
   }

   entry = malloc(sizeof(struct table_entry));
   if (entry == NULL) return false;
   crb_hashtable_hash(_key, &entry->hash);
   index = entry->hash % _tbl->capacity;
   entry->key = strdup(_key);
   entry->value = _data;
   entry->next = NULL;

   if (_tbl->table[index] == NULL) {
      _tbl->table[index] = entry;
   } else {
      next_entry = _tbl->table[index];
      while (next_entry != NULL) {
         if (next_entry->next == NULL) {
               next_entry->next = entry;
               break;
         }
         next_entry = next_entry->next;
      }
   }
   _tbl->size++;
   return true;
}

void *crb_hashtable_find(HashTable *_tbl, const char *_key) {
   uint32_t hash;
   struct table_entry *entry;

   if (_tbl == NULL || _key == NULL) return NULL;

   crb_hashtable_hash(_key, &hash);

   for (entry = _tbl->table[hash % _tbl->capacity]; entry != NULL; entry = entry->next) {
      if (strcmp(_key, entry->key) == 0) return entry->value;
   }

   return NULL;
}

void *crb_hashtable_remove(HashTable *_tbl, const char *_key) {
   uint32_t hash;
   struct table_entry *entry, *sibling;
   bool top = true;
   void *data;

   if (_tbl == NULL || _key == NULL) return NULL;

   crb_hashtable_hash(_key, &hash);

   for (entry = _tbl->table[hash % _tbl->capacity]; entry != NULL; sibling = entry, entry = entry->next) {
      if (strcmp(_key, entry->key) == 0) {
         break;
      } else {
         top = false;
      }
   }
   if (entry != NULL) {
      data = entry->value;
      if (top) {
         if (entry->next != NULL) {
               _tbl->table[hash % _tbl->capacity] = entry->next;
         } else {
               _tbl->table[hash % _tbl->capacity] = NULL;
         }
      } else {
         if (entry->next != NULL) {
               sibling->next = entry->next;
         } else {
               sibling->next = NULL;
         }
      }
      free(entry->key);
      free(entry);
      entry = NULL;
      _tbl->size--;
      return data;
   }

   return NULL;
}
