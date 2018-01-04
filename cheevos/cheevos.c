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

#include <string.h>
#include <ctype.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <formats/jsonsax.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>
#include <compat/strl.h>
#include <rhash.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <net/net_http.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_entries.h"
#endif

#include "badges.h"
#include "cheevos.h"
#include "var.h"
#include "cond.h"

#include "../file_path_special.h"
#include "../command.h"
#include "../dynamic.h"
#include "../configuration.h"
#include "../performance_counters.h"
#include "../msg_hash.h"
#include "../retroarch.h"
#include "../core.h"

#include "../network/net_http_special.h"
#include "../tasks/tasks_internal.h"

#include "../verbosity.h"

/* Define this macro to prevent cheevos from being deactivated. */
#undef CHEEVOS_DONT_DEACTIVATE

/* Define this macro to log URLs (will log the user token). */
#undef CHEEVOS_LOG_URLS

/* Define this macro to dump all cheevos' addresses. */
#undef CHEEVOS_DUMP_ADDRS

/* Define this macro to remove HTTP timeouts. */
#undef CHEEVOS_NO_TIMEOUT

/* Define this macro to load a JSON file from disk instead of downloading
 * from retroachievements.org. */
#undef CHEEVOS_JSON_OVERRIDE

/* Define this macro with a string to save the JSON file to disk with
 * that name. */
#undef CHEEVOS_SAVE_JSON

/* Define this macro to have the password and token logged. THIS WILL DISCLOSE
 * THE USER'S PASSWORD, TAKE CARE! */
#undef CHEEVOS_LOG_PASSWORD

/* Define this macro to log downloaded badge images. */
#undef CHEEVOS_LOG_BADGES

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

#define CHEEVOS_SIX_MB     ( 6 * 1024 * 1024)
#define CHEEVOS_EIGHT_MB   ( 8 * 1024 * 1024)
#define CHEEVOS_SIZE_LIMIT (64 * 1024 * 1024)

typedef struct
{
   cheevos_cond_t *conds;
   unsigned        count;
} cheevos_condset_t;

typedef struct
{
   cheevos_condset_t *condsets;
   unsigned count;
} cheevos_condition_t;

typedef struct
{
   unsigned    id;
   const char *title;
   const char *description;
   const char *author;
   const char *badge;
   unsigned    points;
   unsigned    dirty;
   int         active;
   int         last;
   int         modified;

   cheevos_condition_t condition;
} cheevo_t;

typedef struct
{
   cheevo_t *cheevos;
   unsigned  count;
} cheevoset_t;

typedef struct
{
   int is_element;
   int mode;
} cheevos_deactivate_t;

typedef struct
{
   unsigned    key_hash;
   int         is_key;
   const char *value;
   size_t      length;
} cheevos_getvalueud_t;

typedef struct
{
   int      in_cheevos;
   int      in_lboards;
   uint32_t field_hash;
   unsigned core_count;
   unsigned unofficial_count;
   unsigned lboard_count;
} cheevos_countud_t;

typedef struct
{
   const char *string;
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

   cheevos_field_t *field;
   cheevos_field_t  id, memaddr, title, desc, points, author;
   cheevos_field_t  modified, created, badge, flags, format;
} cheevos_readud_t;

typedef struct
{
   int label;
   const char *name;
   const uint32_t *ext_hashes;
} cheevos_finder_t;

typedef struct
{
   cheevos_var_t var;
   double        multiplier;
   bool          compare_next;
} cheevos_term_t;

typedef struct
{
   cheevos_term_t *terms;
   unsigned        count;
   unsigned        compare_count;
} cheevos_expr_t;

typedef struct
{
   unsigned    id;
   unsigned    format;
   const char *title;
   const char *description;
   int         active;
   int         last_value;

   cheevos_condition_t start;
   cheevos_condition_t cancel;
   cheevos_condition_t submit;
   cheevos_expr_t      value;
} cheevos_leaderboard_t;

typedef struct
{
   cheevos_console_t console_id;
   bool core_supports;
   bool addrs_patched;
   int  add_buffer;
   int  add_hits;

   cheevoset_t core;
   cheevoset_t unofficial;
   cheevos_leaderboard_t *leaderboards;
   unsigned lboard_count;

   char token[32];

   retro_ctx_memory_info_t meminfo[4];
} cheevos_locals_t;

static cheevos_locals_t cheevos_locals =
{
   /* console_id          */ CHEEVOS_CONSOLE_NONE,
   /* core_supports       */ true,
   /* addrs_patched       */ false,
   /* add_buffer          */ 0,
   /* add_hits            */ 0,
   /* core                */ {NULL, 0},
   /* unofficial          */ {NULL, 0},
   /* leaderboards        */ NULL,
   /* lboard_count        */ 0,
   /* token               */ {0},
   {
   /* meminfo[0]          */ {NULL, 0, 0},
   /* meminfo[1]          */ {NULL, 0, 0},
   /* meminfo[2]          */ {NULL, 0, 0},
   /* meminfo[3]          */ {NULL, 0, 0}
   }
};

bool cheevos_loaded      = false;
int  cheats_are_enabled  = 0;
int  cheats_were_enabled = 0;

/*****************************************************************************
Supporting functions.
*****************************************************************************/

#ifdef CHEEVOS_LOG_URLS
static void cheevos_log_url(const char* format, const char* url)
{
#ifdef CHEEVOS_LOG_PASSWORD
   RARCH_LOG(format, url);
#else
   char copy[256];
   char* aux;
   char* next;

   strlcpy(copy, url, sizeof(copy));
   aux = strstr(copy, "?p=");

   if (aux == NULL)
      aux = strstr(copy, "&p=");

   if (aux != NULL)
   {
      aux += 3;
      next = strchr(aux, '&');

      if (next != NULL)
      {
         do
         {
            *aux++ = *next++;
         }
         while (next[-1] != 0);
      }
      else
         *aux = 0;
   }

   aux = strstr(copy, "?t=");

   if (aux == NULL)
      aux = strstr(copy, "&t=");

   if (aux != NULL)
   {
      aux += 3;
      next = strchr(aux, '&');

      if (next != NULL)
      {
         do
         {
            *aux++ = *next++;
         }
         while (next[-1] != 0);
      }
      else
         *aux = 0;
   }

   RARCH_LOG(format, copy);
#endif
}
#endif

#ifdef CHEEVOS_VERBOSE
static void cheevos_add_char(char** aux, size_t* left, char k)
{
   if (*left >= 1)
   {
      **aux = k;
      (*aux)++;
      (*left)--;
   }
}

static void cheevos_add_string(char** aux, size_t* left, const char* s)
{
   size_t len = strlen(s);

   if (*left >= len)
   {
      strcpy(*aux, s);
      *aux += len;
      *left -= len;
   }
}

static void cheevos_add_hex(char** aux, size_t* left, unsigned v)
{
   char buffer[32];

   snprintf(buffer, sizeof(buffer), "%06x", v);
   buffer[sizeof(buffer) - 1] = 0;

   cheevos_add_string(aux, left, buffer);
}

static void cheevos_add_uint(char** aux, size_t* left, unsigned v)
{
   char buffer[32];

   snprintf(buffer, sizeof(buffer), "%u", v);
   buffer[sizeof(buffer) - 1] = 0;

   cheevos_add_string(aux, left, buffer);
}

static void cheevos_add_int(char** aux, size_t* left, int v)
{
   char buffer[32];

   snprintf(buffer, sizeof(buffer), "%d", v);
   buffer[sizeof(buffer) - 1] = 0;

   cheevos_add_string(aux, left, buffer);
}

static void cheevos_log_var(const cheevos_var_t* var)
{
   RARCH_LOG("[CHEEVOS]: size: %s\n",
      var->size == CHEEVOS_VAR_SIZE_BIT_0 ? "bit 0" :
      var->size == CHEEVOS_VAR_SIZE_BIT_1 ? "bit 1" :
      var->size == CHEEVOS_VAR_SIZE_BIT_2 ? "bit 2" :
      var->size == CHEEVOS_VAR_SIZE_BIT_3 ? "bit 3" :
      var->size == CHEEVOS_VAR_SIZE_BIT_4 ? "bit 4" :
      var->size == CHEEVOS_VAR_SIZE_BIT_5 ? "bit 5" :
      var->size == CHEEVOS_VAR_SIZE_BIT_6 ? "bit 6" :
      var->size == CHEEVOS_VAR_SIZE_BIT_7 ? "bit 7" :
      var->size == CHEEVOS_VAR_SIZE_NIBBLE_LOWER ? "low nibble" :
      var->size == CHEEVOS_VAR_SIZE_NIBBLE_UPPER ? "high nibble" :
      var->size == CHEEVOS_VAR_SIZE_EIGHT_BITS ? "byte" :
      var->size == CHEEVOS_VAR_SIZE_SIXTEEN_BITS ? "word" :
      var->size == CHEEVOS_VAR_SIZE_THIRTYTWO_BITS ? "dword" :
      "?"
   );
   RARCH_LOG("[CHEEVOS]: type: %s\n",
      var->type == CHEEVOS_VAR_TYPE_ADDRESS ? "address" :
      var->type == CHEEVOS_VAR_TYPE_VALUE_COMP ? "value" :
      var->type == CHEEVOS_VAR_TYPE_DELTA_MEM ? "delta" :
      var->type == CHEEVOS_VAR_TYPE_DYNAMIC_VAR ? "dynamic" :
      "?"
   );
   RARCH_LOG("[CHEEVOS]: value: %u\n", var->value);
}

static void cheevos_log_cond(const cheevos_cond_t* cond)
{
   RARCH_LOG("[CHEEVOS]: condition %p\n", cond);
   RARCH_LOG("[CHEEVOS]: type:     %s\n",
      cond->type == CHEEVOS_COND_TYPE_STANDARD   ? "standard" :
      cond->type == CHEEVOS_COND_TYPE_PAUSE_IF   ? "pause" :
      cond->type == CHEEVOS_COND_TYPE_RESET_IF   ? "reset" :
      cond->type == CHEEVOS_COND_TYPE_ADD_SOURCE ? "add source" :
      cond->type == CHEEVOS_COND_TYPE_SUB_SOURCE ? "sub source" :
      cond->type == CHEEVOS_COND_TYPE_ADD_HITS   ? "add hits" :
      "?"
   );
   RARCH_LOG("[CHEEVOS]: req_hits: %u\n", cond->req_hits);
   RARCH_LOG("[CHEEVOS]: source:\n");
   cheevos_log_var(&cond->source);
   RARCH_LOG("[CHEEVOS]: op: %s\n",
      cond->op == CHEEVOS_COND_OP_EQUALS ? "==" :
      cond->op == CHEEVOS_COND_OP_LESS_THAN ? "<" :
      cond->op == CHEEVOS_COND_OP_LESS_THAN_OR_EQUAL ? "<=" :
      cond->op == CHEEVOS_COND_OP_GREATER_THAN ? ">" :
      cond->op == CHEEVOS_COND_OP_GREATER_THAN_OR_EQUAL ? ">=" :
      cond->op == CHEEVOS_COND_OP_NOT_EQUAL_TO ? "!=" :
      "?"
   );
   RARCH_LOG("[CHEEVOS]:  target:\n");
   cheevos_log_var(&cond->target);
}

static void cheevos_log_cheevo(const cheevo_t* cheevo,
      const cheevos_field_t* memaddr_ud)
{
   RARCH_LOG("[CHEEVOS]: cheevo %p\n", cheevo);
   RARCH_LOG("[CHEEVOS]:  id:      %u\n", cheevo->id);
   RARCH_LOG("[CHEEVOS]:  title:   %s\n", cheevo->title);
   RARCH_LOG("[CHEEVOS]:  desc:    %s\n", cheevo->description);
   RARCH_LOG("[CHEEVOS]:  author:  %s\n", cheevo->author);
   RARCH_LOG("[CHEEVOS]:  badge:   %s\n", cheevo->badge);
   RARCH_LOG("[CHEEVOS]:  points:  %u\n", cheevo->points);
   RARCH_LOG("[CHEEVOS]:  sets:    TBD\n");
   RARCH_LOG("[CHEEVOS]:  memaddr: %.*s\n", (int)memaddr_ud->length, memaddr_ud->string);
}

static void cheevos_add_var_size(char** aux, size_t* left,
      const cheevos_var_t* var)
{
   switch( var->size )
   {
      case CHEEVOS_VAR_SIZE_BIT_0:
         cheevos_add_char(aux, left, 'M');
         break;
      case CHEEVOS_VAR_SIZE_BIT_1:
         cheevos_add_char(aux, left, 'N');
         break;
      case CHEEVOS_VAR_SIZE_BIT_2:
         cheevos_add_char(aux, left, 'O');
         break;
      case CHEEVOS_VAR_SIZE_BIT_3:
         cheevos_add_char(aux, left, 'P');
         break;
      case CHEEVOS_VAR_SIZE_BIT_4:
         cheevos_add_char(aux, left, 'Q');
         break;
      case CHEEVOS_VAR_SIZE_BIT_5:
         cheevos_add_char(aux, left, 'R');
         break;
      case CHEEVOS_VAR_SIZE_BIT_6:
         cheevos_add_char(aux, left, 'S');
         break;
      case CHEEVOS_VAR_SIZE_BIT_7:
         cheevos_add_char(aux, left, 'T');
         break;
      case CHEEVOS_VAR_SIZE_NIBBLE_LOWER:
         cheevos_add_char(aux, left, 'L');
         break;
      case CHEEVOS_VAR_SIZE_NIBBLE_UPPER:
         cheevos_add_char(aux, left, 'U');
         break;
      case CHEEVOS_VAR_SIZE_EIGHT_BITS:
         cheevos_add_char(aux, left, 'H');
         break;
      case CHEEVOS_VAR_SIZE_THIRTYTWO_BITS:
         cheevos_add_char(aux, left, 'X');
         break;
      case CHEEVOS_VAR_SIZE_SIXTEEN_BITS:
      default:
         cheevos_add_char(aux, left, ' ');
         break;
   }
}

static void cheevos_add_var(const cheevos_var_t* var, char** memaddr,
      size_t *left)
{
   if (   var->type == CHEEVOS_VAR_TYPE_ADDRESS
       || var->type == CHEEVOS_VAR_TYPE_DELTA_MEM)
   {
      if (var->type == CHEEVOS_VAR_TYPE_DELTA_MEM)
         cheevos_add_char(memaddr, left, 'd');
      else if (var->is_bcd)
         cheevos_add_char(memaddr, left, 'b');

      cheevos_add_string(memaddr, left, "0x");
      cheevos_add_var_size(memaddr, left, var);
      cheevos_add_hex(memaddr, left, var->value);
   }
   else if (var->type == CHEEVOS_VAR_TYPE_VALUE_COMP)
   {
      cheevos_add_uint(memaddr, left, var->value);
   }
}

static void cheevos_build_memaddr(const cheevos_condition_t* condition,
      char* memaddr, size_t left)
{
   char *aux = memaddr;
   const cheevos_condset_t* condset;
   const cheevos_cond_t* cond;
   size_t i, j;

   left--; /* reserve one char for the null terminator */

   for (i = 0, condset = condition->condsets; i < condition->count; i++, condset++)
   {
      if (i != 0)
         cheevos_add_char(&aux, &left, 'S');

      for (j = 0, cond = condset->conds; j < condset->count; j++, cond++)
      {
         if (j != 0)
            cheevos_add_char(&aux, &left, '_');

         if (cond->type == CHEEVOS_COND_TYPE_RESET_IF)
            cheevos_add_string(&aux, &left, "R:");
         else if (cond->type == CHEEVOS_COND_TYPE_PAUSE_IF)
            cheevos_add_string(&aux, &left, "P:");
         else if (cond->type == CHEEVOS_COND_TYPE_ADD_SOURCE)
            cheevos_add_string(&aux, &left, "A:");
         else if (cond->type == CHEEVOS_COND_TYPE_SUB_SOURCE)
            cheevos_add_string(&aux, &left, "B:");
         else if (cond->type == CHEEVOS_COND_TYPE_ADD_HITS)
            cheevos_add_string(&aux, &left, "C:");

         cheevos_add_var(&cond->source, &aux, &left);

         switch (cond->op)
         {
            case CHEEVOS_COND_OP_EQUALS:
               cheevos_add_char(&aux, &left, '=');
               break;
      		case CHEEVOS_COND_OP_GREATER_THAN:
               cheevos_add_char(&aux, &left, '>');
               break;
      		case CHEEVOS_COND_OP_GREATER_THAN_OR_EQUAL:
               cheevos_add_string(&aux, &left, ">=");
               break;
      		case CHEEVOS_COND_OP_LESS_THAN:
               cheevos_add_char(&aux, &left, '<');
               break;
      		case CHEEVOS_COND_OP_LESS_THAN_OR_EQUAL:
               cheevos_add_string(&aux, &left, "<=");
               break;
      		case CHEEVOS_COND_OP_NOT_EQUAL_TO:
               cheevos_add_string(&aux, &left, "!=");
               break;
         }

         cheevos_add_var(&cond->target, &aux, &left);

         if (cond->req_hits > 0)
         {
            cheevos_add_char(&aux, &left, '.');
            cheevos_add_uint(&aux, &left, cond->req_hits);
            cheevos_add_char(&aux, &left, '.');
         }
      }
   }

   *aux = 0;
}

static void cheevos_post_log_cheevo(const cheevo_t* cheevo)
{
   char memaddr[256];
   cheevos_build_memaddr(&cheevo->condition, memaddr, sizeof(memaddr));
   RARCH_LOG("[CHEEVOS]: memaddr (computed): %s\n", memaddr);
}

static void cheevos_log_lboard(const cheevos_leaderboard_t* lb)
{
   char mem[256];
   char* aux;
   size_t left;
   unsigned i;

   RARCH_LOG("[CHEEVOS]: leaderboard %p\n", lb);
   RARCH_LOG("[CHEEVOS]:   id:      %u\n", lb->id);
   RARCH_LOG("[CHEEVOS]:   title:   %s\n", lb->title);
   RARCH_LOG("[CHEEVOS]:   desc:    %s\n", lb->description);

   cheevos_build_memaddr(&lb->start, mem, sizeof(mem));
   RARCH_LOG("[CHEEVOS]: start:  %s\n", mem);

   cheevos_build_memaddr(&lb->cancel, mem, sizeof(mem));
   RARCH_LOG("[CHEEVOS]: cancel: %s\n", mem);

   cheevos_build_memaddr(&lb->submit, mem, sizeof(mem));
   RARCH_LOG("[CHEEVOS]: submit: %s\n", mem);

   left = sizeof(mem);
   aux = mem;

   for (i = 0; i < lb->value.count; i++)
   {
      if (i != 0)
         cheevos_add_char(&aux, &left, '_');

      cheevos_add_var(&lb->value.terms[i].var, &aux, &left);
      cheevos_add_char(&aux, &left, '*');
      cheevos_add_int(&aux, &left, lb->value.terms[i].multiplier);
   }

   RARCH_LOG("[CHEEVOS]: value:  %s\n", mem);
}
#endif

static uint32_t cheevos_djb2(const char* str, size_t length)
{
   const unsigned char *aux = (const unsigned char*)str;
   const unsigned char *end = aux + length;
   uint32_t            hash = 5381;

   while (aux < end)
      hash = (hash << 5) + hash + *aux++;

   return hash;
}

static int cheevos_getvalue__json_key(void *userdata,
      const char *name, size_t length)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   ud->is_key = cheevos_djb2(name, length) == ud->key_hash;
   return 0;
}

static int cheevos_getvalue__json_string(void *userdata,
      const char *string, size_t length)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   if (ud->is_key)
   {
      ud->value = string;
      ud->length = length;
      ud->is_key = 0;
   }

   return 0;
}

static int cheevos_getvalue__json_boolean(void *userdata, int istrue)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   if ( ud->is_key )
   {
      ud->value  = istrue ? "true" : "false";
      ud->length = istrue ? 4 : 5;
      ud->is_key = 0;
   }

   return 0;
}

static int cheevos_getvalue__json_null(void *userdata)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   if ( ud->is_key )
   {
      ud->value = "null";
      ud->length = 4;
      ud->is_key = 0;
   }

   return 0;
}

static int cheevos_get_value(const char *json, unsigned key_hash,
      char *value, size_t length)
{
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      cheevos_getvalue__json_key,
      NULL,
      cheevos_getvalue__json_string,
      cheevos_getvalue__json_string, /* number */
      cheevos_getvalue__json_boolean,
      cheevos_getvalue__json_null
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
Count number of achievements in a JSON file.
*****************************************************************************/

static int cheevos_count__json_end_array(void *userdata)
{
  cheevos_countud_t* ud = (cheevos_countud_t*)userdata;
  ud->in_cheevos = 0;
  ud->in_lboards = 0;
  return 0;
}

static int cheevos_count__json_key(void *userdata,
      const char *name, size_t length)
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;
   ud->field_hash        = cheevos_djb2(name, length);

   if (ud->field_hash == CHEEVOS_JSON_KEY_ACHIEVEMENTS)
      ud->in_cheevos = 1;
   else if (ud->field_hash == CHEEVOS_JSON_KEY_LEADERBOARDS)
      ud->in_lboards = 1;

   return 0;
}

static int cheevos_count__json_number(void *userdata,
      const char *number, size_t length)
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;

   if (ud->in_cheevos && ud->field_hash == CHEEVOS_JSON_KEY_FLAGS)
   {
      long flags = strtol(number, NULL, 10);

      if (flags == 3)
         ud->core_count++; /* Core achievements */
      else if (flags == 5)
         ud->unofficial_count++; /* Unofficial achievements */
   }
   else if (ud->in_lboards && ud->field_hash == CHEEVOS_JSON_KEY_ID)
      ud->lboard_count++;

   return 0;
}

static int cheevos_count_cheevos(const char *json,
      unsigned *core_count, unsigned *unofficial_count,
      unsigned *lboard_count)
{
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      cheevos_count__json_end_array,
      cheevos_count__json_key,
      NULL,
      NULL,
      cheevos_count__json_number,
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
Parse the MemAddr field.
*****************************************************************************/

static unsigned cheevos_count_cond_sets(const char *memaddr)
{
   cheevos_cond_t cond;
   unsigned count = 0;

   for (;;)
   {
      count++;

      for (;;)
      {
         cheevos_cond_parse(&cond, &memaddr);

         if (*memaddr != '_')
            break;

         memaddr++;
      }

      if (*memaddr != 'S')
         break;

      memaddr++;
   }

   return count;
}

static int cheevos_parse_condition(cheevos_condition_t *condition, const char* memaddr)
{
   if (!condition)
      return 0;

   condition->count = cheevos_count_cond_sets(memaddr);

   if (condition->count)
   {
      unsigned set                 = 0;
      const cheevos_condset_t* end = NULL;
      cheevos_condset_t *conds     = NULL;
      cheevos_condset_t *condset   = NULL;
      cheevos_condset_t *condsets  = (cheevos_condset_t*)
         calloc(condition->count, sizeof(cheevos_condset_t));

      (void)conds;

      if (!condsets)
         return -1;

      condition->condsets = condsets;
      end                 = condition->condsets + condition->count;

      for (condset = condition->condsets; condset < end; condset++, set++)
      {
         condset->count =
            cheevos_cond_count_in_set(memaddr, set);
         condset->conds = NULL;

#ifdef CHEEVOS_VERBOSE
         RARCH_LOG("[CHEEVOS]: set %p (index=%u)\n", condset, set);
         RARCH_LOG("[CHEEVOS]:   conds: %u\n", condset->count);
#endif

         if (condset->count)
         {
            cheevos_cond_t *conds = (cheevos_cond_t*)
               calloc(condset->count, sizeof(cheevos_cond_t));

            if (!conds)
            {
               while (--condset >= condition->condsets)
               {
                  if ((void*)condset->conds)
                     free((void*)condset->conds);
               }

               return -1;
            }

            condset->conds = conds;
            cheevos_cond_parse_in_set(condset->conds, memaddr, set);
         }
      }
   }

   return 0;
}

static void cheevos_free_condition(cheevos_condition_t* condition)
{
   unsigned i;

   if (condition->condsets)
   {
      for (i = 0; i < condition->count; i++)
      {
         if (condition->condsets[i].conds)
         {
            free(condition->condsets[i].conds);
            condition->condsets[i].conds = NULL;
         }
      }

      if (condition->condsets)
      {
         free(condition->condsets);
         condition->condsets = NULL;
      }
   }
}

/*****************************************************************************
Parse the Mem field of leaderboards.
*****************************************************************************/

static int cheevos_parse_expression(cheevos_expr_t *expr, const char* mem)
{
   const char* aux;
   char* end;
   unsigned i;
   expr->count = 1;
   expr->compare_count = 1;

   for (aux = mem;; aux++)
   {
      if(*aux == '"' || *aux == ':')
         break;
      expr->count += *aux == '_';
   }

   expr->terms = (cheevos_term_t*)calloc(expr->count, sizeof(cheevos_term_t));

   if (!expr->terms)
      return -1;

   for (i = 0; i < expr->count; i++)
   {
      expr->terms[i].compare_next = false;
      expr->terms[i].multiplier = 1;
   }

   for (i = 0, aux = mem; i < expr->count;)
   {
      cheevos_var_parse(&expr->terms[i].var, &aux);

      if (*aux != '*')
      {
         /* expression has no multiplier */
         if (*aux == '_')
         {
            aux++;
            i++;
         }
         else if (*aux == '$')
         {
            expr->terms[i].compare_next = true;
            expr->compare_count++;
            aux++;
            i++;
         }

         /* no multiplier at end of string */
         else if (*aux == '\0' || *aux == '"' || *aux == ',')
            return 0;

         /* invalid character in expression */
         else
         {
            if (expr->terms)
            {
               free(expr->terms);
               expr->terms = NULL;
            }
            return -1;
         }
      }
      else
      {
         if(aux[1] == 'h' || aux[1] == 'H')
            expr->terms[i].multiplier = (double)strtol(aux + 2, &end, 16);
         else
            expr->terms[i].multiplier = strtod(aux + 1, &end);
         aux = end;

         if(*aux == '$')
         {
            aux++;
            expr->terms[i].compare_next = true;
            expr->compare_count++;
         }
         else
            expr->terms[i].compare_next = false;

         aux++;
         i++;
      }
   }
   return 0;
}

static int cheevos_parse_mem(cheevos_leaderboard_t *lb, const char* mem)
{
   lb->start.condsets  = NULL;
   lb->cancel.condsets = NULL;
   lb->submit.condsets = NULL;
   lb->value.terms     = NULL;

   for (;;)
   {
      if (*mem == 'S' && mem[1] == 'T' && mem[2] == 'A' && mem[3] == ':')
      {
         if (cheevos_parse_condition(&lb->start, mem + 4))
            goto error;
      }
      else if (*mem == 'C' && mem[1] == 'A' && mem[2] == 'N' && mem[3] == ':')
      {
         if (cheevos_parse_condition(&lb->cancel, mem + 4))
            goto error;
      }
      else if (*mem == 'S' && mem[1] == 'U' && mem[2] == 'B' && mem[3] == ':')
      {
         if (cheevos_parse_condition(&lb->submit, mem + 4))
            goto error;
      }
      else if (*mem == 'V' && mem[1] == 'A' && mem[2] == 'L' && mem[3] == ':')
      {
         if (cheevos_parse_expression(&lb->value, mem + 4))
            goto error;
      }

      for (mem += 4;; mem++)
      {
         if (*mem == ':' && mem[1] == ':')
         {
            mem += 2;
            break;
         }
         else if (*mem == '"')
            return 0;
      }
   }

error:
   cheevos_free_condition(&lb->start);
   cheevos_free_condition(&lb->cancel);
   cheevos_free_condition(&lb->submit);
   if (lb->value.terms)
   {
      free((void*)lb->value.terms);
      lb->value.terms = NULL;
   }
   return -1;
}

/*****************************************************************************
Load achievements from a JSON string.
*****************************************************************************/

static INLINE const char *cheevos_dupstr(const cheevos_field_t *field)
{
   char *string = (char*)malloc(field->length + 1);

   if (!string)
      return NULL;

   memcpy ((void*)string, (void*)field->string, field->length);
   string[field->length] = 0;

   return string;
}

static int cheevos_new_cheevo(cheevos_readud_t *ud)
{
   cheevo_t *cheevo = NULL;
   long flags = strtol(ud->flags.string, NULL, 10);

   if (flags == 3)
      cheevo = cheevos_locals.core.cheevos + ud->core_count++;
   else if (flags == 5)
      cheevo = cheevos_locals.unofficial.cheevos + ud->unofficial_count++;
   else
      return 0;

   cheevo->id          = (unsigned)strtol(ud->id.string, NULL, 10);
   cheevo->title       = cheevos_dupstr(&ud->title);
   cheevo->description = cheevos_dupstr(&ud->desc);
   cheevo->author      = cheevos_dupstr(&ud->author);
   cheevo->badge       = cheevos_dupstr(&ud->badge);
   cheevo->points      = (unsigned)strtol(ud->points.string, NULL, 10);
   cheevo->dirty       = 0;
   cheevo->active      = CHEEVOS_ACTIVE_SOFTCORE | CHEEVOS_ACTIVE_HARDCORE;
   cheevo->last        = 1;
   cheevo->modified    = 0;

   if (!cheevo->title || !cheevo->description || !cheevo->author || !cheevo->badge)
      goto error;

#ifdef CHEEVOS_VERBOSE
   cheevos_log_cheevo(cheevo, &ud->memaddr);
#endif

   if (cheevos_parse_condition(&cheevo->condition, ud->memaddr.string))
      goto error;

#ifdef CHEEVOS_VERBOSE
   cheevos_post_log_cheevo(cheevo);
#endif

   return 0;

error:
   if (cheevo->title)
   {
      free((void*)cheevo->title);
      cheevo->title = NULL;
   }
   if (cheevo->description)
   {
      free((void*)cheevo->description);
      cheevo->description = NULL;
   }
   if (cheevo->author)
   {
      free((void*)cheevo->author);
      cheevo->author = NULL;
   }
   if (cheevo->badge)
   {
      free((void*)cheevo->badge);
      cheevo->badge = NULL;
   }
   return -1;
}

/*****************************************************************************
Helper functions for displaying leaderboard values.
*****************************************************************************/

static void cheevos_format_value(const unsigned value, const unsigned type,
   char* formatted_value, size_t formatted_size)
{
   unsigned mins, secs, millis;

   switch(type)
   {
      case CHEEVOS_FORMAT_VALUE:
         snprintf(formatted_value, formatted_size, "%u", value);
         break;

      case CHEEVOS_FORMAT_SCORE:
         snprintf(formatted_value, formatted_size, "%06upts", value);
         break;

      case CHEEVOS_FORMAT_FRAMES:
         mins   = value / 3600;
         secs   = (value % 3600) / 60;
         millis = (int) (value % 60) * (10.00 / 6.00);
         snprintf(formatted_value, formatted_size, "%02u:%02u.%02u", mins, secs, millis);
         break;

      case CHEEVOS_FORMAT_MILLIS:
         mins   = value / 6000;
         secs   = (value % 6000) / 100;
         millis = (int) (value % 100);
         snprintf(formatted_value, formatted_size, "%02u:%02u.%02u", mins, secs, millis);
         break;

      case CHEEVOS_FORMAT_SECS:
         mins   = value / 60;
         secs   = value % 60;
         snprintf(formatted_value, formatted_size, "%02u:%02u", mins, secs);
         break;

      default:
         snprintf(formatted_value, formatted_size, "%u (?)", value);
   }
}

unsigned cheevos_parse_format(cheevos_field_t* format)
{
   /* Most likely */
   if (strncmp(format->string, "VALUE", format->length) == 0)
      return CHEEVOS_FORMAT_VALUE;
   else if (strncmp(format->string, "TIME", format->length) == 0)
      return CHEEVOS_FORMAT_FRAMES;
   else if (strncmp(format->string, "SCORE", format->length) == 0)
      return CHEEVOS_FORMAT_SCORE;

   /* Less likely */
   else if (strncmp(format->string, "MILLISECS", format->length) == 0)
      return CHEEVOS_FORMAT_MILLIS;
   else if (strncmp(format->string, "TIMESECS", format->length) == 0)
      return CHEEVOS_FORMAT_SECS;

   /* Rare (RPS only) */
   else if (strncmp(format->string, "POINTS", format->length) == 0)
      return CHEEVOS_FORMAT_SCORE;
   else if (strncmp(format->string, "FRAMES", format->length) == 0)
      return CHEEVOS_FORMAT_FRAMES;
   else
      return CHEEVOS_FORMAT_OTHER;
}

static int cheevos_new_lboard(cheevos_readud_t *ud)
{
   cheevos_leaderboard_t *lboard = cheevos_locals.leaderboards + ud->lboard_count++;

   lboard->id          = strtol(ud->id.string, NULL, 10);
   lboard->format      = cheevos_parse_format(&ud->format);
   lboard->title       = cheevos_dupstr(&ud->title);
   lboard->description = cheevos_dupstr(&ud->desc);

   if (!lboard->title || !lboard->description)
      goto error;

   if (cheevos_parse_mem(lboard, ud->memaddr.string))
      goto error;

#ifdef CHEEVOS_VERBOSE
   cheevos_log_lboard(lboard);
#endif

   return 0;

error:
   if ((void*)lboard->title)
      free((void*)lboard->title);
   if ((void*)lboard->description)
      free((void*)lboard->description);
   return -1;
}

static int cheevos_read__json_key( void *userdata,
      const char *name, size_t length)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;
   uint32_t        hash = cheevos_djb2(name, length);
   int           common = ud->in_cheevos || ud->in_lboards;

   ud->field = NULL;

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

static int cheevos_read__json_string(void *userdata,
      const char *string, size_t length)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;

   if (ud->field)
   {
      ud->field->string = string;
      ud->field->length = length;
   }

   return 0;
}

static int cheevos_read__json_number(void *userdata,
      const char *number, size_t length)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;

   if (ud->field)
   {
      ud->field->string = number;
      ud->field->length = length;
   }
   else if (ud->is_console_id)
   {
      cheevos_locals.console_id = (cheevos_console_t)strtol(number, NULL, 10);
      ud->is_console_id = 0;
   }

   return 0;
}

static int cheevos_read__json_end_object(void *userdata)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;

   if (ud->in_cheevos)
      return cheevos_new_cheevo(ud);
   else if (ud->in_lboards)
      return cheevos_new_lboard(ud);

   return 0;
}

static int cheevos_read__json_end_array(void *userdata)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;
   ud->in_cheevos = 0;
   ud->in_lboards = 0;
   return 0;
}

static int cheevos_parse(const char *json)
{
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      cheevos_read__json_end_object,
      NULL,
      cheevos_read__json_end_array,
      cheevos_read__json_key,
      NULL,
      cheevos_read__json_string,
      cheevos_read__json_number,
      NULL,
      NULL
   };

   cheevos_readud_t ud;
   unsigned core_count, unofficial_count, lboard_count;
   /* Count the number of achievements in the JSON file. */
   int res = cheevos_count_cheevos(json, &core_count, &unofficial_count,
      &lboard_count);

   if (res != JSONSAX_OK)
      return -1;

   /* Allocate the achievements. */

   cheevos_locals.core.cheevos = (cheevo_t*)
      calloc(core_count, sizeof(cheevo_t));
   cheevos_locals.core.count = core_count;

   cheevos_locals.unofficial.cheevos = (cheevo_t*)
      calloc(unofficial_count, sizeof(cheevo_t));
   cheevos_locals.unofficial.count = unofficial_count;

   cheevos_locals.leaderboards = (cheevos_leaderboard_t*)
      calloc(lboard_count, sizeof(cheevos_leaderboard_t));
   cheevos_locals.lboard_count = lboard_count;

   if (   !cheevos_locals.core.cheevos || !cheevos_locals.unofficial.cheevos
       || !cheevos_locals.leaderboards)
   {
      if ((void*)cheevos_locals.core.cheevos)
         free((void*)cheevos_locals.core.cheevos);
      if ((void*)cheevos_locals.unofficial.cheevos)
         free((void*)cheevos_locals.unofficial.cheevos);
      if ((void*)cheevos_locals.leaderboards)
         free((void*)cheevos_locals.leaderboards);
      cheevos_locals.core.count = cheevos_locals.unofficial.count =
         cheevos_locals.lboard_count = 0;

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

   if (jsonsax_parse(json, &handlers, (void*)&ud) != JSONSAX_OK)
      goto error;

   return 0;

error:
   cheevos_unload();

   return -1;
}

/*****************************************************************************
Test all the achievements (call once per frame).
*****************************************************************************/

static int cheevos_test_condition(cheevos_cond_t *cond)
{
   unsigned sval = cheevos_var_get_value(&cond->source) + cheevos_locals.add_buffer;
   unsigned tval = cheevos_var_get_value(&cond->target);

   switch (cond->op)
   {
      case CHEEVOS_COND_OP_EQUALS:
         return sval == tval;
      case CHEEVOS_COND_OP_LESS_THAN:
         return sval < tval;
      case CHEEVOS_COND_OP_LESS_THAN_OR_EQUAL:
         return sval <= tval;
      case CHEEVOS_COND_OP_GREATER_THAN:
         return sval > tval;
      case CHEEVOS_COND_OP_GREATER_THAN_OR_EQUAL:
         return sval >= tval;
      case CHEEVOS_COND_OP_NOT_EQUAL_TO:
         return sval != tval;
      default:
         return 1;
   }
}

static int cheevos_test_cond_set(const cheevos_condset_t *condset,
      int *dirty_conds, int *reset_conds, int match_any)
{
   int cond_valid            = 0;
   int set_valid             = 1;
   const cheevos_cond_t *end = condset->conds + condset->count;
   cheevos_cond_t *cond      = NULL;

   cheevos_locals.add_buffer = 0;
   cheevos_locals.add_hits   = 0;

   /* Now, read all Pause conditions, and if any are true,
    * do not process further (retain old state). */

   for (cond = condset->conds; cond < end; cond++)
   {
      if (cond->type != CHEEVOS_COND_TYPE_PAUSE_IF)
         continue;

      /* Reset by default, set to 1 if hit! */
      cond->curr_hits = 0;

      if (cheevos_test_condition(cond))
      {
         cond->curr_hits = 1;
         *dirty_conds = 1;

         /* Early out: this achievement is paused,
          * do not process any further! */
         return 0;
      }
   }

   /* Read all standard conditions, and process as normal: */
   for (cond = condset->conds; cond < end; cond++)
   {
      if (cond->type == CHEEVOS_COND_TYPE_PAUSE_IF || cond->type == CHEEVOS_COND_TYPE_RESET_IF)
         continue;

      if (cond->type == CHEEVOS_COND_TYPE_ADD_SOURCE)
      {
         cheevos_locals.add_buffer += cheevos_var_get_value(&cond->source);
         set_valid &= 1;
         continue;
      }

      if (cond->type == CHEEVOS_COND_TYPE_SUB_SOURCE)
      {
         cheevos_locals.add_buffer -= cheevos_var_get_value(&cond->source);
         set_valid &= 1;
         continue;
      }

      if (cond->type == CHEEVOS_COND_TYPE_ADD_HITS)
      {
         if (cheevos_test_condition(cond))
         {
            cond->curr_hits++;
            *dirty_conds = 1;
         }

         cheevos_locals.add_hits += cond->curr_hits;
         continue;
      }

      if (cond->req_hits != 0 && (cond->curr_hits + cheevos_locals.add_hits) >= cond->req_hits)
         continue;

      cond_valid = cheevos_test_condition(cond);

      if (cond_valid)
      {
         cond->curr_hits++;
         *dirty_conds = 1;

         /* Process this logic, if this condition is true: */
         if (cond->req_hits == 0)
            ; /* Not a hit-based requirement: ignore any additional logic! */
         else if ((cond->curr_hits + cheevos_locals.add_hits) < cond->req_hits)
            cond_valid = 0; /* Not entirely valid yet! */

         if (match_any)
            break;
      }

      cheevos_locals.add_buffer = 0;
      cheevos_locals.add_hits   = 0;

      /* Sequential or non-sequential? */
      set_valid &= cond_valid;
   }

   /* Now, ONLY read reset conditions! */
   for (cond = condset->conds; cond < end; cond++)
   {
      if (cond->type != CHEEVOS_COND_TYPE_RESET_IF)
         continue;

      cond_valid = cheevos_test_condition(cond);

      if (cond_valid)
      {
         *reset_conds = 1; /* Resets all hits found so far */
         set_valid = 0;    /* Cannot be valid if we've hit a reset condition. */
         break;            /* No point processing any further reset conditions. */
      }
   }

   return set_valid;
}

static int cheevos_reset_cond_set(cheevos_condset_t *condset, int deltas)
{
   int dirty                 = 0;
   const cheevos_cond_t *end = condset->conds + condset->count;
   cheevos_cond_t *cond      = NULL;

   if (deltas)
   {
      for (cond = condset->conds; cond < end; cond++)
      {
         dirty |= cond->curr_hits != 0;
         cond->curr_hits = 0;

         cond->source.previous = cond->source.value;
         cond->target.previous = cond->target.value;
      }
   }
   else
   {
      for (cond = condset->conds; cond < end; cond++)
      {
         dirty |= cond->curr_hits != 0;
         cond->curr_hits = 0;
      }
   }

   return dirty;
}

static int cheevos_test_cheevo(cheevo_t *cheevo)
{
   int dirty;
   int dirty_conds              = 0;
   int reset_conds              = 0;
   int ret_val                  = 0;
   int ret_val_sub_cond         = cheevo->condition.count == 1;
   cheevos_condset_t *condset   = cheevo->condition.condsets;
   const cheevos_condset_t *end = condset + cheevo->condition.count;

   if (condset < end)
   {
      ret_val = cheevos_test_cond_set(condset, &dirty_conds, &reset_conds, 0);
      condset++;
   }

   while (condset < end)
   {
      ret_val_sub_cond |= cheevos_test_cond_set(condset, &dirty_conds, &reset_conds, 0);
      condset++;
   }

   if (dirty_conds)
      cheevo->dirty |= CHEEVOS_DIRTY_CONDITIONS;

   if (reset_conds)
   {
      dirty = 0;

      for (condset = cheevo->condition.condsets; condset < end; condset++)
         dirty |= cheevos_reset_cond_set(condset, 0);

      if (dirty)
         cheevo->dirty |= CHEEVOS_DIRTY_CONDITIONS;
   }

   return ret_val && ret_val_sub_cond;
}

static void cheevos_url_encode(const char *str, char *encoded, size_t len)
{
   while (*str)
   {
      if (     isalnum((unsigned char)*str) || *str == '-'
            || *str == '_' || *str == '.'
            || *str == '~')
      {
         if (len >= 2)
         {
            *encoded++ = *str++;
            len--;
         }
         else
            break;
      }
      else
      {
         if (len >= 4)
         {
            snprintf(encoded, len, "%%%02x", (uint8_t)*str);
            encoded += 3;
            str++;
            len -= 3;
         }
         else
            break;
      }
   }

   *encoded = 0;
}

static void cheevos_make_unlock_url(const cheevo_t *cheevo, char* url, size_t url_size)
{
   settings_t *settings = config_get_ptr();

   snprintf(
      url, url_size,
      "http://retroachievements.org/dorequest.php?r=awardachievement&u=%s&t=%s&a=%u&h=%d",
      settings->arrays.cheevos_username,
      cheevos_locals.token,
      cheevo->id,
      settings->bools.cheevos_hardcore_mode_enable ? 1 : 0
   );

   url[url_size - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
   cheevos_log_url("[CHEEVOS]: url to award the cheevo: %s\n", url);
#endif
}

static void cheevos_unlocked(void *task_data, void *user_data, const char *error)
{
   cheevo_t *cheevo = (cheevo_t *)user_data;

   if (error == NULL)
   {
      RARCH_LOG("[CHEEVOS]: awarded achievement %u.\n", cheevo->id);
   }
   else
   {
      char url[256];
      url[0] = '\0';

      RARCH_ERR("[CHEEVOS]: error awarding achievement %u, retrying...\n", cheevo->id);

      cheevos_make_unlock_url(cheevo, url, sizeof(url));
      task_push_http_transfer(url, true, NULL, cheevos_unlocked, cheevo);
   }
}

static void cheevos_test_cheevo_set(const cheevoset_t *set)
{
   settings_t *settings = config_get_ptr();
   cheevo_t *cheevo    = NULL;
   const cheevo_t *end = set->cheevos + set->count;
   int mode, valid;

   if (settings->bools.cheevos_hardcore_mode_enable)
      mode = CHEEVOS_ACTIVE_HARDCORE;
   else
      mode = CHEEVOS_ACTIVE_SOFTCORE;

   for (cheevo = set->cheevos; cheevo < end; cheevo++)
   {
      if (cheevo->active & mode)
      {
         valid = cheevos_test_cheevo(cheevo);

         if (cheevo->last)
         {
            cheevos_condset_t* condset   = cheevo->condition.condsets;
            const cheevos_condset_t* end = cheevo->condition.condsets + cheevo->condition.count;

            for (; condset < end; condset++)
               cheevos_reset_cond_set(condset, 0);
         }
         else if (valid)
         {
            char msg[256];
            char url[256];
            url[0] = '\0';

            cheevo->active &= ~mode;

            if (mode == CHEEVOS_ACTIVE_HARDCORE)
               cheevo->active &= ~CHEEVOS_ACTIVE_SOFTCORE;

            RARCH_LOG("[CHEEVOS]: awarding cheevo %u: %s (%s).\n",
                  cheevo->id, cheevo->title, cheevo->description);

            snprintf(msg, sizeof(msg), "Achievement Unlocked: %s", cheevo->title);
            msg[sizeof(msg) - 1] = 0;
            runloop_msg_queue_push(msg, 0, 2 * 60, false);
            runloop_msg_queue_push(cheevo->description, 0, 3 * 60, false);

            cheevos_make_unlock_url(cheevo, url, sizeof(url));
            task_push_http_transfer(url, true, NULL, cheevos_unlocked, cheevo);
         }

         cheevo->last = valid;
      }
   }
}

static int cheevos_test_lboard_condition(const cheevos_condition_t* condition)
{
   int dirty_conds              = 0;
   int reset_conds              = 0;
   int ret_val                  = 0;
   int ret_val_sub_cond         = condition->count == 1;
   cheevos_condset_t *condset   = condition->condsets;
   const cheevos_condset_t *end = condset + condition->count;

   if (condset < end)
   {
      ret_val = cheevos_test_cond_set(condset, &dirty_conds, &reset_conds, 0);
      condset++;
   }

   while (condset < end)
   {
      ret_val_sub_cond |= cheevos_test_cond_set(condset, &dirty_conds, &reset_conds, 0);
      condset++;
   }

   if (reset_conds)
   {
      for (condset = condition->condsets; condset < end; condset++)
         cheevos_reset_cond_set(condset, 0);
   }

   return ret_val && ret_val_sub_cond;
}

static int cheevos_expr_value(cheevos_expr_t* expr)
{
   cheevos_term_t* term = expr->terms;
   unsigned i;
   /* Separate possible values with '$' operator, submit the largest */
   unsigned current_value = 0;
   int values[16];

   if (expr->compare_count >= sizeof(values) / sizeof(values[0]))
   {
      RARCH_ERR("[CHEEVOS]: too many values in the leaderboard expression: %u\n", expr->compare_count);
      return 0;
   }

   memset(values, 0, sizeof values);

   for (i = expr->count; i != 0; i--, term++)
   {
      if (current_value >= sizeof(values) / sizeof(values[0]))
      {
         RARCH_ERR("[CHEEVOS]: too many values in the leaderboard expression: %u\n", current_value);
         return 0;
      }

      values[current_value] += cheevos_var_get_value(&term->var) * term->multiplier;

      if (term->compare_next)
         current_value++;
   }

   if (expr->compare_count > 1)
   {
      unsigned j;
      int maximum = values[0];

      for (j = 1; j < expr->compare_count; j++)
         maximum = values[j] > maximum ? values[j] : maximum;

      return maximum;
   }
   else
      return values[0];
}

static void cheevos_make_lboard_url(const cheevos_leaderboard_t *lboard,
      char* url, size_t url_size)
{
   settings_t *settings = config_get_ptr();
   char signature[64];
   MD5_CTX ctx;
   uint8_t hash[16];

   hash[0] = '\0';

   snprintf(signature, sizeof(signature), "%u%s%u", lboard->id,
      settings->arrays.cheevos_username,
      lboard->id);

   MD5_Init(&ctx);
   MD5_Update(&ctx, (void*)signature, strlen(signature));
   MD5_Final(hash, &ctx);

   snprintf(
      url, url_size,
      "http://retroachievements.org/dorequest.php?r=submitlbentry&u=%s&t=%s&i=%u&s=%d"
      "&v=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
      settings->arrays.cheevos_username,
      cheevos_locals.token,
      lboard->id,
      lboard->last_value,
      hash[ 0], hash[ 1], hash[ 2], hash[ 3],
      hash[ 4], hash[ 5], hash[ 6], hash[ 7],
      hash[ 8], hash[ 9], hash[10], hash[11],
      hash[12], hash[13], hash[14], hash[15]
   );

   url[url_size - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
   cheevos_log_url("[CHEEVOS]: url to submit the leaderboard: %s\n", url);
#endif
}

static void cheevos_lboard_submit(void *task_data, void *user_data, const char *error)
{
   cheevos_leaderboard_t *lboard = (cheevos_leaderboard_t *)user_data;

   if (error == NULL)
   {
      RARCH_LOG("[CHEEVOS]: submitted leaderboard %u.\n", lboard->id);
   }
   else
      RARCH_ERR("[CHEEVOS]: error submitting leaderboard %u\n", lboard->id);
#if 0
   {
      char url[256];
      url[0] = '\0';

      RARCH_ERR("[CHEEVOS]: error submitting leaderboard %u, retrying...\n", lboard->id);

      cheevos_make_lboard_url(lboard, url, sizeof(url));
      task_push_http_transfer(url, true, NULL, cheevos_lboard_submit, lboard);
   }
#endif
}

static void cheevos_test_leaderboards(void)
{
   cheevos_leaderboard_t* lboard = cheevos_locals.leaderboards;
   unsigned i;

   for (i = cheevos_locals.lboard_count; i != 0; i--, lboard++)
   {
      if (lboard->active)
      {
         int value = cheevos_expr_value(&lboard->value);

         if (value != lboard->last_value)
         {
#ifdef CHEEVOS_VERBOSE
            RARCH_LOG("[CHEEVOS]: value lboard  %s %u\n", lboard->title, value);
#endif
            lboard->last_value = value;
         }

         if (cheevos_test_lboard_condition(&lboard->submit))
         {
            lboard->active = 0;

            /* failsafe for improper LBs */
            if (value == 0)
            {
               RARCH_LOG("[CHEEVOS]: error: lboard %s tried to submit 0\n", lboard->title);
               runloop_msg_queue_push("Leaderboard attempt cancelled!", 0, 2 * 60, false);
            }
            else
            {
               char url[256];
               char msg[256];
               char formatted_value[16];

               cheevos_make_lboard_url(lboard, url, sizeof(url));
               task_push_http_transfer(url, true, NULL, cheevos_lboard_submit, lboard);
               RARCH_LOG("[CHEEVOS]: submit lboard %s\n", lboard->title);

               cheevos_format_value(value, lboard->format, formatted_value, sizeof(formatted_value));
               snprintf(msg, sizeof(msg), "Submitted %s for %s", formatted_value, lboard->title);
               msg[sizeof(msg) - 1] = 0;
               runloop_msg_queue_push(msg, 0, 2 * 60, false);
            }
         }

         if (cheevos_test_lboard_condition(&lboard->cancel))
         {
            RARCH_LOG("[CHEEVOS]: cancel lboard %s\n", lboard->title);
            lboard->active = 0;
            runloop_msg_queue_push("Leaderboard attempt cancelled!", 0, 2 * 60, false);
         }
      }
      else
      {
         if (cheevos_test_lboard_condition(&lboard->start))
         {
            char msg[256];

            RARCH_LOG("[CHEEVOS]: start lboard  %s\n", lboard->title);
            lboard->active = 1;
            lboard->last_value = -1;

            snprintf(msg, sizeof(msg), "Leaderboard Active: %s", lboard->title);
            msg[sizeof(msg) - 1] = 0;
            runloop_msg_queue_push(msg, 0, 2 * 60, false);
            runloop_msg_queue_push(lboard->description, 0, 3*60, false);
         }
      }
   }
}

/*****************************************************************************
Free the loaded achievements.
*****************************************************************************/

static void cheevos_free_condset(const cheevos_condset_t *set)
{
   if (set->conds)
      free((void*)set->conds);
}

static void cheevos_free_cheevo(const cheevo_t *cheevo)
{
   if (cheevo->title)
      free((void*)cheevo->title);
   if (cheevo->description)
      free((void*)cheevo->description);
   if (cheevo->author)
      free((void*)cheevo->author);
   if (cheevo->badge)
      free((void*)cheevo->badge);
   cheevos_free_condset(cheevo->condition.condsets);
}

static void cheevos_free_cheevo_set(const cheevoset_t *set)
{
   const cheevo_t *cheevo = set->cheevos;
   const cheevo_t *end = cheevo + set->count;

   while (cheevo < end)
      cheevos_free_cheevo(cheevo++);

   if (set->cheevos)
      free((void*)set->cheevos);
}

#ifndef CHEEVOS_DONT_DEACTIVATE
static int cheevos_deactivate__json_index(void *userdata, unsigned int index)
{
   cheevos_deactivate_t *ud = (cheevos_deactivate_t*)userdata;
   ud->is_element = 1;
   return 0;
}

static int cheevos_deactivate__json_number(void *userdata,
      const char *number, size_t length)
{
   long id;
   int found;
   cheevo_t* cheevo         = NULL;
   const cheevo_t* end      = NULL;
   cheevos_deactivate_t *ud = (cheevos_deactivate_t*)userdata;

   if (ud->is_element)
   {
      ud->is_element = 0;
      id             = strtol(number, NULL, 10);
      found          = 0;
      cheevo         = cheevos_locals.core.cheevos;
      end            = cheevo + cheevos_locals.core.count;

      for (; cheevo < end; cheevo++)
      {
         if (cheevo->id == (unsigned)id)
         {
            cheevo->active &= ~ud->mode;
            found = 1;
            break;
         }
      }

      if (!found)
      {
         cheevo = cheevos_locals.unofficial.cheevos;
         end    = cheevo + cheevos_locals.unofficial.count;

         for (; cheevo < end; cheevo++)
         {
            if (cheevo->id == (unsigned)id)
            {
               cheevo->active &= ~ud->mode;
               found = 1;
               break;
            }
         }
      }

      if (found)
         RARCH_LOG("[CHEEVOS]: deactivated unlocked cheevo %u (%s).\n", cheevo->id, cheevo->title);
      else
         RARCH_ERR("[CHEEVOS]: unknown cheevo to deactivate: %u.\n", id);
   }

   return 0;
}

static int cheevos_deactivate_unlocks(const char* json, unsigned mode)
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
      cheevos_deactivate__json_index,
      NULL,
      cheevos_deactivate__json_number,
      NULL,
      NULL
   };

   cheevos_deactivate_t ud;

   ud.is_element = 0;
   ud.mode = mode;
   return jsonsax_parse(json, &handlers, (void*)&ud) != JSONSAX_OK;
}
#endif

void cheevos_reset_game(void)
{
   cheevo_t *cheevo    = cheevos_locals.core.cheevos;
   const cheevo_t *end = cheevo + cheevos_locals.core.count;

   for (; cheevo < end; cheevo++)
      cheevo->last = 1;

   cheevo = cheevos_locals.unofficial.cheevos;
   end    = cheevo + cheevos_locals.unofficial.count;

   for (; cheevo < end; cheevo++)
      cheevo->last = 1;
}

void cheevos_populate_menu(void *data)
{
#ifdef HAVE_MENU
   unsigned i;
   unsigned items_found          = 0;
   settings_t *settings          = config_get_ptr();
   menu_displaylist_info_t *info = (menu_displaylist_info_t*)data;
   cheevo_t *cheevo              = cheevos_locals.core.cheevos;
   const cheevo_t *end           = cheevos_locals.core.cheevos +
                                   cheevos_locals.core.count;

   for (i = 0; cheevo < end; i++, cheevo++)
   {

      if (!(cheevo->active & CHEEVOS_ACTIVE_HARDCORE))
      {
         menu_entries_append_enum(info->list, cheevo->title,
            cheevo->description, MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
            MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
         items_found++;
         set_badge_info(&badges_ctx, i, cheevo->badge, (cheevo->active & CHEEVOS_ACTIVE_HARDCORE));
      }
      else if (!(cheevo->active & CHEEVOS_ACTIVE_SOFTCORE))
      {
         menu_entries_append_enum(info->list, cheevo->title,
            cheevo->description, MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY,
            MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
         items_found++;
         set_badge_info(&badges_ctx, i, cheevo->badge, (cheevo->active & CHEEVOS_ACTIVE_SOFTCORE));
      }
      else
      {
         menu_entries_append_enum(info->list, cheevo->title,
            cheevo->description, MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
            MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
         items_found++;
         set_badge_info(&badges_ctx, i, cheevo->badge, (cheevo->active & CHEEVOS_ACTIVE_SOFTCORE));
      }
   }

   if (settings->bools.cheevos_test_unofficial)
   {
      cheevo = cheevos_locals.unofficial.cheevos;
      end    = cheevos_locals.unofficial.cheevos
         + cheevos_locals.unofficial.count;

      for (i = cheevos_locals.core.count; cheevo < end; i++, cheevo++)
      {
         if (!(cheevo->active & CHEEVOS_ACTIVE_HARDCORE))
         {
            menu_entries_append_enum(info->list, cheevo->title,
               cheevo->description, MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
               MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
            items_found++;
            set_badge_info(&badges_ctx, i, cheevo->badge, (cheevo->active & CHEEVOS_ACTIVE_HARDCORE));
         }
         else if (!(cheevo->active & CHEEVOS_ACTIVE_SOFTCORE))
         {
            menu_entries_append_enum(info->list, cheevo->title,
               cheevo->description, MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY,
               MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
            items_found++;
            set_badge_info(&badges_ctx, i, cheevo->badge, (cheevo->active & CHEEVOS_ACTIVE_SOFTCORE));
         }
         else
         {
            menu_entries_append_enum(info->list, cheevo->title,
               cheevo->description, MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
               MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
            items_found++;
            set_badge_info(&badges_ctx, i, cheevo->badge, (cheevo->active & CHEEVOS_ACTIVE_SOFTCORE));
         }
      }
   }

   if (items_found == 0)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0);
   }
#endif
}

bool cheevos_get_description(cheevos_ctx_desc_t *desc)
{
   if (cheevos_loaded)
   {
      cheevo_t *cheevos = cheevos_locals.core.cheevos;

      if (desc->idx >= cheevos_locals.core.count)
      {
         cheevos    = cheevos_locals.unofficial.cheevos;
         desc->idx -= cheevos_locals.unofficial.count;
      }

      strlcpy(desc->s, cheevos[desc->idx].description, desc->len);
   }
   else
      *desc->s = 0;

   return true;
}

bool cheevos_apply_cheats(bool *data_bool)
{
   cheats_are_enabled   = *data_bool;
   cheats_were_enabled |= cheats_are_enabled;

   return true;
}

bool cheevos_unload(void)
{
   if (!cheevos_loaded)
      return false;

   cheevos_free_cheevo_set(&cheevos_locals.core);
   cheevos_locals.core.cheevos = NULL;
   cheevos_locals.core.count = 0;

   cheevos_free_cheevo_set(&cheevos_locals.unofficial);
   cheevos_locals.unofficial.cheevos = NULL;
   cheevos_locals.unofficial.count = 0;

   cheevos_loaded = 0;

   return true;
}

bool cheevos_toggle_hardcore_mode(void)
{
   settings_t *settings = config_get_ptr();

   /* reset and deinit rewind to avoid cheat the score */
   if (settings->bools.cheevos_hardcore_mode_enable)
   {
      /* send reset core cmd to avoid any user savestate previusly loaded */
      command_event(CMD_EVENT_RESET, NULL);
      if (settings->bools.rewind_enable)
         command_event(CMD_EVENT_REWIND_DEINIT, NULL);

      RARCH_LOG("%s\n", msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_ENABLE));
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_ENABLE), 0, 3 * 60, true);
   }
   else
   {
      if (settings->bools.rewind_enable)
         command_event(CMD_EVENT_REWIND_INIT, NULL);
   }

   return true;
}

static void cheevos_patch_addresses(cheevoset_t* set)
{
   unsigned i, j, k;
   cheevo_t* cheevo = set->cheevos;

   for (i = set->count; i != 0; i--, cheevo++)
   {
      cheevos_condset_t* condset = cheevo->condition.condsets;

      for (j = cheevo->condition.count; j != 0; j--, condset++)
      {
         cheevos_cond_t* cond = condset->conds;

         for (k = condset->count; k != 0; k--, cond++)
         {
            switch (cond->source.type)
            {
               case CHEEVOS_VAR_TYPE_ADDRESS:
               case CHEEVOS_VAR_TYPE_DELTA_MEM:
                  cheevos_var_patch_addr(&cond->source, cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
                  RARCH_LOG("[CHEEVOS]: s-var %03d:%08X\n", cond->source.bank_id + 1, cond->source.value);
#endif
                  break;

               default:
                  break;
            }

            switch (cond->target.type)
            {
               case CHEEVOS_VAR_TYPE_ADDRESS:
               case CHEEVOS_VAR_TYPE_DELTA_MEM:
                  cheevos_var_patch_addr(&cond->target, cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
                  RARCH_LOG("[CHEEVOS]: t-var %03d:%08X\n", cond->target.bank_id + 1, cond->target.value);
#endif
                  break;

               default:
                  break;
            }
         }
      }
   }
}

static void cheevos_patch_lb_conditions(cheevos_condition_t* condition)
{
   unsigned i, j;
   cheevos_condset_t* condset = condition->condsets;

   for (i = condition->count; i != 0; i--, condset++)
   {
      cheevos_cond_t* cond = condset->conds;

      for (j = condset->count; j != 0; j--, cond++)
      {
         switch (cond->source.type)
         {
            case CHEEVOS_VAR_TYPE_ADDRESS:
            case CHEEVOS_VAR_TYPE_DELTA_MEM:
               cheevos_var_patch_addr(&cond->source, cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
               RARCH_LOG("[CHEEVOS]: s-var %03d:%08X\n", cond->source.bank_id + 1, cond->source.value);
#endif
               break;
            default:
               break;
         }
         switch (cond->target.type)
         {
            case CHEEVOS_VAR_TYPE_ADDRESS:
            case CHEEVOS_VAR_TYPE_DELTA_MEM:
               cheevos_var_patch_addr(&cond->target, cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
               RARCH_LOG("[CHEEVOS]: t-var %03d:%08X\n", cond->target.bank_id + 1, cond->target.value);
#endif
               break;
            default:
               break;
         }
      }
   }
}

static void cheevos_patch_lb_expressions(cheevos_expr_t* expression)
{
   unsigned i;
   cheevos_term_t* term = expression->terms;

   for (i = expression->count; i != 0; i--, term++)
   {
      switch (term->var.type)
      {
         case CHEEVOS_VAR_TYPE_ADDRESS:
         case CHEEVOS_VAR_TYPE_DELTA_MEM:
            cheevos_var_patch_addr(&term->var, cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
            RARCH_LOG("[CHEEVOS]: s-var %03d:%08X\n", term->var.bank_id + 1, term->var.value);
#endif
            break;
         default:
            break;
      }
   }
}

static void cheevos_patch_lbs(cheevos_leaderboard_t *leaderboard)
{
   unsigned i;

   for(i = 0; i < cheevos_locals.lboard_count; i++)
   {
      cheevos_condition_t* start = &leaderboard[i].start;
      cheevos_condition_t* cancel = &leaderboard[i].cancel;
      cheevos_condition_t* submit = &leaderboard[i].submit;
      cheevos_expr_t* value = &leaderboard[i].value;

      cheevos_patch_lb_conditions(start);
      cheevos_patch_lb_conditions(cancel);
      cheevos_patch_lb_conditions(submit);
      cheevos_patch_lb_expressions(value);
   }
}

void cheevos_test(void)
{
   settings_t *settings = config_get_ptr();

   if (!cheevos_locals.addrs_patched)
   {
      cheevos_patch_addresses(&cheevos_locals.core);
      cheevos_patch_addresses(&cheevos_locals.unofficial);
      cheevos_patch_lbs(cheevos_locals.leaderboards);

      cheevos_locals.addrs_patched = true;
   }

   cheevos_test_cheevo_set(&cheevos_locals.core);

   if (settings->bools.cheevos_test_unofficial)
      cheevos_test_cheevo_set(&cheevos_locals.unofficial);

   if (settings->bools.cheevos_hardcore_mode_enable && settings->bools.cheevos_leaderboards_enable)
      cheevos_test_leaderboards();
}

bool cheevos_set_cheats(void)
{
   cheats_were_enabled = cheats_are_enabled;

   return true;
}

void cheevos_set_support_cheevos(bool state)
{
   cheevos_locals.core_supports = state;
}

bool cheevos_get_support_cheevos(void)
{
  return cheevos_locals.core_supports;
}

cheevos_console_t cheevos_get_console(void)
{
   return cheevos_locals.console_id;
}

typedef struct
{
   uint8_t id[4]; /* NES^Z */
   uint8_t rom_size;
   uint8_t vrom_size;
   uint8_t rom_type;
   uint8_t rom_type2;
   uint8_t reserve[8];
} cheevos_nes_header_t;

#define CORO_VARS \
   void *data; \
   size_t len; \
   const char *path; \
   settings_t *settings; \
   struct retro_system_info sysinfo; \
   unsigned i; \
   unsigned j; \
   unsigned k; \
   const char *ext; \
   MD5_CTX md5; \
   unsigned char hash[16]; \
   unsigned gameid; \
   char *json; \
   size_t count; \
   size_t offset; \
   cheevos_nes_header_t header; \
   size_t romsize, bytes; \
   int mapper; \
   bool round; \
   intfstream_t *stream; \
   size_t size; \
   char url[256]; \
   struct http_connection_t *conn; \
   struct http_t *http; \
   retro_time_t t0; \
   char badge_basepath[PATH_MAX_LENGTH]; \
   char badge_fullpath[PATH_MAX_LENGTH]; \
   char badge_name[16]; \
   cheevo_t *cheevo; \
   const cheevo_t *cheevo_end;

#include "coro.h"

#define CHEEVOS_VAR_INFO            CORO_VAR(info)
#define CHEEVOS_VAR_DATA            CORO_VAR(data)
#define CHEEVOS_VAR_LEN             CORO_VAR(len)
#define CHEEVOS_VAR_PATH            CORO_VAR(path)
#define CHEEVOS_VAR_SETTINGS        CORO_VAR(settings)
#define CHEEVOS_VAR_SYSINFO         CORO_VAR(sysinfo)
#define CHEEVOS_VAR_I               CORO_VAR(i)
#define CHEEVOS_VAR_J               CORO_VAR(j)
#define CHEEVOS_VAR_K               CORO_VAR(k)
#define CHEEVOS_VAR_EXT             CORO_VAR(ext)
#define CHEEVOS_VAR_MD5             CORO_VAR(md5)
#define CHEEVOS_VAR_HASH            CORO_VAR(hash)
#define CHEEVOS_VAR_GAMEID          CORO_VAR(gameid)
#define CHEEVOS_VAR_JSON            CORO_VAR(json)
#define CHEEVOS_VAR_COUNT           CORO_VAR(count)
#define CHEEVOS_VAR_OFFSET          CORO_VAR(offset)
#define CHEEVOS_VAR_HEADER          CORO_VAR(header)
#define CHEEVOS_VAR_ROMSIZE         CORO_VAR(romsize)
#define CHEEVOS_VAR_BYTES           CORO_VAR(bytes)
#define CHEEVOS_VAR_MAPPER          CORO_VAR(mapper)
#define CHEEVOS_VAR_ROUND           CORO_VAR(round)
#define CHEEVOS_VAR_STREAM          CORO_VAR(stream)
#define CHEEVOS_VAR_SIZE            CORO_VAR(size)
#define CHEEVOS_VAR_URL             CORO_VAR(url)
#define CHEEVOS_VAR_CONN            CORO_VAR(conn)
#define CHEEVOS_VAR_HTTP            CORO_VAR(http)
#define CHEEVOS_VAR_T0              CORO_VAR(t0)
#define CHEEVOS_VAR_BADGE_PATH      CORO_VAR(badge_fullpath)
#define CHEEVOS_VAR_BADGE_BASE_PATH CORO_VAR(badge_fullpath)
#define CHEEVOS_VAR_BADGE_NAME      CORO_VAR(badge_name)
#define CHEEVOS_VAR_CHEEVO_CURR     CORO_VAR(cheevo)
#define CHEEVOS_VAR_CHEEVO_END      CORO_VAR(cheevo_end)

static int cheevos_iterate(coro_t* coro)
{
   ssize_t num_read = 0;
   size_t to_read   = 4096;
   uint8_t *buffer  = NULL;
   const char *end  = NULL;

   enum
   {
      /* Negative values because CORO_SUB generates positive values */
      SNES_MD5    = -1,
      GENESIS_MD5 = -2,
      LYNX_MD5    = -3,
      NES_MD5     = -4,
      GENERIC_MD5 = -5,
      EVAL_MD5    = -6,
      FILL_MD5    = -7,
      GET_GAMEID  = -8,
      GET_CHEEVOS = -9,
      GET_BADGES  = -10,
      LOGIN       = -11,
      HTTP_GET    = -12,
      DEACTIVATE  = -13,
      PLAYING     = -14,
      DELAY       = -15
   };

   static const uint32_t genesis_exts[] =
   {
      0x0b888feeU, /* mdx */
      0x005978b6U, /* md  */
      0x0b88aa89U, /* smd */
      0x0b88767fU, /* gen */
      0x0b8861beU, /* bin */
      0x0b886782U, /* cue */
      0x0b8880d0U, /* iso */
      0x0b88aa98U, /* sms */
      0x005977f3U, /* gg  */
      0x0059797fU, /* sg  */
      0
   };

   static const uint32_t snes_exts[] =
   {
      0x0b88aa88U, /* smc */
      0x0b8872bbU, /* fig */
      0x0b88a9a1U, /* sfc */
      0x0b887623U, /* gd3 */
      0x0b887627U, /* gd7 */
      0x0b886bf3U, /* dx2 */
      0x0b886312U, /* bsx */
      0x0b88abd2U, /* swc */
      0
   };

   static const uint32_t lynx_exts[] =
   {
      0x0b888cf7U, /* lnx */
      0
   };

   static cheevos_finder_t finders[] =
   {
      {SNES_MD5,    "SNES (8Mb padding)",                snes_exts},
      {GENESIS_MD5, "Genesis (6Mb padding)",             genesis_exts},
      {LYNX_MD5,    "Atari Lynx (only first 512 bytes)", lynx_exts},
      {NES_MD5,     "NES (discards VROM)",               NULL},
      {GENERIC_MD5, "Generic (plain content)",           NULL}
   };

   CORO_ENTER()

      cheevos_locals.addrs_patched = false;

      CHEEVOS_VAR_SETTINGS = config_get_ptr();

      cheevos_locals.meminfo[0].id = RETRO_MEMORY_SYSTEM_RAM;
      core_get_memory(&cheevos_locals.meminfo[0]);

      cheevos_locals.meminfo[1].id = RETRO_MEMORY_SAVE_RAM;
      core_get_memory(&cheevos_locals.meminfo[1]);

      cheevos_locals.meminfo[2].id = RETRO_MEMORY_VIDEO_RAM;
      core_get_memory(&cheevos_locals.meminfo[2]);

      cheevos_locals.meminfo[3].id = RETRO_MEMORY_RTC;
      core_get_memory(&cheevos_locals.meminfo[3]);

      RARCH_LOG("[CHEEVOS]: system RAM: %p %u\n",
         cheevos_locals.meminfo[0].data, cheevos_locals.meminfo[0].size);
      RARCH_LOG("[CHEEVOS]: save RAM:   %p %u\n",
         cheevos_locals.meminfo[1].data, cheevos_locals.meminfo[1].size);
      RARCH_LOG("[CHEEVOS]: video RAM:  %p %u\n",
         cheevos_locals.meminfo[2].data, cheevos_locals.meminfo[2].size);
      RARCH_LOG("[CHEEVOS]: RTC:        %p %u\n",
         cheevos_locals.meminfo[3].data, cheevos_locals.meminfo[3].size);

      /* Bail out if cheevos are disabled.
       * But set the above anyways, command_read_ram needs it. */
      if (!CHEEVOS_VAR_SETTINGS->bools.cheevos_enable)
         CORO_STOP();

      /* Load the content into memory, or copy it over to our own buffer */
      if (!CHEEVOS_VAR_DATA)
      {
         CHEEVOS_VAR_STREAM = intfstream_open_file(
               CHEEVOS_VAR_PATH,
               RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE);

         if (!CHEEVOS_VAR_STREAM)
            CORO_STOP();

         CORO_YIELD();
         CHEEVOS_VAR_LEN = 0;
         CHEEVOS_VAR_COUNT = intfstream_get_size(CHEEVOS_VAR_STREAM);

         if (CHEEVOS_VAR_COUNT > CHEEVOS_SIZE_LIMIT)
            CHEEVOS_VAR_COUNT = CHEEVOS_SIZE_LIMIT;

         CHEEVOS_VAR_DATA = malloc(CHEEVOS_VAR_COUNT);

         if (!CHEEVOS_VAR_DATA)
         {
            intfstream_close(CHEEVOS_VAR_STREAM);
            free(CHEEVOS_VAR_STREAM);
            CORO_STOP();
         }

         for (;;)
         {
            buffer   = (uint8_t*)CHEEVOS_VAR_DATA + CHEEVOS_VAR_LEN;
            to_read  = 4096;

            if (to_read > CHEEVOS_VAR_COUNT)
               to_read = CHEEVOS_VAR_COUNT;

            num_read = intfstream_read(CHEEVOS_VAR_STREAM, (void*)buffer, to_read);

            if (num_read <= 0)
               break;

            CHEEVOS_VAR_LEN   += num_read;
            CHEEVOS_VAR_COUNT -= num_read;

            if (CHEEVOS_VAR_COUNT == 0)
               break;

            CORO_YIELD();
         }

         intfstream_close(CHEEVOS_VAR_STREAM);
         free(CHEEVOS_VAR_STREAM);
      }

      /* Use the supported extensions as a hint
       * to what method we should use. */
      core_get_system_info(&CHEEVOS_VAR_SYSINFO);

      for (CHEEVOS_VAR_I = 0; CHEEVOS_VAR_I < ARRAY_SIZE(finders); CHEEVOS_VAR_I++)
      {
         if (finders[CHEEVOS_VAR_I].ext_hashes)
         {
            CHEEVOS_VAR_EXT = CHEEVOS_VAR_SYSINFO.valid_extensions;

            while (CHEEVOS_VAR_EXT)
            {
               unsigned hash;
               end = strchr(CHEEVOS_VAR_EXT, '|');

               if (end)
               {
                  hash = cheevos_djb2(CHEEVOS_VAR_EXT, end - CHEEVOS_VAR_EXT);
                  CHEEVOS_VAR_EXT = end + 1;
               }
               else
               {
                  hash = cheevos_djb2(CHEEVOS_VAR_EXT, strlen(CHEEVOS_VAR_EXT));
                  CHEEVOS_VAR_EXT = NULL;
               }

               for (CHEEVOS_VAR_J = 0; finders[CHEEVOS_VAR_I].ext_hashes[CHEEVOS_VAR_J]; CHEEVOS_VAR_J++)
               {
                  if (finders[CHEEVOS_VAR_I].ext_hashes[CHEEVOS_VAR_J] == hash)
                  {
                     RARCH_LOG("[CHEEVOS]: testing %s.\n", finders[CHEEVOS_VAR_I].name);

                     /*
                      * Inputs:  CHEEVOS_VAR_INFO
                      * Outputs: CHEEVOS_VAR_GAMEID, the game was found if it's different from 0
                      */
                     CORO_GOSUB(finders[CHEEVOS_VAR_I].label);

                     if (CHEEVOS_VAR_GAMEID != 0)
                        goto found;

                     CHEEVOS_VAR_EXT = NULL; /* force next finder */
                     break;
                  }
               }
            }
         }
      }

      for (CHEEVOS_VAR_I = 0; CHEEVOS_VAR_I < ARRAY_SIZE(finders); CHEEVOS_VAR_I++)
      {
         if (finders[CHEEVOS_VAR_I].ext_hashes)
            continue;

         RARCH_LOG("[CHEEVOS]: testing %s.\n", finders[CHEEVOS_VAR_I].name);

         /*
          * Inputs:  CHEEVOS_VAR_INFO
          * Outputs: CHEEVOS_VAR_GAMEID
          */
         CORO_GOSUB(finders[CHEEVOS_VAR_I].label);

         if (CHEEVOS_VAR_GAMEID != 0)
            goto found;
      }

      RARCH_LOG("[CHEEVOS]: this game doesn't feature achievements.\n");
      CORO_STOP();

      found:

#ifdef CHEEVOS_JSON_OVERRIDE
      {
         FILE* file;
         size_t size;

         file = fopen(CHEEVOS_JSON_OVERRIDE, "rb");
         fseek(file, 0, SEEK_END);
         size = ftell(file);
         fseek(file, 0, SEEK_SET);

         CHEEVOS_VAR_JSON = (char*)malloc(size + 1);
         fread((void*)CHEEVOS_VAR_JSON, 1, size, file);

         fclose(file);
         CHEEVOS_VAR_JSON[size] = 0;
      }
#else
      CORO_GOSUB(GET_CHEEVOS);

      if (!CHEEVOS_VAR_JSON)
      {
         runloop_msg_queue_push("Error loading achievements.", 0, 5 * 60, false);
         RARCH_ERR("[CHEEVOS]: error loading achievements.\n");
         CORO_STOP();
      }
#endif

#ifdef CHEEVOS_SAVE_JSON
      {
         FILE* file = fopen(CHEEVOS_SAVE_JSON, "w");
         fwrite((void*)CHEEVOS_VAR_JSON, 1, strlen(CHEEVOS_VAR_JSON), file);
         fclose(file);
      }
#endif
      if (cheevos_parse(CHEEVOS_VAR_JSON))
      {
         if ((void*)CHEEVOS_VAR_JSON)
            free((void*)CHEEVOS_VAR_JSON);
         CORO_STOP();
      }

      if ((void*)CHEEVOS_VAR_JSON)
         free((void*)CHEEVOS_VAR_JSON);

      cheevos_loaded = true;

      /*
       * Inputs:  CHEEVOS_VAR_GAMEID
       * Outputs:
       */
      CORO_GOSUB(DEACTIVATE);

      /*
       * Inputs:  CHEEVOS_VAR_GAMEID
       * Outputs:
       */
      CORO_GOSUB(PLAYING);

      if(CHEEVOS_VAR_SETTINGS->bools.cheevos_verbose_enable)
      {
         if(cheevos_locals.core.count > 0)
         {
            int mode;
            const cheevo_t* cheevo       = cheevos_locals.core.cheevos;
            const cheevo_t* end          = cheevo + cheevos_locals.core.count;
            int number_of_unlocked       = cheevos_locals.core.count;
            char msg[256];

            if(CHEEVOS_VAR_SETTINGS->bools.cheevos_hardcore_mode_enable)
               mode = CHEEVOS_ACTIVE_HARDCORE;
            else
               mode = CHEEVOS_ACTIVE_SOFTCORE;

            for(; cheevo < end; cheevo++)
               if(cheevo->active & mode)
                  number_of_unlocked--;

            snprintf(msg, sizeof(msg), "You have %d of %d achievements unlocked.",
               number_of_unlocked, cheevos_locals.core.count);
            msg[sizeof(msg) - 1] = 0;
            runloop_msg_queue_push(msg, 0, 6 * 60, false);
         }
         else
            runloop_msg_queue_push("This game has no achievements.", 0, 5 * 60, false);

      }

      if (   cheevos_locals.core.count == 0
          && cheevos_locals.unofficial.count == 0
          && cheevos_locals.lboard_count == 0)
         cheevos_unload();

      CORO_GOSUB(GET_BADGES);
      CORO_STOP();

   /**************************************************************************
    * Info   Tries to identify a SNES game
    * Input  CHEEVOS_VAR_INFO the content info
    * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
    *************************************************************************/
   CORO_SUB(SNES_MD5)

      MD5_Init(&CHEEVOS_VAR_MD5);

      CHEEVOS_VAR_OFFSET = CHEEVOS_VAR_COUNT = 0;
      CORO_GOSUB(EVAL_MD5);

      if (CHEEVOS_VAR_COUNT == 0)
      {
         MD5_Final(CHEEVOS_VAR_HASH, &CHEEVOS_VAR_MD5);
         CHEEVOS_VAR_GAMEID = 0;
         CORO_RET();
      }

      if (CHEEVOS_VAR_COUNT < CHEEVOS_EIGHT_MB)
      {
         /*
          * Inputs:  CHEEVOS_VAR_MD5, CHEEVOS_VAR_OFFSET, CHEEVOS_VAR_COUNT
          * Outputs: CHEEVOS_VAR_MD5
          */
         CHEEVOS_VAR_OFFSET = 0;
         CHEEVOS_VAR_COUNT = CHEEVOS_EIGHT_MB - CHEEVOS_VAR_COUNT;
         CORO_GOSUB(FILL_MD5);
      }

      MD5_Final(CHEEVOS_VAR_HASH, &CHEEVOS_VAR_MD5);
      CORO_GOTO(GET_GAMEID);

   /**************************************************************************
    * Info   Tries to identify a Genesis game
    * Input  CHEEVOS_VAR_INFO the content info
    * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
    *************************************************************************/
   CORO_SUB(GENESIS_MD5)

      MD5_Init(&CHEEVOS_VAR_MD5);

      CHEEVOS_VAR_OFFSET = CHEEVOS_VAR_COUNT = 0;
      CORO_GOSUB(EVAL_MD5);

      if (CHEEVOS_VAR_COUNT == 0)
      {
         MD5_Final(CHEEVOS_VAR_HASH, &CHEEVOS_VAR_MD5);
         CHEEVOS_VAR_GAMEID = 0;
         CORO_RET();
      }

      if (CHEEVOS_VAR_COUNT < CHEEVOS_SIX_MB)
      {
         CHEEVOS_VAR_OFFSET = 0;
         CHEEVOS_VAR_COUNT = CHEEVOS_SIX_MB - CHEEVOS_VAR_COUNT;
         CORO_GOSUB(FILL_MD5);
      }

      MD5_Final(CHEEVOS_VAR_HASH, &CHEEVOS_VAR_MD5);
      CORO_GOTO(GET_GAMEID);

   /**************************************************************************
    * Info   Tries to identify an Atari Lynx game
    * Input  CHEEVOS_VAR_INFO the content info
    * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
    *************************************************************************/
   CORO_SUB(LYNX_MD5)

      if (CHEEVOS_VAR_LEN < 0x0240)
      {
         CHEEVOS_VAR_GAMEID = 0;
         CORO_RET();
      }

      MD5_Init(&CHEEVOS_VAR_MD5);

      CHEEVOS_VAR_OFFSET = 0x0040;
      CHEEVOS_VAR_COUNT  = 0x0200;
      CORO_GOSUB(EVAL_MD5);

      MD5_Final(CHEEVOS_VAR_HASH, &CHEEVOS_VAR_MD5);
      CORO_GOTO(GET_GAMEID);

   /**************************************************************************
    * Info   Tries to identify a NES game
    * Input  CHEEVOS_VAR_INFO the content info
    * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
    *************************************************************************/
   CORO_SUB(NES_MD5)

      /* Note about the references to the FCEU emulator below. There is no
       * core-specific code in this function, it's rather Retro Achievements
       * specific code that must be followed to the letter so we compute
       * the correct ROM hash. Retro Achievements does indeed use some
       * FCEU related method to compute the hash, since its NES emulator
       * is based on it. */

      if (CHEEVOS_VAR_LEN < sizeof(CHEEVOS_VAR_HEADER))
      {
         CHEEVOS_VAR_GAMEID = 0;
         CORO_RET();
      }

      memcpy((void*)&CHEEVOS_VAR_HEADER, CHEEVOS_VAR_DATA, sizeof(CHEEVOS_VAR_HEADER));

      if (     CHEEVOS_VAR_HEADER.id[0] != 'N'
            || CHEEVOS_VAR_HEADER.id[1] != 'E'
            || CHEEVOS_VAR_HEADER.id[2] != 'S'
            || CHEEVOS_VAR_HEADER.id[3] != 0x1a)
      {
         CHEEVOS_VAR_GAMEID = 0;
         CORO_RET();
      }

      if (CHEEVOS_VAR_HEADER.rom_size)
         CHEEVOS_VAR_ROMSIZE = next_pow2(CHEEVOS_VAR_HEADER.rom_size);
      else
         CHEEVOS_VAR_ROMSIZE = 256;

      /* from FCEU core - compute size using the cart mapper */
      CHEEVOS_VAR_MAPPER = (CHEEVOS_VAR_HEADER.rom_type >> 4) | (CHEEVOS_VAR_HEADER.rom_type2 & 0xF0);

      /* for games not to the power of 2, so we just read enough
       * PRG rom from it, but we have to keep ROM_size to the power of 2
       * since PRGCartMapping wants ROM_size to be to the power of 2
       * so instead if not to power of 2, we just use head.ROM_size when
       * we use FCEU_read. */
      CHEEVOS_VAR_ROUND = CHEEVOS_VAR_MAPPER != 53 && CHEEVOS_VAR_MAPPER != 198 && CHEEVOS_VAR_MAPPER != 228;
      CHEEVOS_VAR_BYTES = (CHEEVOS_VAR_ROUND) ? CHEEVOS_VAR_ROMSIZE : CHEEVOS_VAR_HEADER.rom_size;

      /* from FCEU core - check if Trainer included in ROM data */
      MD5_Init(&CHEEVOS_VAR_MD5);
      CHEEVOS_VAR_OFFSET = sizeof(CHEEVOS_VAR_HEADER) + (CHEEVOS_VAR_HEADER.rom_type & 4 ? sizeof(CHEEVOS_VAR_HEADER) : 0);
      CHEEVOS_VAR_COUNT = 0x4000 * CHEEVOS_VAR_BYTES;
      CORO_GOSUB(EVAL_MD5);

      if (CHEEVOS_VAR_COUNT < 0x4000 * CHEEVOS_VAR_BYTES)
      {
         CHEEVOS_VAR_OFFSET = 0xff;
         CHEEVOS_VAR_COUNT = 0x4000 * CHEEVOS_VAR_BYTES - CHEEVOS_VAR_COUNT;
         CORO_GOSUB(FILL_MD5);
      }

      MD5_Final(CHEEVOS_VAR_HASH, &CHEEVOS_VAR_MD5);
      CORO_GOTO(GET_GAMEID);

   /**************************************************************************
    * Info   Tries to identify a "generic" game
    * Input  CHEEVOS_VAR_INFO the content info
    * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
    *************************************************************************/
   CORO_SUB(GENERIC_MD5)

      MD5_Init(&CHEEVOS_VAR_MD5);

      CHEEVOS_VAR_OFFSET = 0;
      CHEEVOS_VAR_COUNT = 0;
      CORO_GOSUB(EVAL_MD5);

      MD5_Final(CHEEVOS_VAR_HASH, &CHEEVOS_VAR_MD5);

      if (CHEEVOS_VAR_COUNT == 0)
         CORO_RET();

      CORO_GOTO(GET_GAMEID);

   /**************************************************************************
    * Info    Evaluates the CHEEVOS_VAR_MD5 hash
    * Inputs  CHEEVOS_VAR_INFO, CHEEVOS_VAR_OFFSET, CHEEVOS_VAR_COUNT
    * Outputs CHEEVOS_VAR_MD5, CHEEVOS_VAR_COUNT
    *************************************************************************/
   CORO_SUB(EVAL_MD5)

      if (CHEEVOS_VAR_COUNT == 0)
         CHEEVOS_VAR_COUNT = CHEEVOS_VAR_LEN;

      if (CHEEVOS_VAR_LEN - CHEEVOS_VAR_OFFSET < CHEEVOS_VAR_COUNT)
         CHEEVOS_VAR_COUNT = CHEEVOS_VAR_LEN - CHEEVOS_VAR_OFFSET;

      if (CHEEVOS_VAR_COUNT > CHEEVOS_SIZE_LIMIT)
         CHEEVOS_VAR_COUNT = CHEEVOS_SIZE_LIMIT;

      MD5_Update(&CHEEVOS_VAR_MD5, (void*)((uint8_t*)CHEEVOS_VAR_DATA + CHEEVOS_VAR_OFFSET), CHEEVOS_VAR_COUNT);
      CORO_RET();

   /**************************************************************************
    * Info    Updates the CHEEVOS_VAR_MD5 hash with a repeated value
    * Inputs  CHEEVOS_VAR_OFFSET, CHEEVOS_VAR_COUNT
    * Outputs CHEEVOS_VAR_MD5
    *************************************************************************/
   CORO_SUB(FILL_MD5)

      {
         char buffer[4096];

         while (CHEEVOS_VAR_COUNT > 0)
         {
            size_t len = sizeof(buffer);

            if (len > CHEEVOS_VAR_COUNT)
               len = CHEEVOS_VAR_COUNT;

            memset((void*)buffer, CHEEVOS_VAR_OFFSET, len);
            MD5_Update(&CHEEVOS_VAR_MD5, (void*)buffer, len);
            CHEEVOS_VAR_COUNT -= len;
         }
      }

      CORO_RET();

   /**************************************************************************
    * Info    Gets the achievements from Retro Achievements
    * Inputs  CHEEVOS_VAR_HASH
    * Outputs CHEEVOS_VAR_GAMEID
    *************************************************************************/
   CORO_SUB(GET_GAMEID)

      {
         char gameid[16];

         RARCH_LOG(
            "[CHEEVOS]: getting game id for hash %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
            CHEEVOS_VAR_HASH[ 0], CHEEVOS_VAR_HASH[ 1], CHEEVOS_VAR_HASH[ 2], CHEEVOS_VAR_HASH[ 3],
            CHEEVOS_VAR_HASH[ 4], CHEEVOS_VAR_HASH[ 5], CHEEVOS_VAR_HASH[ 6], CHEEVOS_VAR_HASH[ 7],
            CHEEVOS_VAR_HASH[ 8], CHEEVOS_VAR_HASH[ 9], CHEEVOS_VAR_HASH[10], CHEEVOS_VAR_HASH[11],
            CHEEVOS_VAR_HASH[12], CHEEVOS_VAR_HASH[13], CHEEVOS_VAR_HASH[14], CHEEVOS_VAR_HASH[15]
         );

         snprintf(
            CHEEVOS_VAR_URL, sizeof(CHEEVOS_VAR_URL),
            "http://retroachievements.org/dorequest.php?r=gameid&m=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            CHEEVOS_VAR_HASH[ 0], CHEEVOS_VAR_HASH[ 1], CHEEVOS_VAR_HASH[ 2], CHEEVOS_VAR_HASH[ 3],
            CHEEVOS_VAR_HASH[ 4], CHEEVOS_VAR_HASH[ 5], CHEEVOS_VAR_HASH[ 6], CHEEVOS_VAR_HASH[ 7],
            CHEEVOS_VAR_HASH[ 8], CHEEVOS_VAR_HASH[ 9], CHEEVOS_VAR_HASH[10], CHEEVOS_VAR_HASH[11],
            CHEEVOS_VAR_HASH[12], CHEEVOS_VAR_HASH[13], CHEEVOS_VAR_HASH[14], CHEEVOS_VAR_HASH[15]
         );

         CHEEVOS_VAR_URL[sizeof(CHEEVOS_VAR_URL) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
         cheevos_log_url("[CHEEVOS]: url to get the game's id: %s\n", CHEEVOS_VAR_URL);
#endif

         CORO_GOSUB(HTTP_GET);

         if (!CHEEVOS_VAR_JSON)
            CORO_RET();

         if (cheevos_get_value(CHEEVOS_VAR_JSON, CHEEVOS_JSON_KEY_GAMEID, gameid, sizeof(gameid)))
         {
            if ((void*)CHEEVOS_VAR_JSON)
               free((void*)CHEEVOS_VAR_JSON);
            RARCH_ERR("[CHEEVOS]: error getting game_id.\n");
            CORO_RET();
         }

         if ((void*)CHEEVOS_VAR_JSON)
            free((void*)CHEEVOS_VAR_JSON);
         RARCH_LOG("[CHEEVOS]: got game id %s.\n", gameid);
         CHEEVOS_VAR_GAMEID = strtol(gameid, NULL, 10);
         CORO_RET();
      }

   /**************************************************************************
    * Info    Gets the achievements from Retro Achievements
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs CHEEVOS_VAR_JSON
    *************************************************************************/
   CORO_SUB(GET_CHEEVOS)

   CORO_GOSUB(LOGIN);

   snprintf(CHEEVOS_VAR_URL, sizeof(CHEEVOS_VAR_URL),
      "http://retroachievements.org/dorequest.php?r=patch&g=%u&u=%s&t=%s",
      CHEEVOS_VAR_GAMEID,
      CHEEVOS_VAR_SETTINGS->arrays.cheevos_username,
      cheevos_locals.token);

      CHEEVOS_VAR_URL[sizeof(CHEEVOS_VAR_URL) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to get the list of cheevos: %s\n", CHEEVOS_VAR_URL);
#endif

      CORO_GOSUB(HTTP_GET);

      if (!CHEEVOS_VAR_JSON)
      {
         RARCH_ERR("[CHEEVOS]: error getting achievements for game id %u.\n", CHEEVOS_VAR_GAMEID);
         CORO_STOP();
      }

      RARCH_LOG("[CHEEVOS]: got achievements for game id %u.\n", CHEEVOS_VAR_GAMEID);
      CORO_RET();

   /**************************************************************************
   * Info    Gets the achievements from Retro Achievements
   * Inputs  CHEEVOS_VAR_GAMEID
   * Outputs CHEEVOS_VAR_JSON
   *************************************************************************/
   CORO_SUB(GET_BADGES)

   badges_ctx = new_badges_ctx;

   {
      settings_t *settings = config_get_ptr();
      if (!string_is_equal(settings->arrays.menu_driver, "xmb") ||
            !settings->bools.cheevos_badges_enable)
         CORO_RET();
   }

   CHEEVOS_VAR_CHEEVO_CURR = cheevos_locals.core.cheevos;
   CHEEVOS_VAR_CHEEVO_END = cheevos_locals.core.cheevos + cheevos_locals.core.count;

   for (; CHEEVOS_VAR_CHEEVO_CURR < CHEEVOS_VAR_CHEEVO_END ; CHEEVOS_VAR_CHEEVO_CURR++)
   {
      for (CHEEVOS_VAR_J = 0 ; CHEEVOS_VAR_J < 2; CHEEVOS_VAR_J++)
      {
         CHEEVOS_VAR_BADGE_PATH[0] = '\0';
         fill_pathname_application_special(CHEEVOS_VAR_BADGE_BASE_PATH, sizeof(CHEEVOS_VAR_BADGE_BASE_PATH),
            APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

         if (!path_is_directory(CHEEVOS_VAR_BADGE_BASE_PATH))
            path_mkdir(CHEEVOS_VAR_BADGE_BASE_PATH);
         CORO_YIELD();
         if (CHEEVOS_VAR_J == 0)
            snprintf(CHEEVOS_VAR_BADGE_NAME, sizeof(CHEEVOS_VAR_BADGE_NAME), "%s.png", CHEEVOS_VAR_CHEEVO_CURR->badge);
         else
            snprintf(CHEEVOS_VAR_BADGE_NAME, sizeof(CHEEVOS_VAR_BADGE_NAME), "%s_lock.png", CHEEVOS_VAR_CHEEVO_CURR->badge);

         fill_pathname_join(CHEEVOS_VAR_BADGE_PATH, CHEEVOS_VAR_BADGE_BASE_PATH, CHEEVOS_VAR_BADGE_NAME, sizeof(CHEEVOS_VAR_BADGE_PATH));

         if (!badge_exists(CHEEVOS_VAR_BADGE_PATH))
         {
#ifdef CHEEVOS_LOG_BADGES
            RARCH_LOG("[CHEEVOS]: downloading badge %s\n", CHEEVOS_VAR_BADGE_PATH);
#endif
            snprintf(CHEEVOS_VAR_URL, sizeof(CHEEVOS_VAR_URL), "http://i.retroachievements.org/Badge/%s", CHEEVOS_VAR_BADGE_NAME);

            CORO_GOSUB(HTTP_GET);
            if (CHEEVOS_VAR_JSON != NULL)
            {
               if (!filestream_write_file(CHEEVOS_VAR_BADGE_PATH, CHEEVOS_VAR_JSON, CHEEVOS_VAR_K))
                  RARCH_ERR("[CHEEVOS]: error writing badge %s\n", CHEEVOS_VAR_BADGE_PATH);
               else
                  free(CHEEVOS_VAR_JSON);
            }
         }
      }
   }

    CORO_RET();

   /**************************************************************************
    * Info Logs in the user at Retro Achievements
    *************************************************************************/
   CORO_SUB(LOGIN)

      if (cheevos_locals.token[0])
         CORO_RET();

      {
         const char *username = CHEEVOS_VAR_SETTINGS->arrays.cheevos_username;
         const char *password = CHEEVOS_VAR_SETTINGS->arrays.cheevos_password;
         char urle_user[64];
         char urle_pwd[64];

         if (!username || !*username || !password || !*password)
         {
            runloop_msg_queue_push("Missing Retro Achievements account information.", 0, 5 * 60, false);
            runloop_msg_queue_push("Please fill in your account information in Settings.", 0, 5 * 60, false);
            RARCH_ERR("[CHEEVOS]: username and/or password not informed.\n");
            CORO_STOP();
         }

         cheevos_url_encode(username, urle_user, sizeof(urle_user));
         cheevos_url_encode(password, urle_pwd, sizeof(urle_pwd));

         snprintf(
            CHEEVOS_VAR_URL, sizeof(CHEEVOS_VAR_URL),
            "http://retroachievements.org/dorequest.php?r=login&u=%s&p=%s",
            urle_user, urle_pwd
         );

         CHEEVOS_VAR_URL[sizeof(CHEEVOS_VAR_URL) - 1] = 0;
      }

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to login: %s\n", CHEEVOS_VAR_URL);
#endif

      CORO_GOSUB(HTTP_GET);

      if (CHEEVOS_VAR_JSON)
      {
         int res = cheevos_get_value(CHEEVOS_VAR_JSON, CHEEVOS_JSON_KEY_TOKEN, cheevos_locals.token, sizeof(cheevos_locals.token));
         if ((void*)CHEEVOS_VAR_JSON)
            free((void*)CHEEVOS_VAR_JSON);

         if (!res)
         {
            if(CHEEVOS_VAR_SETTINGS->bools.cheevos_verbose_enable)
            {
               char msg[256];
               snprintf(msg, sizeof(msg), "RetroAchievements: logged in as \"%s\".", CHEEVOS_VAR_SETTINGS->arrays.cheevos_username);
               msg[sizeof(msg) - 1] = 0;
               runloop_msg_queue_push(msg, 0, 3 * 60, false);
            }
            CORO_RET();
         }
      }

      runloop_msg_queue_push("Retro Achievements login error.", 0, 5 * 60, false);
      RARCH_ERR("[CHEEVOS]: error getting user token.\n");
      CORO_STOP();

   /**************************************************************************
    * Info    Pauses execution for five seconds
    *************************************************************************/
   CORO_SUB(DELAY)

      {
         retro_time_t t1;
         CHEEVOS_VAR_T0 = cpu_features_get_time_usec();

         do
         {
            CORO_YIELD();
            t1 = cpu_features_get_time_usec();
         }
         while ((t1 - CHEEVOS_VAR_T0) < 3000000);
      }

      CORO_RET();

   /**************************************************************************
    * Info    Makes a HTTP GET request
    * Inputs  CHEEVOS_VAR_URL
    * Outputs CHEEVOS_VAR_JSON
    *************************************************************************/
    CORO_SUB(HTTP_GET)

      for (CHEEVOS_VAR_K = 0; CHEEVOS_VAR_K < 5; CHEEVOS_VAR_K++)
      {
         if (CHEEVOS_VAR_K != 0)
            RARCH_LOG("[CHEEVOS]: Retrying HTTP request: %u of 5\n", CHEEVOS_VAR_K + 1);

         CHEEVOS_VAR_JSON = NULL;
         CHEEVOS_VAR_CONN = net_http_connection_new(CHEEVOS_VAR_URL, "GET", NULL);

         if (!CHEEVOS_VAR_CONN)
         {
            CORO_GOSUB(DELAY);
            continue;
         }

         /* Don't bother with timeouts here, it's just a string scan. */
         while (!net_http_connection_iterate(CHEEVOS_VAR_CONN)) {}

         /* Error finishing the connection descriptor. */
         if (!net_http_connection_done(CHEEVOS_VAR_CONN))
         {
            net_http_connection_free(CHEEVOS_VAR_CONN);
            continue;
         }

         CHEEVOS_VAR_HTTP = net_http_new(CHEEVOS_VAR_CONN);

         /* Error connecting to the endpoint. */
         if (!CHEEVOS_VAR_HTTP)
         {
            net_http_connection_free(CHEEVOS_VAR_CONN);
            CORO_GOSUB(DELAY);
            continue;
         }

         while (!net_http_update(CHEEVOS_VAR_HTTP, NULL, NULL))
            CORO_YIELD();

         {
            size_t length;
            uint8_t *data = net_http_data(CHEEVOS_VAR_HTTP, &length, false);

            if (data)
            {
               CHEEVOS_VAR_JSON = (char*)malloc(length + 1);

               if (CHEEVOS_VAR_JSON)
               {
                  memcpy((void*)CHEEVOS_VAR_JSON, (void*)data, length);
                  free(data);
                  CHEEVOS_VAR_JSON[length] = 0;
               }

               CHEEVOS_VAR_K = length;
               net_http_delete(CHEEVOS_VAR_HTTP);
               net_http_connection_free(CHEEVOS_VAR_CONN);
               CORO_RET();
            }
         }

         net_http_delete(CHEEVOS_VAR_HTTP);
         net_http_connection_free(CHEEVOS_VAR_CONN);
      }

      RARCH_LOG("[CHEEVOS]: Couldn't connect to server after 5 tries\n");
      CORO_RET();

   /**************************************************************************
    * Info    Deactivates the achievements already awarded
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs
    *************************************************************************/
   CORO_SUB(DEACTIVATE)

#ifndef CHEEVOS_DONT_DEACTIVATE
      CORO_GOSUB(LOGIN);

      /* Deactivate achievements in softcore mode. */
      snprintf(
         CHEEVOS_VAR_URL, sizeof(CHEEVOS_VAR_URL),
         "http://retroachievements.org/dorequest.php?r=unlocks&u=%s&t=%s&g=%u&h=0",
         CHEEVOS_VAR_SETTINGS->arrays.cheevos_username,
         cheevos_locals.token, CHEEVOS_VAR_GAMEID
      );

      CHEEVOS_VAR_URL[sizeof(CHEEVOS_VAR_URL) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to get the list of unlocked cheevos in softcore: %s\n", CHEEVOS_VAR_URL);
#endif

      CORO_GOSUB(HTTP_GET);

      if (CHEEVOS_VAR_JSON)
      {
         if (!cheevos_deactivate_unlocks(CHEEVOS_VAR_JSON, CHEEVOS_ACTIVE_SOFTCORE))
            RARCH_LOG("[CHEEVOS]: deactivated unlocked achievements in softcore mode.\n");
         else
            RARCH_ERR("[CHEEVOS]: error deactivating unlocked achievements in softcore mode.\n");

         if ((void*)CHEEVOS_VAR_JSON)
            free((void*)CHEEVOS_VAR_JSON);
      }
      else
         RARCH_ERR("[CHEEVOS]: error retrieving list of unlocked achievements in softcore mode.\n");

      /* Deactivate achievements in hardcore mode. */
      snprintf(
         CHEEVOS_VAR_URL, sizeof(CHEEVOS_VAR_URL),
         "http://retroachievements.org/dorequest.php?r=unlocks&u=%s&t=%s&g=%u&h=1",
         CHEEVOS_VAR_SETTINGS->arrays.cheevos_username,
         cheevos_locals.token, CHEEVOS_VAR_GAMEID
      );

      CHEEVOS_VAR_URL[sizeof(CHEEVOS_VAR_URL) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to get the list of unlocked cheevos in hardcore: %s\n", CHEEVOS_VAR_URL);
#endif

      CORO_GOSUB(HTTP_GET);

      if (CHEEVOS_VAR_JSON)
      {
         if (!cheevos_deactivate_unlocks(CHEEVOS_VAR_JSON, CHEEVOS_ACTIVE_HARDCORE))
            RARCH_LOG("[CHEEVOS]: deactivated unlocked achievements in hardcore mode.\n");
         else
            RARCH_ERR("[CHEEVOS]: error deactivating unlocked achievements in hardcore mode.\n");

         if ((void*)CHEEVOS_VAR_JSON)
            free((void*)CHEEVOS_VAR_JSON);
      }
      else
         RARCH_ERR("[CHEEVOS]: error retrieving list of unlocked achievements in hardcore mode.\n");

#endif
      CORO_RET();

   /**************************************************************************
    * Info    Posts the "playing" activity to Retro Achievements
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs
    *************************************************************************/
   CORO_SUB(PLAYING)

      snprintf(
         CHEEVOS_VAR_URL, sizeof(CHEEVOS_VAR_URL),
         "http://retroachievements.org/dorequest.php?r=postactivity&u=%s&t=%s&a=3&m=%u",
         CHEEVOS_VAR_SETTINGS->arrays.cheevos_username,
         cheevos_locals.token, CHEEVOS_VAR_GAMEID
      );

      CHEEVOS_VAR_URL[sizeof(CHEEVOS_VAR_URL) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to post the 'playing' activity: %s\n", CHEEVOS_VAR_URL);
#endif

      CORO_GOSUB(HTTP_GET);

      if (CHEEVOS_VAR_JSON)
      {
          RARCH_LOG("[CHEEVOS]: posted playing activity.\n");
          if ((void*)CHEEVOS_VAR_JSON)
             free((void*)CHEEVOS_VAR_JSON);
      }
      else
         RARCH_ERR("[CHEEVOS]: error posting playing activity.\n");

      RARCH_LOG("[CHEEVOS]: posted playing activity.\n");
      CORO_RET();

   CORO_LEAVE();
}

static void cheevos_task_handler(retro_task_t *task)
{
   coro_t *coro = (coro_t*)task->state;

   if (!coro)
      return;

   if (!cheevos_iterate(coro))
   {
      task_set_finished(task, true);
      if (CHEEVOS_VAR_DATA)
         free(CHEEVOS_VAR_DATA);
      if ((void*)CHEEVOS_VAR_PATH)
         free((void*)CHEEVOS_VAR_PATH);
      free((void*)coro);
   }
}

bool cheevos_load(const void *data)
{
   retro_task_t *task;
   const struct retro_game_info *info = NULL;
   coro_t *coro                       = NULL;

   cheevos_loaded = 0;

   if (!cheevos_locals.core_supports || !data)
      return false;

   coro = (coro_t*)calloc(1, sizeof(*coro));

   if (!coro)
      return false;

   task = (retro_task_t*)calloc(1, sizeof(*task));

   if (!task)
   {
      if ((void*)coro)
         free((void*)coro);
      return false;
   }

   CORO_SETUP(coro);

   info = (const struct retro_game_info*)data;

   if (info->data)
   {
      CHEEVOS_VAR_LEN = info->size;

      if (CHEEVOS_VAR_LEN > CHEEVOS_SIZE_LIMIT)
         CHEEVOS_VAR_LEN = CHEEVOS_SIZE_LIMIT;

      CHEEVOS_VAR_DATA = malloc(CHEEVOS_VAR_LEN);

      if (!CHEEVOS_VAR_DATA)
      {
         if ((void*)task)
            free((void*)task);
         if ((void*)coro)
            free((void*)coro);
         return false;
      }

      memcpy(CHEEVOS_VAR_DATA, info->data, CHEEVOS_VAR_LEN);
      CHEEVOS_VAR_PATH = NULL;
   }
   else
   {
      CHEEVOS_VAR_DATA = NULL;
      CHEEVOS_VAR_PATH = strdup(info->path);
   }

   task->handler   = cheevos_task_handler;
   task->state     = (void*)coro;
   task->mute      = true;
   task->callback  = NULL;
   task->user_data = NULL;
   task->progress  = 0;
   task->title     = NULL;

   task_queue_push(task);
   return true;
}
