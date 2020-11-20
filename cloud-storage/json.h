/* Copyright  (C) 2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (json.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _JSON_H
#define _JSON_H

#include <retro_common_api.h>

#include <boolean.h>
#include <formats/jsonsax.h>

RETRO_BEGIN_DECLS

enum json_node_type_t
{
   NULL_VALUE,
   BOOLEAN_VALUE,
   STRING_VALUE,
   INTEGER_VALUE,
   DOUBLE_VALUE,
   ARRAY_VALUE,
   OBJECT_VALUE
};

struct json_array_item_t;
struct json_array_t
{
   struct json_array_item_t *element;
};

struct json_map_pair_t;
struct json_map_t
{
   struct json_map_pair_t *pair;
};

struct json_string_t
{
   char *string;
   size_t length;
};

union json_node_value_t
{
   void *null_value;
   bool boolean_value;
   struct json_string_t string_value;
   int64_t int_value;
   double double_value;
   struct json_array_t array_value;
   struct json_map_t map_value;
};

struct json_node_t
{
   enum json_node_type_t node_type;
   union json_node_value_t value;
};

struct json_array_item_t
{
   struct json_node_t *value;
   struct json_array_item_t *next;
};

struct json_map_pair_t
{
   char *key;
   size_t key_len;
   struct json_node_t *value;
   struct json_map_pair_t *next;
};

struct json_node_t *string_to_json(char *s);

bool json_map_have_key(struct json_map_t map, const char *key);

struct json_node_t *json_map_get_value(struct json_map_t map, const char *key);

bool json_map_have_value_null(struct json_map_t map, const char *key);

bool json_map_get_value_boolean(struct json_map_t map, const char *key, bool *value);

bool json_map_get_value_string(struct json_map_t map, const char *key, char **value, size_t *length);

bool json_map_get_value_int(struct json_map_t map, const char *key, int64_t *value);

bool json_map_get_value_double(struct json_map_t map, const char *key, double *value);

bool json_map_get_value_array(struct json_map_t map, const char *key, struct json_array_t **value);

bool json_map_get_value_map(struct json_map_t map, const char *key, struct json_map_t **value);

void json_node_free(struct json_node_t *node);

RETRO_END_DECLS

#endif
