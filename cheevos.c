/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <formats/jsonsax.h>
#include <net/net_http.h>
#include <rhash.h>
#include <retro_log.h>

#include "cheevos.h"

#include "dynamic.h"
#include "runloop.h"
#include "performance.h"

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
   CHEEVOS_VAR_TYPE_ADDRESS,     /* compare to the value of a live address in RAM */
   CHEEVOS_VAR_TYPE_VALUE_COMP,  /* a number. assume 32 bit */
   CHEEVOS_VAR_TYPE_DELTA_MEM,   /* the value last known at this address. */
   CHEEVOS_VAR_TYPE_DYNAMIC_VAR, /* a custom user-set variable */

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
   CHEEVOS_DIRTY_TITLE			  = 1 << 0,
   CHEEVOS_DIRTY_DESC			  = 1 << 1,
   CHEEVOS_DIRTY_POINTS		     = 1 << 2,
   CHEEVOS_DIRTY_AUTHOR		     = 1 << 3,
   CHEEVOS_DIRTY_ID			     = 1 << 4,
   CHEEVOS_DIRTY_BADGE			  = 1 << 5,
   CHEEVOS_DIRTY_CONDITIONS     = 1 << 6,
   CHEEVOS_DIRTY_VOTES			  = 1 << 7,
   CHEEVOS_DIRTY_DESCRIPTION    = 1 << 8,

   CHEEVOS_DIRTY_ALL			     = ( 1 << 9 ) - 1
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

   const char *expression;
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

   cheevos_condset_t* condsets;
   unsigned count;
} cheevo_t;

typedef struct
{
   cheevo_t* cheevos;
   unsigned  count;
} cheevoset_t;

cheevos_config_t cheevos_config =
{
   /* enable          */ 1,
   /* test_unofficial */ 0,
   /* username        */ "libretro",
   /* password        */ "l1br3tro3456",
   /* token           */ { 0 },
   /* game_id         */ 0,
};

#ifdef HAVE_CHEEVOS

static cheevoset_t core_cheevos = { NULL, 0 };
static cheevoset_t unofficial_cheevos = { NULL, 0 };

/*****************************************************************************
  Supporting functions.
 *****************************************************************************/

static uint32_t cheevos_djb2( const char* str, size_t length )
{
   const unsigned char* aux = (const unsigned char*)str;
   const unsigned char* end = aux + length;
   uint32_t hash = 5381;

   while ( aux < end )
      hash = ( hash << 5 ) + hash + *aux++;

   return hash;
}

/*****************************************************************************
  Count number of achievements in a JSON file.
 *****************************************************************************/

typedef struct
{
   int      in_cheevos;
   uint32_t field_hash;
   unsigned core_count;
   unsigned unofficial_count;
} cheevos_countud_t;

static int count__json_end_array( void* userdata )
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;
   ud->in_cheevos = 0;
   return 0;
}

static int count__json_key( void* userdata, const char* name, size_t length )
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;
   ud->field_hash = cheevos_djb2( name, length );

   if ( ud->field_hash == 0x69749ae1U /* Achievements */ )
      ud->in_cheevos = 1;

   return 0;
}

static int count__json_number( void* userdata, const char* number, size_t length )
{
   cheevos_countud_t* ud = (cheevos_countud_t*)userdata;

   if ( ud->in_cheevos && ud->field_hash == 0x0d2e96b2U /* Flags */ )
   {
      long flags = strtol( number, NULL, 10 );

      if ( flags == 3 ) /* core achievements */
         ud->core_count++;
      else if ( flags == 5 ) /* unofficial achievements */
         ud->unofficial_count++;
   }

   return 0;
}

static int count_cheevos( const char* json, unsigned* core_count, unsigned* unofficial_count )
{
   int res;
   cheevos_countud_t ud;
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      count__json_end_array,
      count__json_key,
      NULL,
      NULL,
      count__json_number,
      NULL,
      NULL
   };

   ud.in_cheevos = 0;
   ud.core_count = 0;
   ud.unofficial_count = 0;

   res = jsonsax_parse( json, &handlers, (void*)&ud );

   *core_count = ud.core_count;
   *unofficial_count = ud.unofficial_count;

   return res;
}

/*****************************************************************************
  Parse the MemAddr field.
 *****************************************************************************/

static unsigned prefix_to_comp_size( char prefix )
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

static unsigned read_hits( const char** memaddr )
{
   char* end;
   const char* str = *memaddr;
   unsigned num_hits = 0;

   if ( *str == '(' || *str == '.' )
   {
      num_hits = strtol( str + 1, &end, 10 );
      str = end + 1;
   }

   *memaddr = str;
   return num_hits;
}

static unsigned parse_operator( const char** memaddr )
{
   unsigned char op;
   const char* str = *memaddr;

   if ( *str == '=' && str[ 1 ] == '=' )
   {
      op = CHEEVOS_COND_OP_EQUALS;
      str += 2;
   }
   else if ( *str == '=' )
   {
      op = CHEEVOS_COND_OP_EQUALS;
      str++;
   }
   else if ( *str == '!' && str[ 1 ] == '=' )
   {
      op = CHEEVOS_COND_OP_NOT_EQUAL_TO;
      str += 2;
   }
   else if ( *str == '<' && str[ 1 ] == '=' )
   {
      op = CHEEVOS_COND_OP_LESS_THAN_OR_EQUAL;
      str += 2;
   }
   else if ( *str == '<' )
   {
      op = CHEEVOS_COND_OP_LESS_THAN;
      str++;
   }
   else if ( *str == '>' && str[ 1 ] == '=' )
   {
      op = CHEEVOS_COND_OP_GREATER_THAN_OR_EQUAL;
      str += 2;
   }
   else if ( *str == '>' )
   {
      op = CHEEVOS_COND_OP_GREATER_THAN;
      str++;
   }
   else /* TODO log the exception */
      op = CHEEVOS_COND_OP_EQUALS;

   *memaddr = str;
   return op;
}

static void parse_var( cheevos_var_t* var, const char** memaddr )
{
   char* end;
   const char* str = *memaddr;
   unsigned base = 16;

   if ( toupper( *str ) == 'D' && str[ 1 ] == '0' && toupper( str[ 2 ] ) == 'X' )
   {
      /* d0x + 4 hex digits */
      str += 3;
      var->type = CHEEVOS_VAR_TYPE_DELTA_MEM;
   }
   else if ( *str == '0' && toupper( str[ 1 ] ) == 'X' )
   {
      /* 0x + 4 hex digits */
      str += 2;
      var->type = CHEEVOS_VAR_TYPE_ADDRESS;
   }
   else
   {
      var->type = CHEEVOS_VAR_TYPE_VALUE_COMP;

      if ( toupper( *str ) == 'H' )
         str++;
      else
         base = 10;
   }

   if ( var->type != CHEEVOS_VAR_TYPE_VALUE_COMP )
   {
      var->size = prefix_to_comp_size( *str );

      if ( var->size != CHEEVOS_VAR_SIZE_SIXTEEN_BITS )
         str++;
   }

   var->value = strtol( str, &end, base );
   *memaddr = end;
}

static void parse_cond( cheevos_cond_t* cond, const char** memaddr )
{
   const char* str = *memaddr;

   if ( *str == 'R' && str[ 1 ] == ':' )
   {
      cond->type = CHEEVOS_COND_TYPE_RESET_IF;
      str += 2;
   }
   else if ( *str == 'P' && str[ 1 ] == ':' )
   {
      cond->type = CHEEVOS_COND_TYPE_PAUSE_IF;
      str += 2;
   }
   else
      cond->type = CHEEVOS_COND_TYPE_STANDARD;

   parse_var( &cond->source, &str );
   cond->op = parse_operator( &str );
   parse_var( &cond->target, &str );
   cond->curr_hits = 0;
   cond->req_hits = read_hits( &str );

   *memaddr = str;
}

static unsigned count_cond_sets( const char* memaddr )
{
   cheevos_cond_t   cond;
   unsigned count = 0;

   do
   {
      do
      {
         while( *memaddr == ' ' || *memaddr == '_' || *memaddr == '|' || *memaddr == 'S' )
            memaddr++; /* Skip any chars up til the start of the achievement condition */

         parse_cond( &cond, &memaddr );
      }while( *memaddr == '_' || *memaddr == 'R' || *memaddr == 'P' ); /* AND, ResetIf, PauseIf */

      count++;
   }while( *memaddr == 'S' ); /* Repeat for all subconditions if they exist */

   return count;
}

static unsigned count_conds_in_set( const char* memaddr, unsigned set )
{
   cheevos_cond_t   cond;
   unsigned index = 0;
   unsigned count = 0;

   do
   {
      do
      {
         while ( *memaddr == ' ' || *memaddr == '_' || *memaddr == '|' || *memaddr == 'S' )
            memaddr++; /* Skip any chars up til the start of the achievement condition */

         parse_cond( &cond, &memaddr );

         if ( index == set )
            count++;
      }while ( *memaddr == '_' || *memaddr == 'R' || *memaddr == 'P' ); /* AND, ResetIf, PauseIf */
   }while( *memaddr == 'S' ); /* Repeat for all subconditions if they exist */

   return count;
}

static void parse_memaddr( cheevos_cond_t* cond, const char* memaddr )
{
   do
   {
      do
      {
         while( *memaddr == ' ' || *memaddr == '_' || *memaddr == '|' || *memaddr == 'S' )
            memaddr++; /* Skip any chars up til the start of the achievement condition */

         parse_cond( cond++, &memaddr );
      }while( *memaddr == '_' || *memaddr == 'R' || *memaddr == 'P' ); /* AND, ResetIf, PauseIf */
   }while( *memaddr == 'S' ); /* Repeat for all subconditions if they exist */
}

/*****************************************************************************
  Load achievements from a JSON string.
 *****************************************************************************/

typedef struct
{
   const char* string;
   size_t      length;
} cheevos_field_t;

typedef struct
{
   int      in_cheevos;
   unsigned core_count;
   unsigned unofficial_count;

   cheevos_field_t* field;
   cheevos_field_t  id, memaddr, title, desc, points, author;
   cheevos_field_t  modified, created, badge, flags;
} cheevos_readud_t;

static inline const char* dupstr( const cheevos_field_t* field )
{
   char* string = (char*)malloc( field->length + 1 );

   if ( string )
   {
      memcpy( (void*)string, (void*)field->string, field->length );
      string[ field->length ] = 0;
   }

   return string;
}

static int new_cheevo( cheevos_readud_t* ud )
{
   const cheevos_condset_t* end;
   unsigned set;
   cheevos_condset_t* condset;
   cheevo_t* cheevo;
   int flags = strtol( ud->flags.string, NULL, 10 );

   if ( flags == 3 )
      cheevo = core_cheevos.cheevos + ud->core_count++;
   else
      cheevo = unofficial_cheevos.cheevos + ud->unofficial_count++;

   cheevo->id = strtol( ud->id.string, NULL, 10 );
   cheevo->title = dupstr( &ud->title );
   cheevo->description = dupstr( &ud->desc );
   cheevo->author = dupstr( &ud->author );
   cheevo->badge = dupstr( &ud->badge );
   cheevo->points = strtol( ud->points.string, NULL, 10 );
   cheevo->dirty = 0;
   cheevo->active = flags == 3;
   cheevo->modified = 0;

   if ( !cheevo->title || !cheevo->description || !cheevo->author || !cheevo->badge )
   {
      free( (void*)cheevo->title );
      free( (void*)cheevo->description );
      free( (void*)cheevo->author );
      free( (void*)cheevo->badge );
      return -1;
   }

   cheevo->count = count_cond_sets( ud->memaddr.string );

   if ( cheevo->count )
   {
      cheevo->condsets = (cheevos_condset_t*)malloc( cheevo->count * sizeof( cheevos_condset_t ) );

      if ( !cheevo->condsets )
         return -1;

      memset( (void*)cheevo->condsets, 0, cheevo->count * sizeof( cheevos_condset_t ) );
      end = cheevo->condsets + cheevo->count;
      set = 0;

      for ( condset = cheevo->condsets; condset < end; condset++ )
      {
         condset->count = count_conds_in_set( ud->memaddr.string, set++ );
         condset->conds = NULL;

         if ( condset->count )
         {
            condset->conds = (cheevos_cond_t*)malloc( condset->count * sizeof( cheevos_cond_t ) );

            if ( !condset->conds )
               return -1;

            memset( (void*)condset->conds, 0, condset->count * sizeof( cheevos_cond_t ) );
            condset->expression = dupstr( &ud->memaddr );
            parse_memaddr( condset->conds, ud->memaddr.string );
         }
      }
   }

   return 0;
}

static int read__json_key( void* userdata, const char* name, size_t length )
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;
   uint32_t hash = cheevos_djb2( name, length );

   ud->field = NULL;

   if ( hash == 0x69749ae1U /* Achievements */ )
      ud->in_cheevos = 1;
   else if ( ud->in_cheevos )
   {
      switch ( hash )
      {
         case 0x005973f2U: /* ID */
            ud->field = &ud->id;
            break;
         case 0x1e76b53fU: /* MemAddr     */
            ud->field = &ud->memaddr;
            break;
         case 0x0e2a9a07U: /* Title       */
            ud->field = &ud->title;
            break;
         case 0xe61a1f69U: /* Description */
            ud->field = &ud->desc;
            break;
         case 0xca8fce22U: /* Points      */
            ud->field = &ud->points;
            break;
         case 0xa804edb8U: /* Author      */
            ud->field = &ud->author;
            break;
         case 0xdcea4fe6U: /* Modified    */
            ud->field = &ud->modified;
            break;
         case 0x3a84721dU: /* Created     */
            ud->field = &ud->created;
            break;
         case 0x887685d9U: /* BadgeName   */
            ud->field = &ud->badge;
            break;
         case 0x0d2e96b2U: /* Flags       */
            ud->field = &ud->flags;
            break;
      }
   }

   return 0;
}

static int read__json_string( void* userdata, const char* string, size_t length )
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;

   if ( ud->field )
   {
      ud->field->string = string;
      ud->field->length = length;
   }

   return 0;
}

static int read__json_number( void* userdata, const char* number, size_t length )
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;

   if ( ud->field )
   {
      ud->field->string = number;
      ud->field->length = length;
   }

   return 0;
}

static int read__json_end_object( void* userdata )
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;

   if ( ud->in_cheevos )
      return new_cheevo( ud );

   return 0;
}

static int read__json_end_array( void* userdata )
{
   cheevos_readud_t* ud = (cheevos_readud_t*)userdata;
   ud->in_cheevos = 0;
   return 0;
}

int cheevos_load( const char* json )
{
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      read__json_end_object,
      NULL,
      read__json_end_array,
      read__json_key,
      NULL,
      read__json_string,
      read__json_number,
      NULL,
      NULL
   };

   /* Count the number of achievements in the JSON file. */

   unsigned core_count, unofficial_count;

   if ( count_cheevos( json, &core_count, &unofficial_count ) != JSONSAX_OK )
      return -1;

   /* Allocate the achievements. */

   core_cheevos.cheevos = (cheevo_t*)malloc( core_count * sizeof( cheevo_t ) );
   core_cheevos.count = core_count;

   unofficial_cheevos.cheevos = (cheevo_t*)malloc( unofficial_count * sizeof( cheevo_t ) );
   unofficial_cheevos.count = unofficial_count;

   if ( !core_cheevos.cheevos || !unofficial_cheevos.cheevos )
   {
      free( (void*)core_cheevos.cheevos );
      free( (void*)unofficial_cheevos.cheevos );
      core_cheevos.count = unofficial_cheevos.count = 0;

      return -1;
   }

   memset( (void*)core_cheevos.cheevos, 0, core_count * sizeof( cheevo_t ) );
   memset( (void*)unofficial_cheevos.cheevos, 0, unofficial_count * sizeof( cheevo_t ) );

   /* Load the achievements. */

   cheevos_readud_t ud;
   ud.in_cheevos = 0;
   ud.field = NULL;
   ud.core_count = 0;
   ud.unofficial_count = 0;

   if ( jsonsax_parse( json, &handlers, (void*)&ud ) == JSONSAX_OK )
      return 0;

   cheevos_unload();
   return -1;
}

/*****************************************************************************
  Test all the achievements (call once per frame).
 *****************************************************************************/

static unsigned get_var_value( cheevos_var_t* var )
{
   uint8_t* memory;
   unsigned previous = var->previous;
   unsigned live_val = 0;

   if ( var->type == CHEEVOS_VAR_TYPE_VALUE_COMP )
      return var->value;

   if ( var->type == CHEEVOS_VAR_TYPE_ADDRESS || var->type == CHEEVOS_VAR_TYPE_DELTA_MEM )
   {
      /* TODO Check with Scott if the bank id is needed */
      /* memory = (uint8_t*)core.retro_get_memory_data( var->bank_id ); */
      memory = (uint8_t*)core.retro_get_memory_data( RETRO_MEMORY_SYSTEM_RAM );
      live_val = memory[ var->value ];

      if ( var->size >= CHEEVOS_VAR_SIZE_BIT_0 && var->size <= CHEEVOS_VAR_SIZE_BIT_7 )
         live_val = ( live_val & ( 1 << ( var->size - CHEEVOS_VAR_SIZE_BIT_0 ) ) ) != 0;
      else if ( var->size == CHEEVOS_VAR_SIZE_NIBBLE_LOWER )
         live_val &= 0x0f;
      else if ( var->size == CHEEVOS_VAR_SIZE_NIBBLE_UPPER )
         live_val = ( live_val >> 4 ) & 0x0f;
      else if ( var->size == CHEEVOS_VAR_SIZE_EIGHT_BITS ) { }
      else if ( var->size == CHEEVOS_VAR_SIZE_SIXTEEN_BITS )
         live_val |= memory[ var->value + 1 ] << 8;
      else if ( var->size == CHEEVOS_VAR_SIZE_THIRTYTWO_BITS )
      {
         live_val |= memory[ var->value + 1 ] << 8;
         live_val |= memory[ var->value + 2 ] << 16;
         live_val |= memory[ var->value + 3 ] << 24;
      }

      if ( var->type == CHEEVOS_VAR_TYPE_DELTA_MEM )
      {
         var->previous = live_val;
         return previous;
      }

      return live_val;
   }

   /* We shouldn't get here... */
   return 0;
}

static int test_condition( cheevos_cond_t* cond )
{
   unsigned sval = get_var_value( &cond->source );
   unsigned tval = get_var_value( &cond->target );

   switch ( cond->op )
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
         break;
   }

   return 1;
}

static int test_cond_set( const cheevos_condset_t* condset, int* dirty_conds, int* reset_conds, int match_any )
{
   cheevos_cond_t* cond;
   int cond_valid   = 0;
   int set_valid    = 1;
   int pause_active = 0;
   const cheevos_cond_t* end = condset->conds + condset->count;

   /* Now, read all Pause conditions, and if any are true, do not process further (retain old state) */
   for ( cond = condset->conds; cond < end; cond++ )
   {
      if ( cond->type == CHEEVOS_COND_TYPE_PAUSE_IF )
      {
         /* Reset by default, set to 1 if hit! */
         cond->curr_hits = 0;

         if ( test_condition( cond ) )
         {
            cond->curr_hits = 1;
            *dirty_conds = 1;

            /* Early out: this achievement is paused, do not process any further! */
            return 0;
         }
      }
   }

   /* Read all standard conditions, and process as normal: */
   for ( cond = condset->conds; cond < end; cond++ )
   {
      if ( cond->type == CHEEVOS_COND_TYPE_PAUSE_IF || cond->type == CHEEVOS_COND_TYPE_RESET_IF )
         continue;

      if ( cond->req_hits != 0 && cond->curr_hits >= cond->req_hits )
         continue;

      cond_valid = test_condition( cond );

      if ( cond_valid )
      {
         cond->curr_hits++;
         *dirty_conds = 1;

         /* Process this logic, if this condition is true: */
         if ( cond->req_hits == 0 ) 
         {
            /* Not a hit-based requirement: ignore any additional logic! */
         }
         else if ( cond->curr_hits < cond->req_hits )
         {
            /* Not entirely valid yet! */
            cond_valid = 0;
         }

         if ( match_any )
            break;
      }

      /* Sequential or non-sequential? */
      set_valid &= cond_valid;
   }

   /* Now, ONLY read reset conditions! */
   for ( cond = condset->conds; cond < end; cond++ )
   {
      if ( cond->type == CHEEVOS_COND_TYPE_RESET_IF )
      {
         cond_valid = test_condition( cond );

         if ( cond_valid )
         {
            *reset_conds = 1; /* Resets all hits found so far */
            set_valid = 0;   /* Cannot be valid if we've hit a reset condition. */
            break;           /* No point processing any further reset conditions. */
         }
      }
   }

   return set_valid;
}

static int reset_cond_set( cheevos_condset_t* condset, int deltas )
{
   cheevos_cond_t* cond;
   int dirty = 0;
   const cheevos_cond_t* end = condset->conds + condset->count;

   if ( deltas )
   {
      for ( cond = condset->conds; cond < end; cond++ )
      {
         dirty |= cond->curr_hits != 0;
         cond->curr_hits = 0;

         cond->source.previous = cond->source.value;
         cond->target.previous = cond->target.value;
      }
   }
   else
   {
      for ( cond = condset->conds; cond < end; cond++ )
      {
         dirty |= cond->curr_hits != 0;
         cond->curr_hits = 0;
      }
   }

   return dirty;
}

static int test_cheevo( cheevo_t* cheevo )
{
   int dirty;
   int dirty_conds = 0;
   int reset_conds = 0;
   int ret_val = 0;
   int ret_val_sub_cond = cheevo->count == 1;
   cheevos_condset_t* condset = cheevo->condsets;
   const cheevos_condset_t* end = condset + cheevo->count;

   if ( condset < end )
   {
      ret_val = test_cond_set( condset, &dirty_conds, &reset_conds, 0 );
      if ( ret_val ) RARCH_LOG( "CHEEVOS %s\n", condset->expression );
      condset++;
   }

   while ( condset < end )
   {
      int res = test_cond_set( condset, &dirty_conds, &reset_conds, 0 );
      ret_val_sub_cond |= res;
      if ( res ) RARCH_LOG( "CHEEVOS %s\n", condset->expression );
      condset++;
   }

   if ( dirty_conds )
      cheevo->dirty |= CHEEVOS_DIRTY_CONDITIONS;

   if ( reset_conds )
   {
      dirty = 0;

      for ( condset = cheevo->condsets; condset < end; condset++ )
         dirty |= reset_cond_set( condset, 0 );

      if ( dirty )
         cheevo->dirty |= CHEEVOS_DIRTY_CONDITIONS;
   }

   return ret_val && ret_val_sub_cond;
}

static void test_cheevo_set( const cheevoset_t* set )
{
   cheevo_t* cheevo;
   const cheevo_t* end = set->cheevos + set->count;

   for ( cheevo = set->cheevos; cheevo < end; cheevo++ )
   {
      if ( cheevo->active && test_cheevo( cheevo ) )
      {
         RARCH_LOG( "CHEEVOS %s\n", cheevo->title );
         RARCH_LOG( "CHEEVOS %s\n", cheevo->description );

         rarch_main_msg_queue_push( cheevo->title, 0, 3 * 60, false );
         rarch_main_msg_queue_push( cheevo->description, 0, 5 * 60, false );

         cheevo->active = 0;
      }
   }
}

void cheevos_test(void)
{
   if ( cheevos_config.enable )
   {
      test_cheevo_set( &core_cheevos );

      if ( cheevos_config.test_unofficial )
         test_cheevo_set( &unofficial_cheevos );
   }
}

/*****************************************************************************
  Free the loaded achievements.
 *****************************************************************************/

static void free_condset( const cheevos_condset_t* set )
{
   free( (void*)set->conds );
}

static void free_cheevo( const cheevo_t* cheevo )
{
   free( (void*)cheevo->title );
   free( (void*)cheevo->description );
   free( (void*)cheevo->author );
   free( (void*)cheevo->badge );
   free_condset( cheevo->condsets );
   free( (void*)cheevo );
}

static void free_cheevo_set( const cheevoset_t* set )
{
   const cheevo_t* cheevo = set->cheevos;
   const cheevo_t* end = cheevo + set->count;

   while ( cheevo < end )
      free_cheevo( cheevo++ );

   free( (void*)set->cheevos );
}

void cheevos_unload(void)
{
   free_cheevo_set( &core_cheevos );
   free_cheevo_set( &unofficial_cheevos );
}

/*****************************************************************************
  Load achievements from retroachievements.org.
 *****************************************************************************/

static const char* cheevos_http_get( const char* url, size_t* size )
{
#ifdef HAVE_NETWORKING
   struct http_t* http;
   uint8_t* data;
   size_t length;
   char *result;
   struct http_connection_t *conn = net_http_connection_new( url );

   RARCH_LOG( "CHEEVOS http get %s\n", url );

   if ( !conn )
      return NULL;

   while ( !net_http_connection_iterate( conn ) ) { }

   if ( !net_http_connection_done( conn ) )
      goto error;

   http = net_http_new( conn );

   if ( !http )
      goto error1;

   while ( !net_http_update( http, NULL, NULL ) ) { }

   data   = net_http_data( http, &length, false );
   result = NULL;

   if ( data )
   {
      result = (char*)malloc( length + 1 );
      memcpy( (void*)result, (void*)data, length );
      result[ length ] = 0;
   }

   net_http_delete( http );
   net_http_connection_free( conn );

   RARCH_LOG( "CHEEVOS http result is %s\n", result );

   if ( size )
      *size = length;

   return (char*)result;

error1:
   net_http_connection_free( conn );
   return NULL;

#else /* HAVE_NETWORKING */

   RARCH_LOG( "CHEEVOS http get %s\n", url );
   RARCH_ERROR( "CHEEVOS Network unavailable\n" );

   if ( size )
      *size = 0;

   return NULL;

#endif /* HAVE_NETWORKING */
}

typedef struct
{
   unsigned    key_hash;
   int         is_key;
   const char* value;
   size_t      length;
}
cheevo_getvalueud_t;

static int getvalue__json_key( void* userdata, const char* name, size_t length )
{
   cheevo_getvalueud_t* ud = (cheevo_getvalueud_t*)userdata;

   ud->is_key = cheevos_djb2( name, length ) == ud->key_hash;
   return 0;
}

static int getvalue__json_string( void* userdata, const char* string, size_t length )
{
   cheevo_getvalueud_t* ud = (cheevo_getvalueud_t*)userdata;

   if ( ud->is_key )
   {
      ud->value = string;
      ud->length = length;
      ud->is_key = 0;
   }

   return 0;
}

static int getvalue__json_boolean( void* userdata, int istrue )
{
   cheevo_getvalueud_t* ud = (cheevo_getvalueud_t*)userdata;

   if ( ud->is_key )
   {
      ud->value = istrue ? "true" : "false";
      ud->length = istrue ? 4 : 5;
      ud->is_key = 0;
   }

   return 0;
}

static int getvalue__json_null( void* userdata )
{
   cheevo_getvalueud_t* ud = (cheevo_getvalueud_t*)userdata;

   if ( ud->is_key )
   {
      ud->value = "null";
      ud->length = 4;
      ud->is_key = 0;
   }

   return 0;
}

static int cheevos_get_value( const char* json, unsigned key_hash, char* value, size_t length )
{
   static const jsonsax_handlers_t handlers =
   {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      getvalue__json_key,
      NULL,
      getvalue__json_string,
      getvalue__json_string, /* number */
      getvalue__json_boolean,
      getvalue__json_null
   };

   cheevo_getvalueud_t ud;

   ud.key_hash = key_hash;
   ud.is_key = 0;
   ud.length = 0;
   *value = 0;

   if ( jsonsax_parse( json, &handlers, (void*)&ud ) == JSONSAX_OK && ud.length < length )
   {
      strncpy( value, ud.value, length );
      value[ ud.length ] = 0;
      return 0;
   }

   return -1;
}

static int cheevos_login(void)
{
   char request[ 256 ];
   const char* json;
   cheevo_getvalueud_t ud;
   int res = 0;

   if ( cheevos_config.token[ 0 ] == 0 )
   {
      cheevos_config.token[ 0 ] = 0;

      snprintf(
            request, sizeof( request ),
            "http://retroachievements.org/dorequest.php?r=login&u=%s&p=%s",
            cheevos_config.username, cheevos_config.password
            );

      request[ sizeof( request ) - 1 ] = 0;

      json = cheevos_http_get( request, NULL );

      if ( !json )
         return -1;

      res = cheevos_get_value( json, 0x0e2dbd26U /* Token */, cheevos_config.token, sizeof( cheevos_config.token ) );
   }

   RARCH_LOG( "CHEEVOS user token is %s\n", cheevos_config.token );
   return res;
}

int cheevos_get_by_game_id( const char** json, unsigned game_id )
{
   char request[ 256 ];

   cheevos_login();

   snprintf(
         request, sizeof( request ),
         "http://retroachievements.org/dorequest.php?r=patch&u=%s&g=%u&f=3&l=1&t=%s",
         cheevos_config.username, game_id, cheevos_config.token
         );

   request[ sizeof( request ) - 1 ] = 0;

   *json = cheevos_http_get( request, NULL );

   if ( !*json )
      return -1;

   return 0;
}

static unsigned cheevos_get_game_id( unsigned char* hash )
{
   MD5_CTX ctx;
   char request[ 256 ];
   const char* json;
   char game_id[ 16 ];
   int res;

   snprintf(
         request, sizeof( request ),
         "http://retroachievements.org/dorequest.php?r=gameid&u=%s&m=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
         cheevos_config.username,
         hash[ 0 ], hash[ 1 ], hash[ 2 ], hash[ 3 ],
         hash[ 4 ], hash[ 5 ], hash[ 6 ], hash[ 7 ],
         hash[ 8 ], hash[ 9 ], hash[ 10 ], hash[ 11 ],
         hash[ 12 ], hash[ 13 ], hash[ 14 ], hash[ 15 ]
         );

   request[ sizeof( request ) - 1 ] = 0;

   json = cheevos_http_get( request, NULL );

   if ( !json )
      return -1;

   res = cheevos_get_value( json, 0xb4960eecU /* GameID */, game_id, sizeof( game_id ) );
   free( (void*)json );

   return res ? 0 : strtoul( game_id, NULL, 10 );
}

#define CHEEVOS_EIGHT_MB ( 8 * 1024 * 1024 )

int cheevos_get_by_content( const char** json, const void* data, size_t size )
{
   MD5_CTX ctx, saved_ctx;
   size_t len;
   unsigned char hash[ 16 ];
   char request[ 256 ];
   char buffer[ 4096 ];
   unsigned game_id;
   int res;

   MD5_Init( &ctx );
   MD5_Update( &ctx, data, size );
   saved_ctx = ctx;
   MD5_Final( hash, &ctx );

   game_id = cheevos_get_game_id( hash );

   if ( !game_id && size < CHEEVOS_EIGHT_MB )
   {
      /* Maybe the content is a SNES game, continue MD5 with zeroes up to 8MB. */
      size = CHEEVOS_EIGHT_MB - size;
      memset( (void*)buffer, 0, sizeof( buffer ) );

      do
      {
         len = sizeof( buffer );

         if ( len > size )
            len = size;

         MD5_Update( &saved_ctx, (void*)buffer, len );
         size -= len;
      }while(size);

      MD5_Final( hash, &saved_ctx );

      game_id = cheevos_get_game_id( hash );

      if ( !game_id )
         return -1;
   }

   RARCH_LOG( "CHEEVOS game id is %u\n", game_id );
   return cheevos_get_by_game_id( json, game_id );
}

#else /* HAVE_CHEEVOS */

int cheevos_load( const char* json )
{
   return -1;
}

void cheevos_test(void)
{
}

void cheevos_unload(void)
{
}

int cheevos_get_by_game_id( const char** json, unsigned game_id )
{
   *json = "{}";
   return -1;
}

int cheevos_get_by_content( const char** json, const void* data, size_t size )
{
   *json = "{}";
   return -1;
}

#endif /* HAVE_CHEEVOS */
