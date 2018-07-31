#ifndef DEJSON_H
#define DEJSON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RC_JSON_OFFSETOF(s, f) ((size_t)(&((s*)0)->f))
#define RC_JSON_ALIGNOF(t) RC_JSON_OFFSETOF(struct{char c; t d;}, d)

enum {
  RC_JSON_TYPE_CHAR,
  RC_JSON_TYPE_UCHAR,
  RC_JSON_TYPE_SHORT,
  RC_JSON_TYPE_USHORT,
  RC_JSON_TYPE_INT,
  RC_JSON_TYPE_UINT,
  RC_JSON_TYPE_LONG,
  RC_JSON_TYPE_LONGLONG,
  RC_JSON_TYPE_ULONG,
  RC_JSON_TYPE_ULONGLONG,
  RC_JSON_TYPE_INT8,
  RC_JSON_TYPE_INT16,
  RC_JSON_TYPE_INT32,
  RC_JSON_TYPE_INT64,
  RC_JSON_TYPE_UINT8,
  RC_JSON_TYPE_UINT16,
  RC_JSON_TYPE_UINT32,
  RC_JSON_TYPE_UINT64,
  RC_JSON_TYPE_FLOAT,
  RC_JSON_TYPE_DOUBLE,
  RC_JSON_TYPE_BOOL,
  RC_JSON_TYPE_STRING,
  RC_JSON_TYPE_RECORD
};

enum {
  RC_JSON_FLAG_ARRAY   = 1 << 0,
  RC_JSON_FLAG_POINTER = 1 << 1
};

typedef struct {
  uint32_t name_hash;
  uint32_t type_hash;
  uint16_t offset;
  uint8_t  type;
  uint8_t  flags;
}
rc_json_field_meta_t;

typedef struct {
  const rc_json_field_meta_t* fields;
  uint32_t name_hash;
  uint32_t size;
  uint16_t alignment;
  uint16_t num_fields;
}
rc_json_struct_meta_t;

void* rc_json_deserialize(void* buffer, uint32_t hash, const uint8_t* json);
int rc_json_get_size(size_t* size, uint32_t hash, const uint8_t* json);
uint32_t rc_json_hash(const uint8_t* str, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* DEJSON_H */
