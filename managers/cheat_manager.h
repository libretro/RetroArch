/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __CHEAT_MANAGER_H
#define __CHEAT_MANAGER_H

#include <boolean.h>
#include <retro_common_api.h>

#include "../setting_list.h"

RETRO_BEGIN_DECLS

enum cheat_handler_type
{
   CHEAT_HANDLER_TYPE_EMU = 0,
   CHEAT_HANDLER_TYPE_RETRO,
   CHEAT_HANDLER_TYPE_END
};

enum cheat_type
{
   CHEAT_TYPE_DISABLED = 0,
   CHEAT_TYPE_SET_TO_VALUE,
   CHEAT_TYPE_INCREASE_VALUE,
   CHEAT_TYPE_DECREASE_VALUE,
   CHEAT_TYPE_RUN_NEXT_IF_EQ,
   CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   CHEAT_TYPE_RUN_NEXT_IF_LT,
   CHEAT_TYPE_RUN_NEXT_IF_GT
};

enum cheat_search_type
{
   CHEAT_SEARCH_TYPE_EXACT = 0,
   CHEAT_SEARCH_TYPE_LT,
   CHEAT_SEARCH_TYPE_LTE,
   CHEAT_SEARCH_TYPE_GT,
   CHEAT_SEARCH_TYPE_GTE,
   CHEAT_SEARCH_TYPE_EQ,
   CHEAT_SEARCH_TYPE_NEQ,
   CHEAT_SEARCH_TYPE_EQPLUS,
   CHEAT_SEARCH_TYPE_EQMINUS
};

enum cheat_match_action_type
{
   CHEAT_MATCH_ACTION_TYPE_VIEW = 0,
   CHEAT_MATCH_ACTION_TYPE_DELETE,
   CHEAT_MATCH_ACTION_TYPE_COPY,
   CHEAT_MATCH_ACTION_TYPE_BROWSE
};

enum cheat_rumble_type
{
   RUMBLE_TYPE_DISABLED = 0,
   RUMBLE_TYPE_CHANGES,
   RUMBLE_TYPE_DOES_NOT_CHANGE,
   RUMBLE_TYPE_INCREASE,
   RUMBLE_TYPE_DECREASE,
   RUMBLE_TYPE_EQ_VALUE,
   RUMBLE_TYPE_NEQ_VALUE,
   RUMBLE_TYPE_LT_VALUE,
   RUMBLE_TYPE_GT_VALUE,
   RUMBLE_TYPE_INCREASE_BY_VALUE,
   RUMBLE_TYPE_DECREASE_BY_VALUE,
   RUMBLE_TYPE_END_LIST
};

/* Some codes are ridiculously large - over 10000 bytes */
#define CHEAT_CODE_SCRATCH_SIZE 16*1024
#define CHEAT_DESC_SCRATCH_SIZE 255

struct item_cheat
{
   unsigned int idx;
   char *desc;
   bool state;
   char *code;
   unsigned int handler;
   /* Number of bits = 2^memory_search_size
    * 0=1, 1=2, 2=4, 3=8, 4=16, 5=32
    */
   unsigned int memory_search_size;
   unsigned int cheat_type;
   unsigned int value;
   unsigned int address;
   /*
    * address_mask used when memory_search_size <8 bits
    * if memory_search_size=0, then the number of bits is 1 and this value can be one of the following:
    * 0 : 00000001
    * 1 : 00000010
    * 2 : 00000100
    * 3 : 00001000
    * 4 : 00010000
    * 5 : 00100000
    * 6 : 01000000
    * 7 : 10000000
    * if memory_search_size=1, then the number of bits is 2 and this value can be one of the following:
    * 0 : 00000011
    * 1 : 00001100
    * 2 : 00110000
    * 3 : 11000000
    * if memory_search_size=2, then the number of bits is 4 and this value can be one of the following:
    * 0 : 00001111
    * 1 : 11110000
    */
   unsigned int address_mask;
   /* Whether to apply the cheat based on big-endian console memory or not */
   bool big_endian;
   unsigned int rumble_type;
   unsigned int rumble_value;
   unsigned int rumble_prev_value;
   unsigned int rumble_initialized;
   unsigned int rumble_port; /* 0-15 for specific port, anything else means "all ports" */
   unsigned int rumble_primary_strength; /* 0-65535 */
   unsigned int rumble_primary_duration; /* in milliseconds */
   retro_time_t rumble_primary_end_time; /* clock value for when rumbling should stop */
   unsigned int rumble_secondary_strength; /* 0-65535 */
   unsigned int rumble_secondary_duration; /* in milliseconds */
   retro_time_t rumble_secondary_end_time; /* clock value for when rumbling should stop */

   /*
    * The repeat_ variables allow for a single cheat code to affect multiple memory addresses.
    * repeat_count - the number of times the cheat code should be applied
    * repeat_add_to_value - every iteration of repeat_count will have this amount added to item_cheat.value
    * repeat_add_to_address - every iteration of repeat_count will have this amount added to item_cheat.address
    *
    * Note that repeat_add_to_address represents the number of "memory_search_size" blocks to add to
    * item_cheat.address.  If memory_seach_size is 16-bits and repeat_add_to_address is 2, then item_cheat.address
    * will be increased by 4 bytes 2*(16-bits) for every iteration.
    *
    * This is a cheating structure used for codes like unlocking all levels, giving yourself 1 of every item,etc.
    */
   unsigned int repeat_count;
   unsigned int repeat_add_to_value;
   unsigned int repeat_add_to_address;

};

struct cheat_manager
{
   struct item_cheat *cheats;
   unsigned ptr;
   unsigned size;
   unsigned buf_size;
   unsigned total_memory_size;
   uint8_t *curr_memory_buf;
   uint8_t *prev_memory_buf;
   uint8_t *matches;
   uint8_t **memory_buf_list;
   unsigned *memory_size_list;
   unsigned num_memory_buffers;
   struct item_cheat working_cheat;
   unsigned match_idx;
   unsigned match_action;
   unsigned search_bit_size;
   unsigned dummy;
   unsigned search_exact_value;
   unsigned search_eqplus_value;
   unsigned search_eqminus_value;
   unsigned num_matches;
   bool  big_endian;
   bool  memory_initialized;
   bool  memory_search_initialized;
   unsigned int delete_state;
   unsigned browse_address;
   char working_desc[CHEAT_DESC_SCRATCH_SIZE];
   char working_code[CHEAT_CODE_SCRATCH_SIZE];
   unsigned int loading_cheat_size;
   unsigned int loading_cheat_offset;
};

typedef struct cheat_manager cheat_manager_t;

extern cheat_manager_t cheat_manager_state;

unsigned cheat_manager_get_size(void);

bool cheat_manager_load(const char *path, bool append);

/**
 * cheat_manager_save:
 * @path                      : Path to cheats file (absolute path).
 *
 * Saves cheats to file on disk.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool cheat_manager_save(const char *path,
      const char *cheat_database, bool overwrite);

bool cheat_manager_realloc(unsigned new_size, unsigned default_handler);

void cheat_manager_set_code(unsigned index, const char *str);

void cheat_manager_index_next(void);

void cheat_manager_index_prev(void);

void cheat_manager_toggle(void);

void cheat_manager_apply_cheats(void);

void cheat_manager_update(cheat_manager_t *handle, unsigned handle_idx);

void cheat_manager_toggle_index(unsigned i);

unsigned cheat_manager_get_buf_size(void);

const char *cheat_manager_get_desc(unsigned i);

const char *cheat_manager_get_code(unsigned i);

bool cheat_manager_get_code_state(unsigned i);

void cheat_manager_state_free(void);

bool cheat_manager_alloc_if_empty(void);

bool cheat_manager_copy_idx_to_working(unsigned idx);

bool cheat_manager_copy_working_to_idx(unsigned idx);

void cheat_manager_load_game_specific_cheats(void);

void cheat_manager_save_game_specific_cheats(void);

int cheat_manager_initialize_memory(rarch_setting_t *setting, bool wraparound);

int cheat_manager_search_exact(rarch_setting_t *setting, bool wraparound);

int cheat_manager_search_lt(rarch_setting_t *setting, bool wraparound);

int cheat_manager_search_gt(rarch_setting_t *setting, bool wraparound);

int cheat_manager_search_lte(rarch_setting_t *setting, bool wraparound);

int cheat_manager_search_gte(rarch_setting_t *setting, bool wraparound);

int cheat_manager_search_eq(rarch_setting_t *setting, bool wraparound);

int cheat_manager_search_neq(rarch_setting_t *setting, bool wraparound);

int cheat_manager_search_eqplus(rarch_setting_t *setting, bool wraparound);

int cheat_manager_search_eqminus(rarch_setting_t *setting, bool wraparound);

int cheat_manager_add_matches(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx);

void cheat_manager_apply_retro_cheats(void);

void cheat_manager_match_action(
      enum cheat_match_action_type match_action,
      unsigned int target_match_idx,
      unsigned int *address, unsigned int *address_mask,
      unsigned int *prev_value, unsigned int *curr_value);

int cheat_manager_copy_match(rarch_setting_t *setting, bool wraparound);

int cheat_manager_delete_match(rarch_setting_t *setting, bool wraparound);

RETRO_END_DECLS

#endif
