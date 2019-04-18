#include "parser.h"

#include "hash.h"
#include "util.h"

#include <formats/jsonsax.h>
#include <string/stdstring.h>
#include <compat/strl.h>

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
#define CHEEVOS_JSON_KEY_MEM          0x0b8807e4U
#define CHEEVOS_JSON_KEY_FORMAT       0xb341208eU
#define CHEEVOS_JSON_KEY_SUCCESS      0x110461deU
#define CHEEVOS_JSON_KEY_ERROR        0x0d2011cfU

/*****************************************************************************
Gets a value in a JSON
*****************************************************************************/

typedef struct
{
   unsigned    key_hash;
   int         is_key;
   const char* value;
   size_t      length;
} cheevos_getvalueud_t;

static int cheevos_getvalue_key(void* userdata,
      const char* name, size_t length)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   ud->is_key = cheevos_djb2(name, length) == ud->key_hash;
   return 0;
}

static int cheevos_getvalue_string(void* userdata,
      const char* string, size_t length)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   if (ud->is_key)
   {
      ud->value  = string;
      ud->length = length;
      ud->is_key = 0;
   }

   return 0;
}

static int cheevos_getvalue_boolean(void* userdata, int istrue)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   if (ud->is_key)
   {
      if (istrue)
      {
         ud->value  = "true";
         ud->length = 4;
      }
      else
      {
         ud->value  = "false";
         ud->length = 5;
      }

      ud->is_key    = 0;
   }

   return 0;
}

static int cheevos_getvalue_null(void* userdata)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   if (ud->is_key )
   {
      ud->value = "null";
      ud->length = 4;
      ud->is_key = 0;
   }

   return 0;
}

static int cheevos_get_value(const char* json, unsigned key_hash,
      char* value, size_t length)
{
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      cheevos_getvalue_key,
      NULL,
      cheevos_getvalue_string,
      cheevos_getvalue_string, /* number */
      cheevos_getvalue_boolean,
      cheevos_getvalue_null
   };

   cheevos_getvalueud_t ud;

   ud.key_hash = key_hash;
   ud.is_key   = 0;
   ud.value    = NULL;
   ud.length   = 0;
   *value      = 0;

   if ((jsonsax_parse(json, &handlers, (void*)&ud) == JSONSAX_OK)
         && ud.value && ud.length < length)
   {
      strlcpy(value, ud.value, ud.length + 1);
      return 0;
   }

   return -1;
}

/*****************************************************************************
Returns the token of the error message
*****************************************************************************/

int cheevos_get_token(const char* json, char* token, size_t length)
{
   cheevos_get_value(json, CHEEVOS_JSON_KEY_ERROR, token, length);

   if (!string_is_empty(token))
      return -1;

   return cheevos_get_value(json, CHEEVOS_JSON_KEY_TOKEN, token, length);
}

/*****************************************************************************
Count number of achievements in a JSON file
*****************************************************************************/

typedef struct
{
   int      in_cheevos;
   int      in_lboards;
   uint32_t field_hash;
   unsigned core_count;
   unsigned unofficial_count;
   unsigned lboard_count;
} cheevos_countud_t;

static int cheevos_count_end_array(void* userdata)
{
  cheevos_countud_t* ud = (cheevos_countud_t*)userdata;

   ud->in_cheevos       = 0;
   ud->in_lboards       = 0;
   return 0;
}

static int cheevos_count_key(void* userdata,
      const char* name, size_t length)
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;

   ud->field_hash        = cheevos_djb2(name, length);

   if (ud->field_hash == CHEEVOS_JSON_KEY_ACHIEVEMENTS)
      ud->in_cheevos     = 1;
   else if (ud->field_hash == CHEEVOS_JSON_KEY_LEADERBOARDS)
      ud->in_lboards     = 1;

   return 0;
}

static int cheevos_count_number(void* userdata,
      const char* number, size_t length)
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;

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

   return 0;
}

static int cheevos_count_cheevos(const char* json,
      unsigned* core_count, unsigned* unofficial_count,
      unsigned* lboard_count)
{
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      cheevos_count_end_array,
      cheevos_count_key,
      NULL,
      NULL,
      cheevos_count_number,
      NULL,
      NULL
   };

   int res;
   cheevos_countud_t ud;
   ud.in_cheevos       = 0;
   ud.in_lboards       = 0;
   ud.core_count       = 0;
   ud.unofficial_count = 0;
   ud.lboard_count     = 0;

   res                 = jsonsax_parse(json, &handlers, (void*)&ud);

   *core_count         = ud.core_count;
   *unofficial_count   = ud.unofficial_count;
   *lboard_count       = ud.lboard_count;

   return res;
}

/*****************************************************************************
Parses the cheevos in the JSON
*****************************************************************************/

typedef struct
{
   const char* string;
   size_t      length;
} cheevos_field_t;

typedef struct
{
   int      in_cheevos;
   int      in_lboards;
   int      is_console_id;
   unsigned core_count;
   unsigned unofficial_count;
   unsigned lboard_count;

   cheevos_field_t* field;
   cheevos_field_t  id, memaddr, title, desc, points, author;
   cheevos_field_t  modified, created, badge, flags, format;

   cheevos_rapatchdata_t* patchdata;
} cheevos_readud_t;

static const char* cheevos_dupstr(const cheevos_field_t* field)
{
   char* string = (char*)malloc(field->length + 1);

   if (!string)
      return NULL;

   memcpy((void*)string, (void*)field->string, field->length);
   string[field->length] = 0;
   return string;
}

static int cheevos_new_cheevo(cheevos_readud_t* ud)
{
   cheevos_racheevo_t* cheevo = NULL;
   unsigned flags             = (unsigned)strtol(ud->flags.string, NULL, 10);

   if (flags == 3)
      cheevo = ud->patchdata->core + ud->core_count++;
   else if (flags == 5)
      cheevo = ud->patchdata->unofficial + ud->unofficial_count++;
   else
      return 0;

   cheevo->title       = cheevos_dupstr(&ud->title);
   cheevo->description = cheevos_dupstr(&ud->desc);
   cheevo->badge       = cheevos_dupstr(&ud->badge);
   cheevo->memaddr     = cheevos_dupstr(&ud->memaddr);
   cheevo->points      = (unsigned)strtol(ud->points.string, NULL, 10);
   cheevo->id          = (unsigned)strtol(ud->id.string, NULL, 10);

   if (   !cheevo->title
       || !cheevo->description
       || !cheevo->badge
       || !cheevo->memaddr)
   {
      CHEEVOS_FREE(cheevo->title);
      CHEEVOS_FREE(cheevo->description);
      CHEEVOS_FREE(cheevo->badge);
      CHEEVOS_FREE(cheevo->memaddr);
      return -1;
   }

   return 0;
}

static int cheevos_new_lboard(cheevos_readud_t* ud)
{
   cheevos_ralboard_t* lboard = ud->patchdata->lboards + ud->lboard_count++;

   lboard->title              = cheevos_dupstr(&ud->title);
   lboard->description        = cheevos_dupstr(&ud->desc);
   lboard->format             = cheevos_dupstr(&ud->format);
   lboard->mem                = cheevos_dupstr(&ud->memaddr);
   lboard->id                 = (unsigned)strtol(ud->id.string, NULL, 10);

   if (   !lboard->title
       || !lboard->description
       || !lboard->format
       || !lboard->mem)
   {
      CHEEVOS_FREE(lboard->title);
      CHEEVOS_FREE(lboard->description);
      CHEEVOS_FREE(lboard->format);
      CHEEVOS_FREE(lboard->mem);
      return -1;
   }

   return 0;
}

static int cheevos_read_end_object(void* userdata)
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;

   if (ud->in_cheevos)
      return cheevos_new_cheevo(ud);

   if (ud->in_lboards)
      return cheevos_new_lboard(ud);

   return 0;
}

static int cheevos_read_end_array(void* userdata)
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;

   ud->in_cheevos       = 0;
   ud->in_lboards       = 0;
   return 0;
}

static int cheevos_read_key(void* userdata,
      const char* name, size_t length)
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;

   int common           = ud->in_cheevos || ud->in_lboards;
   uint32_t hash        = cheevos_djb2(name, length);
   ud->field            = NULL;

   switch (hash)
   {
      case CHEEVOS_JSON_KEY_ACHIEVEMENTS:
         ud->in_cheevos = 1;
         break;
      case CHEEVOS_JSON_KEY_LEADERBOARDS:
         ud->in_lboards = 1;
         break;
      case CHEEVOS_JSON_KEY_CONSOLE_ID:
         ud->is_console_id = 1;
         break;
      case CHEEVOS_JSON_KEY_ID:
         if (common)
            ud->field = &ud->id;
         break;
      case CHEEVOS_JSON_KEY_MEMADDR:
         if (ud->in_cheevos)
            ud->field = &ud->memaddr;
         break;
      case CHEEVOS_JSON_KEY_MEM:
         if (ud->in_lboards)
            ud->field = &ud->memaddr;
         break;
      case CHEEVOS_JSON_KEY_TITLE:
         if (common)
            ud->field = &ud->title;
         break;
      case CHEEVOS_JSON_KEY_DESCRIPTION:
         if (common)
            ud->field = &ud->desc;
         break;
      case CHEEVOS_JSON_KEY_POINTS:
         if (ud->in_cheevos)
            ud->field = &ud->points;
         break;
      case CHEEVOS_JSON_KEY_AUTHOR:
         if (ud->in_cheevos)
            ud->field = &ud->author;
         break;
      case CHEEVOS_JSON_KEY_MODIFIED:
         if (ud->in_cheevos)
            ud->field = &ud->modified;
         break;
      case CHEEVOS_JSON_KEY_CREATED:
         if (ud->in_cheevos)
            ud->field = &ud->created;
         break;
      case CHEEVOS_JSON_KEY_BADGENAME:
         if (ud->in_cheevos)
            ud->field = &ud->badge;
         break;
      case CHEEVOS_JSON_KEY_FLAGS:
         if (ud->in_cheevos)
            ud->field = &ud->flags;
         break;
      case CHEEVOS_JSON_KEY_FORMAT:
         if (ud->in_lboards)
            ud->field = &ud->format;
         break;
      default:
         break;
   }

   return 0;
}

static int cheevos_read_string(void* userdata,
      const char* string, size_t length)
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;

   if (ud->field)
   {
      ud->field->string = string;
      ud->field->length = length;
   }

   return 0;
}

static int cheevos_read_number(void* userdata,
      const char* number, size_t length)
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;

   if (ud->field)
   {
      ud->field->string = number;
      ud->field->length = length;
   }
   else if (ud->is_console_id)
   {
      ud->patchdata->console_id = strtol(number, NULL, 10);
      ud->is_console_id         = 0;
   }

   return 0;
}

int cheevos_get_patchdata(const char* json, cheevos_rapatchdata_t* patchdata)
{
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      cheevos_read_end_object,
      NULL,
      cheevos_read_end_array,
      cheevos_read_key,
      NULL,
      cheevos_read_string,
      cheevos_read_number,
      NULL,
      NULL
   };

   cheevos_readud_t ud;
   int res;

   /* Count the number of achievements in the JSON file. */
   res = cheevos_count_cheevos(json, &patchdata->core_count,
      &patchdata->unofficial_count, &patchdata->lboard_count);

   if (res != JSONSAX_OK)
      return -1;

   /* Allocate the achievements. */

   patchdata->core = (cheevos_racheevo_t*)
      calloc(patchdata->core_count, sizeof(cheevos_racheevo_t));

   patchdata->unofficial = (cheevos_racheevo_t*)
      calloc(patchdata->unofficial_count, sizeof(cheevos_racheevo_t));

   patchdata->lboards = (cheevos_ralboard_t*)
      calloc(patchdata->lboard_count, sizeof(cheevos_ralboard_t));

   if (!patchdata->core       ||
       !patchdata->unofficial ||
       !patchdata->lboards)
   {
      CHEEVOS_FREE(patchdata->core);
      CHEEVOS_FREE(patchdata->unofficial);
      CHEEVOS_FREE(patchdata->lboards);

      return -1;
   }

   /* Load the achievements. */
   ud.in_cheevos       = 0;
   ud.in_lboards       = 0;
   ud.is_console_id    = 0;
   ud.field            = NULL;
   ud.core_count       = 0;
   ud.unofficial_count = 0;
   ud.lboard_count     = 0;
   ud.patchdata        = patchdata;

   if (jsonsax_parse(json, &handlers, (void*)&ud) != JSONSAX_OK)
   {
      cheevos_free_patchdata(patchdata);
      return -1;
   }

   return 0;
}

/*****************************************************************************
Frees the patchdata
*****************************************************************************/

void cheevos_free_patchdata(cheevos_rapatchdata_t* patchdata)
{
   unsigned i = 0, count = 0;
   const cheevos_racheevo_t* cheevo = NULL;
   const cheevos_ralboard_t* lboard = NULL;

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
      CHEEVOS_FREE(lboard->format);
      CHEEVOS_FREE(lboard->mem);
   }

   CHEEVOS_FREE(patchdata->core);
   CHEEVOS_FREE(patchdata->unofficial);
   CHEEVOS_FREE(patchdata->lboards);

   patchdata->console_id       = 0;
   patchdata->core             = NULL;
   patchdata->unofficial       = NULL;
   patchdata->lboards          = NULL;
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
   cheevos_unlock_cb_t unlock_cb;
   void* userdata;
} cheevos_deactivate_t;

static int cheevos_deactivate_index(void* userdata, unsigned int index)
{
   cheevos_deactivate_t* ud = (cheevos_deactivate_t*)userdata;

   ud->is_element           = 1;
   return 0;
}

static int cheevos_deactivate_number(void* userdata,
      const char* number, size_t length)
{
   cheevos_deactivate_t* ud = (cheevos_deactivate_t*)userdata;
   unsigned id              = 0;

   if (ud->is_element)
   {
      ud->is_element = 0;
      id             = (unsigned)strtol(number, NULL, 10);

      ud->unlock_cb(id, ud->userdata);
   }

   return 0;
}

void cheevos_deactivate_unlocks(const char* json, cheevos_unlock_cb_t unlock_cb, void* userdata)
{
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      cheevos_deactivate_index,
      NULL,
      cheevos_deactivate_number,
      NULL,
      NULL
   };

   cheevos_deactivate_t ud;

   ud.is_element = 0;
   ud.unlock_cb  = unlock_cb;
   ud.userdata   = userdata;

   jsonsax_parse(json, &handlers, (void*)&ud);
}

/*****************************************************************************
Returns the game ID
*****************************************************************************/

unsigned chevos_get_gameid(const char* json)
{
   char gameid[32];

   if (cheevos_get_value(json, CHEEVOS_JSON_KEY_GAMEID, gameid, sizeof(gameid)) != 0)
      return 0;

   return (unsigned)strtol(gameid, NULL, 10);
}
