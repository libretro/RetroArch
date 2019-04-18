/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2018 - Andre Leiradella
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
#ifdef HAVE_MENU_WIDGETS
#include "../menu/widgets/menu_widgets.h"
#endif
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "badges.h"
#include "cheevos.h"
#include "var.h"
#include "cond.h"

#include "../file_path_special.h"
#include "../paths.h"
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
#define CHEEVOS_JSON_KEY_SUCCESS      0x110461deU
#define CHEEVOS_JSON_KEY_ERROR        0x0d2011cfU

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
   retro_task_t* task;
#ifdef HAVE_THREADS
   slock_t*      task_lock;
#endif

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

typedef struct
{
   uint8_t id[4]; /* NES^Z */
   uint8_t rom_size;
   uint8_t vrom_size;
   uint8_t rom_type;
   uint8_t rom_type2;
   uint8_t reserve[8];
} cheevos_nes_header_t;

static cheevos_locals_t cheevos_locals =
{
   /* task                */ NULL,
#ifdef HAVE_THREADS
   /* task_lock           */ NULL,
#endif

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

bool cheevos_loaded = false;
bool cheevos_hardcore_active = false;
bool cheevos_hardcore_paused = false;
bool cheevos_state_loaded_flag = false;
int cheats_are_enabled = 0;
int cheats_were_enabled = 0;

#ifdef HAVE_THREADS
#define CHEEVOS_LOCK(l)   do { slock_lock(l); } while (0)
#define CHEEVOS_UNLOCK(l) do { slock_unlock(l); } while (0)
#else
#define CHEEVOS_LOCK(l)
#define CHEEVOS_UNLOCK(l)
#endif

/*****************************************************************************
Supporting functions.
*****************************************************************************/

#ifndef CHEEVOS_VERBOSE

void cheevos_log(const char *fmt, ...)
{
   (void)fmt;
}

#endif

static unsigned size_in_megabytes(unsigned val)
{
   return (val * 1024 * 1024);
}

#ifdef CHEEVOS_LOG_URLS
static void cheevos_log_url(const char* format, const char* url)
{
#ifdef CHEEVOS_LOG_PASSWORD
   CHEEVOS_LOG(format, url);
#else
   char copy[256];
   char* aux      = NULL;
   char* next     = NULL;

   if (!string_is_empty(url))
      strlcpy(copy, url, sizeof(copy));

   aux = strstr(copy, "?p=");

   if (!aux)
      aux = strstr(copy, "&p=");

   if (aux)
   {
      aux += 3;
      next = strchr(aux, '&');

      if (next)
      {
         do
         {
            *aux++ = *next++;
         }while (next[-1] != 0);
      }
      else
         *aux = 0;
   }

   aux = strstr(copy, "?t=");

   if (!aux)
      aux = strstr(copy, "&t=");

   if (aux)
   {
      aux += 3;
      next = strchr(aux, '&');

      if (next)
      {
         do
         {
            *aux++ = *next++;
         }while (next[-1] != 0);
      }
      else
         *aux = 0;
   }

   CHEEVOS_LOG(format, copy);
#endif
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

   if (ud)
      ud->is_key = cheevos_djb2(name, length) == ud->key_hash;
   return 0;
}

static int cheevos_getvalue__json_string(void *userdata,
      const char *string, size_t length)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   if (ud && ud->is_key)
   {
      ud->value  = string;
      ud->length = length;
      ud->is_key = 0;
   }

   return 0;
}

static int cheevos_getvalue__json_boolean(void *userdata, int istrue)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   if (ud && ud->is_key)
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

static int cheevos_getvalue__json_null(void *userdata)
{
   cheevos_getvalueud_t* ud = (cheevos_getvalueud_t*)userdata;

   if (ud && ud->is_key )
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
      if (!string_is_empty(ud.value))
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

  if (ud)
  {
     ud->in_cheevos     = 0;
     ud->in_lboards     = 0;
  }

  return 0;
}

static int cheevos_count__json_key(void *userdata,
      const char *name, size_t length)
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;

   if (ud)
   {
      ud->field_hash        = cheevos_djb2(name, length);
      if (ud->field_hash == CHEEVOS_JSON_KEY_ACHIEVEMENTS)
         ud->in_cheevos = 1;
      else if (ud->field_hash == CHEEVOS_JSON_KEY_LEADERBOARDS)
         ud->in_lboards = 1;
   }

   return 0;
}

static int cheevos_count__json_number(void *userdata,
      const char *number, size_t length)
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;

   if (ud)
   {
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
   }

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

static int cheevos_parse_condition(
      cheevos_condition_t *condition,
      const char* memaddr)
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

         CHEEVOS_LOG("[CHEEVOS]: set %p (index=%u)\n", condset, set);
         CHEEVOS_LOG("[CHEEVOS]:   conds: %u\n", condset->count);

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

   if (!condition)
      return;

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
   unsigned i;
   const char *aux;
   cheevos_term_t *terms  = NULL;
   char       *end        = NULL;

   if (!expr)
      return -1;

   expr->count            = 1;
   expr->compare_count    = 1;

   for (aux = mem;; aux++)
   {
      if (*aux == '"' || *aux == ':')
         break;
      expr->count += *aux == '_';
   }

   if (expr->count > 0)
      terms = (cheevos_term_t*)
         calloc(expr->count, sizeof(cheevos_term_t));

   if (!terms)
      return -1;

   expr->terms = terms;

   for (i = 0; i < expr->count; i++)
   {
      expr->terms[i].compare_next = false;
      expr->terms[i].multiplier   = 1;
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
         if (aux[1] == 'h' || aux[1] == 'H')
            expr->terms[i].multiplier = (double)strtol(aux + 2, &end, 16);
         else
            expr->terms[i].multiplier = strtod(aux + 1, &end);
         aux = end;

         if (*aux == '$')
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
   cheevo_t *cheevo    = NULL;
   long flags          = strtol(ud->flags.string, NULL, 10);

   if (flags == 3)
      cheevo           = cheevos_locals.core.cheevos + ud->core_count++;
   else if (flags == 5)
      cheevo           = cheevos_locals.unofficial.cheevos + ud->unofficial_count++;
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

   if (  !cheevo->title       ||
         !cheevo->description ||
         !cheevo->author      ||
         !cheevo->badge)
      goto error;

   if (cheevos_parse_condition(&cheevo->condition, ud->memaddr.string))
      goto error;

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
         snprintf(formatted_value, formatted_size,
               "%06upts", value);
         break;

      case CHEEVOS_FORMAT_FRAMES:
         mins   = value / 3600;
         secs   = (value % 3600) / 60;
         millis = (int) (value % 60) * (10.00 / 6.00);
         snprintf(formatted_value, formatted_size,
               "%02u:%02u.%02u", mins, secs, millis);
         break;

      case CHEEVOS_FORMAT_MILLIS:
         mins   = value / 6000;
         secs   = (value % 6000) / 100;
         millis = (int) (value % 100);
         snprintf(formatted_value, formatted_size,
               "%02u:%02u.%02u", mins, secs, millis);
         break;

      case CHEEVOS_FORMAT_SECS:
         mins   = value / 60;
         secs   = value % 60;
         snprintf(formatted_value, formatted_size,
               "%02u:%02u", mins, secs);
         break;

      default:
         snprintf(formatted_value, formatted_size,
               "%u (?)", value);
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
   cheevos_leaderboard_t *lboard = NULL;
   cheevos_leaderboard_t *ldb    = cheevos_locals.leaderboards;

   if (!ldb || !ud)
      return -1;

   lboard                        = ldb + ud->lboard_count++;

   lboard->id                    = (unsigned)strtol(ud->id.string, NULL, 10);
   lboard->format                = cheevos_parse_format(&ud->format);
   lboard->title                 = cheevos_dupstr(&ud->title);
   lboard->description           = cheevos_dupstr(&ud->desc);

   if (!lboard->title || !lboard->description)
      goto error;

   if (cheevos_parse_mem(lboard, ud->memaddr.string))
      goto error;

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

   if (ud)
   {
      int        common = ud->in_cheevos || ud->in_lboards;
      uint32_t     hash = cheevos_djb2(name, length);
      ud->field         = NULL;

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
   }

   return 0;
}

static int cheevos_read__json_string(void *userdata,
      const char *string, size_t length)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;

   if (ud && ud->field)
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

   if (ud)
   {
      if (ud->field)
      {
         ud->field->string = number;
         ud->field->length = length;
      }
      else if (ud->is_console_id)
      {
         cheevos_locals.console_id = (cheevos_console_t)
            strtol(number, NULL, 10);
         ud->is_console_id         = 0;
      }
   }

   return 0;
}

static int cheevos_read__json_end_object(void *userdata)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;

   if (ud)
   {
      if (ud->in_cheevos)
         return cheevos_new_cheevo(ud);
      if (ud->in_lboards)
         return cheevos_new_lboard(ud);
   }

   return 0;
}

static int cheevos_read__json_end_array(void *userdata)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;

   if (ud)
   {
      ud->in_cheevos    = 0;
      ud->in_lboards    = 0;
   }

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
   cheevos_locals.core.count   = core_count;

   cheevos_locals.unofficial.cheevos = (cheevo_t*)
      calloc(unofficial_count, sizeof(cheevo_t));
   cheevos_locals.unofficial.count   = unofficial_count;

   cheevos_locals.leaderboards = (cheevos_leaderboard_t*)
      calloc(lboard_count, sizeof(cheevos_leaderboard_t));
   cheevos_locals.lboard_count = lboard_count;

   if (   !cheevos_locals.core.cheevos       ||
          !cheevos_locals.unofficial.cheevos ||
          !cheevos_locals.leaderboards)
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
   unsigned sval = 0;
   unsigned tval = 0;

   if (!cond)
      return 0;

   sval          = cheevos_var_get_value(&cond->source) +
                   cheevos_locals.add_buffer;
   tval          = cheevos_var_get_value(&cond->target);

   switch (cond->op)
   {
      case CHEEVOS_COND_OP_EQUALS:
         return (sval == tval);
      case CHEEVOS_COND_OP_LESS_THAN:
         return (sval < tval);
      case CHEEVOS_COND_OP_LESS_THAN_OR_EQUAL:
         return (sval <= tval);
      case CHEEVOS_COND_OP_GREATER_THAN:
         return (sval > tval);
      case CHEEVOS_COND_OP_GREATER_THAN_OR_EQUAL:
         return (sval >= tval);
      case CHEEVOS_COND_OP_NOT_EQUAL_TO:
         return (sval != tval);
      default:
         break;
   }

   return 1;
}

static int cheevos_test_pause_cond_set(const cheevos_condset_t *condset,
      int *dirty_conds, int *reset_conds, int process_pause)
{
   int cond_valid            = 0;
   int set_valid             = 1; /* must start true so AND logic works */
   cheevos_cond_t *cond      = NULL;
   const cheevos_cond_t *end = condset->conds + condset->count;

   cheevos_locals.add_buffer = 0;
   cheevos_locals.add_hits   = 0;

   for (cond = condset->conds; cond < end; cond++)
   {
      if (cond->pause != process_pause)
         continue;

      if (cond->type == CHEEVOS_COND_TYPE_ADD_SOURCE)
      {
         cheevos_locals.add_buffer += cheevos_var_get_value(&cond->source);
         continue;
      }

      if (cond->type == CHEEVOS_COND_TYPE_SUB_SOURCE)
      {
         cheevos_locals.add_buffer -= cheevos_var_get_value(&cond->source);
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

      /* always evaluate the condition to ensure delta values get tracked correctly */
      cond_valid = cheevos_test_condition(cond);

      /* if the condition has a target hit count that has already been met,
       * it's automatically true, even if not currently true. */
      if (  (cond->req_hits != 0) &&
            (cond->curr_hits + cheevos_locals.add_hits) >= cond->req_hits)
      {
            cond_valid = 1;
      }
      else if (cond_valid)
      {
         cond->curr_hits++;
         *dirty_conds = 1;

         /* Process this logic, if this condition is true: */
         if (cond->req_hits == 0)
            ; /* Not a hit-based requirement: ignore any additional logic! */
         else if ((cond->curr_hits + cheevos_locals.add_hits) < cond->req_hits)
            cond_valid = 0; /* HitCount target has not yet been met, condition is not yet valid. */
      }

      cheevos_locals.add_buffer = 0;
      cheevos_locals.add_hits   = 0;

      if (cond->type == CHEEVOS_COND_TYPE_PAUSE_IF)
      {
         /* as soon as we find a PauseIf that evaluates to true,
          * stop processing the rest of the group. */
         if (cond_valid)
            return 1;

         /* if we make it to the end of the function, make sure we are
          * indicating nothing matched. if we do find a later PauseIf match,
          * it'll automatically return true via the previous condition. */
         set_valid = 0;

         if (cond->req_hits == 0)
         {
            /* PauseIf didn't evaluate true, and doesn't have a HitCount,
             * reset the HitCount to indicate the condition didn't match. */
            if (cond->curr_hits != 0)
            {
               cond->curr_hits = 0;
               *dirty_conds = 1;
            }
         }
         else
         {
            /* PauseIf has a HitCount that hasn't been met, ignore it for now. */
         }
      }
      else if (cond->type == CHEEVOS_COND_TYPE_RESET_IF)
      {
         if (cond_valid)
         {
            *reset_conds = 1; /* Resets all hits found so far */
            set_valid    = 0; /* Cannot be valid if we've hit a reset condition. */
         }
      }
      else /* Sequential or non-sequential? */
         set_valid &= cond_valid;
   }

   return set_valid;
}

static int cheevos_test_cond_set(const cheevos_condset_t *condset,
      int *dirty_conds, int *reset_conds)
{
   int in_pause              = 0;
   int has_pause             = 0;
   cheevos_cond_t *cond      = NULL;

   if (!condset)
      return 1; /* important: empty group must evaluate true */

   /* the ints below are used for Pause conditions and their dependent AddSource/AddHits. */

   /* this loop needs to go backwards to check AddSource/AddHits */
   cond = condset->conds + condset->count - 1;
   for (; cond >= condset->conds; cond--)
   {
      if (cond->type == CHEEVOS_COND_TYPE_PAUSE_IF)
      {
         has_pause = 1;
         in_pause = 1;
         cond->pause = 1;
      }
      else if (cond->type == CHEEVOS_COND_TYPE_ADD_SOURCE ||
               cond->type == CHEEVOS_COND_TYPE_SUB_SOURCE ||
               cond->type == CHEEVOS_COND_TYPE_ADD_HITS)
      {
         cond->pause = in_pause;
      }
      else
      {
         in_pause = 0;
         cond->pause = 0;
      }
   }

   if (has_pause)
   {  /* one or more Pause conditions exists, if any of them are true,
       * stop processing this group. */
      if (cheevos_test_pause_cond_set(condset, dirty_conds, reset_conds, 1))
         return 0;
   }

   /* process the non-Pause conditions to see if the group is true */
   return cheevos_test_pause_cond_set(condset, dirty_conds, reset_conds, 0);
}

static int cheevos_reset_cond_set(cheevos_condset_t *condset, int deltas)
{
   int dirty                 = 0;
   const cheevos_cond_t *end = NULL;

   if (!condset)
      return 0;

   end                       = condset->conds + condset->count;

   if (deltas)
   {
      cheevos_cond_t *cond      = NULL;
      for (cond = condset->conds; cond < end; cond++)
      {
         dirty                 |= cond->curr_hits != 0;

         cond->curr_hits        = 0;

         cond->source.previous  = cond->source.value;
         cond->target.previous  = cond->target.value;
      }
   }
   else
   {
      cheevos_cond_t *cond      = NULL;
      for (cond = condset->conds; cond < end; cond++)
      {
         dirty           |= cond->curr_hits != 0;
         cond->curr_hits  = 0;
      }
   }

   return dirty;
}

static int cheevos_test_cheevo(cheevo_t *cheevo)
{
   int dirty_conds              = 0;
   int reset_conds              = 0;
   int ret_val                  = 0;
   int ret_val_sub_cond         = 0;
   cheevos_condset_t *condset   = NULL;
   cheevos_condset_t *end       = NULL;

   if (!cheevo)
      return 0;

   ret_val_sub_cond             = cheevo->condition.count == 1;
   condset                      = cheevo->condition.condsets;

   if (!condset)
      return 0;

   end                          = condset + cheevo->condition.count;

   if (condset < end)
   {
      ret_val = cheevos_test_cond_set(condset, &dirty_conds, &reset_conds);
      condset++;
   }

   while (condset < end)
   {
      ret_val_sub_cond |= cheevos_test_cond_set(
            condset, &dirty_conds, &reset_conds);
      condset++;
   }

   if (dirty_conds)
      cheevo->dirty |= CHEEVOS_DIRTY_CONDITIONS;

   if (reset_conds)
   {
      int dirty = 0;

      for (condset = cheevo->condition.condsets; condset < end; condset++)
         dirty |= cheevos_reset_cond_set(condset, 0);

      if (dirty)
         cheevo->dirty |= CHEEVOS_DIRTY_CONDITIONS;
   }

   return (ret_val && ret_val_sub_cond);
}

static void cheevos_url_encode(const char *str, char *encoded, size_t len)
{
   if (!str)
      return;

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

static void cheevos_make_unlock_url(const cheevo_t *cheevo,
      char* url, size_t url_size)
{
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   snprintf(
      url, url_size,
      "http://retroachievements.org/dorequest.php?r=awardachievement&u=%s&t=%s&a=%u&h=%d",
      settings->arrays.cheevos_username,
      cheevos_locals.token,
      cheevo->id,
      settings->bools.cheevos_hardcore_mode_enable && !cheevos_hardcore_paused ? 1 : 0
   );

   url[url_size - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
   cheevos_log_url("[CHEEVOS]: url to award the cheevo: %s\n", url);
#endif
}

static void cheevos_unlocked(retro_task_t *task,
      void *task_data, void *user_data,
      const char *error)
{
   cheevo_t *cheevo = (cheevo_t *)user_data;

   if (!error)
   {
      CHEEVOS_LOG("[CHEEVOS]: awarded achievement %u.\n", cheevo->id);
   }
   else
   {
      char url[256];
      url[0] = '\0';

      CHEEVOS_ERR("[CHEEVOS]: error awarding achievement %u, retrying...\n", cheevo->id);

      cheevos_make_unlock_url(cheevo, url, sizeof(url));
      task_push_http_transfer(url, true, NULL, cheevos_unlocked, cheevo);
   }
}

static void cheevos_test_cheevo_set(const cheevoset_t *set)
{
   settings_t *settings = config_get_ptr();
   int mode             = CHEEVOS_ACTIVE_SOFTCORE;
   cheevo_t *cheevo     = NULL;
   const cheevo_t *end  = NULL;

   if (!set)
      return;

   end                  = set->cheevos + set->count;

   if (settings && settings->bools.cheevos_hardcore_mode_enable && !cheevos_hardcore_paused)
      mode = CHEEVOS_ACTIVE_HARDCORE;

   for (cheevo = set->cheevos; cheevo < end; cheevo++)
   {
      if (cheevo->active & mode)
      {
         int valid = cheevos_test_cheevo(cheevo);

         if (cheevo->last)
         {
            cheevos_condset_t* condset   = cheevo->condition.condsets;
            const cheevos_condset_t* end = cheevo->condition.condsets
               + cheevo->condition.count;

            for (; condset < end; condset++)
               cheevos_reset_cond_set(condset, 0);
         }
         else if (valid)
         {
            char url[256];
            url[0] = '\0';

            cheevo->active &= ~mode;

            if (mode == CHEEVOS_ACTIVE_HARDCORE)
               cheevo->active &= ~CHEEVOS_ACTIVE_SOFTCORE;

            CHEEVOS_LOG("[CHEEVOS]: awarding cheevo %u: %s (%s).\n",
                  cheevo->id, cheevo->title, cheevo->description);

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
            if (!video_driver_has_widgets() || !menu_widgets_push_achievement(cheevo->title, cheevo->badge))
#endif
            {
               char msg[256];
               msg[0] = '\0';
               snprintf(msg, sizeof(msg), "Achievement Unlocked: %s",
                     cheevo->title);
               msg[sizeof(msg) - 1] = 0;
               runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               runloop_msg_queue_push(cheevo->description, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }

            cheevos_make_unlock_url(cheevo, url, sizeof(url));
            task_push_http_transfer(url, true, NULL,
                  cheevos_unlocked, cheevo);

            if (settings && settings->bools.cheevos_auto_screenshot)
            {
               char shotname[4200];

               snprintf(shotname, sizeof(shotname), "%s/%s-cheevo-%u",
                  settings->paths.directory_screenshot,
                  path_basename(path_get(RARCH_PATH_BASENAME)),
                  cheevo->id);
               shotname[sizeof(shotname) - 1] = '\0';

               if (take_screenshot(shotname, true,
                        video_driver_cached_frame_has_valid_framebuffer(), false, true))
                  CHEEVOS_LOG("[CHEEVOS]: got a screenshot for cheevo %u\n", cheevo->id);
               else
                  CHEEVOS_LOG("[CHEEVOS]: failed to get screenshot for cheevo %u\n", cheevo->id);
            }
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
   int ret_val_sub_cond         = 0;
   cheevos_condset_t *condset   = NULL;
   const cheevos_condset_t *end = NULL;

   if (!condition)
      return 0;

   ret_val_sub_cond             = condition->count == 1;
   condset                      = condition->condsets;
   end                          = condset + condition->count;

   if (condset < end)
   {
      ret_val = cheevos_test_cond_set(
            condset, &dirty_conds, &reset_conds);
      condset++;
   }

   while (condset < end)
   {
      ret_val_sub_cond |= cheevos_test_cond_set(
            condset, &dirty_conds, &reset_conds);
      condset++;
   }

   if (reset_conds)
   {
      for (condset = condition->condsets; condset < end; condset++)
         cheevos_reset_cond_set(condset, 0);
   }

   return (ret_val && ret_val_sub_cond);
}

static int cheevos_expr_value(cheevos_expr_t* expr)
{
   unsigned i;
   int values[16];
   /* Separate possible values with '$' operator, submit the largest */
   unsigned current_value = 0;
   cheevos_term_t* term   = NULL;

   if (!expr)
      return 0;

   term                   = expr->terms;

   if (!term)
      return 0;

   if (expr->compare_count >=  ARRAY_SIZE(values))
   {
      CHEEVOS_ERR("[CHEEVOS]: too many values in the leaderboard expression: %u\n", expr->compare_count);
      return 0;
   }

   memset(values, 0, sizeof values);

   for (i = expr->count; i != 0; i--, term++)
   {
      if (current_value >= ARRAY_SIZE(values))
      {
         CHEEVOS_ERR("[CHEEVOS]: too many values in the leaderboard expression: %u\n", current_value);
         return 0;
      }

      values[current_value] +=
         cheevos_var_get_value(&term->var) * term->multiplier;

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
   MD5_CTX ctx;
   uint8_t hash[16];
   char signature[64];
   settings_t *settings = config_get_ptr();

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

static void cheevos_lboard_submit(retro_task_t *task,
      void *task_data, void *user_data,
      const char *error)
{
   cheevos_leaderboard_t *lboard = (cheevos_leaderboard_t *)user_data;

   if (!lboard)
      return;

   if (!error)
   {
      CHEEVOS_ERR("[CHEEVOS]: error submitting leaderboard %u\n", lboard->id);
      return;
   }

   CHEEVOS_LOG("[CHEEVOS]: submitted leaderboard %u.\n", lboard->id);
}

static void cheevos_test_leaderboards(void)
{
   unsigned i;
   cheevos_leaderboard_t* lboard = cheevos_locals.leaderboards;

   if (!lboard)
      return;

   for (i = cheevos_locals.lboard_count; i != 0; i--, lboard++)
   {
      if (lboard->active)
      {
         int value = cheevos_expr_value(&lboard->value);

         if (value != lboard->last_value)
         {
            CHEEVOS_LOG("[CHEEVOS]: value lboard  %s %u\n",
                  lboard->title, value);
            lboard->last_value = value;
         }

         if (cheevos_test_lboard_condition(&lboard->submit))
         {
            lboard->active = 0;

            /* failsafe for improper LBs */
            if (value == 0)
            {
               CHEEVOS_LOG("[CHEEVOS]: error: lboard %s tried to submit 0\n",
                     lboard->title);
               runloop_msg_queue_push("Leaderboard attempt cancelled!",
                     0, 2 * 60, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
            else
            {
               char url[256];
               char msg[256];
               char formatted_value[16];

               cheevos_make_lboard_url(lboard, url, sizeof(url));
               task_push_http_transfer(url, true, NULL,
                     cheevos_lboard_submit, lboard);
               CHEEVOS_LOG("[CHEEVOS]: submit lboard %s\n", lboard->title);

               cheevos_format_value(value, lboard->format,
                     formatted_value, sizeof(formatted_value));
               snprintf(msg, sizeof(msg), "Submitted %s for %s",
                     formatted_value, lboard->title);
               msg[sizeof(msg) - 1] = 0;
               runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
         }

         if (cheevos_test_lboard_condition(&lboard->cancel))
         {
            CHEEVOS_LOG("[CHEEVOS]: cancel lboard %s\n", lboard->title);
            lboard->active = 0;
            runloop_msg_queue_push("Leaderboard attempt cancelled!",
                  0, 2 * 60, false,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }
      else
      {
         if (cheevos_test_lboard_condition(&lboard->start))
         {
            char msg[256];

            CHEEVOS_LOG("[CHEEVOS]: start lboard  %s\n", lboard->title);
            lboard->active     = 1;
            lboard->last_value = -1;

            snprintf(msg, sizeof(msg),
                  "Leaderboard Active: %s", lboard->title);
            msg[sizeof(msg) - 1] = 0;
            runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            runloop_msg_queue_push(lboard->description, 0, 3*60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }
   }
}

/*****************************************************************************
Free the loaded achievements.
*****************************************************************************/

static void cheevos_free_condset(const cheevos_condset_t *set)
{
   if (set && set->conds)
      free((void*)set->conds);
}

static void cheevos_free_cheevo(const cheevo_t *cheevo)
{
   if (!cheevo)
      return;

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
   const cheevo_t *cheevo = NULL;
   const cheevo_t *end    = NULL;

   if (!set)
      return;

   cheevo                 = set->cheevos;
   end                    = cheevo + set->count;

   while (cheevo < end)
      cheevos_free_cheevo(cheevo++);

   if (set->cheevos)
      free((void*)set->cheevos);
}

#ifndef CHEEVOS_DONT_DEACTIVATE
static int cheevos_deactivate__json_index(void *userdata, unsigned int index)
{
   cheevos_deactivate_t *ud = (cheevos_deactivate_t*)userdata;

   if (ud)
      ud->is_element        = 1;

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

   if (ud && ud->is_element)
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
         CHEEVOS_LOG("[CHEEVOS]: deactivated unlocked cheevo %u (%s).\n",
               cheevo->id, cheevo->title);
      else
         CHEEVOS_ERR("[CHEEVOS]: unknown cheevo to deactivate: %u.\n", id);
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
   ud.mode       = mode;
   return jsonsax_parse(json, &handlers, (void*)&ud) != JSONSAX_OK;
}
#endif

void cheevos_reset_game(void)
{
   cheevo_t *cheevo;
   cheevos_leaderboard_t *lboard;
   unsigned i;

   cheevo = cheevos_locals.core.cheevos;
   for (i = 0; i < cheevos_locals.core.count; i++, cheevo++)
      cheevo->last     = 1;

   cheevo = cheevos_locals.unofficial.cheevos;
   for (i = 0; i < cheevos_locals.unofficial.count; i++, cheevo++)
      cheevo->last = 1;

   lboard = cheevos_locals.leaderboards;
   for (i = 0; i < cheevos_locals.lboard_count; i++, lboard++)
      lboard->active = 0;
}

void cheevos_populate_menu(void *data)
{
#ifdef HAVE_MENU
   unsigned i                    = 0;
   unsigned items_found          = 0;
   settings_t *settings          = config_get_ptr();
   menu_displaylist_info_t *info = (menu_displaylist_info_t*)data;
   cheevo_t *end                 = NULL;
   cheevo_t *cheevo              = cheevos_locals.core.cheevos;
   end                           = cheevo + cheevos_locals.core.count;

   if(settings->bools.cheevos_enable && settings->bools.cheevos_hardcore_mode_enable
      && cheevos_loaded)
   {
      if (!cheevos_hardcore_paused)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE,
               MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS, 0, 0);
      else
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_RESUME),
               MENU_ENUM_LABEL_ACHIEVEMENT_RESUME,
               MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS, 0, 0);
   }

   if (cheevo)
   {
      for (i = 0; cheevo < end; i++, cheevo++)
      {
         if (!(cheevo->active & CHEEVOS_ACTIVE_HARDCORE))
         {
            menu_entries_append_enum(info->list, cheevo->title,
               cheevo->description,
               MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
               MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
            set_badge_info(&badges_ctx, i, cheevo->badge,
               (cheevo->active & CHEEVOS_ACTIVE_HARDCORE));
         }
         else if (!(cheevo->active & CHEEVOS_ACTIVE_SOFTCORE))
         {
            menu_entries_append_enum(info->list, cheevo->title,
               cheevo->description,
               MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY,
               MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
            set_badge_info(&badges_ctx, i, cheevo->badge,
               (cheevo->active & CHEEVOS_ACTIVE_SOFTCORE));
         }
         else
         {
            menu_entries_append_enum(info->list, cheevo->title,
               cheevo->description,
               MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
               MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
            set_badge_info(&badges_ctx, i, cheevo->badge,
               (cheevo->active & CHEEVOS_ACTIVE_SOFTCORE));
         }
         items_found++;
      }
   }

   cheevo = cheevos_locals.unofficial.cheevos;

   if (cheevo && settings->bools.cheevos_test_unofficial)
   {
      end    = cheevo + cheevos_locals.unofficial.count;

      for (i = items_found; cheevo < end; i++, cheevo++)
      {
         if (!(cheevo->active & CHEEVOS_ACTIVE_HARDCORE))
         {
            menu_entries_append_enum(info->list, cheevo->title,
               cheevo->description,
               MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
               MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
            set_badge_info(&badges_ctx, i, cheevo->badge,
                  (cheevo->active & CHEEVOS_ACTIVE_HARDCORE));
         }
         else if (!(cheevo->active & CHEEVOS_ACTIVE_SOFTCORE))
         {
            menu_entries_append_enum(info->list, cheevo->title,
               cheevo->description,
               MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY,
               MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
            set_badge_info(&badges_ctx, i, cheevo->badge,
                  (cheevo->active & CHEEVOS_ACTIVE_SOFTCORE));
         }
         else
         {
            menu_entries_append_enum(info->list, cheevo->title,
               cheevo->description,
               MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
               MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
            set_badge_info(&badges_ctx, i, cheevo->badge,
                  (cheevo->active & CHEEVOS_ACTIVE_SOFTCORE));
         }
         items_found++;
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
   if (!desc)
      return false;

   if (cheevos_loaded)
   {
      cheevo_t *cheevos = cheevos_locals.core.cheevos;

      if (!cheevos)
         return false;

      if (desc->idx >= cheevos_locals.core.count)
      {
         cheevos    = cheevos_locals.unofficial.cheevos;
         desc->idx -= cheevos_locals.core.count;
      }

      if (!string_is_empty(cheevos[desc->idx].description))
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
   bool running;
   CHEEVOS_LOCK(cheevos_locals.task_lock);
   running = cheevos_locals.task != NULL;
   CHEEVOS_UNLOCK(cheevos_locals.task_lock);

   if (running)
   {
      CHEEVOS_LOG("[CHEEVOS]: Asked the load thread to terminate\n");
      task_queue_cancel_task(cheevos_locals.task);

#ifdef HAVE_THREADS
      do
      {
         CHEEVOS_LOCK(cheevos_locals.task_lock);
         running = cheevos_locals.task != NULL;
         CHEEVOS_UNLOCK(cheevos_locals.task_lock);
      }
      while (running);
#endif
   }

   if (cheevos_loaded)
   {
      cheevos_free_cheevo_set(&cheevos_locals.core);
      cheevos_free_cheevo_set(&cheevos_locals.unofficial);
   }

   cheevos_locals.core.cheevos       = NULL;
   cheevos_locals.unofficial.cheevos = NULL;
   cheevos_locals.core.count         = 0;
   cheevos_locals.unofficial.count   = 0;

   cheevos_loaded     = false;
   cheevos_hardcore_paused = false;

   return true;
}

bool cheevos_toggle_hardcore_mode(void)
{
   settings_t *settings = config_get_ptr();

   if (!settings)
      return false;

   /* reset and deinit rewind to avoid cheat the score */
   if (settings->bools.cheevos_hardcore_mode_enable && !cheevos_hardcore_paused)
   {
      const char *msg = msg_hash_to_str(
            MSG_CHEEVOS_HARDCORE_MODE_ENABLE);

      /* send reset core cmd to avoid any user
       * savestate previusly loaded. */
      command_event(CMD_EVENT_RESET, NULL);

      if (settings->bools.rewind_enable)
         command_event(CMD_EVENT_REWIND_DEINIT, NULL);

      CHEEVOS_LOG("%s\n", msg);
      runloop_msg_queue_push(msg, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
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
   unsigned i;
   cheevo_t* cheevo = NULL;

   if (!set)
      return;

   cheevo           = set->cheevos;

   if (!cheevo)
      return;

   for (i = set->count; i != 0; i--, cheevo++)
   {
      unsigned j;
      cheevos_condset_t* condset = cheevo->condition.condsets;

      for (j = cheevo->condition.count; j != 0; j--, condset++)
      {
         unsigned k;
         cheevos_cond_t* cond = condset->conds;

         for (k = condset->count; k != 0; k--, cond++)
         {
            switch (cond->source.type)
            {
               case CHEEVOS_VAR_TYPE_ADDRESS:
               case CHEEVOS_VAR_TYPE_DELTA_MEM:
                  cheevos_var_patch_addr(&cond->source,
                        cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
                  CHEEVOS_LOG("[CHEEVOS]: s-var %03d:%08X\n",
                        cond->source.bank_id + 1, cond->source.value);
#endif
                  break;

               default:
                  break;
            }

            switch (cond->target.type)
            {
               case CHEEVOS_VAR_TYPE_ADDRESS:
               case CHEEVOS_VAR_TYPE_DELTA_MEM:
                  cheevos_var_patch_addr(&cond->target,
                        cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
                  CHEEVOS_LOG("[CHEEVOS]: t-var %03d:%08X\n",
                        cond->target.bank_id + 1, cond->target.value);
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
   unsigned i;
   cheevos_condset_t* condset = NULL;

   if (!condition)
      return;

   condset                    = condition->condsets;

   for (i = condition->count; i != 0; i--, condset++)
   {
      unsigned j;
      cheevos_cond_t* cond = condset->conds;

      for (j = condset->count; j != 0; j--, cond++)
      {
         switch (cond->source.type)
         {
            case CHEEVOS_VAR_TYPE_ADDRESS:
            case CHEEVOS_VAR_TYPE_DELTA_MEM:
               cheevos_var_patch_addr(&cond->source,
                     cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
               CHEEVOS_LOG("[CHEEVOS]: s-var %03d:%08X\n",
                     cond->source.bank_id + 1, cond->source.value);
#endif
               break;
            default:
               break;
         }
         switch (cond->target.type)
         {
            case CHEEVOS_VAR_TYPE_ADDRESS:
            case CHEEVOS_VAR_TYPE_DELTA_MEM:
               cheevos_var_patch_addr(&cond->target,
                     cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
               CHEEVOS_LOG("[CHEEVOS]: t-var %03d:%08X\n",
                     cond->target.bank_id + 1, cond->target.value);
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
   cheevos_term_t* term = NULL;

   if (!expression)
      return;

   term                 = expression->terms;

   for (i = expression->count; i != 0; i--, term++)
   {
      switch (term->var.type)
      {
         case CHEEVOS_VAR_TYPE_ADDRESS:
         case CHEEVOS_VAR_TYPE_DELTA_MEM:
            cheevos_var_patch_addr(&term->var, cheevos_locals.console_id);
#ifdef CHEEVOS_DUMP_ADDRS
            CHEEVOS_LOG("[CHEEVOS]: s-var %03d:%08X\n",
                  term->var.bank_id + 1, term->var.value);
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

   for (i = 0; i < cheevos_locals.lboard_count; i++)
   {
      cheevos_condition_t *start  = &leaderboard[i].start;
      cheevos_condition_t *cancel = &leaderboard[i].cancel;
      cheevos_condition_t *submit = &leaderboard[i].submit;
      cheevos_expr_t *value       = &leaderboard[i].value;

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

   if (settings)
   {
      if (settings->bools.cheevos_test_unofficial)
         cheevos_test_cheevo_set(&cheevos_locals.unofficial);

      if (settings->bools.cheevos_hardcore_mode_enable &&
          settings->bools.cheevos_leaderboards_enable  &&
          !cheevos_hardcore_paused)
         cheevos_test_leaderboards();
   }
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

#include "coro.h"

/* Uncomment the following two lines to debug cheevos_iterate, this will
 * disable the coroutine yielding.
 *
 * The code is very easy to understand. It's meant to be like BASIC:
 * CORO_GOTO will jump execution to another label, CORO_GOSUB will
 * call another label, and CORO_RET will return from a CORO_GOSUB.
 *
 * This coroutine code is inspired in a very old pure C implementation
 * that runs everywhere:
 *
 * https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
 */
/*#undef CORO_YIELD
#define CORO_YIELD()*/

typedef struct
{
   /* variables used in the co-routine */
   char badge_name[16];
   char url[256];
   char badge_basepath[PATH_MAX_LENGTH];
   char badge_fullpath[PATH_MAX_LENGTH];
   unsigned char hash[16];
   bool round;
   unsigned gameid;
   unsigned i;
   unsigned j;
   unsigned k;
   size_t bytes;
   size_t count;
   size_t offset;
   size_t len;
   size_t size;
   MD5_CTX md5;
   cheevos_nes_header_t header;
   retro_time_t t0;
   struct retro_system_info sysinfo;
   void *data;
   char *json;
   const char *path;
   const char *ext;
   intfstream_t *stream;
   cheevo_t *cheevo;
   settings_t *settings;
   struct http_connection_t *conn;
   struct http_t *http;
   const cheevo_t *cheevo_end;

   /* co-routine required fields */
   CORO_FIELDS
} coro_t;

enum
{
   /* Negative values because CORO_SUB generates positive values */
   SNES_MD5     = -1,
   GENESIS_MD5  = -2,
   LYNX_MD5     = -3,
   NES_MD5      = -4,
   GENERIC_MD5  = -5,
   FILENAME_MD5 = -6,
   EVAL_MD5     = -7,
   FILL_MD5     = -8,
   GET_GAMEID   = -9,
   GET_CHEEVOS  = -10,
   GET_BADGES   = -11,
   LOGIN        = -12,
   HTTP_GET     = -13,
   DEACTIVATE   = -14,
   PLAYING      = -15,
   DELAY        = -16
};

static int cheevos_iterate(coro_t *coro)
{
   const int snes_header_len = 0x200;
   const int lynx_header_len = 0x40;
   ssize_t num_read          = 0;
   size_t to_read            = 4096;
   uint8_t *buffer           = NULL;
   const char *end           = NULL;

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
      {SNES_MD5,    "SNES (discards header)",            snes_exts},
      {GENESIS_MD5, "Genesis (6Mb padding)",             genesis_exts},
      {LYNX_MD5,    "Atari Lynx (discards header)",      lynx_exts},
      {NES_MD5,     "NES (discards header)",             NULL},
      {GENERIC_MD5, "Generic (plain content)",           NULL},
      {FILENAME_MD5, "Generic (filename)",               NULL}
   };

   CORO_ENTER();

      cheevos_locals.addrs_patched = false;

      coro->settings               = config_get_ptr();

      cheevos_locals.meminfo[0].id = RETRO_MEMORY_SYSTEM_RAM;
      core_get_memory(&cheevos_locals.meminfo[0]);

      cheevos_locals.meminfo[1].id = RETRO_MEMORY_SAVE_RAM;
      core_get_memory(&cheevos_locals.meminfo[1]);

      cheevos_locals.meminfo[2].id = RETRO_MEMORY_VIDEO_RAM;
      core_get_memory(&cheevos_locals.meminfo[2]);

      cheevos_locals.meminfo[3].id = RETRO_MEMORY_RTC;
      core_get_memory(&cheevos_locals.meminfo[3]);

      CHEEVOS_LOG("[CHEEVOS]: system RAM: %p %u\n",
            cheevos_locals.meminfo[0].data,
            cheevos_locals.meminfo[0].size);
      CHEEVOS_LOG("[CHEEVOS]: save RAM:   %p %u\n",
            cheevos_locals.meminfo[1].data,
            cheevos_locals.meminfo[1].size);
      CHEEVOS_LOG("[CHEEVOS]: video RAM:  %p %u\n",
            cheevos_locals.meminfo[2].data,
            cheevos_locals.meminfo[2].size);
      CHEEVOS_LOG("[CHEEVOS]: RTC:        %p %u\n",
            cheevos_locals.meminfo[3].data,
            cheevos_locals.meminfo[3].size);

      /* Bail out if cheevos are disabled.
         * But set the above anyways,
         * command_read_ram needs it. */
      if (!coro->settings->bools.cheevos_enable)
         CORO_STOP();

      /* Load the content into memory, or copy it
         * over to our own buffer */
      if (!coro->data)
      {
         coro->stream = intfstream_open_file(
               coro->path,
               RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE);

         if (!coro->stream)
            CORO_STOP();

         CORO_YIELD();
         coro->len         = 0;
         coro->count       = intfstream_get_size(coro->stream);

         /* size limit */
         if (coro->count > size_in_megabytes(64))
            coro->count    = size_in_megabytes(64);

         coro->data        = malloc(coro->count);

         if (!coro->data)
         {
            intfstream_close(coro->stream);
            free(coro->stream);
            CORO_STOP();
         }

         for (;;)
         {
            buffer   = (uint8_t*)coro->data + coro->len;
            to_read  = 4096;

            if (to_read > coro->count)
               to_read = coro->count;

            num_read = intfstream_read(coro->stream,
                  (void*)buffer, to_read);

            if (num_read <= 0)
               break;

            coro->len         += num_read;
            coro->count       -= num_read;

            if (coro->count == 0)
               break;

            CORO_YIELD();
         }

         intfstream_close(coro->stream);
         free(coro->stream);
      }

      /* Use the supported extensions as a hint
         * to what method we should use. */
      core_get_system_info(&coro->sysinfo);

      for (coro->i = 0; coro->i < ARRAY_SIZE(finders); coro->i++)
      {
         if (finders[coro->i].ext_hashes)
         {
            coro->ext = coro->sysinfo.valid_extensions;

            while (coro->ext)
            {
               unsigned hash;
               end = strchr(coro->ext, '|');

               if (end)
               {
                  hash      = cheevos_djb2(
                        coro->ext, end - coro->ext);
                  coro->ext = end + 1;
               }
               else
               {
                  hash      = cheevos_djb2(
                        coro->ext, strlen(coro->ext));
                  coro->ext = NULL;
               }

               for (coro->j = 0; finders[coro->i].ext_hashes[coro->j]; coro->j++)
               {
                  if (finders[coro->i].ext_hashes[coro->j] == hash)
                  {
                     CHEEVOS_LOG("[CHEEVOS]: testing %s.\n",
                           finders[coro->i].name);

                     /*
                        * Inputs:  CHEEVOS_VAR_INFO
                        * Outputs: CHEEVOS_VAR_GAMEID, the game was found if it's different from 0
                        */
                     CORO_GOSUB(finders[coro->i].label);

                     if (coro->gameid != 0)
                        goto found;

                     coro->ext = NULL; /* force next finder */
                     break;
                  }
               }
            }
         }
      }

      for (coro->i = 0; coro->i < ARRAY_SIZE(finders); coro->i++)
      {
         if (finders[coro->i].ext_hashes)
            continue;

         CHEEVOS_LOG("[CHEEVOS]: testing %s.\n",
               finders[coro->i].name);

         /*
            * Inputs:  CHEEVOS_VAR_INFO
            * Outputs: CHEEVOS_VAR_GAMEID
            */
         CORO_GOSUB(finders[coro->i].label);

         if (coro->gameid != 0)
            goto found;
      }

      CHEEVOS_LOG("[CHEEVOS]: this game doesn't feature achievements.\n");
      CORO_STOP();

found:

#ifdef CHEEVOS_JSON_OVERRIDE
      {
         size_t size = 0;
         FILE *file  = fopen(CHEEVOS_JSON_OVERRIDE, "rb");

         fseek(file, 0, SEEK_END);
         size = ftell(file);
         fseek(file, 0, SEEK_SET);

         coro->json = (char*)malloc(size + 1);
         fread((void*)coro->json, 1, size, file);

         fclose(file);
         coro->json[size] = 0;
      }
#else
      CORO_GOSUB(GET_CHEEVOS);

      if (!coro->json)
      {
         runloop_msg_queue_push("Error loading achievements.", 0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         CHEEVOS_ERR("[CHEEVOS]: error loading achievements.\n");
         CORO_STOP();
      }
#endif

#ifdef CHEEVOS_SAVE_JSON
      {
         FILE *file = fopen(CHEEVOS_SAVE_JSON, "w");
         fwrite((void*)coro->json, 1, strlen(coro->json), file);
         fclose(file);
      }
#endif
      if (cheevos_parse(coro->json))
      {
         if ((void*)coro->json)
            free((void*)coro->json);
         CORO_STOP();
      }

      if ((void*)coro->json)
         free((void*)coro->json);

      if (   cheevos_locals.core.count == 0
            && cheevos_locals.unofficial.count == 0
            && cheevos_locals.lboard_count == 0)
      {
         runloop_msg_queue_push(
               "This game has no achievements.",
               0, 5 * 60, false,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         cheevos_free_cheevo_set(&cheevos_locals.core);
         cheevos_free_cheevo_set(&cheevos_locals.unofficial);

         cheevos_locals.core.cheevos       = NULL;
         cheevos_locals.unofficial.cheevos = NULL;
         cheevos_locals.core.count         = 0;
         cheevos_locals.unofficial.count   = 0;

         cheevos_loaded     = false;
         cheevos_hardcore_paused = false;
         CORO_STOP();
      }

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

      if (coro->settings->bools.cheevos_verbose_enable && cheevos_locals.core.count > 0)
      {
         char msg[256];
         int mode                     = CHEEVOS_ACTIVE_SOFTCORE;
         const cheevo_t* cheevo       = cheevos_locals.core.cheevos;
         const cheevo_t* end          = cheevo + cheevos_locals.core.count;
         int number_of_unlocked       = cheevos_locals.core.count;

         if (coro->settings->bools.cheevos_hardcore_mode_enable && !cheevos_hardcore_paused)
            mode = CHEEVOS_ACTIVE_HARDCORE;

         for (; cheevo < end; cheevo++)
            if (cheevo->active & mode)
               number_of_unlocked--;

         snprintf(msg, sizeof(msg),
               "You have %d of %d achievements unlocked.",
               number_of_unlocked, cheevos_locals.core.count);
         msg[sizeof(msg) - 1] = 0;
         runloop_msg_queue_push(msg, 0, 6 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      CORO_GOSUB(GET_BADGES);
      CORO_STOP();

      /**************************************************************************
       * Info   Tries to identify a SNES game
         * Input  CHEEVOS_VAR_INFO the content info
         * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
         *************************************************************************/
   CORO_SUB(SNES_MD5)

      MD5_Init(&coro->md5);

      /* Checks for the existence of a headered SNES file.
         Unheadered files fall back to GENERIC_MD5. */

      if (coro->len < 0x2000 || coro->len % 0x2000 != snes_header_len)
      {
          coro->gameid = 0;
          CORO_RET();
      }

      coro->offset = 512;
      coro->count  = 0;

      CORO_GOSUB(EVAL_MD5);
      MD5_Final(coro->hash, &coro->md5);

      CORO_GOTO(GET_GAMEID);

      /**************************************************************************
       * Info   Tries to identify a Genesis game
         * Input  CHEEVOS_VAR_INFO the content info
         * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
         *************************************************************************/
   CORO_SUB(GENESIS_MD5)

      MD5_Init(&coro->md5);

      coro->offset = 0;
      coro->count  = 0;
      CORO_GOSUB(EVAL_MD5);

      if (coro->count == 0)
      {
         MD5_Final(coro->hash, &coro->md5);
         coro->gameid = 0;
         CORO_RET();
      }

      if (coro->count < size_in_megabytes(6))
      {
         coro->offset = 0;
         coro->count  = size_in_megabytes(6) - coro->count;
         CORO_GOSUB(FILL_MD5);
      }

      MD5_Final(coro->hash, &coro->md5);
      CORO_GOTO(GET_GAMEID);

      /**************************************************************************
       * Info   Tries to identify an Atari Lynx game
         * Input  CHEEVOS_VAR_INFO the content info
         * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
         *************************************************************************/
   CORO_SUB(LYNX_MD5)

      /* Checks for the existence of a headered Lynx file.
         Unheadered files fall back to GENERIC_MD5. */

      if (coro->len <= (unsigned)lynx_header_len ||
        memcmp("LYNX", (void *)coro->data, 5) != 0)
      {
         coro->gameid = 0;
         CORO_RET();
      }

      MD5_Init(&coro->md5);
      coro->offset = lynx_header_len;
      coro->count  = coro->len - lynx_header_len;
      CORO_GOSUB(EVAL_MD5);

      MD5_Final(coro->hash, &coro->md5);
      CORO_GOTO(GET_GAMEID);

      /**************************************************************************
       * Info   Tries to identify a NES game
         * Input  CHEEVOS_VAR_INFO the content info
         * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
         *************************************************************************/
   CORO_SUB(NES_MD5)

      /* Checks for the existence of a headered NES file.
         Unheadered files fall back to GENERIC_MD5. */

      if (coro->len < sizeof(coro->header))
      {
         coro->gameid = 0;
         CORO_RET();
      }

      memcpy((void*)&coro->header, coro->data,
            sizeof(coro->header));

      if (     coro->header.id[0] != 'N'
            || coro->header.id[1] != 'E'
            || coro->header.id[2] != 'S'
            || coro->header.id[3] != 0x1a)
      {
         coro->gameid = 0;
         CORO_RET();
      }

      MD5_Init(&coro->md5);
      coro->offset = sizeof(coro->header);
      coro->count  = coro->len - coro->offset;
      CORO_GOSUB(EVAL_MD5);

      MD5_Final(coro->hash, &coro->md5);
      CORO_GOTO(GET_GAMEID);

      /**************************************************************************
       * Info   Tries to identify a "generic" game
         * Input  CHEEVOS_VAR_INFO the content info
         * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
         *************************************************************************/
   CORO_SUB(GENERIC_MD5)

      MD5_Init(&coro->md5);

      coro->offset      = 0;
      coro->count       = 0;
      CORO_GOSUB(EVAL_MD5);

      MD5_Final(coro->hash, &coro->md5);

      if (coro->count == 0)
         CORO_RET();

      CORO_GOTO(GET_GAMEID);

      /**************************************************************************
       * Info  Tries to identify a game based on its filename (with no extension)
         * Input  CHEEVOS_VAR_INFO the content info
         * Output CHEEVOS_VAR_GAMEID the Retro Achievements game ID, or 0 if not found
         *************************************************************************/
   CORO_SUB(FILENAME_MD5)
      if (!string_is_empty(coro->path))
      {
         char base_noext[PATH_MAX_LENGTH];
         fill_pathname_base_noext(base_noext, coro->path, sizeof(base_noext));

         MD5_Init(&coro->md5);
         MD5_Update(&coro->md5, (void*)base_noext, strlen(base_noext));
         MD5_Final(coro->hash, &coro->md5);

         CORO_GOTO(GET_GAMEID);
      }
      CORO_RET();

      /**************************************************************************
       * Info    Evaluates the CHEEVOS_VAR_MD5 hash
         * Inputs  CHEEVOS_VAR_INFO, CHEEVOS_VAR_OFFSET, CHEEVOS_VAR_COUNT
         * Outputs CHEEVOS_VAR_MD5, CHEEVOS_VAR_COUNT
         *************************************************************************/
   CORO_SUB(EVAL_MD5)

      if (coro->count == 0)
         coro->count = coro->len;

      if (coro->len - coro->offset < coro->count)
         coro->count = coro->len - coro->offset;

      /* size limit */
      if (coro->count > size_in_megabytes(64))
         coro->count = size_in_megabytes(64);

      MD5_Update(&coro->md5,
            (void*)((uint8_t*)coro->data + coro->offset),
            coro->count);
      CORO_RET();

      /**************************************************************************
       * Info    Updates the CHEEVOS_VAR_MD5 hash with a repeated value
         * Inputs  CHEEVOS_VAR_OFFSET, CHEEVOS_VAR_COUNT
         * Outputs CHEEVOS_VAR_MD5
         *************************************************************************/
   CORO_SUB(FILL_MD5)

      {
         char buffer[4096];

         while (coro->count > 0)
         {
            size_t len = sizeof(buffer);

            if (len > coro->count)
               len = coro->count;

            memset((void*)buffer, coro->offset, len);
            MD5_Update(&coro->md5, (void*)buffer, len);
            coro->count -= len;
         }
      }

      CORO_RET();

      /**************************************************************************
       * Info    Gets the achievements from Retro Achievements
         * Inputs  coro->hash
         * Outputs CHEEVOS_VAR_GAMEID
         *************************************************************************/
   CORO_SUB(GET_GAMEID)

      {
         char gameid[16];

         CHEEVOS_LOG(
               "[CHEEVOS]: getting game id for hash %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
               coro->hash[ 0], coro->hash[ 1], coro->hash[ 2], coro->hash[ 3],
               coro->hash[ 4], coro->hash[ 5], coro->hash[ 6], coro->hash[ 7],
               coro->hash[ 8], coro->hash[ 9], coro->hash[10], coro->hash[11],
               coro->hash[12], coro->hash[13], coro->hash[14], coro->hash[15]
               );

         snprintf(
               coro->url, sizeof(coro->url),
               "http://retroachievements.org/dorequest.php?r=gameid&m=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               coro->hash[ 0], coro->hash[ 1], coro->hash[ 2], coro->hash[ 3],
               coro->hash[ 4], coro->hash[ 5], coro->hash[ 6], coro->hash[ 7],
               coro->hash[ 8], coro->hash[ 9], coro->hash[10], coro->hash[11],
               coro->hash[12], coro->hash[13], coro->hash[14], coro->hash[15]
               );

         coro->url[sizeof(coro->url) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
         cheevos_log_url("[CHEEVOS]: url to get the game's id: %s\n", coro->url);
#endif

         CORO_GOSUB(HTTP_GET);

         if (!coro->json)
            CORO_RET();

         if (cheevos_get_value(coro->json,
                  CHEEVOS_JSON_KEY_GAMEID, gameid, sizeof(gameid)))
         {
            if ((void*)coro->json)
               free((void*)coro->json);
            CHEEVOS_ERR("[CHEEVOS]: error getting game_id.\n");
            CORO_RET();
         }

         if ((void*)coro->json)
            free((void*)coro->json);
         CHEEVOS_LOG("[CHEEVOS]: got game id %s.\n", gameid);
         coro->gameid = (unsigned)strtol(gameid, NULL, 10);
         CORO_RET();
      }

      /**************************************************************************
       * Info    Gets the achievements from Retro Achievements
         * Inputs  CHEEVOS_VAR_GAMEID
         * Outputs CHEEVOS_VAR_JSON
         *************************************************************************/
   CORO_SUB(GET_CHEEVOS)

      CORO_GOSUB(LOGIN);

      snprintf(coro->url, sizeof(coro->url),
            "http://retroachievements.org/dorequest.php?r=patch&g=%u&u=%s&t=%s",
            coro->gameid,
            coro->settings->arrays.cheevos_username,
            cheevos_locals.token);

      coro->url[sizeof(coro->url) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to get the list of cheevos: %s\n", coro->url);
#endif

      CORO_GOSUB(HTTP_GET);

      if (!coro->json)
      {
         CHEEVOS_ERR("[CHEEVOS]: error getting achievements for game id %u.\n", coro->gameid);
         CORO_STOP();
      }

      CHEEVOS_LOG("[CHEEVOS]: got achievements for game id %u.\n", coro->gameid);
      CORO_RET();

      /**************************************************************************
       * Info    Gets the achievements from Retro Achievements
         * Inputs  CHEEVOS_VAR_GAMEID
         * Outputs CHEEVOS_VAR_JSON
         *************************************************************************/
   CORO_SUB(GET_BADGES)

      badges_ctx = new_badges_ctx;

#ifdef HAVE_MENU_WIDGETS
      if (false) /* we always want badges if menu widgets are enabled */
#endif
      {
         settings_t *settings = config_get_ptr();
         if (!(
               string_is_equal(settings->arrays.menu_driver, "xmb") ||
               string_is_equal(settings->arrays.menu_driver, "ozone")
            ) ||
               !settings->bools.cheevos_badges_enable)
            CORO_RET();
      }

      coro->cheevo            = cheevos_locals.core.cheevos;
      coro->cheevo_end        = cheevos_locals.core.cheevos + cheevos_locals.core.count;

      for (; coro->cheevo < coro->cheevo_end; coro->cheevo++)
      {
         for (coro->j = 0 ; coro->j < 2; coro->j++)
         {
            coro->badge_fullpath[0] = '\0';
            fill_pathname_application_special(
                  coro->badge_fullpath,
                  sizeof(coro->badge_fullpath),
                  APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

            if (!path_is_directory(coro->badge_fullpath))
               path_mkdir(coro->badge_fullpath);
            CORO_YIELD();
            if (coro->j == 0)
               snprintf(coro->badge_name,
                     sizeof(coro->badge_name),
                     "%s.png", coro->cheevo->badge);
            else
               snprintf(coro->badge_name,
                     sizeof(coro->badge_name),
                     "%s_lock.png", coro->cheevo->badge);

            fill_pathname_join(
                  coro->badge_fullpath,
                  coro->badge_fullpath,
                  coro->badge_name,
                  sizeof(coro->badge_fullpath));

            if (!badge_exists(coro->badge_fullpath))
            {
#ifdef CHEEVOS_LOG_BADGES
               CHEEVOS_LOG(
                     "[CHEEVOS]: downloading badge %s\n",
                     coro->badge_fullpath);
#endif
               snprintf(coro->url,
                     sizeof(coro->url),
                     "http://i.retroachievements.org/Badge/%s",
                     coro->badge_name);

               CORO_GOSUB(HTTP_GET);

               if (coro->json)
               {
                  if (!filestream_write_file(coro->badge_fullpath,
                           coro->json, coro->k))
                     CHEEVOS_ERR("[CHEEVOS]: error writing badge %s\n", coro->badge_fullpath);
                  else
                     free(coro->json);
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
         char urle_user[64];
         char urle_login[64];
         const char *username = coro ? coro->settings->arrays.cheevos_username : NULL;
         const char *login    = NULL;
         bool via_token       = false;

         if (coro)
         {
            if (string_is_empty(coro->settings->arrays.cheevos_password))
            {
               via_token      = true;
               login          = coro->settings->arrays.cheevos_token;
            }
            else
               login          = coro->settings->arrays.cheevos_password;
         }
         else
            login = NULL;

         if (string_is_empty(username) || string_is_empty(login))
         {
            runloop_msg_queue_push(
                  "Missing RetroAchievements account information.",
                  0, 5 * 60, false,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            runloop_msg_queue_push(
                  "Please fill in your account information in Settings.",
                  0, 5 * 60, false,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            CHEEVOS_ERR("[CHEEVOS]: login info not informed.\n");
            CORO_STOP();
         }

         cheevos_url_encode(username, urle_user, sizeof(urle_user));
         cheevos_url_encode(login, urle_login, sizeof(urle_login));

         snprintf(
               coro->url, sizeof(coro->url),
               "http://retroachievements.org/dorequest.php?r=login&u=%s&%c=%s",
                     urle_user, via_token ? 't' : 'p', urle_login
               );

         coro->url[sizeof(coro->url) - 1] = 0;
      }

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to login: %s\n",
            coro->url);
#endif

      CORO_GOSUB(HTTP_GET);

      if (coro->json)
      {
         char error_response[64];
         char error_message[256];

         cheevos_get_value(
               coro->json,
               CHEEVOS_JSON_KEY_ERROR,
               error_response,
               sizeof(error_response)
         );

         /* No error, continue with login */
         if (string_is_empty(error_response))
         {
            int res = cheevos_get_value(
               coro->json,
               CHEEVOS_JSON_KEY_TOKEN,
               cheevos_locals.token,
               sizeof(cheevos_locals.token));

            if ((void*)coro->json)
               free((void*)coro->json);

            if (!res)
            {
               if (coro->settings->bools.cheevos_verbose_enable)
               {
                  char msg[256];
                  snprintf(msg, sizeof(msg),
                        "RetroAchievements: Logged in as \"%s\".",
                        coro->settings->arrays.cheevos_username);
                  msg[sizeof(msg) - 1] = 0;
                  runloop_msg_queue_push(msg, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               }

               /* Save token to config and clear pass on success */
               *coro->settings->arrays.cheevos_password = '\0';
               strncpy(
                     coro->settings->arrays.cheevos_token,
                     cheevos_locals.token, sizeof(coro->settings->arrays.cheevos_token)
               );
               CORO_RET();
            }
         }

         if ((void*)coro->json)
            free((void*)coro->json);

         /* Site returned error, display it */
         snprintf(error_message, sizeof(error_message),
               "RetroAchievements: %s",
               error_response);
         error_message[sizeof(error_message) - 1] = 0;
         runloop_msg_queue_push(error_message, 0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         *coro->settings->arrays.cheevos_token = '\0';

         CORO_STOP();
      }

      runloop_msg_queue_push("RetroAchievements: Error contacting server.", 0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      CHEEVOS_ERR("[CHEEVOS]: error getting user token.\n");

      CORO_STOP();

      /**************************************************************************
       * Info    Pauses execution for five seconds
         *************************************************************************/
   CORO_SUB(DELAY)

      {
         retro_time_t t1;
         coro->t0         = cpu_features_get_time_usec();

         do
         {
            CORO_YIELD();
            t1 = cpu_features_get_time_usec();
         }while ((t1 - coro->t0) < 3000000);
      }

      CORO_RET();

      /**************************************************************************
       * Info    Makes a HTTP GET request
         * Inputs  CHEEVOS_VAR_URL
         * Outputs CHEEVOS_VAR_JSON
         *************************************************************************/
   CORO_SUB(HTTP_GET)

      for (coro->k = 0; coro->k < 5; coro->k++)
      {
         if (coro->k != 0)
            CHEEVOS_LOG("[CHEEVOS]: Retrying HTTP request: %u of 5\n", coro->k + 1);

         coro->json       = NULL;
         coro->conn       = net_http_connection_new(
               coro->url, "GET", NULL);

         if (!coro->conn)
         {
            CORO_GOSUB(DELAY);
            continue;
         }

         /* Don't bother with timeouts here, it's just a string scan. */
         while (!net_http_connection_iterate(coro->conn)) {}

         /* Error finishing the connection descriptor. */
         if (!net_http_connection_done(coro->conn))
         {
            net_http_connection_free(coro->conn);
            continue;
         }

         coro->http = net_http_new(coro->conn);

         /* Error connecting to the endpoint. */
         if (!coro->http)
         {
            net_http_connection_free(coro->conn);
            CORO_GOSUB(DELAY);
            continue;
         }

         while (!net_http_update(coro->http, NULL, NULL))
            CORO_YIELD();

         {
            size_t length;
            uint8_t *data = net_http_data(coro->http,
                  &length, false);

            if (data)
            {
               coro->json = (char*)malloc(length + 1);

               if (coro->json)
               {
                  memcpy((void*)coro->json, (void*)data, length);
                  free(data);
                  coro->json[length] = 0;
               }

               coro->k = (unsigned)length;
               net_http_delete(coro->http);
               net_http_connection_free(coro->conn);
               CORO_RET();
            }
         }

         net_http_delete(coro->http);
         net_http_connection_free(coro->conn);
      }

      CHEEVOS_LOG("[CHEEVOS]: Couldn't connect to server after 5 tries\n");
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
            coro->url, sizeof(coro->url),
            "http://retroachievements.org/dorequest.php?r=unlocks&u=%s&t=%s&g=%u&h=0",
            coro->settings->arrays.cheevos_username,
            cheevos_locals.token, coro->gameid
            );

      coro->url[sizeof(coro->url) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to get the list of unlocked cheevos in softcore: %s\n", coro->url);
#endif

      CORO_GOSUB(HTTP_GET);

      if (coro->json)
      {
         if (!cheevos_deactivate_unlocks(coro->json, CHEEVOS_ACTIVE_SOFTCORE))
            CHEEVOS_LOG("[CHEEVOS]: deactivated unlocked achievements in softcore mode.\n");
         else
            CHEEVOS_ERR("[CHEEVOS]: error deactivating unlocked achievements in softcore mode.\n");

         if ((void*)coro->json)
            free((void*)coro->json);
      }
      else
         CHEEVOS_ERR("[CHEEVOS]: error retrieving list of unlocked achievements in softcore mode.\n");

      /* Deactivate achievements in hardcore mode. */
      snprintf(
            coro->url, sizeof(coro->url),
            "http://retroachievements.org/dorequest.php?r=unlocks&u=%s&t=%s&g=%u&h=1",
            coro->settings->arrays.cheevos_username,
            cheevos_locals.token, coro->gameid
            );

      coro->url[sizeof(coro->url) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to get the list of unlocked cheevos in hardcore: %s\n", coro->url);
#endif

      CORO_GOSUB(HTTP_GET);

      if (coro->json)
      {
         if (!cheevos_deactivate_unlocks(coro->json, CHEEVOS_ACTIVE_HARDCORE))
            CHEEVOS_LOG("[CHEEVOS]: deactivated unlocked achievements in hardcore mode.\n");
         else
            CHEEVOS_ERR("[CHEEVOS]: error deactivating unlocked achievements in hardcore mode.\n");

         if ((void*)coro->json)
            free((void*)coro->json);
      }
      else
         CHEEVOS_ERR("[CHEEVOS]: error retrieving list of unlocked achievements in hardcore mode.\n");

#endif
      CORO_RET();

      /**************************************************************************
       * Info    Posts the "playing" activity to Retro Achievements
         * Inputs  CHEEVOS_VAR_GAMEID
         * Outputs
         *************************************************************************/
   CORO_SUB(PLAYING)

      snprintf(
            coro->url, sizeof(coro->url),
            "http://retroachievements.org/dorequest.php?r=postactivity&u=%s&t=%s&a=3&m=%u",
            coro->settings->arrays.cheevos_username,
            cheevos_locals.token, coro->gameid
            );

      coro->url[sizeof(coro->url) - 1] = 0;

#ifdef CHEEVOS_LOG_URLS
      cheevos_log_url("[CHEEVOS]: url to post the 'playing' activity: %s\n", coro->url);
#endif

      CORO_GOSUB(HTTP_GET);

      if (coro->json)
      {
         CHEEVOS_LOG("[CHEEVOS]: posted playing activity.\n");
         if ((void*)coro->json)
            free((void*)coro->json);
      }
      else
         CHEEVOS_ERR("[CHEEVOS]: error posting playing activity.\n");

      CHEEVOS_LOG("[CHEEVOS]: posted playing activity.\n");
      CORO_RET();

   CORO_LEAVE();
}

static void cheevos_task_handler(retro_task_t *task)
{
   coro_t *coro = (coro_t*)task->state;

   if (!coro)
      return;

   if (!cheevos_iterate(coro) || task_get_cancelled(task))
   {
      task_set_finished(task, true);

      CHEEVOS_LOCK(cheevos_locals.task_lock);
      cheevos_locals.task = NULL;
      CHEEVOS_UNLOCK(cheevos_locals.task_lock);

      if (task_get_cancelled(task))
      {
         CHEEVOS_LOG("[CHEEVOS]: Load task cancelled\n");
      }
      else
      {
         CHEEVOS_LOG("[CHEEVOS]: Load task finished\n");
      }

      if (coro->data)
         free(coro->data);

      if ((void*)coro->path)
         free((void*)coro->path);

      free((void*)coro);
   }
}

bool cheevos_load(const void *data)
{
   retro_task_t *task;
   const struct retro_game_info *info = NULL;
   coro_t *coro                       = NULL;

   cheevos_loaded = false;
   cheevos_hardcore_paused = false;

   if (!cheevos_locals.core_supports || !data)
      return false;

   coro = (coro_t*)calloc(1, sizeof(*coro));

   if (!coro)
      return false;

   task = task_init();

   if (!task)
   {
      if ((void*)coro)
         free((void*)coro);
      return false;
   }

   CORO_SETUP();

   info = (const struct retro_game_info*)data;

   if (info->data)
   {
      coro->len = info->size;

      /* size limit */
      if (coro->len > size_in_megabytes(64))
         coro->len = size_in_megabytes(64);

      coro->data = malloc(coro->len);

      if (!coro->data)
      {
         if ((void*)task)
            free((void*)task);
         if ((void*)coro)
            free((void*)coro);
         return false;
      }

      memcpy(coro->data, info->data, coro->len);
      coro->path       = NULL;
   }
   else
   {
      coro->data       = NULL;
      coro->path       = strdup(info->path);
   }

   task->handler   = cheevos_task_handler;
   task->state     = (void*)coro;
   task->mute      = true;
   task->callback  = NULL;
   task->user_data = NULL;
   task->progress  = 0;
   task->title     = NULL;

#ifdef HAVE_THREADS
   if (cheevos_locals.task_lock == NULL)
   {
      cheevos_locals.task_lock = slock_new();
   }
#endif

   CHEEVOS_LOCK(cheevos_locals.task_lock);
   cheevos_locals.task = task;
   CHEEVOS_UNLOCK(cheevos_locals.task_lock);

   task_queue_push(task);

   return true;
}
