/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#include "json_parser.h"

#include "../deps/lua/src/lctype.h"

int parse_json_field(const char** json_ptr, json_field_t* field) {
  int result;

  field->value_start = *json_ptr;

  switch (**json_ptr)
  {
    case '"': /* quoted string */
      ++(*json_ptr);
      while (**json_ptr != '"') {
        if (**json_ptr == '\\')
          ++(*json_ptr);

        if (**json_ptr == '\0')
          return JSON_INVALID_JSON;

        ++(*json_ptr);
      }
      ++(*json_ptr);
      break;

    case '-':
    case '+': /* signed number */
      ++(*json_ptr);
      /* fallthrough to number */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': /* number */
      do {
        ++(*json_ptr);
      } while (**json_ptr >= '0' && **json_ptr <= '9');
      if (**json_ptr == '.') {
        do {
          ++(*json_ptr);
        } while (**json_ptr >= '0' && **json_ptr <= '9');
      }
      break;

    case '[': /* array */
      result = parse_json_array(json_ptr, field);
      if (result != JSON_OK)
        return result;

      break;

    case '{': /* object */
      result = parse_json_object(json_ptr, NULL, 0, &field->array_size);
      if (result != JSON_OK)
        return result;

      break;

    default: /* non-quoted text [true,false,null] */
      if (!isalpha((unsigned char)**json_ptr))
        return JSON_INVALID_JSON;

      do {
        ++(*json_ptr);
      } while (isalnum((unsigned char)**json_ptr));
      break;
  }

  field->value_end = *json_ptr;
  return JSON_OK;
}

int parse_json_array(const char** json_ptr, json_field_t* field) {
  json_field_t unused_field;
  const char* json = *json_ptr;
  int result;

  if (*json != '[')
    return JSON_INVALID_JSON;
  ++json;

  field->array_size = 0;
  if (*json != ']') {
    do
    {
      while (isspace((unsigned char)*json))
        ++json;

      result = parse_json_field(&json, &unused_field);
      if (result != JSON_OK)
        return result;

      ++field->array_size;

      while (isspace((unsigned char)*json))
        ++json;

      if (*json != ',')
        break;

      ++json;
    } while (1);

    if (*json != ']')
      return JSON_INVALID_JSON;
  }

  *json_ptr = ++json;
  return JSON_OK;
}

int get_json_next_field(json_object_field_iterator_t* iterator) {
  const char* json = iterator->json;

  while (isspace((unsigned char)*json))
    ++json;

  if (*json != '"')
    return JSON_INVALID_JSON;

  iterator->field.name = ++json;
  while (*json != '"') {
    if (!*json)
      return JSON_INVALID_JSON;
    ++json;
  }
  iterator->name_len = json - iterator->field.name;
  ++json;

  while (isspace((unsigned char)*json))
    ++json;

  if (*json != ':')
    return JSON_INVALID_JSON;

  ++json;

  while (isspace((unsigned char)*json))
    ++json;

  if (parse_json_field(&json, &iterator->field) < 0)
    return JSON_INVALID_JSON;

  while (isspace((unsigned char)*json))
    ++json;

  iterator->json = json;
  return JSON_OK;
}

int parse_json_object(const char** json_ptr, json_field_t* fields, size_t field_count, unsigned* fields_seen) {
  json_object_field_iterator_t iterator;
  const char* json = *json_ptr;
  size_t i;
  unsigned num_fields = 0;
  int result;

  if (fields_seen)
    *fields_seen = 0;

  for (i = 0; i < field_count; ++i)
    fields[i].value_start = fields[i].value_end = NULL;

  if (*json != '{')
    return JSON_INVALID_JSON;
  ++json;

  if (*json == '}') {
    *json_ptr = ++json;
    return JSON_OK;
  }

  memset(&iterator, 0, sizeof(iterator));
  iterator.json = json;

  do
  {
    result = get_json_next_field(&iterator);
    if (result != JSON_OK)
      return result;

    for (i = 0; i < field_count; ++i) {
      if (!fields[i].value_start && strncmp(fields[i].name, iterator.field.name, iterator.name_len) == 0 &&
          fields[i].name[iterator.name_len] == '\0') {
        fields[i].value_start = iterator.field.value_start;
        fields[i].value_end = iterator.field.value_end;
        fields[i].array_size = iterator.field.array_size;
        break;
      }
    }

    ++num_fields;
    if (*iterator.json != ',')
      break;

    ++iterator.json;
  } while (1);

  if (*iterator.json != '}')
    return JSON_INVALID_JSON;

  if (fields_seen)
    *fields_seen = num_fields;

  *json_ptr = ++iterator.json;
  return JSON_OK;
}

