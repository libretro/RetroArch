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

#ifndef RETRO_ARCH_JSON_PARSER_H
#define RETRO_ARCH_JSON_PARSER_H

#include <stdint.h>
#include <stdlib.h>

#define JSON_NEW_FIELD(n) {n,0,0,0}

typedef struct json_field_t {
    const char* name;
    const char* value_start;
    const char* value_end;
    unsigned array_size;
} json_field_t;

typedef struct json_object_field_iterator_t {
    json_field_t field;
    const char* json;
    size_t name_len;
} json_object_field_iterator_t;

enum {
    JSON_OK = 0,
    JSON_INVALID_LUA_OPERAND = -1,
    JSON_INVALID_MEMORY_OPERAND = -2,
    JSON_INVALID_CONST_OPERAND = -3,
    JSON_INVALID_FP_OPERAND = -4,
    JSON_INVALID_CONDITION_TYPE = -5,
    JSON_INVALID_OPERATOR = -6,
    JSON_INVALID_REQUIRED_HITS = -7,
    JSON_DUPLICATED_START = -8,
    JSON_DUPLICATED_CANCEL = -9,
    JSON_DUPLICATED_SUBMIT = -10,
    JSON_DUPLICATED_VALUE = -11,
    JSON_DUPLICATED_PROGRESS = -12,
    JSON_MISSING_START = -13,
    JSON_MISSING_CANCEL = -14,
    JSON_MISSING_SUBMIT = -15,
    JSON_MISSING_VALUE = -16,
    JSON_INVALID_LBOARD_FIELD = -17,
    JSON_MISSING_DISPLAY_STRING = -18,
    JSON_OUT_OF_MEMORY = -19,
    JSON_INVALID_VALUE_FLAG = -20,
    JSON_MISSING_VALUE_MEASURED = -21,
    JSON_MULTIPLE_MEASURED = -22,
    JSON_INVALID_MEASURED_TARGET = -23,
    JSON_INVALID_COMPARISON = -24,
    JSON_INVALID_STATE = -25,
    JSON_INVALID_JSON = -26
};

int parse_json_array(const char** json_ptr, json_field_t* field);

int parse_json_object(const char** json_ptr, json_field_t* fields, size_t field_count, unsigned* fields_seen);

#endif //RETRO_ARCH_JSON_PARSER_H
