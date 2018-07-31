#include "rjson.h"
#include "dejson.h"

#include <setjmp.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <errno.h>

typedef struct {
  const uint8_t* json;
  uintptr_t      buffer;
  int            counting;
  jmp_buf        rollback;
}
rc_json_state_t;

static void* rc_json_alloc(rc_json_state_t* state, size_t size, size_t alignment) {
  state->buffer = (state->buffer + alignment - 1) & ~(alignment - 1);
  void* ptr = (void*)state->buffer;
  state->buffer += size;
  return ptr;
}

static void rc_json_skip_spaces(rc_json_state_t* state) {
  if (isspace(*state->json)) {
    do {
      state->json++;
    }
    while (isspace(*state->json));
  }
}

static size_t rc_json_skip_string(rc_json_state_t*);
static void rc_json_skip_value(rc_json_state_t*);

static void rc_json_skip_object(rc_json_state_t* state) {
  state->json++;
  rc_json_skip_spaces(state);
  
  while (*state->json != '}') {
    rc_json_skip_string(state);
    rc_json_skip_spaces(state);

    if (*state->json != ':') {
      longjmp(state->rollback, RC_JSON_INVALID_VALUE);
    }

    state->json++;
    rc_json_skip_spaces(state);
    rc_json_skip_value(state);
    rc_json_skip_spaces(state);

    if (*state->json != ',') {
      break;
    }

    state->json++;
    rc_json_skip_spaces(state);
  }

  if (*state->json != '}') {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  state->json++;
}

static size_t rc_json_skip_array(rc_json_state_t* state) {
  size_t count = 0;
  state->json++;
  rc_json_skip_spaces(state);
  
  while (*state->json != ']') {
    rc_json_skip_value(state);
    rc_json_skip_spaces(state);

    count++;

    if (*state->json != ',') {
      break;
    }

    state->json++;
    rc_json_skip_spaces(state);
  }

  if (*state->json != ']') {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  state->json++;
  return count;
}

static void rc_json_skip_number(rc_json_state_t* state) {
  errno = 0;

  char* end;
  double result = strtod((const char*)state->json, &end);

  if ((result == 0.0 && end == (const char*)state->json) || errno == ERANGE) {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  state->json = (uint8_t*)end;
}

static void rc_json_skip_boolean(rc_json_state_t* state) {
  const uint8_t* json = state->json;

  if (json[0] == 't' && json[1] == 'r' && json[2] == 'u' && json[3] == 'e' && !isalpha(json[4])) {
    state->json += 4;
  }
  else if (json[0] == 'f' && json[1] == 'a' && json[2] == 'l' && json[3] == 's' && json[4] == 'e' && !isalpha(json[5])) {
    state->json += 5;
  }
  else {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }
}

static size_t rc_json_skip_string(rc_json_state_t* state) {
  const uint8_t* aux = state->json + 1;
  size_t length = 0;
  
  if (*aux !='"') {
    do {
      length++;

      if (*aux++ == '\\') {
        char digits[5];
        uint32_t utf32;

        switch (*aux++) {
          case '"':
          case '\\':
          case '/':
          case 'b':
          case 'f':
          case 'n':
          case 'r':
          case 't':
            break;
          
          case 'u':
            if (!isxdigit(aux[0] || !isxdigit(aux[1]) || !isxdigit(aux[2]) || !isxdigit(aux[3]))) {
              longjmp(state->rollback, RC_JSON_INVALID_ESCAPE);
            }

            digits[0] = aux[0];
            digits[1] = aux[1];
            digits[2] = aux[2];
            digits[3] = aux[3];
            digits[4] = 0;
            aux += 4;
            
            utf32 = strtoul(digits, NULL, 16);

            if (utf32 < 0x80U) {
              length += 0;
            }
            else if (utf32 < 0x800U) {
              length += 1;
            }
            else if (utf32 < 0x10000U) {
              length += 2;
            }
            else if (utf32 < 0x200000U) {
              length += 3;
            }
            else {
              longjmp(state->rollback, RC_JSON_INVALID_ESCAPE);
            }

            break;
          
          default:
            longjmp(state->rollback, RC_JSON_INVALID_ESCAPE);
        }
      }
    }
    while (*aux != '"');
  }

  state->json = aux + 1;
  return length + 1;
}

static void rc_json_skip_null(rc_json_state_t* state) {
  const uint8_t* json = state->json;

  if (json[0] == 'n' && json[1] == 'u' && json[2] == 'l' && json[3] == 'l' && !isalpha(json[4])) {
    state->json += 4;
  }
  else {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }
}

static void rc_json_skip_value(rc_json_state_t* state) {
  switch (*state->json) {
    case '{':
      rc_json_skip_object(state);
      break;

    case '[':
      rc_json_skip_array(state);
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      rc_json_skip_number(state);
      break;

    case 't':
    case 'f':
      rc_json_skip_boolean(state);
      break;

    case '"':
      rc_json_skip_string(state);
      break;

    case 'n':
      rc_json_skip_null(state);
      break;

    default:
      longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  rc_json_skip_spaces(state);
}

static int64_t rc_json_get_int64(rc_json_state_t* state, int64_t min, int64_t max) {
  errno = 0;

  char* end;
  long long result = strtoll((const char*)state->json, &end, 10);

  if ((result == 0 && end == (const char*)state->json) || errno == ERANGE || result < min || result > max) {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  state->json = (uint8_t*)end;
  return result;
}

static uint64_t rc_json_get_uint64(rc_json_state_t* state, uint64_t max) {
  errno = 0;

  char* end;
  unsigned long long result = strtoull((char*)state->json, &end, 10);

  if ((result == 0 && end == (const char*)state->json) || errno == ERANGE || result > max) {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  state->json = (uint8_t*)end;
  return result;
}

static double rc_json_get_double(rc_json_state_t* state, double min, double max) {
  errno = 0;

  char* end;
  double result = strtod((const char*)state->json, &end);

  if ((result == 0.0 && end == (const char*)state->json) || errno == ERANGE || result < min || result > max) {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  state->json = (uint8_t*)end;
  return result;
}

static void rc_json_parse_char(rc_json_state_t* state, void* data) {
  *(char*)data = rc_json_get_int64(state, CHAR_MIN, CHAR_MAX);
}

static void rc_json_parse_uchar(rc_json_state_t* state, void* data) {
  *(unsigned char*)data = rc_json_get_uint64(state, UCHAR_MAX);
}

static void rc_json_parse_short(rc_json_state_t* state, void* data) {
  *(short*)data = rc_json_get_int64(state, SHRT_MIN, SHRT_MAX);
}

static void rc_json_parse_ushort(rc_json_state_t* state, void* data) {
  *(unsigned short*)data = rc_json_get_uint64(state, USHRT_MAX);
}

static void rc_json_parse_int(rc_json_state_t* state, void* data) {
  *(int*)data = rc_json_get_int64(state, INT_MIN, INT_MAX);
}

static void rc_json_parse_uint(rc_json_state_t* state, void* data) {
  *(unsigned int*)data = rc_json_get_uint64(state, UINT_MAX);
}

static void rc_json_parse_long(rc_json_state_t* state, void* data) {
  *(long*)data = rc_json_get_int64(state, LONG_MIN, LONG_MAX);
}

static void rc_json_parse_ulong(rc_json_state_t* state, void* data) {
  *(unsigned long*)data = rc_json_get_uint64(state, ULONG_MAX);
}

static void rc_json_parse_longlong(rc_json_state_t* state, void* data) {
  *(unsigned long*)data = rc_json_get_int64(state, LLONG_MIN, LLONG_MAX);
}

static void rc_json_parse_ulonglong(rc_json_state_t* state, void* data) {
  *(unsigned long*)data = rc_json_get_uint64(state, ULLONG_MAX);
}

static void rc_json_parse_int8(rc_json_state_t* state, void* data) {
  *(int8_t*)data = rc_json_get_int64(state, INT8_MIN, INT8_MAX);
}

static void rc_json_parse_int16(rc_json_state_t* state, void* data) {
  *(int16_t*)data = rc_json_get_int64(state, INT16_MIN, INT16_MAX);
}

static void rc_json_parse_int32(rc_json_state_t* state, void* data) {
  *(int32_t*)data = rc_json_get_int64(state, INT32_MIN, INT32_MAX);
}

static void rc_json_parse_int64(rc_json_state_t* state, void* data) {
  *(int64_t*)data = rc_json_get_int64(state, INT64_MIN, INT64_MAX);
}

static void rc_json_parse_uint8(rc_json_state_t* state, void* data) {
  *(uint8_t*)data = rc_json_get_uint64(state, UINT8_MAX);
}

static void rc_json_parse_uint16(rc_json_state_t* state, void* data) {
  *(uint16_t*)data = rc_json_get_uint64(state, UINT16_MAX);
}

static void rc_json_parse_uint32(rc_json_state_t* state, void* data) {
  *(uint32_t*)data = rc_json_get_uint64(state, UINT32_MAX);
}

static void rc_json_parse_uint64(rc_json_state_t* state, void* data) {
  *(uint64_t*)data = rc_json_get_uint64(state, UINT64_MAX);
}

static void rc_json_parse_float(rc_json_state_t* state, void* data) {
  *(float*)data = rc_json_get_double(state, FLT_MIN, FLT_MAX);
}

static void rc_json_parse_double(rc_json_state_t* state, void* data) {
  *(double*)data = rc_json_get_double(state, DBL_MIN, DBL_MAX);
}

static void rc_json_parse_boolean(rc_json_state_t* state, void* data) {
  const uint8_t* json = state->json;

  if (json[0] == 't' && json[1] == 'r' && json[2] == 'u' && json[3] == 'e' && !isalpha(json[4])) {
    *(char*)data = 1;
    state->json += 4;
  }
  else if (json[0] == 'f' && json[1] == 'a' && json[2] == 'l' && json[3] == 's' && json[4] == 'e' && !isalpha(json[5])) {
    *(char*)data = 0;
    state->json += 5;
  }
  else {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }
}

static void rc_json_parse_string(rc_json_state_t* state, void* data) {
  const uint8_t* aux = state->json;
  size_t length = rc_json_skip_string(state);
  uint8_t* str = (uint8_t*)rc_json_alloc(state, length + 1, RC_JSON_ALIGNOF(char));

  if (state->counting) {
    return;
  }

  if (*aux++ != '"') {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  *(char**)data = (char*)str;
  
  if (*aux !='"') {
    do {
      if (*aux == '\\') {
        char digits[5];
        uint32_t utf32;

        aux++;

        switch (*aux++) {
          case '"':  *str++ = '"'; break;
          case '\\': *str++ = '\\'; break;
          case '/':  *str++ = '/'; break;
          case 'b':  *str++ = '\b'; break;
          case 'f':  *str++ = '\f'; break;
          case 'n':  *str++ = '\n'; break;
          case 'r':  *str++ = '\r'; break;
          case 't':  *str++ = '\t'; break;
          
          case 'u':
            digits[0] = aux[0];
            digits[1] = aux[1];
            digits[2] = aux[2];
            digits[3] = aux[3];
            digits[4] = 0;
            aux += 4;

            utf32 = strtoul(digits, NULL, 16);

            if (utf32 < 0x80) {
              *str++ = utf32;
            }
            else if (utf32 < 0x800) {
              str[0] = 0xc0 | (utf32 >> 6);
              str[1] = 0x80 | (utf32 & 0x3f);
              str += 2;
            }
            else if (utf32 < 0x10000) {
              str[0] = 0xe0 | (utf32 >> 12);
              str[1] = 0x80 | ((utf32 >> 6) & 0x3f);
              str[2] = 0x80 | (utf32 & 0x3f);
              str += 3;
            }
            else {
              str[0] = 0xf0 | (utf32 >> 18);
              str[1] = 0x80 | ((utf32 >> 12) & 0x3f);
              str[2] = 0x80 | ((utf32 >> 6) & 0x3f);
              str[3] = 0x80 | (utf32 & 0x3f);
              str += 4;
            }

            break;
          
          default:
            longjmp(state->rollback, RC_JSON_INVALID_ESCAPE);
        }
      }
      else {
        *str++ = *aux++;
      }
    }
    while (*aux != '"');
  }

  *str = 0;
}

typedef void (*rc_json_parser_t)(rc_json_state_t*, void*);

static const rc_json_parser_t rc_json_parsers[] = {
  rc_json_parse_char, rc_json_parse_uchar, rc_json_parse_short, rc_json_parse_ushort,
  rc_json_parse_int, rc_json_parse_uint, rc_json_parse_long, rc_json_parse_ulong,
  rc_json_parse_longlong, rc_json_parse_ulonglong,
  rc_json_parse_int8, rc_json_parse_int16, rc_json_parse_int32, rc_json_parse_int64,
  rc_json_parse_uint8, rc_json_parse_uint16, rc_json_parse_uint32, rc_json_parse_uint64,
  rc_json_parse_float, rc_json_parse_double, rc_json_parse_boolean, rc_json_parse_string
};

static void rc_json_parse_value(rc_json_state_t*, void*, const rc_json_field_meta_t*);
static void rc_json_parse_object(rc_json_state_t*, void*, const rc_json_struct_meta_t*);

static void rc_json_parse_array(rc_json_state_t* state, void* value, size_t element_size, size_t element_alignment, const rc_json_field_meta_t* field) {
  if (*state->json != '[') {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  const uint8_t* save = state->json;
  size_t count = rc_json_skip_array(state);
  state->json = save + 1;

  uint8_t* elements = (uint8_t*)rc_json_alloc(state, element_size * count, element_alignment);
  
  if (!state->counting) {
    struct {
      void* elements;
      int count;
    }
    *array = value;

    array->elements = elements;
    array->count = count;
  }

  rc_json_skip_spaces(state);

  rc_json_field_meta_t field_scalar = *field;
  field_scalar.flags &= ~RC_JSON_FLAG_ARRAY;

  while (*state->json != ']') {
    rc_json_parse_value(state, (void*)elements, &field_scalar);
    rc_json_skip_spaces(state);

    elements += element_size;
    
    if (*state->json != ',') {
      break;
    }

    state->json++;
    rc_json_skip_spaces(state);
  }

  if (*state->json != ']') {
    longjmp(state->rollback, RC_JSON_UNTERMINATED_ARRAY);
  }

  state->json++;
}

const rc_json_struct_meta_t* rc_json_resolve_struct(uint32_t hash);

static void rc_json_parse_value(rc_json_state_t* state, void* value, const rc_json_field_meta_t* field) {
#define RC_JSON_TYPE_INFO(t) sizeof(t), RC_JSON_ALIGNOF(t)
  
  static const size_t rc_json_type_info[] = {
    RC_JSON_TYPE_INFO(char), RC_JSON_TYPE_INFO(unsigned char), RC_JSON_TYPE_INFO(short), RC_JSON_TYPE_INFO(unsigned short),
    RC_JSON_TYPE_INFO(int), RC_JSON_TYPE_INFO(unsigned int), RC_JSON_TYPE_INFO(long), RC_JSON_TYPE_INFO(unsigned long),
    RC_JSON_TYPE_INFO(long long), RC_JSON_TYPE_INFO(unsigned long long),
    RC_JSON_TYPE_INFO(int8_t), RC_JSON_TYPE_INFO(int16_t), RC_JSON_TYPE_INFO(int32_t), RC_JSON_TYPE_INFO(int64_t),
    RC_JSON_TYPE_INFO(uint8_t), RC_JSON_TYPE_INFO(uint16_t), RC_JSON_TYPE_INFO(uint32_t), RC_JSON_TYPE_INFO(uint64_t),
    RC_JSON_TYPE_INFO(float), RC_JSON_TYPE_INFO(double), RC_JSON_TYPE_INFO(char), RC_JSON_TYPE_INFO(const char*)
  };

  const rc_json_struct_meta_t* meta = NULL;

  if ((field->flags & (RC_JSON_FLAG_ARRAY | RC_JSON_FLAG_POINTER)) == 0) {
    if (field->type != RC_JSON_TYPE_RECORD) {
      char dummy[64];

      if (state->counting) {
        value = (void*)dummy;
      }

      rc_json_parsers[field->type](state, value);
    }
    else {
      meta = rc_json_resolve_struct(field->type_hash);
      
      if (meta == NULL) {
        longjmp(state->rollback, RC_JSON_UNKOWN_RECORD);
      }

      rc_json_parse_object(state, value, meta);
    }

    return;
  }

  size_t size, alignment;

  if (field->type != RC_JSON_TYPE_RECORD) {
    unsigned ndx = field->type * 2;
    size = rc_json_type_info[ndx];
    alignment = rc_json_type_info[ndx + 1];
  }
  else { /* field->type == RC_JSON_TYPE_RECORD */
    meta = rc_json_resolve_struct(field->type_hash);
    
    if (meta == NULL) {
      longjmp(state->rollback, RC_JSON_UNKOWN_RECORD);
    }

    size = meta->size;
    alignment = meta->alignment;
  }

  if ((field->flags & RC_JSON_FLAG_ARRAY) != 0) {
    rc_json_parse_array(state, value, size, alignment, field);
    return;
  }

  const uint8_t* json = state->json;

  if (json[0] == 'n' && json[1] == 'u' && json[2] == 'l' && json[3] == 'l' && !isalpha(json[4])) {
    if (!state->counting) {
      *(void**)value = NULL;
    }

    state->json += 4;
    return;
  }

  void* pointer = rc_json_alloc(state, size, alignment);

  if (!state->counting) {
    *(void**)value = pointer;
  }

  value = pointer;

  if (field->type != RC_JSON_TYPE_RECORD) {
    rc_json_parsers[field->type](state, value);
  }
  else {
    rc_json_parse_object(state, value, meta);
  }
}

static void rc_json_parse_object(rc_json_state_t* state, void* record, const rc_json_struct_meta_t* meta) {
  if (*state->json != '{') {
    longjmp(state->rollback, RC_JSON_INVALID_VALUE);
  }

  if (!state->counting) {
    memset((void*)record, 0, meta->size);
  }

  state->json++;
  rc_json_skip_spaces(state);
  
  while (*state->json != '}') {
    if (*state->json != '"') {
      longjmp(state->rollback, RC_JSON_MISSING_KEY);
    }

    const char* key = (const char*)++state->json;
    const char* quote = key;
    
    for (;;) {
      quote = strchr(quote, '"');

      if (!quote) {
        longjmp(state->rollback, RC_JSON_UNTERMINATED_KEY);
      }

      if (quote[-1] != '\\') {
        break;
      }
    }

    state->json = (const uint8_t*)quote + 1;
    uint32_t hash = rc_json_hash((const uint8_t*)key, quote - key);

    unsigned i;
    const rc_json_field_meta_t* field;
    
    for (i = 0, field = meta->fields; i < meta->num_fields; i++, field++) {
      if (field->name_hash == hash) {
        break;
      }
    }

    rc_json_skip_spaces(state);

    if (*state->json != ':') {
      longjmp(state->rollback, RC_JSON_MISSING_VALUE);
    }

    state->json++;
    rc_json_skip_spaces(state);

    if (i != meta->num_fields) {
      rc_json_parse_value(state, (void*)((uint8_t*)record + field->offset), field);
    }
    else {
      rc_json_skip_value(state);
    }

    rc_json_skip_spaces(state);

    if (*state->json != ',') {
      break;
    }

    state->json++;
    rc_json_skip_spaces(state);
  }

  if (*state->json != '}') {
    longjmp(state->rollback, RC_JSON_UNTERMINATED_OBJECT);
  }

  state->json++;
}

static int rc_json_execute(void* buffer, uint32_t hash, const uint8_t* json, int counting) {
  const rc_json_struct_meta_t* meta = rc_json_resolve_struct(hash);
  
  if (!meta) {
    return RC_JSON_UNKOWN_RECORD;
  }

  rc_json_state_t state;
  int res;
  
  if ((res = setjmp(state.rollback)) != 0) {
    return res;
  }

  state.json = json;
  state.buffer = counting ? 0 : (uintptr_t)*(void**)buffer;
  state.counting = counting;

  if (!counting) {
    *(void**)buffer = rc_json_alloc(&state, meta->size, meta->alignment);
  }
  
  rc_json_skip_spaces(&state);
  rc_json_parse_object(&state, *(void**)buffer, meta);
  rc_json_skip_spaces(&state);

  if (counting) {
    *(size_t*)buffer = state.buffer;
  }

  return *state.json == 0 ? RC_JSON_OK : RC_JSON_EOF_EXPECTED;
}

void* rc_json_deserialize(void* buffer, uint32_t hash, const uint8_t* json) {
  void** record = &buffer;
  int res = rc_json_execute(record, hash, json, 0);
  return res == RC_JSON_OK ? *record : NULL;
}

int rc_json_get_size(size_t* size, uint32_t hash, const uint8_t* json) {
  return rc_json_execute((void*)size, hash, json, 1);
}

uint32_t rc_json_hash(const uint8_t* str, size_t length) {
  typedef char unsigned_must_have_32_bits_minimum[sizeof(unsigned) >= 4 ? 1 : -1];

  uint32_t hash = 5381;

  if (length != 0) {
    do {
      hash = hash * 33 + *str++;
    }
    while (--length != 0);
  }

  return hash & 0xffffffffU;
}
