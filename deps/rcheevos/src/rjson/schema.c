#include "rjson.h"
#include "dejson.h"

static const rc_json_field_meta_t rc_json_field_meta_gameid[] = {
  {
    /* Metadata for field unsigned int gameid;. */
    /* name_hash */ 0xb4960eecU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_gameid_t, gameid),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field char success;. */
    /* name_hash */ 0x110461deU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_gameid_t, success),
    /* type      */ RC_JSON_TYPE_BOOL,
    /* flags     */ 0
  },
};

static const rc_json_struct_meta_t rc_json_struct_meta_gameid = {
  /* fields     */ rc_json_field_meta_gameid,
  /* name_hash  */ 0xb4960eecU,
  /* size       */ sizeof(rc_json_gameid_t),
  /* alignment  */ RC_JSON_ALIGNOF(rc_json_gameid_t),
  /* num_fields */ 2
};

int rc_json_get_gameid_size(const char* json) {
  size_t size;
  int res = rc_json_get_size(&size, 0xb4960eecU, (const uint8_t*)json);

  if (res == RC_JSON_OK) {
    res = (int)size;
  }

  return res;
}

const rc_json_gameid_t* rc_json_parse_gameid(void* buffer, const char* json) {
  return (const rc_json_gameid_t*)rc_json_deserialize(buffer, 0xb4960eecU, (const uint8_t*)json);
}

static const rc_json_field_meta_t rc_json_field_meta_login[] = {
  {
    /* Metadata for field const char* token;. */
    /* name_hash */ 0x0e2dbd26U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_login_t, token),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* user;. */
    /* name_hash */ 0x7c8da264U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_login_t, user),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int score;. */
    /* name_hash */ 0x0e1522c1U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_login_t, score),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int messages;. */
    /* name_hash */ 0xfed3807dU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_login_t, messages),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field char success;. */
    /* name_hash */ 0x110461deU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_login_t, success),
    /* type      */ RC_JSON_TYPE_BOOL,
    /* flags     */ 0
  },
};

static const rc_json_struct_meta_t rc_json_struct_meta_login = {
  /* fields     */ rc_json_field_meta_login,
  /* name_hash  */ 0x0d9ce89eU,
  /* size       */ sizeof(rc_json_login_t),
  /* alignment  */ RC_JSON_ALIGNOF(rc_json_login_t),
  /* num_fields */ 5
};

int rc_json_get_login_size(const char* json) {
  size_t size;
  int res = rc_json_get_size(&size, 0x0d9ce89eU, (const uint8_t*)json);

  if (res == RC_JSON_OK) {
    res = (int)size;
  }

  return res;
}

const rc_json_login_t* rc_json_parse_login(void* buffer, const char* json) {
  return (const rc_json_login_t*)rc_json_deserialize(buffer, 0x0d9ce89eU, (const uint8_t*)json);
}

static const rc_json_field_meta_t rc_json_field_meta_cheevo[] = {
  {
    /* Metadata for field unsigned long long created;. */
    /* name_hash */ 0x3a84721dU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, created),
    /* type      */ RC_JSON_TYPE_ULONGLONG,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned long long modified;. */
    /* name_hash */ 0xdcea4fe6U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, modified),
    /* type      */ RC_JSON_TYPE_ULONGLONG,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* author;. */
    /* name_hash */ 0xa804edb8U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, author),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* badge;. */
    /* name_hash */ 0x887685d9U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, badge),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* description;. */
    /* name_hash */ 0xe61a1f69U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, description),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* memaddr;. */
    /* name_hash */ 0x1e76b53fU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, memaddr),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* title;. */
    /* name_hash */ 0x0e2a9a07U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, title),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int flags;. */
    /* name_hash */ 0x0d2e96b2U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, flags),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int points;. */
    /* name_hash */ 0xca8fce22U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, points),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int id;. */
    /* name_hash */ 0x005973f2U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_cheevo_t, id),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
};

static const rc_json_struct_meta_t rc_json_struct_meta_cheevo = {
  /* fields     */ rc_json_field_meta_cheevo,
  /* name_hash  */ 0x0af404aeU,
  /* size       */ sizeof(rc_json_cheevo_t),
  /* alignment  */ RC_JSON_ALIGNOF(rc_json_cheevo_t),
  /* num_fields */ 10
};

int rc_json_get_cheevo_size(const char* json) {
  size_t size;
  int res = rc_json_get_size(&size, 0x0af404aeU, (const uint8_t*)json);

  if (res == RC_JSON_OK) {
    res = (int)size;
  }

  return res;
}

const rc_json_cheevo_t* rc_json_parse_cheevo(void* buffer, const char* json) {
  return (const rc_json_cheevo_t*)rc_json_deserialize(buffer, 0x0af404aeU, (const uint8_t*)json);
}

static const rc_json_field_meta_t rc_json_field_meta_lboard[] = {
  {
    /* Metadata for field const char* description;. */
    /* name_hash */ 0xe61a1f69U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_lboard_t, description),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* title;. */
    /* name_hash */ 0x0e2a9a07U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_lboard_t, title),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* format;. */
    /* name_hash */ 0xb341208eU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_lboard_t, format),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* mem;. */
    /* name_hash */ 0x0b8807e4U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_lboard_t, mem),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int id;. */
    /* name_hash */ 0x005973f2U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_lboard_t, id),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
};

static const rc_json_struct_meta_t rc_json_struct_meta_lboard = {
  /* fields     */ rc_json_field_meta_lboard,
  /* name_hash  */ 0xf7cacd7aU,
  /* size       */ sizeof(rc_json_lboard_t),
  /* alignment  */ RC_JSON_ALIGNOF(rc_json_lboard_t),
  /* num_fields */ 5
};

int rc_json_get_lboard_size(const char* json) {
  size_t size;
  int res = rc_json_get_size(&size, 0xf7cacd7aU, (const uint8_t*)json);

  if (res == RC_JSON_OK) {
    res = (int)size;
  }

  return res;
}

const rc_json_lboard_t* rc_json_parse_lboard(void* buffer, const char* json) {
  return (const rc_json_lboard_t*)rc_json_deserialize(buffer, 0xf7cacd7aU, (const uint8_t*)json);
}

static const rc_json_field_meta_t rc_json_field_meta_patchdata[] = {
  {
    /* Metadata for field const rc_json_lboard_t* lboards; int lboards_count;. */
    /* name_hash */ 0xf1247d2dU,
    /* type_hash */ 0xf7cacd7aU,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, lboards),
    /* type      */ RC_JSON_TYPE_RECORD,
    /* flags     */ RC_JSON_FLAG_ARRAY
  },
  {
    /* Metadata for field const char* genre;. */
    /* name_hash */ 0x0d3d1136U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, genre),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* developer;. */
    /* name_hash */ 0x87f5b28bU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, developer),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* publisher;. */
    /* name_hash */ 0xce7b6ff3U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, publisher),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* released;. */
    /* name_hash */ 0x8acb686aU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, released),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char** presence;. */
    /* name_hash */ 0xf18dd230U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, presence),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ RC_JSON_FLAG_POINTER
  },
  {
    /* Metadata for field const char* console;. */
    /* name_hash */ 0x260aebd9U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, console),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const rc_json_cheevo_t* cheevos; int cheevos_count;. */
    /* name_hash */ 0x69749ae1U,
    /* type_hash */ 0x0af404aeU,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, cheevos),
    /* type      */ RC_JSON_TYPE_RECORD,
    /* flags     */ RC_JSON_FLAG_ARRAY
  },
  {
    /* Metadata for field const char* image_boxart;. */
    /* name_hash */ 0xddc6bd18U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, image_boxart),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* image_title;. */
    /* name_hash */ 0x65121b6aU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, image_title),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* image_icon;. */
    /* name_hash */ 0xe4022c11U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, image_icon),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* title;. */
    /* name_hash */ 0x0e2a9a07U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, title),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field const char* image_ingame;. */
    /* name_hash */ 0xedfff5f9U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, image_ingame),
    /* type      */ RC_JSON_TYPE_STRING,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int consoleid;. */
    /* name_hash */ 0x071656e5U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, consoleid),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int id;. */
    /* name_hash */ 0x005973f2U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, id),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int flags;. */
    /* name_hash */ 0x0d2e96b2U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, flags),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field unsigned int topicid;. */
    /* name_hash */ 0x7d2b565aU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, topicid),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field char is_final;. */
    /* name_hash */ 0x088a58abU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patchdata_t, is_final),
    /* type      */ RC_JSON_TYPE_BOOL,
    /* flags     */ 0
  },
};

static const rc_json_struct_meta_t rc_json_struct_meta_patchdata = {
  /* fields     */ rc_json_field_meta_patchdata,
  /* name_hash  */ 0xadc4ac0fU,
  /* size       */ sizeof(rc_json_patchdata_t),
  /* alignment  */ RC_JSON_ALIGNOF(rc_json_patchdata_t),
  /* num_fields */ 18
};

int rc_json_get_patchdata_size(const char* json) {
  size_t size;
  int res = rc_json_get_size(&size, 0xadc4ac0fU, (const uint8_t*)json);

  if (res == RC_JSON_OK) {
    res = (int)size;
  }

  return res;
}

const rc_json_patchdata_t* rc_json_parse_patchdata(void* buffer, const char* json) {
  return (const rc_json_patchdata_t*)rc_json_deserialize(buffer, 0xadc4ac0fU, (const uint8_t*)json);
}

static const rc_json_field_meta_t rc_json_field_meta_patch[] = {
  {
    /* Metadata for field rc_json_patchdata_t patchdata;. */
    /* name_hash */ 0xadc4ac0fU,
    /* type_hash */ 0xadc4ac0fU,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patch_t, patchdata),
    /* type      */ RC_JSON_TYPE_RECORD,
    /* flags     */ 0
  },
  {
    /* Metadata for field char success;. */
    /* name_hash */ 0x110461deU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_patch_t, success),
    /* type      */ RC_JSON_TYPE_BOOL,
    /* flags     */ 0
  },
};

static const rc_json_struct_meta_t rc_json_struct_meta_patch = {
  /* fields     */ rc_json_field_meta_patch,
  /* name_hash  */ 0x0dddd3d5U,
  /* size       */ sizeof(rc_json_patch_t),
  /* alignment  */ RC_JSON_ALIGNOF(rc_json_patch_t),
  /* num_fields */ 2
};

int rc_json_get_patch_size(const char* json) {
  size_t size;
  int res = rc_json_get_size(&size, 0x0dddd3d5U, (const uint8_t*)json);

  if (res == RC_JSON_OK) {
    res = (int)size;
  }

  return res;
}

const rc_json_patch_t* rc_json_parse_patch(void* buffer, const char* json) {
  return (const rc_json_patch_t*)rc_json_deserialize(buffer, 0x0dddd3d5U, (const uint8_t*)json);
}

static const rc_json_field_meta_t rc_json_field_meta_unlocks[] = {
  {
    /* Metadata for field const unsigned int* ids; int ids_count;. */
    /* name_hash */ 0xc5e91303U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_unlocks_t, ids),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ RC_JSON_FLAG_ARRAY
  },
  {
    /* Metadata for field unsigned int gameid;. */
    /* name_hash */ 0xb4960eecU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_unlocks_t, gameid),
    /* type      */ RC_JSON_TYPE_UINT,
    /* flags     */ 0
  },
  {
    /* Metadata for field char success;. */
    /* name_hash */ 0x110461deU,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_unlocks_t, success),
    /* type      */ RC_JSON_TYPE_BOOL,
    /* flags     */ 0
  },
  {
    /* Metadata for field char hardcore;. */
    /* name_hash */ 0xc1b80672U,
    /* type_hash */ 0x00000000U,
    /* offset    */ RC_JSON_OFFSETOF(rc_json_unlocks_t, hardcore),
    /* type      */ RC_JSON_TYPE_BOOL,
    /* flags     */ 0
  },
};

static const rc_json_struct_meta_t rc_json_struct_meta_unlocks = {
  /* fields     */ rc_json_field_meta_unlocks,
  /* name_hash  */ 0x9b4e2684U,
  /* size       */ sizeof(rc_json_unlocks_t),
  /* alignment  */ RC_JSON_ALIGNOF(rc_json_unlocks_t),
  /* num_fields */ 4
};

int rc_json_get_unlocks_size(const char* json) {
  size_t size;
  int res = rc_json_get_size(&size, 0x9b4e2684U, (const uint8_t*)json);

  if (res == RC_JSON_OK) {
    res = (int)size;
  }

  return res;
}

const rc_json_unlocks_t* rc_json_parse_unlocks(void* buffer, const char* json) {
  return (const rc_json_unlocks_t*)rc_json_deserialize(buffer, 0x9b4e2684U, (const uint8_t*)json);
}

const rc_json_struct_meta_t* rc_json_resolve_struct(unsigned hash) {

  switch (hash) {
    case 0xb4960eecU: return &rc_json_struct_meta_gameid;
    case 0x0d9ce89eU: return &rc_json_struct_meta_login;
    case 0x0af404aeU: return &rc_json_struct_meta_cheevo;
    case 0xf7cacd7aU: return &rc_json_struct_meta_lboard;
    case 0xadc4ac0fU: return &rc_json_struct_meta_patchdata;
    case 0x0dddd3d5U: return &rc_json_struct_meta_patch;
    case 0x9b4e2684U: return &rc_json_struct_meta_unlocks;
    default: return NULL;
  }
}
