/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef KHRN_GENERIC_MAP_RELOCATABLE
   #include "middleware/khronos/common/khrn_mem.h"
#endif

typedef struct {
   KHRN_GENERIC_MAP_KEY_T key;
   KHRN_GENERIC_MAP_VALUE_T value;
} KHRN_GENERIC_MAP(ENTRY_T);

typedef struct {
   uint32_t entries;
   uint32_t deletes;

#ifdef KHRN_GENERIC_MAP_RELOCATABLE
   MEM_HANDLE_T storage;
#else
   KHRN_GENERIC_MAP(ENTRY_T) *storage;
#endif
   uint32_t capacity;
} KHRN_GENERIC_MAP(T);

/*
   bool khrn_generic_map(init)(KHRN_GENERIC_MAP(T) *map, uint32_t capacity)

   Initialises the map

   Preconditions:

   map is a valid pointer to an uninitialised KHRN_GENERIC_MAP(T) structure
   capacity >= 8

   Postconditions:

   Either:
   - true is returned and the structure that map points to is valid, or
   - false is returned and map is still uninitialised
*/

extern bool khrn_generic_map(init)(KHRN_GENERIC_MAP(T) *map, uint32_t capacity);

/*
   void khrn_generic_map(term)(KHRN_GENERIC_MAP(T) *map)

   Terminates the map

   Preconditions:

   map is a valid pointer to a map whose values are of type X:
   - where type X does not imply any external references, or
   - KHRN_GENERIC_MAP_RELEASE_VALUE releases all of these references, or
   - the map is empty

   Postconditions:

   The structure map points to is uninitialised
*/

extern void khrn_generic_map(term)(KHRN_GENERIC_MAP(T) *map);

/*
   bool khrn_generic_map(insert)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key, KHRN_GENERIC_MAP_VALUE_T value)

   Inserts value into map with key key. If another value is already in the map
   with this key, the function will not fail; the new value replaces the old

   Preconditions:

   map is a valid pointer to a map whose values are of type X
   value is a valid value of type X

   Postconditions:

   If the function succeeds:
   - true is returned
   - key key is now associated with value
   otherwise:
   - false is returned
   - map is unchanged
*/

extern bool khrn_generic_map(insert)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key, KHRN_GENERIC_MAP_VALUE_T value);

/*
   bool khrn_generic_map(delete)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key)

   If present, deletes the element identified by key from the map and returns
   true. If not present, returns false

   Preconditions:

   map is a valid pointer to a map

   Postconditions:

   key is not present in map
*/

extern bool khrn_generic_map(delete)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key);

extern uint32_t khrn_generic_map(get_count)(KHRN_GENERIC_MAP(T) *map);

/*
   KHRN_GENERIC_MAP_VALUE_T khrn_generic_map(lookup)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key)

   Returns the element of the map identified by key, or
   KHRN_GENERIC_MAP_VALUE_NONE if no such element exists in the map

   Preconditions:

   map is a valid pointer to a map whose elements are of type X

   Postconditions:

   result is either KHRN_GENERIC_MAP_VALUE_NONE or a valid value of type X
*/

extern KHRN_GENERIC_MAP_VALUE_T khrn_generic_map(lookup)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key);

/*
   KHRN_GENERIC_MAP_VALUE_T khrn_generic_map(lookup_locked)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key, void *storage)

   Returns the element of the map identified by key, or
   KHRN_GENERIC_MAP_VALUE_NONE if no such element exists in the map

   Preconditions:

   map is a valid pointer to a map whose elements are of type X
   storage is the locked pointer to map->storage

   Postconditions:

   result is either KHRN_GENERIC_MAP_VALUE_NONE or a valid value of type X
*/

#ifdef KHRN_GENERIC_MAP_RELOCATABLE
   extern KHRN_GENERIC_MAP_VALUE_T khrn_generic_map(lookup_locked)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key, void *storage);
#endif

/*
   void khrn_generic_map(iterate)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP(CALLBACK_T) func, void *data)

   Runs the given callback function once for every (key, value) pair in the map.
   Also passes data to the function

   Implementation notes:

   The iterator function is allowed to delete the element it is given, but not
   modify the structure of map in any other way (eg by adding new elements)

   Preconditions:

   map is a valid pointer to a map of element type X
   data, map satisfy P
   func satisfies:
   [
      void func(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key, KHRN_GENERIC_MAP_VALUE_T value, void *data)

      Preconditions:

      data, map satisfy P
      value is of type X
      key is a key in map
      map[key] == value

      Postconditions:

      func does not alter map, except possibly by deleting the element it is given
      value is of type Y
   ]

   Postconditions:

   func has been called on every (key, value) pair in the map
   map is a valid pointer to a map of element type Y
*/

typedef void (*KHRN_GENERIC_MAP(CALLBACK_T))(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP_KEY_T key, KHRN_GENERIC_MAP_VALUE_T value, void *);
extern void khrn_generic_map(iterate)(KHRN_GENERIC_MAP(T) *map, KHRN_GENERIC_MAP(CALLBACK_T) func, void *);
