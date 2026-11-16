#include "rc_api_common.h"
#include "rc_api_request.h"
#include "rc_api_runtime.h"

#include "../rc_compat.h"

#include <formats/rjson.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETROACHIEVEMENTS_HOST "https://retroachievements.org"
#define RETROACHIEVEMENTS_IMAGE_HOST "https://media.retroachievements.org"
#define RETROACHIEVEMENTS_HOST_NONSSL "http://retroachievements.org"
#define RETROACHIEVEMENTS_IMAGE_HOST_NONSSL "http://media.retroachievements.org"
rc_api_host_t g_host = { NULL, NULL };

/* --- rc_json --- */

static int rc_json_parse_object(rc_json_iterator_t* iterator, rc_json_field_t* fields, size_t field_count, uint32_t* fields_seen);

static int rc_json_match_char(rc_json_iterator_t* iterator, char c)
{
  if (iterator->json < iterator->end && *iterator->json == c) {
    ++iterator->json;
    return 1;
  }

  return 0;
}

static void rc_json_skip_whitespace(rc_json_iterator_t* iterator)
{
  while (iterator->json < iterator->end && isspace((unsigned char)*iterator->json))
    ++iterator->json;
}

static int rc_json_find_substring(rc_json_iterator_t* iterator, const char* substring)
{
  const char first = *substring;
  const size_t substring_len = strlen(substring);
  const char* end = iterator->end - substring_len;

  while (iterator->json <= end) {
    if (*iterator->json == first) {
      if (memcmp(iterator->json, substring, substring_len) == 0)
        return 1;
    }

    ++iterator->json;
  }

  return 0;
}

typedef struct rc_json_fields_t {
  rc_json_field_t* fields;
  size_t count;
  size_t next;
} rc_json_fields_t;

static int rc_json_parse_field(rc_json_iterator_t* iterator, rc_json_field_t* field) {
  rjson_span_t value;
  if (iterator->json >= iterator->end ||
      !(iterator->validated ? rjson_read_span_validated : rjson_read_span)(
        iterator->json, iterator->end - iterator->json, &value, NULL, NULL) ||
      value.count > UINT32_MAX)
    return RC_INVALID_JSON;
  field->value_start = value.data;
  field->value_end = value.data + value.length;
  field->array_size = (uint32_t)value.count;
  field->validated = 1;
  iterator->json = field->value_end;
  return RC_OK;
}

static bool rc_json_collect_field(void* context, const rjson_span_t* name, const rjson_span_t* value) {
  rc_json_fields_t* selected = (rc_json_fields_t*)context;
  size_t i;
  int escaped = memchr(name->data + 1, '\\', name->length - 2) != NULL;
  if (value->count > UINT32_MAX)
    return false;
  for (i = selected->next; i < selected->count; ++i) {
    rc_json_field_t* field = &selected->fields[i];
    if (!field->value_start && (escaped ?
        rjson_string_equal(name->data, name->length, field->name) :
        (field->name_len == name->length - 2 &&
         !memcmp(field->name, name->data + 1, field->name_len)))) {
      field->value_start = value->data;
      field->value_end = value->data + value->length;
      field->array_size = (uint32_t)value->count;
      field->validated = 1;
      while (selected->next < selected->count && selected->fields[selected->next].value_start)
        ++selected->next;
      break;
    }
  }
  return true;
}

static int rc_json_get_next_field(rc_json_iterator_t* iterator, rc_json_field_t* field) {
  rjson_span_t name;
  if (iterator->json >= iterator->end ||
      !(iterator->validated ? rjson_read_span_validated : rjson_read_span)(
        iterator->json, iterator->end - iterator->json, &name, NULL, NULL) ||
      name.type != RJSON_STRING)
    return RC_INVALID_JSON;
  field->name = name.data + 1;
  field->name_len = name.length - 2;
  iterator->json = name.data + name.length;
  rc_json_skip_whitespace(iterator);
  if (!rc_json_match_char(iterator, ':'))
    return RC_INVALID_JSON;
  rc_json_skip_whitespace(iterator);
  if (rc_json_parse_field(iterator, field) != RC_OK)
    return RC_INVALID_JSON;
  rc_json_skip_whitespace(iterator);
  return RC_OK;
}

static int rc_json_parse_object(rc_json_iterator_t* iterator, rc_json_field_t* fields, size_t field_count, uint32_t* fields_seen) {
  rc_json_fields_t selected;
  rjson_span_t object;
  size_t i;
  for (i = 0; i < field_count; ++i) {
    fields[i].value_start = fields[i].value_end = NULL;
    fields[i].validated = 0;
  }
  if (fields_seen)
    *fields_seen = 0;
  selected.fields = fields;
  selected.count = field_count;
  selected.next = 0;
  if (iterator->json >= iterator->end ||
      !(iterator->validated ? rjson_read_span_validated : rjson_read_span)(
        iterator->json, iterator->end - iterator->json, &object,
        field_count ? rc_json_collect_field : NULL, &selected) ||
      object.type != RJSON_OBJECT || object.count > UINT32_MAX)
    return RC_INVALID_JSON;
  iterator->json = object.data + object.length;
  if (fields_seen)
    *fields_seen = (uint32_t)object.count;
  return RC_OK;
}

int rc_json_get_next_object_field(rc_json_iterator_t* iterator, rc_json_field_t* field) {
  if (!rc_json_match_char(iterator, ',') && !rc_json_match_char(iterator, '{'))
    return 0;

  return (rc_json_get_next_field(iterator, field) == RC_OK);
}

int rc_json_get_object_string_length(const char* json) {
  rjson_span_t object;
  size_t length = strlen(json);
  if (rjson_read_span(json, length, &object, NULL, NULL) && object.type == RJSON_OBJECT)
    return (int)(object.data + object.length - json);
  return (int)length;
}

static int rc_json_extract_html_error(rc_api_response_t* response, const rc_api_server_response_t* server_response) {
  rc_json_iterator_t iterator;
  memset(&iterator, 0, sizeof(iterator));
  iterator.json = server_response->body;
  iterator.end = server_response->body + server_response->body_length;

  /* assume the title contains the most appropriate message to display to the user */
  if (rc_json_find_substring(&iterator, "<title>")) {
    const char* title_start = iterator.json + 7;
    if (rc_json_find_substring(&iterator, "</title>")) {
      response->error_message = rc_buffer_strncpy(&response->buffer, title_start, iterator.json - title_start);
      response->succeeded = 0;
      return RC_INVALID_JSON;
    }
  }

  /* title not found, return the first line of the response (up to 200 characters) */
  iterator.json = server_response->body;

  while (iterator.json < iterator.end && *iterator.json != '\n' &&
         iterator.json - server_response->body < 200) {
    ++iterator.json;
  }

  if (iterator.json > server_response->body && iterator.json[-1] == '\r')
    --iterator.json;

  if (iterator.json > server_response->body)
    response->error_message = rc_buffer_strncpy(&response->buffer, server_response->body, iterator.json - server_response->body);

  response->succeeded = 0;
  return RC_INVALID_JSON;
}

static int rc_json_convert_error_code(const char* server_error_code)
{
  switch (server_error_code[0]) {
    case 'a':
      if (strcmp(server_error_code, "access_denied") == 0)
        return RC_ACCESS_DENIED;
      break;

    case 'e':
      if (strcmp(server_error_code, "expired_token") == 0)
        return RC_EXPIRED_TOKEN;
      break;

    case 'i':
      if (strcmp(server_error_code, "invalid_credentials") == 0)
        return RC_INVALID_CREDENTIALS;
      if (strcmp(server_error_code, "invalid_parameter") == 0)
        return RC_INVALID_STATE;
      break;

    case 'm':
      if (strcmp(server_error_code, "missing_parameter") == 0)
        return RC_INVALID_STATE;
      break;

    case 'n':
      if (strcmp(server_error_code, "not_found") == 0)
        return RC_NOT_FOUND;
      break;

    default:
      break;
  }

  return RC_API_FAILURE;
}

int rc_json_parse_server_response(rc_api_response_t* response, const rc_api_server_response_t* server_response, rc_json_field_t* fields, size_t field_count) {
  int result;

#ifndef NDEBUG
  if (field_count < 2)
    return RC_INVALID_STATE;
  if (strcmp(fields[0].name, "Success") != 0)
    return RC_INVALID_STATE;
  if (strcmp(fields[1].name, "Error") != 0)
    return RC_INVALID_STATE;
#endif

  response->error_message = NULL;

  if (!server_response) {
    response->succeeded = 0;
    return RC_NO_RESPONSE;
  }

  if (server_response->http_status_code == RC_API_SERVER_RESPONSE_CLIENT_ERROR ||
      server_response->http_status_code == RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR) {
    /* client provided error message is passed as the response body */
    response->error_message = rc_buffer_strncpy(&response->buffer, server_response->body, server_response->body_length);
    response->succeeded = 0;
    return RC_NO_RESPONSE;
  }

  if (!server_response->body || !*server_response->body) {
    /* expect valid HTTP status codes to have bodies that we can extract the message from,
     * but provide some default messages in case they don't. */
    switch (server_response->http_status_code) {
      case 504: /* 504 Gateway Timeout */
      case 522: /* 522 Connection Timed Out */
      case 524: /* 524 A Timeout Occurred */
        response->error_message = "Request has timed out.";
        break;

      case 521: /* 521 Web Server is Down */
      case 523: /* 523 Origin is Unreachable */
        response->error_message = "Could not connect to server.";
        break;

      default:
        break;
    }

    response->succeeded = 0;
    return RC_NO_RESPONSE;
  }

  if (*server_response->body != '{') {
    result = rc_json_extract_html_error(response, server_response);
  }
  else {
    rc_json_iterator_t iterator;
    memset(&iterator, 0, sizeof(iterator));
    iterator.json = server_response->body;
    iterator.end = server_response->body + server_response->body_length;
    result = rc_json_parse_object(&iterator, fields, field_count, NULL);

    rc_json_get_optional_string(&response->error_message, response, &fields[1], "Error", NULL);
    rc_json_get_optional_bool(&response->succeeded, &fields[0], "Success", 1);

    /* Code will be the third field in the fields array, but may not always be present */
    if (field_count > 2 && strcmp(fields[2].name, "Code") == 0) {
      rc_json_get_optional_string(&response->error_code, response, &fields[2], "Code", NULL);
      if (response->error_code != NULL)
        result = rc_json_convert_error_code(response->error_code);
    }
  }

  return result;
}

static int rc_json_missing_field(rc_api_response_t* response, const rc_json_field_t* field) {
  const char* not_found = " not found in response";
  const size_t not_found_len = strlen(not_found);
  const size_t field_len = strlen(field->name);

  uint8_t* write = rc_buffer_reserve(&response->buffer, field_len + not_found_len + 1);
  if (write) {
    response->error_message = (char*)write;
    memcpy(write, field->name, field_len);
    write += field_len;
    memcpy(write, not_found, not_found_len + 1);
    write += not_found_len + 1;
    rc_buffer_consume(&response->buffer, (uint8_t*)response->error_message, write);
  }

  response->succeeded = 0;
  return 0;
}

int rc_json_get_required_object(rc_json_field_t* fields, size_t field_count, rc_api_response_t* response, rc_json_field_t* field, const char* field_name) {
  rc_json_iterator_t iterator;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!field->value_start)
    return rc_json_missing_field(response, field);

  memset(&iterator, 0, sizeof(iterator));
  iterator.json = field->value_start;
  iterator.end = field->value_end;
  iterator.validated = field->validated;
  return (rc_json_parse_object(&iterator, fields, field_count, &field->array_size) == RC_OK);
}

static int rc_json_get_array_entry_value(rc_json_field_t* field, rc_json_iterator_t* iterator) {
  rc_json_skip_whitespace(iterator);

  if (iterator->json >= iterator->end)
    return 0;

  if (rc_json_parse_field(iterator, field) != RC_OK)
    return 0;

  rc_json_skip_whitespace(iterator);

  if (!rc_json_match_char(iterator, ','))
    rc_json_match_char(iterator, ']');

  return 1;
}

static int rc_json_get_unum_array(uint32_t** entries, uint32_t* num_entries, rc_api_response_t* response, const rc_json_field_t* array, const char* field_name) {
  if (*num_entries) {
    rc_json_iterator_t iterator;
    rc_json_field_t value;
    uint32_t* entry;

    *entries = (uint32_t*)rc_buffer_alloc(&response->buffer, *num_entries * sizeof(uint32_t));
    if (!*entries)
      return RC_OUT_OF_MEMORY;

    value.name = field_name;

    memset(&iterator, 0, sizeof(iterator));
    iterator.json = array->value_start;
    iterator.end = array->value_end;
    iterator.validated = array->validated;

    entry = *entries;
    while (rc_json_get_array_entry_value(&value, &iterator)) {
      if (!rc_json_get_unum(entry, &value, field_name))
        return RC_MISSING_VALUE;

      ++entry;
    }
  }
  else {
    *entries = NULL;
  }

  return RC_OK;
}

int rc_json_get_required_unum_array(uint32_t** entries, uint32_t* num_entries, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  rc_json_field_t array;
  memset(&array, 0, sizeof(array));

  if (!rc_json_get_required_array(num_entries, &array, response, field, field_name))
    return RC_MISSING_VALUE;

  return rc_json_get_unum_array(entries, num_entries, response, &array, field_name);
}

int rc_json_get_optional_unum_array(uint32_t** entries, uint32_t* num_entries, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  rc_json_field_t array;
  memset(&array, 0, sizeof(array));

  if (!rc_json_get_optional_array(num_entries, &array, field, field_name))
    *num_entries = 0;

  return rc_json_get_unum_array(entries, num_entries, response, &array, field_name);
}

static int rc_json_get_string_array(const char*** entries, uint32_t* num_entries, rc_api_response_t* response, const rc_json_field_t* array, const char* field_name) {
  if (*num_entries) {
    rc_json_iterator_t iterator;
    rc_json_field_t value;
    const char** entry;

    *entries = (const char**)rc_buffer_alloc(&response->buffer, *num_entries * sizeof(const char*));
    if (!*entries)
      return RC_OUT_OF_MEMORY;

    value.name = field_name;

    memset(&iterator, 0, sizeof(iterator));
    iterator.json = array->value_start;
    iterator.end = array->value_end;
    iterator.validated = array->validated;

    entry = *entries;
    while (rc_json_get_array_entry_value(&value, &iterator)) {
      if (!rc_json_get_string(entry, &response->buffer, &value, field_name))
        return RC_MISSING_VALUE;

      ++entry;
    }
  }
  else {
    *entries = NULL;
  }

  return RC_OK;
}

int rc_json_get_required_string_array(const char*** entries, uint32_t* num_entries, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  rc_json_field_t array;

  memset(&array, 0, sizeof(array));
  if (!rc_json_get_required_array(num_entries, &array, response, field, field_name))
    return RC_MISSING_VALUE;

  return rc_json_get_string_array(entries, num_entries, response, &array, field_name);
}

int rc_json_get_optional_string_array(const char*** entries, uint32_t* num_entries, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  rc_json_field_t array;

  memset(&array, 0, sizeof(array));
  if (!rc_json_get_optional_array(num_entries, &array, field, field_name))
    *num_entries = 0;

  return rc_json_get_string_array(entries, num_entries, response, &array, field_name);
}

int rc_json_get_required_array(uint32_t* num_entries, rc_json_field_t* array_field, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (!rc_json_get_optional_array(num_entries, array_field, field, field_name))
    return rc_json_missing_field(response, field);

  return 1;
}

int rc_json_get_optional_array(uint32_t* num_entries, rc_json_field_t* array_field, const rc_json_field_t* field, const char* field_name) {
#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!field->value_start || *field->value_start != '[') {
    *num_entries = 0;
    return 0;
  }

  memcpy(array_field, field, sizeof(*array_field));
  ++array_field->value_start; /* skip [ */

  *num_entries = field->array_size;
  return 1;
}

int rc_json_get_array_entry_object(rc_json_field_t* fields, size_t field_count, rc_json_iterator_t* iterator) {
  rc_json_skip_whitespace(iterator);

  if (iterator->json >= iterator->end)
    return 0;

  if (rc_json_parse_object(iterator, fields, field_count, NULL) != RC_OK)
    return 0;

  rc_json_skip_whitespace(iterator);

  if (!rc_json_match_char(iterator, ','))
    rc_json_match_char(iterator, ']');

  return 1;
}

int rc_json_get_string(const char** out, rc_buffer_t* buffer, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;
  char* dst;
  size_t length, written;
#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif
  if (!src)
    return 0;
  length = field->value_end - src;
  if (length == 4 && !memcmp(src, "null", 4)) {
    *out = NULL;
    return 1;
  }
  if (!length || (length == 2 && src[0] == '"' && src[1] == '"')) {
    *out = "";
    return 1;
  }
  if (*src == '"') {
    if (length < 2 || src[length - 1] != '"')
      return 0;
    dst = (char*)rc_buffer_reserve(buffer, length - 1);
    if (!dst)
      return 0;
    if (memchr(src + 1, '\\', length - 2)) {
      if (!rjson_decode_string(src, length, dst, length - 1, &written))
        return 0;
    }
    else {
      /* The span reader already validated this literal. */
      written = length - 2;
      memcpy(dst, src + 1, written);
      dst[written] = '\0';
    }
  }
  else {
    dst = (char*)rc_buffer_reserve(buffer, length + 1);
    if (!dst)
      return 0;
    memcpy(dst, src, length);
    written = length;
    dst[written] = '\0';
  }
  *out = dst;
  rc_buffer_consume(buffer, (uint8_t*)dst, (uint8_t*)dst + written + 1);
  return 1;
}

int rc_json_field_string_matches(const rc_json_field_t* field, const char* text) {
  size_t length;
  if (!field->value_start || !text)
    return 0;
  length = field->value_end - field->value_start;
  if (*field->value_start == '"') {
    if (length < 2)
      return 0;
    if (memchr(field->value_start + 1, '\\', length - 2))
      return rjson_string_equal(field->value_start, length, text);
    return strlen(text) == length - 2 && !memcmp(field->value_start + 1, text, length - 2);
  }
  return strlen(text) == length && !memcmp(field->value_start, text, length);
}

void rc_json_get_optional_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name, const char* default_value) {
  if (!rc_json_get_string(out, &response->buffer, field, field_name))
    *out = default_value;
}

int rc_json_get_required_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_string(out, &response->buffer, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_num(int32_t* out, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;
  int32_t value = 0;
  int negative = 0;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!src) {
    *out = 0;
    return 0;
  }

  /* assert: string contains only numerals and an optional sign per rc_json_parse_field */
  if (*src == '-') {
    negative = 1;
    ++src;
  } else if (*src == '+') {
    ++src;
  } else if (*src < '0' || *src > '9') {
    *out = 0;
    return 0;
  }

  while (src < field->value_end && *src != '.') {
    value *= 10;
    value += *src - '0';
    ++src;
  }

  if (negative)
    *out = -value;
  else
    *out = value;

  return 1;
}

void rc_json_get_optional_num(int32_t* out, const rc_json_field_t* field, const char* field_name, int default_value) {
  if (!rc_json_get_num(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_num(int32_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_num(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_unum(uint32_t* out, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;
  uint32_t value = 0;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!src) {
    *out = 0;
    return 0;
  }

  if (*src < '0' || *src > '9') {
    *out = 0;
    return 0;
  }

  /* assert: string contains only numerals per rc_json_parse_field */
  while (src < field->value_end && *src != '.') {
    value *= 10;
    value += *src - '0';
    ++src;
  }

  *out = value;
  return 1;
}

void rc_json_get_optional_unum(uint32_t* out, const rc_json_field_t* field, const char* field_name, uint32_t default_value) {
  if (!rc_json_get_unum(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_unum(uint32_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_unum(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_float(float* out, const rc_json_field_t* field, const char* field_name) {
  int32_t whole, fraction, fraction_denominator;
  const char* decimal = field->value_start;

  if (!decimal) {
    *out = 0.0f;
    return 0;
  }

  if (!rc_json_get_num(&whole, field, field_name))
    return 0;

  while (decimal < field->value_end && *decimal != '.')
    ++decimal;

  fraction = 0;
  fraction_denominator = 1;
  if (decimal) {
    ++decimal;
    while (decimal < field->value_end && *decimal >= '0' && *decimal <= '9') {
      fraction *= 10;
      fraction += *decimal - '0';
      fraction_denominator *= 10;
      ++decimal;
    }
  }

  if (whole < 0)
    fraction = -fraction;

  *out = (float)whole + ((float)fraction / (float)fraction_denominator);
  return 1;
}

void rc_json_get_optional_float(float* out, const rc_json_field_t* field, const char* field_name, float default_value) {
  if (!rc_json_get_float(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_float(float* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_float(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_datetime(time_t* out, const rc_json_field_t* field, const char* field_name) {
  struct tm tm;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (field->value_start && *field->value_start == '\"') {
    memset(&tm, 0, sizeof(tm));
    if (sscanf_s(field->value_start + 1, "%d-%d-%d %d:%d:%d", /* DB format "2013-10-20 22:12:21" */
                 &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6 ||
        /* NOTE: relies on sscanf stopping when it sees a non-digit after the seconds. could be 'Z', '.', '+', or '-' */
        sscanf_s(field->value_start + 1, "%d-%d-%dT%d:%d:%d", /* ISO format "2013-10-20T22:12:21.000000Z */
                 &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
      tm.tm_mon--; /* 0-based */
      tm.tm_year -= 1900; /* 1900 based */

      /* mktime converts a struct tm to a time_t using the local timezone.
       * the input string is UTC. since timegm is not universally cross-platform,
       * figure out the offset between UTC and local time by applying the
       * timezone conversion twice and manually removing the difference */
      {
         time_t local_timet = mktime(&tm);
         time_t skewed_timet, tz_offset;
         struct tm gmt_tm;
         gmtime_s(&gmt_tm, &local_timet);
         skewed_timet = mktime(&gmt_tm); /* applies local time adjustment second time */
         tz_offset = skewed_timet - local_timet;
         *out = local_timet - tz_offset;
      }

      return 1;
    }
  }

  *out = 0;
  return 0;
}

int rc_json_get_required_datetime(time_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_datetime(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_timet(time_t* out, const rc_json_field_t* field, const char* field_name)
{
  const char* src = field->value_start;
  int64_t value = 0;
  int negative = 0;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!src) {
    *out = 0;
    return 0;
  }

  /* assert: string contains only numerals and an optional sign per rc_json_parse_field */
  if (*src == '-') {
    negative = 1;
    ++src;
  } else if (*src == '+') {
    ++src;
  } else if (*src < '0' || *src > '9') {
    *out = 0;
    return 0;
  }

  while (src < field->value_end && *src != '.') {
    value *= 10;
    value += *src - '0';
    ++src;
  }

  if (negative)
    *out = (time_t)-value;
  else
    *out = (time_t)value;

  return 1;
}

void rc_json_get_optional_timet(time_t* out, const rc_json_field_t* field, const char* field_name, time_t default_value) {
  if (!rc_json_get_timet(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_timet(time_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_timet(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_bool(int* out, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (src) {
    const size_t len = field->value_end - field->value_start;
    if (len == 4 && strncasecmp(src, "true", 4) == 0) {
      *out = 1;
      return 1;
    } else if (len == 5 && strncasecmp(src, "false", 5) == 0) {
      *out = 0;
      return 1;
    } else if (len == 1) {
      *out = (*src != '0');
      return 1;
    }
  }

  *out = 0;
  return 0;
}

void rc_json_get_optional_bool(int* out, const rc_json_field_t* field, const char* field_name, int default_value) {
  if (!rc_json_get_bool(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_bool(int* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_bool(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

void rc_json_extract_filename(rc_json_field_t* field) {
  if (field->value_end && field->value_end > field->value_start) {
    const char* str = field->value_end;
    if (str[-1] == '"') {
      /* ignore trailing quote */
      field->value_end = --str;

      while (str > field->value_start) {
        const char c = *(--str);
        if (c == '.') {
          /* found an extension. remove it */
          field->value_end = str;
        }
        else if (c == '/' || c == '"') {
          /* found path separator or opening quote. stop */
          field->value_start = str + 1;
          break;
        }
      }
    }
  }
}

/* --- rc_api_request --- */

void rc_api_destroy_request(rc_api_request_t* request)
{
  rc_buffer_destroy(&request->buffer);
}

/* --- rc_url_builder --- */

void rc_url_builder_init(rc_api_url_builder_t* builder, rc_buffer_t* buffer, size_t estimated_size) {
  rc_buffer_chunk_t* used_buffer;

  memset(builder, 0, sizeof(*builder));
  builder->buffer = buffer;
  builder->write = builder->start = (char*)rc_buffer_reserve(buffer, estimated_size);

  used_buffer = &buffer->chunk;
  while (used_buffer && used_buffer->write != (uint8_t*)builder->write)
    used_buffer = used_buffer->next;

  builder->end = (used_buffer) ? (char*)used_buffer->end : builder->start + estimated_size;
}

const char* rc_url_builder_finalize(rc_api_url_builder_t* builder) {
  rc_url_builder_append(builder, "", 1);

  if (builder->result != RC_OK)
    return NULL;

  rc_buffer_consume(builder->buffer, (uint8_t*)builder->start, (uint8_t*)builder->write);
  return builder->start;
}

static int rc_url_builder_reserve(rc_api_url_builder_t* builder, size_t amount) {
  if (builder->result == RC_OK) {
    size_t remaining = builder->end - builder->write;
    if (remaining < amount) {
      const size_t used = builder->write - builder->start;
      const size_t current_size = builder->end - builder->start;
      const size_t buffer_prefix_size = sizeof(rc_buffer_chunk_t);
      char* new_start;
      size_t new_size = (current_size < 256) ? 256 : current_size * 2;
      do {
        remaining = new_size - used;
        if (remaining >= amount)
          break;

        new_size *= 2;
      } while (1);

      /* rc_buffer_reserve will align to 256 bytes after including the buffer prefix. attempt to account for that */
      if ((remaining - amount) > buffer_prefix_size)
        new_size -= buffer_prefix_size;

      new_start = (char*)rc_buffer_reserve(builder->buffer, new_size);
      if (!new_start) {
        builder->result = RC_OUT_OF_MEMORY;
        return RC_OUT_OF_MEMORY;
      }

      if (new_start != builder->start) {
        memcpy(new_start, builder->start, used);
        builder->start = new_start;
        builder->write = new_start + used;
      }

      builder->end = builder->start + new_size;
    }
  }

  return builder->result;
}

void rc_url_builder_append_encoded_str(rc_api_url_builder_t* builder, const char* str) {
  static const char hex[] = "0123456789abcdef";
  const char* start = str;
  size_t len = 0;
  for (;;) {
    const char c = *str++;
    switch (c) {
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      case '-': case '_': case '.': case '~':
        len++;
        continue;

      case '\0':
        if (len)
          rc_url_builder_append(builder, start, len);

        return;

      default:
        if (rc_url_builder_reserve(builder, len + 3) != RC_OK)
          return;

        if (len) {
          memcpy(builder->write, start, len);
          builder->write += len;
        }

        if (c == ' ') {
          *builder->write++ = '+';
        } else {
          *builder->write++ = '%';
          *builder->write++ = hex[((unsigned char)c) >> 4];
          *builder->write++ = hex[c & 0x0F];
        }
        break;
    }

    start = str;
    len = 0;
  }
}

void rc_url_builder_append(rc_api_url_builder_t* builder, const char* data, size_t len) {
  if (rc_url_builder_reserve(builder, len) == RC_OK) {
    memcpy(builder->write, data, len);
    builder->write += len;
  }
}

static int rc_url_builder_append_param_equals(rc_api_url_builder_t* builder, const char* param) {
  size_t param_len = strlen(param);

  if (rc_url_builder_reserve(builder, param_len + 2) == RC_OK) {
    if (builder->write > builder->start) {
      if (builder->write[-1] != '?')
        *builder->write++ = '&';
    }

    memcpy(builder->write, param, param_len);
    builder->write += param_len;
    *builder->write++ = '=';
  }

  return builder->result;
}

void rc_url_builder_append_unum_param(rc_api_url_builder_t* builder, const char* param, uint32_t value) {
  if (rc_url_builder_append_param_equals(builder, param) == RC_OK) {
    char num[16];
    int chars = snprintf(num, sizeof(num), "%u", value);
    rc_url_builder_append(builder, num, chars);
  }
}

void rc_url_builder_append_num_param(rc_api_url_builder_t* builder, const char* param, int32_t value) {
  if (rc_url_builder_append_param_equals(builder, param) == RC_OK) {
    char num[16];
    int chars = snprintf(num, sizeof(num), "%d", value);
    rc_url_builder_append(builder, num, chars);
  }
}

void rc_url_builder_append_str_param(rc_api_url_builder_t* builder, const char* param, const char* value) {
  rc_url_builder_append_param_equals(builder, param);
  rc_url_builder_append_encoded_str(builder, value);
}

void rc_api_url_build_dorequest_url(rc_api_request_t* request, const rc_api_host_t* host) {
  #define DOREQUEST_ENDPOINT "/dorequest.php"
  rc_buffer_init(&request->buffer);

  if (!host || !host->host) {
    request->url = RETROACHIEVEMENTS_HOST DOREQUEST_ENDPOINT;
  }
  else {
    const size_t endpoint_len = sizeof(DOREQUEST_ENDPOINT);
    const size_t host_len = strlen(host->host);
    const size_t protocol_len = (strstr(host->host, "://")) ? 0 : 7;
    const size_t url_len = protocol_len + host_len + endpoint_len;
    uint8_t* url = rc_buffer_reserve(&request->buffer, url_len);

    if (protocol_len)
      memcpy(url, "http://", protocol_len);

    memcpy(url + protocol_len, host->host, host_len);
    memcpy(url + protocol_len + host_len, DOREQUEST_ENDPOINT, endpoint_len);
    rc_buffer_consume(&request->buffer, url, url + url_len);

    request->url = (char*)url;
  }
  #undef DOREQUEST_ENDPOINT
}

int rc_api_url_build_dorequest(rc_api_url_builder_t* builder, const char* api, const char* username, const char* api_token) {
  if (!username || !*username || !api_token || !*api_token) {
    builder->result = RC_INVALID_STATE;
    return 0;
  }

  rc_url_builder_append_str_param(builder, "r", api);
  rc_url_builder_append_str_param(builder, "u", username);
  rc_url_builder_append_str_param(builder, "t", api_token);

  return (builder->result == RC_OK);
}

/* --- Set Host --- */

static void rc_api_update_host(const char** host, const char* hostname) {
  if (*host != NULL)
    free((void*)*host);

  if (hostname != NULL) {
    if (strstr(hostname, "://")) {
      *host = strdup(hostname);
    }
    else {
      const size_t hostname_len = strlen(hostname);
      if (hostname_len == 0) {
        *host = NULL;
      }
      else {
        char* newhost = (char*)malloc(hostname_len + 7 + 1);
        if (newhost) {
          memcpy(newhost, "http://", 7);
          memcpy(&newhost[7], hostname, hostname_len + 1);
          *host = newhost;
        }
        else {
          *host = NULL;
        }
      }
    }
  }
  else {
    *host = NULL;
  }
}

const char* rc_api_default_host(void) {
  return RETROACHIEVEMENTS_HOST;
}

void rc_api_set_host(const char* hostname) {
  if (hostname && strcmp(hostname, RETROACHIEVEMENTS_HOST) == 0)
    hostname = NULL;

  rc_api_update_host(&g_host.host, hostname);

  if (!hostname) {
    /* also clear out the image hostname */
    rc_api_set_image_host(NULL);
  }
  else if (strcmp(hostname, RETROACHIEVEMENTS_HOST_NONSSL) == 0) {
    /* if just pointing at the non-HTTPS host, explicitly use the default image host
     * so it doesn't try to use the web host directly */
    rc_api_set_image_host(RETROACHIEVEMENTS_IMAGE_HOST_NONSSL);
  }
}

void rc_api_set_image_host(const char* hostname) {
  rc_api_update_host(&g_host.media_host, hostname);
}

/* --- Fetch Image --- */

int rc_api_init_fetch_image_request(rc_api_request_t* request, const rc_api_fetch_image_request_t* api_params) {
  return rc_api_init_fetch_image_request_hosted(request, api_params, &g_host);
}

int rc_api_init_fetch_image_request_hosted(rc_api_request_t* request, const rc_api_fetch_image_request_t* api_params, const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  if (!api_params->image_name || !api_params->image_name[0])
    return RC_INVALID_STATE;

  rc_buffer_init(&request->buffer);
  rc_url_builder_init(&builder, &request->buffer, 64);

  if (host && host->media_host) {
    /* custom media host provided */
    if (!strstr(host->media_host, "://"))
      rc_url_builder_append(&builder, "http://", 7);
    rc_url_builder_append(&builder, host->media_host, strlen(host->media_host));
  }
  else if (host && host->host) {
    if (strcmp(host->host, RETROACHIEVEMENTS_HOST_NONSSL) == 0) {
      /* if host specifically set to non-ssl host, and no media host provided, use non-ssl media host */
      rc_url_builder_append(&builder, RETROACHIEVEMENTS_IMAGE_HOST_NONSSL, sizeof(RETROACHIEVEMENTS_IMAGE_HOST_NONSSL) - 1);
    }
    else if (strcmp(host->host, RETROACHIEVEMENTS_HOST) == 0) {
      /* if host specifically set to ssl host, and no media host provided, use media host */
      rc_url_builder_append(&builder, RETROACHIEVEMENTS_IMAGE_HOST, sizeof(RETROACHIEVEMENTS_IMAGE_HOST) - 1);
    }
    else {
      /* custom host and no media host provided. assume custom host is also media host */
      if (!strstr(host->host, "://"))
        rc_url_builder_append(&builder, "http://", 7);
      rc_url_builder_append(&builder, host->host, strlen(host->host));
    }
  }
  else {
    /* no custom host provided */
    rc_url_builder_append(&builder, RETROACHIEVEMENTS_IMAGE_HOST, sizeof(RETROACHIEVEMENTS_IMAGE_HOST) - 1);
  }

  switch (api_params->image_type)
  {
    case RC_IMAGE_TYPE_GAME:
      rc_url_builder_append(&builder, "/Images/", 8);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, ".png", 4);
      break;

    case RC_IMAGE_TYPE_ACHIEVEMENT:
      rc_url_builder_append(&builder, "/Badge/", 7);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, ".png", 4);
      break;

    case RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED:
      rc_url_builder_append(&builder, "/Badge/", 7);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, "_lock.png", 9);
      break;

    case RC_IMAGE_TYPE_USER:
      rc_url_builder_append(&builder, "/UserPic/", 9);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, ".png", 4);
      break;

    default:
      return RC_INVALID_STATE;
  }

  request->url = rc_url_builder_finalize(&builder);
  request->post_data = NULL;
  request->content_type = NULL;

  return builder.result;
}

const char* rc_api_build_avatar_url(rc_buffer_t* buffer, uint32_t image_type, const char* image_name) {
  rc_api_fetch_image_request_t image_request;
  rc_api_request_t request;
  int result;

  memset(&image_request, 0, sizeof(image_request));
  image_request.image_type = image_type;
  image_request.image_name = image_name;

  result = rc_api_init_fetch_image_request(&request, &image_request);
  if (result == RC_OK)
    return rc_buffer_strcpy(buffer, request.url);

  return NULL;
}
