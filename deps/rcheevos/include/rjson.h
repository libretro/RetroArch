#ifndef RJSON_H
#define RJSON_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
  RC_JSON_OK = 0,
  RC_JSON_OBJECT_EXPECTED = -1,
  RC_JSON_UNKOWN_RECORD = -2,
  RC_JSON_EOF_EXPECTED = -3,
  RC_JSON_MISSING_KEY = -4,
  RC_JSON_UNTERMINATED_KEY = -5,
  RC_JSON_MISSING_VALUE = -6,
  RC_JSON_UNTERMINATED_OBJECT = -7,
  RC_JSON_INVALID_VALUE = -8,
  RC_JSON_UNTERMINATED_STRING = -9,
  RC_JSON_UNTERMINATED_ARRAY = -10,
  RC_JSON_INVALID_ESCAPE = -11
};

typedef struct {
  unsigned int gameid;
  char success;
}
rc_json_gameid_t;

int rc_json_get_gameid_size(const char* json);
const rc_json_gameid_t* rc_json_parse_gameid(void* buffer, const char* json);

typedef struct {
  const char* token;
  const char* user;
  unsigned int score;
  unsigned int messages;
  char success;
}
rc_json_login_t;

int rc_json_get_login_size(const char* json);
const rc_json_login_t* rc_json_parse_login(void* buffer, const char* json);

typedef struct {
  unsigned long long created;
  unsigned long long modified;
  const char* author;
  const char* badge;
  const char* description;
  const char* memaddr;
  const char* title;
  unsigned int flags;
  unsigned int points;
  unsigned int id;
}
rc_json_cheevo_t;

int rc_json_get_cheevo_size(const char* json);
const rc_json_cheevo_t* rc_json_parse_cheevo(void* buffer, const char* json);

typedef struct {
  const char* description;
  const char* title;
  const char* format;
  const char* mem;
  unsigned int id;
}
rc_json_lboard_t;

int rc_json_get_lboard_size(const char* json);
const rc_json_lboard_t* rc_json_parse_lboard(void* buffer, const char* json);

typedef struct {
  const rc_json_lboard_t* lboards; int lboards_count;
  const char* genre;
  const char* developer;
  const char* publisher;
  const char* released;
  const char** presence;
  const char* console;
  const rc_json_cheevo_t* cheevos; int cheevos_count;
  const char* image_boxart;
  const char* image_title;
  const char* image_icon;
  const char* title;
  const char* image_ingame;
  unsigned int consoleid;
  unsigned int id;
  unsigned int flags;
  unsigned int topicid;
  char is_final;
}
rc_json_patchdata_t;

int rc_json_get_patchdata_size(const char* json);
const rc_json_patchdata_t* rc_json_parse_patchdata(void* buffer, const char* json);

typedef struct {
  rc_json_patchdata_t patchdata;
  char success;
}
rc_json_patch_t;

int rc_json_get_patch_size(const char* json);
const rc_json_patch_t* rc_json_parse_patch(void* buffer, const char* json);

typedef struct {
  const unsigned int* ids; int ids_count;
  unsigned int gameid;
  char success;
  char hardcore;
}
rc_json_unlocks_t;

int rc_json_get_unlocks_size(const char* json);
const rc_json_unlocks_t* rc_json_parse_unlocks(void* buffer, const char* json);

typedef struct {
  const char* error;
  char success;
}
rc_json_error_t;

int rc_json_get_error_size(const char* json);
const rc_json_error_t* rc_json_parse_error(void* buffer, const char* json);

#ifdef __cplusplus
}
#endif

#endif /* RJSON_H */
