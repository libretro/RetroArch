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

#include <formats/jsonsax.h>
#include <retro_file.h>
#include <rhash.h>
#include <rthreads/async_job.h>

#include "cheevos.h"
#include "dynamic.h"
#include "libretro.h"
#include "net_http_special.h"
#include "configuration.h"
#include "performance.h"
#include "runloop.h"
#include "libretro_version_1.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#include "menu/menu_entries.h"
#endif

#include "verbosity.h"

enum
{
   CHEEVOS_VAR_SIZE_BIT_0,
   CHEEVOS_VAR_SIZE_BIT_1,
   CHEEVOS_VAR_SIZE_BIT_2,
   CHEEVOS_VAR_SIZE_BIT_3,
   CHEEVOS_VAR_SIZE_BIT_4,
   CHEEVOS_VAR_SIZE_BIT_5,
   CHEEVOS_VAR_SIZE_BIT_6,
   CHEEVOS_VAR_SIZE_BIT_7,
   CHEEVOS_VAR_SIZE_NIBBLE_LOWER,
   CHEEVOS_VAR_SIZE_NIBBLE_UPPER,
   /* Byte, */
   CHEEVOS_VAR_SIZE_EIGHT_BITS, /* =Byte, */
   CHEEVOS_VAR_SIZE_SIXTEEN_BITS,
   CHEEVOS_VAR_SIZE_THIRTYTWO_BITS,

   CHEEVOS_VAR_SIZE_LAST
}; /* cheevos_var_t.size */

enum
{
   /* compare to the value of a live address in RAM */
   CHEEVOS_VAR_TYPE_ADDRESS,

   /* a number. assume 32 bit */
   CHEEVOS_VAR_TYPE_VALUE_COMP,  

   /* the value last known at this address. */
   CHEEVOS_VAR_TYPE_DELTA_MEM,   

   /* a custom user-set variable */
   CHEEVOS_VAR_TYPE_DYNAMIC_VAR,

   CHEEVOS_VAR_TYPE_LAST
}; /* cheevos_var_t.type */

enum
{
   CHEEVOS_COND_OP_EQUALS,
   CHEEVOS_COND_OP_LESS_THAN,
   CHEEVOS_COND_OP_LESS_THAN_OR_EQUAL,
   CHEEVOS_COND_OP_GREATER_THAN,
   CHEEVOS_COND_OP_GREATER_THAN_OR_EQUAL,
   CHEEVOS_COND_OP_NOT_EQUAL_TO,

   CHEEVOS_COND_OP_LAST
}; /* cheevos_cond_t.op */

enum
{
   CHEEVOS_COND_TYPE_STANDARD,
   CHEEVOS_COND_TYPE_PAUSE_IF,
   CHEEVOS_COND_TYPE_RESET_IF,

   CHEEVOS_COND_TYPE_LAST
}; /* cheevos_cond_t.type */

enum
{
   CHEEVOS_DIRTY_TITLE       = 1 << 0,
   CHEEVOS_DIRTY_DESC        = 1 << 1,
   CHEEVOS_DIRTY_POINTS      = 1 << 2,
   CHEEVOS_DIRTY_AUTHOR      = 1 << 3,
   CHEEVOS_DIRTY_ID          = 1 << 4,
   CHEEVOS_DIRTY_BADGE       = 1 << 5,
   CHEEVOS_DIRTY_CONDITIONS  = 1 << 6,
   CHEEVOS_DIRTY_VOTES       = 1 << 7,
   CHEEVOS_DIRTY_DESCRIPTION = 1 << 8,

   CHEEVOS_DIRTY_ALL         = (1 << 9) - 1
};

typedef struct
{
   unsigned size;
   unsigned type;
   unsigned bank_id;
   unsigned value;
   unsigned previous;
} cheevos_var_t;

typedef struct
{
   unsigned type;
   unsigned req_hits;
   unsigned curr_hits;

   cheevos_var_t source;
   unsigned      op;
   cheevos_var_t target;
} cheevos_cond_t;

typedef struct
{
   cheevos_cond_t *conds;
   unsigned        count;

   const char* expression;
} cheevos_condset_t;

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
   int         modified;

   cheevos_condset_t *condsets;
   unsigned count;
} cheevo_t;

typedef struct
{
   cheevo_t *cheevos;
   unsigned  count;
} cheevoset_t;

typedef struct
{
   int loaded;
   
   cheevoset_t core;
   cheevoset_t unofficial;

   char token[32];
} cheevos_locals_t;

typedef struct
{
   int is_element;
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
   uint32_t field_hash;
   unsigned core_count;
   unsigned unofficial_count;
} cheevos_countud_t;

typedef struct
{
   const char *string;
   size_t      length;
} cheevos_field_t;

typedef struct
{
   int      in_cheevos;
   unsigned core_count;
   unsigned unofficial_count;

   cheevos_field_t *field;
   cheevos_field_t  id, memaddr, title, desc, points, author;
   cheevos_field_t  modified, created, badge, flags;
} cheevos_readud_t;

typedef struct
{
   unsigned (*finder)(const struct retro_game_info *, retro_time_t);
   const char *name;
   const uint32_t *ext_hashes;
} cheevos_finder_t;

typedef struct
{
   int cheats_are_enabled;
   int cheats_were_enabled;
} cheevos_globals_t;

static cheevos_locals_t cheevos_locals =
{
   0,
   {NULL, 0},
   {NULL, 0},
   {0},
};

static cheevos_globals_t cheevos_globals =
{
   0,
   0
};

/* forward declaration */

int rarch_main_async_job_add(async_task_t task, void *payload);

/*****************************************************************************
Supporting functions.
*****************************************************************************/

static uint32_t cheevos_djb2(const char* str, size_t length)
{
   const unsigned char *aux = (const unsigned char*)str;
   const unsigned char *end = aux + length;
   uint32_t hash = 5381;

   while (aux < end)
      hash = (hash << 5) + hash + *aux++;

   return hash;
}

static int cheevos_http_get(const char **result, size_t *size,
      const char *url, retro_time_t *timeout)
{
   const char *msg;
   int ret = net_http_get(result, size, url, timeout);
   
   switch (ret)
   {
      case NET_HTTP_GET_OK:
         return ret;
         
      case NET_HTTP_GET_MALFORMED_URL:
         msg = "malformed url";
         break;
         
      case NET_HTTP_GET_CONNECT_ERROR:
         msg = "connect error";
         break;
         
      case NET_HTTP_GET_TIMEOUT:
         msg = "timeout";
         break;
         
      default:
         msg = "?";
         break;
   }
   
   RARCH_LOG("CHEEVOS error getting %s: %s\n", url, msg);
   RARCH_LOG("CHEEVOS http result was %s\n", *result ? *result : "(null)");
   return ret;
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
      ud->value = istrue ? "true" : "false";
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
   ud.is_key = 0;
   ud.value = NULL;
   ud.length = 0;
   *value = 0;

   if (jsonsax_parse(json, &handlers,
            (void*)&ud) == JSONSAX_OK && ud.value && ud.length < length)
   {
      strncpy(value, ud.value, length);
      value[ud.length] = 0;
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
  return 0;
}

static int cheevos_count__json_key(void *userdata,
      const char *name, size_t length)
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;
   ud->field_hash        = cheevos_djb2(name, length);

   if (ud->field_hash == 0x69749ae1U /* Achievements */)
      ud->in_cheevos = 1;

   return 0;
}

static int cheevos_count__json_number(void *userdata,
      const char *number, size_t length)
{
   long flags;
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;

   if (ud->in_cheevos && ud->field_hash == 0x0d2e96b2U /* Flags */)
   {
      flags = strtol(number, NULL, 10);

      if (flags == 3) /* core achievements */
         ud->core_count++;
      else if (flags == 5) /* unofficial achievements */
         ud->unofficial_count++;
   }

   return 0;
}

static int cheevos_count_cheevos(const char *json,
      unsigned *core_count, unsigned *unofficial_count)
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
   ud.in_cheevos = 0;
   ud.core_count = 0;
   ud.unofficial_count = 0;

   res = jsonsax_parse(json, &handlers, (void*)&ud);

   *core_count = ud.core_count;
   *unofficial_count = ud.unofficial_count;

   return res;
}

/*****************************************************************************
Parse the MemAddr field.
*****************************************************************************/

static unsigned cheevos_prefix_to_comp_size(char prefix)
{
   /* Careful not to use ABCDEF here, this denotes part of an actual variable! */

   switch( toupper( prefix ) )
   {
      case 'M':
         return CHEEVOS_VAR_SIZE_BIT_0;
      case 'N':
         return CHEEVOS_VAR_SIZE_BIT_1;
      case 'O':
         return CHEEVOS_VAR_SIZE_BIT_2;
      case 'P':
         return CHEEVOS_VAR_SIZE_BIT_3;
      case 'Q':
         return CHEEVOS_VAR_SIZE_BIT_4;
      case 'R':
         return CHEEVOS_VAR_SIZE_BIT_5;
      case 'S':
         return CHEEVOS_VAR_SIZE_BIT_6;
      case 'T':
         return CHEEVOS_VAR_SIZE_BIT_7;
      case 'L':
         return CHEEVOS_VAR_SIZE_NIBBLE_LOWER;
      case 'U':
         return CHEEVOS_VAR_SIZE_NIBBLE_UPPER;
      case 'H':
         return CHEEVOS_VAR_SIZE_EIGHT_BITS;
      case 'X':
         return CHEEVOS_VAR_SIZE_THIRTYTWO_BITS;
      default:
      case ' ':
         break;
   }

   return CHEEVOS_VAR_SIZE_SIXTEEN_BITS;
}

static unsigned cheevos_read_hits(const char **memaddr)
{
   char *end;
   const char *str = *memaddr;
   unsigned num_hits = 0;

   if (*str == '(' || *str == '.')
   {
      num_hits = strtol(str + 1, &end, 10);
      str = end + 1;
   }

   *memaddr = str;
   return num_hits;
}

static unsigned cheevos_parse_operator(const char **memaddr)
{
   unsigned char op;
   const char *str = *memaddr;

   if (*str == '=' && str[1] == '=')
   {
      op = CHEEVOS_COND_OP_EQUALS;
      str += 2;
   }
   else if (*str == '=')
   {
      op = CHEEVOS_COND_OP_EQUALS;
      str++;
   }
   else if (*str == '!' && str[1] == '=')
   {
      op = CHEEVOS_COND_OP_NOT_EQUAL_TO;
      str += 2;
   }
   else if (*str == '<' && str[1] == '=')
   {
      op = CHEEVOS_COND_OP_LESS_THAN_OR_EQUAL;
      str += 2;
   }
   else if (*str == '<')
   {
      op = CHEEVOS_COND_OP_LESS_THAN;
      str++;
   }
   else if (*str == '>' && str[1] == '=')
   {
      op = CHEEVOS_COND_OP_GREATER_THAN_OR_EQUAL;
      str += 2;
   }
   else if (*str == '>')
   {
      op = CHEEVOS_COND_OP_GREATER_THAN;
      str++;
   }
   else
   {
      /* TODO log the exception */
      op = CHEEVOS_COND_OP_EQUALS;
   }

   *memaddr = str;
   return op;
}

static void cheevos_parse_var(cheevos_var_t *var, const char **memaddr)
{
   char *end;
   const char *str = *memaddr;
   unsigned base = 16;

   if (toupper(*str) == 'D' && str[1] == '0' && toupper(str[2]) == 'X')
   {
      /* d0x + 4 hex digits */
      str += 3;
      var->type = CHEEVOS_VAR_TYPE_DELTA_MEM;
   }
   else if (*str == '0' && toupper(str[1]) == 'X')
   {
      /* 0x + 4 hex digits */
      str += 2;
      var->type = CHEEVOS_VAR_TYPE_ADDRESS;
   }
   else
   {
      var->type = CHEEVOS_VAR_TYPE_VALUE_COMP;

      if (toupper(*str) == 'H')
         str++;
      else
         base = 10;
   }

   if (var->type != CHEEVOS_VAR_TYPE_VALUE_COMP)
   {
      var->size = cheevos_prefix_to_comp_size(*str);

      if (var->size != CHEEVOS_VAR_SIZE_SIXTEEN_BITS)
         str++;
   }

   var->value = strtol(str, &end, base);
   *memaddr = end;
}

static void cheevos_parse_cond(cheevos_cond_t *cond, const char **memaddr)
{
   const char* str = *memaddr;

   if (*str == 'R' && str[1] == ':')
   {
      cond->type = CHEEVOS_COND_TYPE_RESET_IF;
      str += 2;
   }
   else if (*str == 'P' && str[1] == ':')
   {
      cond->type = CHEEVOS_COND_TYPE_PAUSE_IF;
      str += 2;
   }
   else
      cond->type = CHEEVOS_COND_TYPE_STANDARD;

   cheevos_parse_var(&cond->source, &str);
   cond->op = cheevos_parse_operator(&str);
   cheevos_parse_var(&cond->target, &str);
   cond->curr_hits = 0;
   cond->req_hits = cheevos_read_hits(&str);

   *memaddr = str;
}

static unsigned cheevos_count_cond_sets(const char *memaddr)
{
   cheevos_cond_t cond;
   unsigned count = 0;

   do
   {
      do
      {
         while (  *memaddr == ' ' 
               || *memaddr == '_' 
               || *memaddr == '|' 
               || *memaddr == 'S')
            memaddr++; /* Skip any chars up til the start of the achievement condition */

         cheevos_parse_cond(&cond, &memaddr);
      }
      while (  *memaddr == '_' 
            || *memaddr == 'R' 
            || *memaddr == 'P'); /* AND, ResetIf, PauseIf */

      count++;
   }
   while (*memaddr == 'S'); /* Repeat for all subconditions if they exist */

   return count;
}

static unsigned cheevos_count_conds_in_set(const char *memaddr, unsigned set)
{
   cheevos_cond_t cond;
   unsigned index = 0;
   unsigned count = 0;

   do
   {
      do
      {
         while (*memaddr == ' ' || *memaddr == '_' || *memaddr == '|' || *memaddr == 'S')
            memaddr++; /* Skip any chars up til the start of the achievement condition */

         cheevos_parse_cond(&cond, &memaddr);

         if (index == set)
            count++;
      }
      while (*memaddr == '_' || *memaddr == 'R' || *memaddr == 'P'); /* AND, ResetIf, PauseIf */
   }
   while (*memaddr == 'S'); /* Repeat for all subconditions if they exist */

   return count;
}

static void cheevos_parse_memaddr(cheevos_cond_t *cond, const char *memaddr)
{
   do
   {
      do
      {
         while (*memaddr == ' ' || *memaddr == '_' || *memaddr == '|' || *memaddr == 'S')
            memaddr++; /* Skip any chars up til the start of the achievement condition */

         cheevos_parse_cond(cond++, &memaddr);
      }
      while (*memaddr == '_' || *memaddr == 'R' || *memaddr == 'P'); /* AND, ResetIf, PauseIf */
   }
   while (*memaddr == 'S'); /* Repeat for all subconditions if they exist */
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
   const cheevos_condset_t *end;
   unsigned set;
   cheevos_condset_t *condset;
   cheevo_t *cheevo;
   int flags = strtol(ud->flags.string, NULL, 10);

   if (flags == 3)
      cheevo = cheevos_locals.core.cheevos + ud->core_count++;
   else
      cheevo = cheevos_locals.unofficial.cheevos + ud->unofficial_count++;

   cheevo->id          = strtol(ud->id.string, NULL, 10);
   cheevo->title       = cheevos_dupstr(&ud->title);
   cheevo->description = cheevos_dupstr(&ud->desc);
   cheevo->author      = cheevos_dupstr(&ud->author);
   cheevo->badge       = cheevos_dupstr(&ud->badge);
   cheevo->points      = strtol(ud->points.string, NULL, 10);
   cheevo->dirty       = 0;
   cheevo->active      = 1; /* flags == 3; */
   cheevo->modified    = 0;

   if (!cheevo->title || !cheevo->description || !cheevo->author || !cheevo->badge)
   {
      free((void*)cheevo->title);
      free((void*)cheevo->description);
      free((void*)cheevo->author);
      free((void*)cheevo->badge);
      return -1;
   }

   cheevo->count = cheevos_count_cond_sets(ud->memaddr.string);

   if (cheevo->count)
   {
      cheevo->condsets = (cheevos_condset_t*)
         malloc(cheevo->count * sizeof(cheevos_condset_t));

      if (!cheevo->condsets)
         return -1;

      memset((void*)cheevo->condsets, 0, cheevo->count * sizeof(cheevos_condset_t));
      end = cheevo->condsets + cheevo->count;
      set = 0;

      for (condset = cheevo->condsets; condset < end; condset++)
      {
         condset->count = cheevos_count_conds_in_set(ud->memaddr.string, set++);

         if (condset->count)
         {
            condset->conds = (cheevos_cond_t*)
               malloc(condset->count * sizeof(cheevos_cond_t));

            if (!condset->conds)
               return -1;

            memset((void*)condset->conds, 0,
                  condset->count * sizeof(cheevos_cond_t));

            condset->expression = cheevos_dupstr(&ud->memaddr);
            cheevos_parse_memaddr(condset->conds, ud->memaddr.string);
         }
         else
            condset->conds = NULL;
      }
   }

   return 0;
}

static int cheevos_read__json_key( void *userdata,
      const char *name, size_t length)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;
   uint32_t        hash = cheevos_djb2(name, length);

   ud->field = NULL;

   if (hash == 0x69749ae1U /* Achievements */)
      ud->in_cheevos = 1;
   else if (ud->in_cheevos)
   {
      switch ( hash )
      {
         case 0x005973f2U: /* ID          */ ud->field = &ud->id;       break;
         case 0x1e76b53fU: /* MemAddr     */ ud->field = &ud->memaddr;  break;
         case 0x0e2a9a07U: /* Title       */ ud->field = &ud->title;    break;
         case 0xe61a1f69U: /* Description */ ud->field = &ud->desc;     break;
         case 0xca8fce22U: /* Points      */ ud->field = &ud->points;   break;
         case 0xa804edb8U: /* Author      */ ud->field = &ud->author;   break;
         case 0xdcea4fe6U: /* Modified    */ ud->field = &ud->modified; break;
         case 0x3a84721dU: /* Created     */ ud->field = &ud->created;  break;
         case 0x887685d9U: /* BadgeName   */ ud->field = &ud->badge;    break;
         case 0x0d2e96b2U: /* Flags       */ ud->field = &ud->flags;    break;
      }
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

   return 0;
}

static int cheevos_read__json_end_object(void *userdata)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;

   if (ud->in_cheevos)
      return cheevos_new_cheevo(ud);

   return 0;
}

static int cheevos_read__json_end_array(void *userdata)
{
   cheevos_readud_t *ud = (cheevos_readud_t*)userdata;
   ud->in_cheevos = 0;
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

   unsigned core_count, unofficial_count;
   cheevos_readud_t ud;
   settings_t *settings         = config_get_ptr();

   /* Just return OK if cheevos are disabled. */
   if (!settings->cheevos.enable)
      return 0;

   /* Count the number of achievements in the JSON file. */

   if (cheevos_count_cheevos(json, &core_count, &unofficial_count) != JSONSAX_OK)
      return -1;

   /* Allocate the achievements. */

   cheevos_locals.core.cheevos = (cheevo_t*)
      malloc(core_count * sizeof(cheevo_t));
   cheevos_locals.core.count = core_count;

   cheevos_locals.unofficial.cheevos = (cheevo_t*)
      malloc(unofficial_count * sizeof(cheevo_t));
   cheevos_locals.unofficial.count = unofficial_count;

   if (!cheevos_locals.core.cheevos || !cheevos_locals.unofficial.cheevos)
   {
      free((void*)cheevos_locals.core.cheevos);
      free((void*)cheevos_locals.unofficial.cheevos);
      cheevos_locals.core.count = cheevos_locals.unofficial.count = 0;

      return -1;
   }

   memset((void*)cheevos_locals.core.cheevos,
         0, core_count * sizeof(cheevo_t));
   memset((void*)cheevos_locals.unofficial.cheevos,
         0, unofficial_count * sizeof(cheevo_t));

   /* Load the achievements. */

   ud.in_cheevos = 0;
   ud.field = NULL;
   ud.core_count = 0;
   ud.unofficial_count = 0;

   if (jsonsax_parse(json, &handlers, (void*)&ud) != JSONSAX_OK)
   {
      cheevos_unload();
      return -1;
   }
   
   return 0;
}

/*****************************************************************************
Test all the achievements (call once per frame).
*****************************************************************************/

static const uint8_t *cheevos_get_memory(unsigned offset)
{
   uint8_t *memory;
   retro_ctx_memory_info_t mem_info;

   mem_info.id = RETRO_MEMORY_SYSTEM_RAM;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   if (offset < mem_info.size)
   {
      memory = (uint8_t*)mem_info.data;
      return memory + offset;
   }

   offset -= mem_info.size;

   mem_info.data = NULL;
   mem_info.size = 0;
   mem_info.id   = RETRO_MEMORY_SAVE_RAM;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   if (offset < mem_info.size)
   {
      memory = (uint8_t*)mem_info.data;
      return memory + offset;
   }

   offset -= mem_info.size;

   mem_info.data = NULL;
   mem_info.size = 0;
   mem_info.id   = RETRO_MEMORY_VIDEO_RAM;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   if (offset < mem_info.size)
   {
      memory = (uint8_t*)mem_info.data;
      return memory + offset;
   }

   offset -= mem_info.size;

   mem_info.data = NULL;
   mem_info.size = 0;
   mem_info.id   = RETRO_MEMORY_RTC;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   if (offset < mem_info.size)
   {
      memory = (uint8_t*)mem_info.data;
      return memory + offset;
   }

   return NULL;
}

static unsigned cheevos_get_var_value(cheevos_var_t *var)
{
   unsigned previous = var->previous;
   unsigned live_val = 0;
   const uint8_t *memory;

   if (var->type == CHEEVOS_VAR_TYPE_VALUE_COMP)
      return var->value;

   if (     var->type == CHEEVOS_VAR_TYPE_ADDRESS 
         || var->type == CHEEVOS_VAR_TYPE_DELTA_MEM)
   {
      /* TODO Check with Scott if the bank id is needed */
      memory = cheevos_get_memory(var->value);
      
      if (memory)
      {
         live_val = memory[0];

         if (var->size >= CHEEVOS_VAR_SIZE_BIT_0 && var->size <= CHEEVOS_VAR_SIZE_BIT_7)
            live_val = (live_val & (1 << (var->size - CHEEVOS_VAR_SIZE_BIT_0))) != 0;
         else if (var->size == CHEEVOS_VAR_SIZE_NIBBLE_LOWER)
            live_val &= 0x0f;
         else if (var->size == CHEEVOS_VAR_SIZE_NIBBLE_UPPER)
            live_val = (live_val >> 4) & 0x0f;
         else if (var->size == CHEEVOS_VAR_SIZE_EIGHT_BITS)
            ; /* nothing */
         else if (var->size == CHEEVOS_VAR_SIZE_SIXTEEN_BITS)
            live_val |= memory[1] << 8;
         else if (var->size == CHEEVOS_VAR_SIZE_THIRTYTWO_BITS)
         {
            live_val |= memory[1] << 8;
            live_val |= memory[2] << 16;
            live_val |= memory[3] << 24;
         }
      }
      else
         live_val = 0;
      
      if (var->type == CHEEVOS_VAR_TYPE_DELTA_MEM)
      {
         var->previous = live_val;
         return previous;
      }

      return live_val;
   }

   /* We shouldn't get here... */
   return 0;
}

static int cheevos_test_condition(cheevos_cond_t *cond)
{
   unsigned sval = cheevos_get_var_value(&cond->source);
   unsigned tval = cheevos_get_var_value(&cond->target);

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
   int cond_valid = 0;
   int set_valid = 1;
   const cheevos_cond_t *end = condset->conds + condset->count;
   cheevos_cond_t *cond;

   /* Now, read all Pause conditions, and if any are true, 
    * do not process further (retain old state). */

   for (cond = condset->conds; cond < end; cond++)
   {
      if (cond->type == CHEEVOS_COND_TYPE_PAUSE_IF)
      {
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
   }

   /* Read all standard conditions, and process as normal: */
   for (cond = condset->conds; cond < end; cond++)
   {
      if (     cond->type == CHEEVOS_COND_TYPE_PAUSE_IF 
            || cond->type == CHEEVOS_COND_TYPE_RESET_IF)
         continue;

      if (cond->req_hits != 0 && cond->curr_hits >= cond->req_hits)
         continue;

      cond_valid = cheevos_test_condition(cond);

      if (cond_valid)
      {
         cond->curr_hits++;
         *dirty_conds = 1;

         /* Process this logic, if this condition is true: */
         if (cond->req_hits == 0)
            ; /* Not a hit-based requirement: ignore any additional logic! */
         else if (cond->curr_hits < cond->req_hits)
            cond_valid = 0; /* Not entirely valid yet! */

         if (match_any)
            break;
      }

      /* Sequential or non-sequential? */
      set_valid &= cond_valid;
   }

   /* Now, ONLY read reset conditions! */
   for (cond = condset->conds; cond < end; cond++)
   {
      if (cond->type == CHEEVOS_COND_TYPE_RESET_IF)
      {
         cond_valid = cheevos_test_condition(cond);

         if (cond_valid)
         {
            *reset_conds = 1; /* Resets all hits found so far */
            set_valid = 0;    /* Cannot be valid if we've hit a reset condition. */
            break;            /* No point processing any further reset conditions. */
         }
      }
   }

   return set_valid;
}

static int cheevos_reset_cond_set(cheevos_condset_t *condset, int deltas)
{
   int dirty = 0;
   const cheevos_cond_t *end = condset->conds + condset->count;
   cheevos_cond_t *cond;

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
   int dirty_conds = 0;
   int reset_conds = 0;
   int ret_val = 0;
   int ret_val_sub_cond = cheevo->count == 1;
   cheevos_condset_t *condset = cheevo->condsets;
   const cheevos_condset_t *end = condset + cheevo->count;
   int dirty;

   if (condset < end)
   {
      ret_val = cheevos_test_cond_set(condset, &dirty_conds, &reset_conds, 0);
      condset++;
   }

   while (condset < end)
   {
      int res = cheevos_test_cond_set(condset, &dirty_conds, &reset_conds, 0);
      ret_val_sub_cond |= res;
      condset++;
   }

   if (dirty_conds)
      cheevo->dirty |= CHEEVOS_DIRTY_CONDITIONS;

   if (reset_conds)
   {
      dirty = 0;

      for (condset = cheevo->condsets; condset < end; condset++)
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
      if (isalnum(*str) || *str == '-' || *str == '_' || *str == '.' || *str == '~')
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
            sprintf(encoded, "%%%02x", (uint8_t)*str);
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

static int cheevos_login(retro_time_t *timeout)
{
   const char *username;
   const char *password;
   char urle_user[64];
   char urle_pwd[64];
   char request[256];
   const char *json;
   int res;
   settings_t *settings         = config_get_ptr();

   if (cheevos_locals.token[0])
      return 0;
   
   username = settings->cheevos.username;
   password = settings->cheevos.password;
   
   if (!username || !*username || !password || !*password)
   {
      runloop_msg_queue_push("Missing Retro Achievements account information", 0, 5 * 60, false);
      runloop_msg_queue_push("Please fill in your account information in Settings", 0, 5 * 60, false);
      RARCH_LOG("CHEEVOS username and/or password not informed\n");
      return -1;
   }
   
   cheevos_url_encode(username, urle_user, sizeof(urle_user));
   cheevos_url_encode(password, urle_pwd, sizeof(urle_pwd));
   
   snprintf(
      request, sizeof(request),
      "http://retroachievements.org/dorequest.php?r=login&u=%s&p=%s",
      urle_user, urle_pwd
   );
   
   request[sizeof(request) - 1] = 0;

   if (!cheevos_http_get(&json, NULL, request, timeout))
   {
      res = cheevos_get_value(json, 0x0e2dbd26U /* Token */, cheevos_locals.token, sizeof(cheevos_locals.token));
      free((void*)json);

      if (!res)
      {
         RARCH_LOG("CHEEVOS user token is '%s'\n", cheevos_locals.token);
         return 0;
      }
   }

   runloop_msg_queue_push("Retro Achievements login error", 0, 5 * 60, false);
   runloop_msg_queue_push("Please make sure your account information is correct", 0, 5 * 60, false);
   RARCH_LOG("CHEEVOS error getting user token\n");
   return -1;
}

static void cheevos_unlocker(void *payload)
{
   char request[256];
   const char *result;
   settings_t *settings = config_get_ptr();
   unsigned cheevo_id   = (unsigned)(uintptr_t)payload;

   if (!cheevos_login(NULL))
   {
      snprintf(
         request, sizeof(request),
         "http://retroachievements.org/dorequest.php?r=awardachievement&u=%s&t=%s&a=%u&h=%d",
         settings->cheevos.username, cheevos_locals.token, cheevo_id, 0
      );

      request[sizeof(request) - 1] = 0;
      RARCH_LOG("CHEEVOS awarding achievement %u: %s\n", cheevo_id, request);

      if (!cheevos_http_get(&result, NULL, request, NULL))
      {
         RARCH_LOG("CHEEVOS awarded achievement %u: %s\n", cheevo_id, result);
         free((void*)result);
      }
      else
      {
         RARCH_LOG("CHEEVOS error awarding achievement %u, will retry\n", cheevo_id);
         rarch_main_async_job_add(cheevos_unlocker, (void*)(uintptr_t)cheevo_id);
      }
   }
}

static void cheevos_test_cheevo_set(const cheevoset_t *set)
{
   cheevo_t *cheevo    = NULL;
   const cheevo_t *end = set->cheevos + set->count;

   for (cheevo = set->cheevos; cheevo < end; cheevo++)
   {
      if (cheevo->active && cheevos_test_cheevo(cheevo))
      {
         RARCH_LOG("CHEEVOS %s\n", cheevo->title);
         RARCH_LOG("CHEEVOS %s\n", cheevo->description);

         runloop_msg_queue_push(cheevo->title, 0, 3 * 60, false);
         runloop_msg_queue_push(cheevo->description, 0, 5 * 60, false);

         rarch_main_async_job_add(cheevos_unlocker,
               (void*)(uintptr_t)cheevo->id);

         cheevo->active = 0;
      }
   }
}

void cheevos_test(void)
{
   if (!cheevos_locals.loaded)
      return;

   if (     !cheevos_globals.cheats_are_enabled 
         && !cheevos_globals.cheats_were_enabled)
   {
      settings_t *settings = config_get_ptr();

      if (!settings->cheevos.enable)
         return;

      cheevos_test_cheevo_set(&cheevos_locals.core);

      if (settings->cheevos.test_unofficial)
         cheevos_test_cheevo_set(&cheevos_locals.unofficial);
   }
}

/*****************************************************************************
Free the loaded achievements.
*****************************************************************************/

static void cheevos_free_condset(const cheevos_condset_t *set)
{
   free((void*)set->conds);
}

static void cheevos_free_cheevo(const cheevo_t *cheevo)
{
   free((void*)cheevo->title);
   free((void*)cheevo->description);
   free((void*)cheevo->author);
   free((void*)cheevo->badge);
   cheevos_free_condset(cheevo->condsets);
}

static void cheevos_free_cheevo_set(const cheevoset_t *set)
{
   const cheevo_t *cheevo = set->cheevos;
   const cheevo_t *end = cheevo + set->count;

   while (cheevo < end)
      cheevos_free_cheevo(cheevo++);

   free((void*)set->cheevos);
}

void cheevos_unload(void)
{
   if (cheevos_locals.loaded)
   {
      cheevos_free_cheevo_set(&cheevos_locals.core);
      cheevos_free_cheevo_set(&cheevos_locals.unofficial);
      
      cheevos_locals.loaded = 0;
   }
}

/*****************************************************************************
Load achievements from retroachievements.org.
*****************************************************************************/

static int cheevos_get_by_game_id(const char **json,
      unsigned game_id, retro_time_t *timeout)
{
   char request[256];
   settings_t *settings         = config_get_ptr();

   /* Just return OK if cheevos are disabled. */
   if (!settings->cheevos.enable)
      return 0;

   if (!cheevos_login(timeout))
   {
      snprintf(
         request, sizeof(request),
         "http://retroachievements.org/dorequest.php?r=patch&u=%s&g=%u&f=3&l=1&t=%s",
         settings->cheevos.username, game_id, cheevos_locals.token
      );

      request[sizeof(request) - 1] = 0;

      if (!cheevos_http_get(json, NULL, request, timeout))
      {
         RARCH_LOG("CHEEVOS got achievements for game id %u\n", game_id);
         return 0;
      }

      RARCH_LOG("CHEEVOS error getting achievements for game id %u\n", game_id);
   }

   return -1;
}

static unsigned cheevos_get_game_id(unsigned char *hash, retro_time_t *timeout)
{
   char request[256];
   const char* json;
   char game_id[16];
   int res;
   
   RARCH_LOG(
      "CHEEVOS getting game id for hash %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
      hash[ 0], hash[ 1], hash[ 2], hash[ 3],
      hash[ 4], hash[ 5], hash[ 6], hash[ 7],
      hash[ 8], hash[ 9], hash[10], hash[11],
      hash[12], hash[13], hash[14], hash[15]
   );

   snprintf(
      request, sizeof(request),
      "http://retroachievements.org/dorequest.php?r=gameid&m=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
      hash[ 0], hash[ 1], hash[ 2], hash[ 3],
      hash[ 4], hash[ 5], hash[ 6], hash[ 7],
      hash[ 8], hash[ 9], hash[10], hash[11],
      hash[12], hash[13], hash[14], hash[15]
   );

   request[sizeof(request) - 1] = 0;

   if (!cheevos_http_get(&json, NULL, request, timeout))
   {
      res = cheevos_get_value(json, 0xb4960eecU /* GameID */, game_id, sizeof(game_id));
      free((void*)json);

      if (!res)
      {
         RARCH_LOG("CHEEVOS got game id %s\n", game_id);
         return strtoul(game_id, NULL, 10);
      }
   }

   RARCH_LOG("CHEEVOS error getting game_id\n");
   return 0;
}

static void cheevos_playing(void *payload)
{
   char request[256];
   const char* json;
   unsigned game_id     = (unsigned)(uintptr_t)payload;
   settings_t *settings = config_get_ptr();
   
   if (!cheevos_login(NULL))
   {
      snprintf(
         request, sizeof(request),
         "http://retroachievements.org/dorequest.php?r=postactivity&u=%s&t=%s&a=3&m=%u",
         settings->cheevos.username, cheevos_locals.token, game_id
      );

      request[sizeof(request) - 1] = 0;

      if (!cheevos_http_get(&json, NULL, request, NULL))
      {
         free((void*)json);
         RARCH_LOG("CHEEVOS posted playing game %u activity\n", game_id);
         return;
      }
      else
      {
         RARCH_LOG("CHEEVOS error posting playing game %u activity, will retry\n", game_id);
         rarch_main_async_job_add(cheevos_playing, (void*)(uintptr_t)game_id);
      }
   }
}


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
         if (cheevo->id == id)
         {
            cheevo->active = 0;
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
            if (cheevo->id != id)
               continue;

            cheevo->active = 0;
            break;
         }
      }
   }
   
   return 0;
}

static int cheevos_deactivate_unlocks(unsigned game_id, retro_time_t *timeout)
{
   /* Only call this function after the cheevos have been loaded. */
   
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
   
   char request[256];
   const char* json;
   cheevos_deactivate_t ud;
   int res;
   settings_t *settings         = config_get_ptr();
   
   if (!cheevos_login(timeout))
   {
      snprintf(
         request, sizeof(request),
         "http://retroachievements.org/dorequest.php?r=unlocks&u=%s&t=%s&g=%u&h=0",
         settings->cheevos.username, cheevos_locals.token, game_id
      );

      request[sizeof(request) - 1] = 0;

      if (!cheevos_http_get(&json, NULL, request, timeout))
      {
         ud.is_element = 0;
         res = jsonsax_parse(json, &handlers, (void*)&ud);
         free((void*)json);
         
         if (res == JSONSAX_OK)
         {
            RARCH_LOG("CHEEVOS deactivated unlocked achievements\n");
            return 0;
         }
      }
   }
   
   RARCH_LOG("CHEEVOS error deactivating unlocked achievements\n");
   return -1;
}

#define CHEEVOS_SIX_MB   (6 * 1024 * 1024)
#define CHEEVOS_EIGHT_MB (8 * 1024 * 1024)

static INLINE unsigned cheevos_next_power_of_2(unsigned n)
{
   n--;
   
   n |= n >> 1;
   n |= n >> 2;
   n |= n >> 4;
   n |= n >> 8;
   n |= n >> 16;
   
   return n + 1;
}

static size_t cheevos_eval_md5(
      const struct retro_game_info *info,
      MD5_CTX *ctx)
{
   MD5_Init(ctx);
   
   if (info->data)
   {
      MD5_Update(ctx, info->data, info->size);
      return info->size;
   }
   else
   {
      RFILE *file = retro_fopen(info->path, RFILE_MODE_READ, 0);
      size_t size = 0;
      
      if (!file)
         return 0;
      
      for (;;)
      {
         uint8_t buffer[4096];
         ssize_t num_read = retro_fread(file,
               (void*)buffer, sizeof(buffer));
         
         if (num_read <= 0)
            break;
         
         MD5_Update(ctx, (void*)buffer, num_read);
         size += num_read;
      }
      
      retro_fclose(file);
      return size;
   }
}

static void cheevos_fill_md5(size_t size, size_t total, MD5_CTX *ctx)
{
   ssize_t fill = total - size;
   char buffer[4096];
   
   memset((void*)buffer, 0, sizeof(buffer));

   while (fill > 0)
   {
      ssize_t len = sizeof(buffer);

      if (len > fill)
         len = fill;

      MD5_Update(ctx, (void*)buffer, len);
      fill -= len;
   }
}

static unsigned cheevos_find_game_id_generic(
      const struct retro_game_info *info,
      retro_time_t timeout)
{
   MD5_CTX ctx;
   uint8_t hash[16];
   retro_time_t to;
   size_t size;
   
   size = cheevos_eval_md5(info, &ctx);
   MD5_Final(hash, &ctx);
   
   if (!size)
      return 0;
   
   to = timeout;
   return cheevos_get_game_id(hash, &to);
}

static unsigned cheevos_find_game_id_snes(
      const struct retro_game_info *info,
      retro_time_t timeout)
{
   MD5_CTX ctx;
   uint8_t hash[16];
   retro_time_t to;
   size_t size;
   
   size = cheevos_eval_md5(info, &ctx);
   
   if (!size)
   {
      MD5_Final(hash, &ctx);
      return 0;
   }
   
   cheevos_fill_md5(size, CHEEVOS_EIGHT_MB, &ctx);
   MD5_Final(hash, &ctx);
   
   to = timeout;
   return cheevos_get_game_id(hash, &to);
}

static unsigned cheevos_find_game_id_genesis(
      const struct retro_game_info *info, retro_time_t timeout)
{
   MD5_CTX ctx;
   uint8_t hash[16];
   retro_time_t to;
   size_t size;
   
   size = cheevos_eval_md5(info, &ctx);
   
   if (!size)
   {
      MD5_Final(hash, &ctx);
      return 0;
   }
   
   cheevos_fill_md5(size, CHEEVOS_SIX_MB, &ctx);
   MD5_Final(hash, &ctx);
   
   to = timeout;
   return cheevos_get_game_id(hash, &to);
}

static unsigned cheevos_find_game_id_nes(
      const struct retro_game_info *info,
      retro_time_t timeout)
{
   struct
   {
      uint8_t id[4]; /* NES^Z */
      uint8_t rom_size;
      uint8_t vrom_size;
      uint8_t rom_type;
      uint8_t rom_type2;
      uint8_t reserve[8];
   } header;
   
   size_t rom_size;
   MD5_CTX ctx;
   uint8_t hash[16];
   retro_time_t to;
   
   if (info->data)
   {
      if (info->size < sizeof(header))
         return 0;
      
      memcpy((void*)&header, info->data, sizeof(header));
   }
   else
   {
      RFILE *file = retro_fopen(info->path, RFILE_MODE_READ, 0);
      ssize_t num_read;
      
      if (!file)
         return 0;
      
      num_read = retro_fread(file, (void*)&header, sizeof(header));
      retro_fclose(file);
      
      if (num_read < (ssize_t)sizeof(header))
         return 0;
   }
   
   if (     header.id[0] != 'N' 
         || header.id[1] != 'E' 
         || header.id[2] != 'S' 
         || header.id[3] != 0x1a)
      return 0;
   
   if (header.rom_size)
      rom_size = cheevos_next_power_of_2(header.rom_size) * 16384;
   else
      rom_size = 4194304;
   
   if (info->data)
   {
      if (rom_size + sizeof(header) > info->size)
         return 0;
      
      MD5_Init(&ctx);
      MD5_Update(&ctx,
            (void*)((char*)info->data + sizeof(header)), rom_size);
      MD5_Final(hash, &ctx);
   }
   else
   {
      RFILE *file = retro_fopen(info->path, RFILE_MODE_READ, 0);
      
      if (!file)
         return 0;
      
      MD5_Init(&ctx);
      retro_fseek(file, sizeof(header), SEEK_SET);
      
      for (;;)
      {
         uint8_t buffer[4096];
         ssize_t num_read = retro_fread(file,
               (void*)buffer, sizeof(buffer));
         
         if (num_read <= 0)
            break;
         
         if (num_read >= (ssize_t)rom_size)
         {
            MD5_Update(&ctx, (void*)buffer, rom_size);
            break;
         }
         
         MD5_Update(&ctx, (void*)buffer, num_read);
         rom_size -= num_read;
      }
      
      retro_fclose(file);
   }
   
   to = timeout;
   return cheevos_get_game_id(hash, &to);
}


int cheevos_load(const void *data)
{
   retro_ctx_memory_info_t mem_info;

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
   
   static cheevos_finder_t finders[] =
   {
      {cheevos_find_game_id_snes,    "SNES (8Mb padding)",      snes_exts},
      {cheevos_find_game_id_genesis, "Genesis (6Mb padding)",   genesis_exts},
      {cheevos_find_game_id_nes,     "NES (discards VROM)",     NULL},
      {cheevos_find_game_id_generic, "Generic (plain content)", NULL},
   };
   
   size_t memory;
   struct retro_system_info sysinfo;
   unsigned i;
   const char *json;
   retro_time_t timeout = 5000000;
   unsigned game_id     = 0;
   settings_t *settings = config_get_ptr();
   const struct retro_game_info *info = (const struct retro_game_info*)data;
   
   cheevos_locals.loaded = 0;
   
   /* Just return OK if cheevos are disabled. */
   if (!settings->cheevos.enable)
      return 0;
   
   /* Also return OK if there's no content. */
   if (!info)
      return 0;

   mem_info.data = NULL;
   mem_info.size = 0;
   mem_info.id = RETRO_MEMORY_SYSTEM_RAM;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);
   
   memory  = mem_info.size;

   mem_info.data = NULL;
   mem_info.size = 0;
   mem_info.id   = RETRO_MEMORY_VIDEO_RAM;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   memory += mem_info.size;

   mem_info.data = NULL;
   mem_info.size = 0;
   mem_info.id   = RETRO_MEMORY_RTC;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   memory += mem_info.size;

   mem_info.data = NULL;
   mem_info.size = 0;
   mem_info.id   = RETRO_MEMORY_SAVE_RAM;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   memory += mem_info.size;

   if (!memory)
   {
      runloop_msg_queue_push("This core doesn't support achievements", 0, 5 * 60, false);
      RARCH_LOG("This core doesn't support achievements\n");
      return -1;
   }
   
   /* The the supported extensions as a hint to what method we should use. */
   
   core_ctl(CORE_CTL_RETRO_GET_SYSTEM_INFO, &sysinfo);
   
   for (i = 0; i < sizeof(finders) / sizeof(finders[0]); i++)
   {
      if (finders[i].ext_hashes)
      {
         const char *ext = sysinfo.valid_extensions;
         
         while (ext)
         {
            const char *end = strchr(ext, '|');
            unsigned hash;
            int j;
            
            if (end)
            {
               hash = cheevos_djb2(ext, end - ext);
               ext = end + 1;
            }
            else
            {
               hash = cheevos_djb2(ext, strlen(ext));
               ext = NULL;
            }
            
            for (j = 0; finders[i].ext_hashes[j]; j++)
            {
               if (finders[i].ext_hashes[j] == hash)
               {
                  RARCH_LOG("CHEEVOS testing %s\n", finders[i].name);
                  
                  game_id = finders[i].finder(info, 5000000);
                  
                  if (game_id)
                     goto found;
                  
                  ext = NULL; /* force next finder */
                  break;
               }
            }
         }
      }
   }
   
   for (i = 0; i < sizeof(finders) / sizeof(finders[0]); i++)
   {
      if (!finders[i].ext_hashes)
      {
         RARCH_LOG("CHEEVOS testing %s\n", finders[i].name);
         
         game_id = finders[i].finder(info, 5000000);
         
         if (game_id)
            goto found;
      }
   }
   
   runloop_msg_queue_push("This game doesn't feature achievements",
         0, 5 * 60, false);
   return -1;
   
   found:
   
   if (!cheevos_get_by_game_id(&json, game_id, &timeout))
   {
      if (!cheevos_parse(json))
      {
         cheevos_deactivate_unlocks(game_id, &timeout);
         free((void*)json);
         cheevos_locals.loaded = 1;
         
         rarch_main_async_job_add(cheevos_playing, (void*)(uintptr_t)game_id);
         return 0;
      }
      
      free((void*)json);
   }
   
   runloop_msg_queue_push("Error loading achievements", 0, 5 * 60, false);
   return -1;
}

#ifdef HAVE_MENU
void cheevos_populate_menu(void *data)
{
   unsigned i;
   const cheevo_t *end           = NULL;
   cheevo_t *cheevo              = NULL;
   settings_t *settings          = config_get_ptr();
   menu_displaylist_info_t *info = (menu_displaylist_info_t*)data;
   
   menu_entries_push(info->list, "Unlocked Achievements:",
         "", MENU_SETTINGS_CHEEVOS_NONE, 0, 0);
   menu_entries_push(info->list, "", "", MENU_SETTINGS_CHEEVOS_NONE, 0, 0);

   cheevo = cheevos_locals.core.cheevos;
   end    = cheevos_locals.core.cheevos + cheevos_locals.core.count;
   
   for (i = 0; cheevo < end; i++, cheevo++)
   {
      if (!cheevo->active)
         menu_entries_push(info->list, cheevo->title,
               cheevo->description, MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
   }
   
   if (settings->cheevos.test_unofficial)
   {
      cheevo = cheevos_locals.unofficial.cheevos;
      end    = cheevos_locals.unofficial.cheevos 
         + cheevos_locals.unofficial.count;

      for (i = cheevos_locals.core.count; cheevo < end; i++, cheevo++)
      {
         if (!cheevo->active)
            menu_entries_push(info->list, cheevo->title,
                  cheevo->description, MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
      }
   }
   
   menu_entries_push(info->list, "", "", MENU_SETTINGS_CHEEVOS_NONE, 0, 0);
   menu_entries_push(info->list, "Locked Achievements:", "", MENU_SETTINGS_CHEEVOS_NONE, 0, 0);
   menu_entries_push(info->list, "", "", MENU_SETTINGS_CHEEVOS_NONE, 0, 0);

   cheevo = cheevos_locals.core.cheevos;
   end    = cheevos_locals.core.cheevos + cheevos_locals.core.count;

   for (i = 0; cheevo < end; i++, cheevo++)
   {
      if (cheevo->active)
         menu_entries_push(info->list, cheevo->title,
               cheevo->description, MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
   }
   
   if (settings->cheevos.test_unofficial)
   {
      cheevo = cheevos_locals.unofficial.cheevos;
      end    = cheevos_locals.unofficial.cheevos 
         + cheevos_locals.unofficial.count;

      for (i = cheevos_locals.core.count; cheevo < end; i++, cheevo++)
      {
         if (cheevo->active)
            menu_entries_push(info->list, cheevo->title,
                  cheevo->description, MENU_SETTINGS_CHEEVOS_START + i, 0, 0);
      }
   }
}
#endif

void cheevos_get_description(unsigned idx, char *str, size_t len)
{
   cheevo_t *cheevos;
   
   if (idx >= cheevos_locals.core.count)
   {
      cheevos = cheevos_locals.unofficial.cheevos;
      idx    -= cheevos_locals.unofficial.count;
   }
   else
      cheevos = cheevos_locals.core.cheevos;
   
   strncpy(str, cheevos[idx].description, len);
   str[len - 1] = 0;
}

void cheevos_set_cheats(void)
{
   cheevos_globals.cheats_were_enabled = cheevos_globals.cheats_are_enabled;
}

void cheevos_apply_cheats(bool enable)
{
   cheevos_globals.cheats_are_enabled = enable;
   cheevos_globals.cheats_were_enabled |= cheevos_globals.cheats_are_enabled;
}
