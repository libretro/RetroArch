#include "cheevos_parser.h"

#include "cheevos_locals.h"

#include <encodings/utf.h>
#include <formats/rjson.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include "../deps/rcheevos/include/rcheevos.h"

/* C89 wants only int values in enums. */
#define CHEEVOS_JSON_KEY_GAMEID       0xb4960eecU
#define CHEEVOS_JSON_KEY_ACHIEVEMENTS 0x69749ae1U
#define CHEEVOS_JSON_KEY_ID           0x005973f2U
#define CHEEVOS_JSON_KEY_MEMADDR      0x1e76b53fU
#define CHEEVOS_JSON_KEY_TITLE        0x0e2a9a07U
#define CHEEVOS_JSON_KEY_DESCRIPTION  0xe61a1f69U
#define CHEEVOS_JSON_KEY_POINTS       0xca8fce22U
#define CHEEVOS_JSON_KEY_AUTHOR       0xa804edb8U
#define CHEEVOS_JSON_KEY_MODIFIED     0xdcea4fe6U
#define CHEEVOS_JSON_KEY_CREATED      0x3a84721dU
#define CHEEVOS_JSON_KEY_BADGENAME    0x887685d9U
#define CHEEVOS_JSON_KEY_CONSOLE_ID   0x071656e5U
#define CHEEVOS_JSON_KEY_TOKEN        0x0e2dbd26U
#define CHEEVOS_JSON_KEY_FLAGS        0x0d2e96b2U
#define CHEEVOS_JSON_KEY_LEADERBOARDS 0xf1247d2dU
#define CHEEVOS_JSON_KEY_RICHPRESENCE 0xf18dd230U
#define CHEEVOS_JSON_KEY_MEM          0x0b8807e4U
#define CHEEVOS_JSON_KEY_FORMAT       0xb341208eU
#define CHEEVOS_JSON_KEY_SUCCESS      0x110461deU
#define CHEEVOS_JSON_KEY_ERROR        0x0d2011cfU

typedef struct
{
   char*       value;
   int         is_key;
   size_t      length;
   unsigned    key_hash;
} rcheevos_getvalueud_t;

#define CHEEVOS_RJSON_OPTIONS                               \
      /* Inside the field RichPresencePatch newlines are
       * encoded as '\r\n'. This will filter the \r out. */ \
        RJSON_OPTION_IGNORE_STRING_CARRIAGE_RETURN          \
      /* This matches the behavior of the previously used
       * json parser. It is probably not required */        \
      | RJSON_OPTION_ALLOW_TRAILING_DATA

/*****************************************************************************
Gets a value in a JSON
*****************************************************************************/

static uint32_t rcheevos_djb2(const char* str, size_t length)
{
   const unsigned char* aux = (const unsigned char*)str;
   uint32_t            hash = 5381;

   while (length--)
      hash = (hash << 5) + hash + *aux++;

   return hash;
}

static bool rcheevos_getvalue_key(void* userdata,
      const char* name, size_t length)
{
   rcheevos_getvalueud_t* ud = (rcheevos_getvalueud_t*)userdata;

   ud->is_key = rcheevos_djb2(name, length) == ud->key_hash;
   return true;
}

static bool rcheevos_getvalue_string(void* userdata,
      const char* string, size_t length)
{
   rcheevos_getvalueud_t* ud = (rcheevos_getvalueud_t*)userdata;

   if (ud->is_key && ud->length > length)
   {
      strlcpy(ud->value, string, ud->length);
      ud->is_key = 2;
      return false;
   }

   return true;
}

static bool rcheevos_getvalue_boolean(void* userdata, bool istrue)
{
   rcheevos_getvalueud_t* ud = (rcheevos_getvalueud_t*)userdata;

   if (ud->is_key)
   {
      if (istrue && ud->length > 4)
      {
         strlcpy(ud->value, "true", ud->length);
         ud->is_key = 2;
         return false;
      }
      if (!istrue && ud->length > 5)
      {
         strlcpy(ud->value, "false", ud->length);
         ud->is_key = 2;
         return false;
      }
   }

   return true;
}

static bool rcheevos_getvalue_null(void* userdata)
{
   rcheevos_getvalueud_t* ud = (rcheevos_getvalueud_t*)userdata;

   if (ud->is_key && ud->length > 4)
   {
      strlcpy(ud->value, "null", ud->length);
      ud->is_key = 2;
      return false;
   }

   return true;
}

static int rcheevos_get_value(const char* json, unsigned key_hash,
      char* value, size_t length)
{
   rcheevos_getvalueud_t ud;

   ud.key_hash = key_hash;
   ud.is_key   = 0;
   ud.value    = value;
   ud.length   = length;
   *value      = 0;

   rjson_parse_quick(json, &ud, CHEEVOS_RJSON_OPTIONS,
         rcheevos_getvalue_key,
         rcheevos_getvalue_string,
         rcheevos_getvalue_string, /* number */
         NULL, NULL, NULL, NULL,
         rcheevos_getvalue_boolean,
         rcheevos_getvalue_null, NULL);

   return (ud.is_key == 2 ? 0 : -1);
}

/*****************************************************************************
Returns the token or the error message
*****************************************************************************/

int rcheevos_get_token(const char* json, char* token, size_t length)
{
   rcheevos_get_value(json, CHEEVOS_JSON_KEY_ERROR, token, length);

   if (!string_is_empty(token))
      return -1;

   return rcheevos_get_value(json, CHEEVOS_JSON_KEY_TOKEN, token, length);
}

int rcheevos_get_json_error(const char* json, char* token, size_t length)
{
   return rcheevos_get_value(json, CHEEVOS_JSON_KEY_ERROR, token, length);
}

/*****************************************************************************
Count number of achievements in a JSON file
*****************************************************************************/

typedef struct
{
   int      in_cheevos;
   int      in_lboards;
   int      has_error;
   uint32_t field_hash;
   unsigned core_count;
   unsigned unofficial_count;
   unsigned lboard_count;
} rcheevos_countud_t;

static bool rcheevos_count_end_array(void* userdata)
{
  rcheevos_countud_t* ud = (rcheevos_countud_t*)userdata;

   ud->in_cheevos       = 0;
   ud->in_lboards       = 0;
   return true;
}

static bool rcheevos_count_key(void* userdata,
      const char* name, size_t length)
{
   rcheevos_countud_t* ud = (rcheevos_countud_t*)userdata;

   ud->field_hash        = rcheevos_djb2(name, length);

   if (ud->field_hash == CHEEVOS_JSON_KEY_ACHIEVEMENTS)
      ud->in_cheevos = 1;
   else if (ud->field_hash == CHEEVOS_JSON_KEY_LEADERBOARDS)
      ud->in_lboards = 1;
   else if (ud->field_hash == CHEEVOS_JSON_KEY_ERROR)
      ud->has_error  = 1;

   return true;
}

static bool rcheevos_count_number(void* userdata,
      const char* number, size_t length)
{
   rcheevos_countud_t* ud = (rcheevos_countud_t*)userdata;

   if (ud->in_cheevos && ud->field_hash == CHEEVOS_JSON_KEY_FLAGS)
   {
      long flags = strtol(number, NULL, 10);

      if (flags == 3)
         ud->core_count++;       /* Core achievements */
      else if (flags == 5)
         ud->unofficial_count++; /* Unofficial achievements */
   }
   else if (ud->in_lboards && ud->field_hash == CHEEVOS_JSON_KEY_ID)
      ud->lboard_count++;

   return true;
}

static bool rcheevos_count_cheevos(const char* json,
      unsigned* core_count, unsigned* unofficial_count,
      unsigned* lboard_count, int* has_error)
{
   bool res;
   rcheevos_countud_t ud;
   ud.in_cheevos       = 0;
   ud.in_lboards       = 0;
   ud.has_error        = 0;
   ud.core_count       = 0;
   ud.unofficial_count = 0;
   ud.lboard_count     = 0;


   res = rjson_parse_quick(json, &ud, CHEEVOS_RJSON_OPTIONS,
               rcheevos_count_key,
               NULL,
               rcheevos_count_number,
               NULL, NULL, NULL,
               rcheevos_count_end_array,
               NULL, NULL, NULL);

   *core_count         = ud.core_count;
   *unofficial_count   = ud.unofficial_count;
   *lboard_count       = ud.lboard_count;
   *has_error          = ud.has_error;

   return res;
}

/*****************************************************************************
Parses the cheevos in the JSON
*****************************************************************************/

typedef struct
{
   int      in_cheevos;
   int      in_lboards;
   int      lboard_had_id;
   unsigned core_count;
   unsigned unofficial_count;
   unsigned lboard_count;

   unsigned     cheevo_flags;
   const char*  lboard_format;
   const char** field_string;
   unsigned*    field_unsigned;

   rcheevos_racheevo_t cheevo;
   rcheevos_ralboard_t lboard;

   rcheevos_rapatchdata_t* patchdata;
} rcheevos_readud_t;

static bool rcheevos_new_cheevo(rcheevos_readud_t* ud)
{
   rcheevos_racheevo_t* cheevo = NULL;

   if (ud->cheevo_flags == 3)
      cheevo = ud->patchdata->core + ud->core_count++;
   else if (ud->cheevo_flags == 5)
      cheevo = ud->patchdata->unofficial + ud->unofficial_count++;
   ud->cheevo_flags = 0;

   if (!cheevo
       || !ud->cheevo.title
       || !ud->cheevo.description
       || !ud->cheevo.badge
       || !ud->cheevo.memaddr)
   {
      CHEEVOS_FREE(ud->cheevo.title);
      CHEEVOS_FREE(ud->cheevo.description);
      CHEEVOS_FREE(ud->cheevo.badge);
      CHEEVOS_FREE(ud->cheevo.memaddr);
      memset(&ud->cheevo, 0, sizeof(ud->cheevo));
      return (cheevo ? false : true);
   }

   *cheevo = ud->cheevo;
   memset(&ud->cheevo, 0, sizeof(ud->cheevo));

   return true;
}

static bool rcheevos_new_lboard(rcheevos_readud_t* ud)
{
   rcheevos_ralboard_t* lboard = NULL;

   if (ud->lboard_had_id)
      lboard = ud->patchdata->lboards + ud->lboard_count++;
   ud->lboard_had_id = 0;

   if (!lboard
       || !ud->lboard.title
       || !ud->lboard.description
       || !ud->lboard.mem)
   {
      CHEEVOS_FREE(ud->lboard.title);
      CHEEVOS_FREE(ud->lboard.description);
      CHEEVOS_FREE(ud->lboard.mem);
      memset(&ud->lboard, 0, sizeof(ud->lboard));
      CHEEVOS_FREE(ud->lboard_format);
      ud->lboard_format = NULL;
      return (lboard ? false : true);
   }

   *lboard = ud->lboard;
   memset(&ud->lboard, 0, sizeof(ud->lboard));

   if (ud->lboard_format)
   {
      lboard->format = rc_parse_format(ud->lboard_format);
      CHEEVOS_FREE(ud->lboard_format);
      ud->lboard_format = NULL;
   }

   return true;
}

static bool rcheevos_read_end_object(void* userdata)
{
   rcheevos_readud_t* ud = (rcheevos_readud_t*)userdata;

   if (ud->in_cheevos)
      return rcheevos_new_cheevo(ud);

   if (ud->in_lboards)
      return rcheevos_new_lboard(ud);

   return true;
}

static bool rcheevos_read_end_array(void* userdata)
{
   rcheevos_readud_t* ud = (rcheevos_readud_t*)userdata;

   ud->in_cheevos       = 0;
   ud->in_lboards       = 0;
   return true;
}

static bool rcheevos_read_key(void* userdata,
      const char* name, size_t length)
{
   rcheevos_readud_t* ud = (rcheevos_readud_t*)userdata;

   uint32_t hash        = rcheevos_djb2(name, length);
   ud->field_unsigned   = NULL;
   ud->field_string     = NULL;

   switch (hash)
   {
      case CHEEVOS_JSON_KEY_ACHIEVEMENTS:
         ud->in_cheevos = 1;
         break;
      case CHEEVOS_JSON_KEY_LEADERBOARDS:
         ud->in_lboards = 1;
         break;
      case CHEEVOS_JSON_KEY_CONSOLE_ID:
         ud->field_unsigned = &ud->patchdata->console_id;
         break;
      case CHEEVOS_JSON_KEY_RICHPRESENCE:
         ud->field_string = (const char**)&ud->patchdata->richpresence_script;
         break;
      case CHEEVOS_JSON_KEY_ID:
         if (ud->in_cheevos)
            ud->field_unsigned = &ud->cheevo.id;
         else if (ud->in_lboards)
         {
            ud->field_unsigned = &ud->lboard.id;
            ud->lboard_had_id = 1;
         }
         else
            ud->field_unsigned = &ud->patchdata->game_id;
         break;
      case CHEEVOS_JSON_KEY_MEMADDR:
         if (ud->in_cheevos)
            ud->field_string = &ud->cheevo.memaddr;
         break;
      case CHEEVOS_JSON_KEY_MEM:
         if (ud->in_lboards)
            ud->field_string = &ud->lboard.mem;
         break;
      case CHEEVOS_JSON_KEY_TITLE:
         if (ud->in_cheevos)
            ud->field_string = &ud->cheevo.title;
         else if (ud->in_lboards)
            ud->field_string = &ud->lboard.title;
         else
            ud->field_string = (const char**)&ud->patchdata->title;
         break;
      case CHEEVOS_JSON_KEY_DESCRIPTION:
         if (ud->in_cheevos)
            ud->field_string = &ud->cheevo.description;
         else if (ud->in_lboards)
            ud->field_string = &ud->lboard.description;
         break;
      case CHEEVOS_JSON_KEY_POINTS:
         if (ud->in_cheevos)
            ud->field_unsigned = &ud->cheevo.points;
         break;
      /* UNUSED
      case CHEEVOS_JSON_KEY_AUTHOR:
         if (ud->in_cheevos)
            ud->field_string = &ud->cheevo.author;
         break;
      case CHEEVOS_JSON_KEY_MODIFIED:
         if (ud->in_cheevos)
            ud->field_string = &ud->cheevo.modified;
         break;
      case CHEEVOS_JSON_KEY_CREATED:
         if (ud->in_cheevos)
            ud->field_string = &ud->cheevo.created;
         break; */
      case CHEEVOS_JSON_KEY_BADGENAME:
         if (ud->in_cheevos)
            ud->field_string = &ud->cheevo.badge;
         break;
      case CHEEVOS_JSON_KEY_FLAGS:
         if (ud->in_cheevos)
            ud->field_unsigned = &ud->cheevo_flags;
         break;
      case CHEEVOS_JSON_KEY_FORMAT:
         if (ud->in_lboards)
            ud->field_string = &ud->lboard_format;
         break;
      default:
         break;
   }

   return true;
}

static bool rcheevos_read_string(void* userdata,
      const char* string, size_t length)
{
   rcheevos_readud_t* ud = (rcheevos_readud_t*)userdata;

   if (ud->field_string)
   {
      if (*ud->field_string)
         CHEEVOS_FREE((*ud->field_string));
      *ud->field_string = strdup(string);
      ud->field_string = NULL;
   }

   return true;
}

static bool rcheevos_read_number(void* userdata,
      const char* number, size_t length)
{
   rcheevos_readud_t* ud = (rcheevos_readud_t*)userdata;

   if (ud->field_unsigned)
   {
      *ud->field_unsigned = (unsigned)strtol(number, NULL, 10);
      ud->field_unsigned = NULL;
   }

   return true;
}

int rcheevos_get_patchdata(const char* json, rcheevos_rapatchdata_t* patchdata)
{
   rcheevos_readud_t ud;
   bool res;
   int has_error;

   /* Count the number of achievements in the JSON file. */
   res = rcheevos_count_cheevos(json, &patchdata->core_count,
      &patchdata->unofficial_count, &patchdata->lboard_count, &has_error);

   if (!res || has_error)
      return -1;

   /* Allocate the achievements. */

   patchdata->core = (rcheevos_racheevo_t*)
      calloc(patchdata->core_count, sizeof(rcheevos_racheevo_t));

   patchdata->unofficial = (rcheevos_racheevo_t*)
      calloc(patchdata->unofficial_count, sizeof(rcheevos_racheevo_t));

   patchdata->lboards = (rcheevos_ralboard_t*)
      calloc(patchdata->lboard_count, sizeof(rcheevos_ralboard_t));

   if (!patchdata->core       ||
       !patchdata->unofficial ||
       !patchdata->lboards)
   {
      CHEEVOS_FREE(patchdata->core);
      CHEEVOS_FREE(patchdata->unofficial);
      CHEEVOS_FREE(patchdata->lboards);

      return -1;
   }

   patchdata->richpresence_script = NULL;
   patchdata->title = NULL;

   /* Load the achievements. */
   memset(&ud, 0, sizeof(ud));
   ud.patchdata        = patchdata;

   res = rjson_parse_quick(json, &ud, CHEEVOS_RJSON_OPTIONS,
            rcheevos_read_key,
            rcheevos_read_string,
            rcheevos_read_number,
            NULL, rcheevos_read_end_object,
            NULL, rcheevos_read_end_array,
            NULL, NULL, NULL);

   CHEEVOS_FREE(ud.cheevo.title);
   CHEEVOS_FREE(ud.cheevo.description);
   CHEEVOS_FREE(ud.cheevo.badge);
   CHEEVOS_FREE(ud.cheevo.memaddr);
   CHEEVOS_FREE(ud.lboard.title);
   CHEEVOS_FREE(ud.lboard.description);
   CHEEVOS_FREE(ud.lboard.mem);
   CHEEVOS_FREE(ud.lboard_format);

   if (!res)
   {
      rcheevos_free_patchdata(patchdata);
      return -1;
   }

   return 0;
}

/*****************************************************************************
Frees the patchdata
*****************************************************************************/

void rcheevos_free_patchdata(rcheevos_rapatchdata_t* patchdata)
{
   unsigned i = 0, count = 0;
   const rcheevos_racheevo_t* cheevo = NULL;
   const rcheevos_ralboard_t* lboard = NULL;

   cheevo = patchdata->core;

   for (i = 0, count = patchdata->core_count; i < count; i++, cheevo++)
   {
      CHEEVOS_FREE(cheevo->title);
      CHEEVOS_FREE(cheevo->description);
      CHEEVOS_FREE(cheevo->badge);
      CHEEVOS_FREE(cheevo->memaddr);
   }

   cheevo = patchdata->unofficial;

   for (i = 0, count = patchdata->unofficial_count; i < count; i++, cheevo++)
   {
      CHEEVOS_FREE(cheevo->title);
      CHEEVOS_FREE(cheevo->description);
      CHEEVOS_FREE(cheevo->badge);
      CHEEVOS_FREE(cheevo->memaddr);
   }

   lboard = patchdata->lboards;

   for (i = 0, count = patchdata->lboard_count; i < count; i++, lboard++)
   {
      CHEEVOS_FREE(lboard->title);
      CHEEVOS_FREE(lboard->description);
      CHEEVOS_FREE(lboard->mem);
   }

   CHEEVOS_FREE(patchdata->core);
   CHEEVOS_FREE(patchdata->unofficial);
   CHEEVOS_FREE(patchdata->lboards);
   CHEEVOS_FREE(patchdata->richpresence_script);
   CHEEVOS_FREE(patchdata->title);

   patchdata->game_id          = 0;
   patchdata->console_id       = 0;
   patchdata->core             = NULL;
   patchdata->unofficial       = NULL;
   patchdata->lboards          = NULL;
   patchdata->title            = NULL;
   patchdata->richpresence_script = NULL;
   patchdata->core_count       = 0;
   patchdata->unofficial_count = 0;
   patchdata->lboard_count     = 0;
}

/*****************************************************************************
Deactivates unlocked cheevos
*****************************************************************************/

typedef struct
{
   int is_element;
   rcheevos_unlock_cb_t unlock_cb;
   void* userdata;
} rcheevos_deactivate_t;

static bool rcheevos_deactivate_elements_begin(void* userdata)
{
   rcheevos_deactivate_t* ud = (rcheevos_deactivate_t*)userdata;

   ud->is_element           = 1;
   return true;
}

static bool rcheevos_deactivate_elements_stop(void* userdata)
{
   rcheevos_deactivate_t* ud = (rcheevos_deactivate_t*)userdata;

   ud->is_element           = 0;
   return true;
}

static bool rcheevos_deactivate_number(void* userdata,
      const char* number, size_t length)
{
   rcheevos_deactivate_t* ud = (rcheevos_deactivate_t*)userdata;
   unsigned id              = 0;

   if (ud->is_element)
   {
      id             = (unsigned)strtol(number, NULL, 10);

      ud->unlock_cb(id, ud->userdata);
   }

   return true;
}

void rcheevos_deactivate_unlocks(const char* json, rcheevos_unlock_cb_t unlock_cb, void* userdata)
{
   rcheevos_deactivate_t ud;

   ud.is_element = 0;
   ud.unlock_cb  = unlock_cb;
   ud.userdata   = userdata;

   rjson_parse_quick(json, &ud, CHEEVOS_RJSON_OPTIONS,
         NULL, NULL,
         rcheevos_deactivate_number,
         rcheevos_deactivate_elements_stop,
         rcheevos_deactivate_elements_stop,
         rcheevos_deactivate_elements_begin,
         rcheevos_deactivate_elements_stop,
         NULL, NULL, NULL);
}

/*****************************************************************************
Returns the game ID
*****************************************************************************/

unsigned chevos_get_gameid(const char* json)
{
   char gameid[32];

   if (rcheevos_get_value(json, CHEEVOS_JSON_KEY_GAMEID, gameid, sizeof(gameid)) != 0)
      return 0;

   return (unsigned)strtol(gameid, NULL, 10);
}
