#ifndef RCHEEVOS_H
#define RCHEEVOS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lua_State lua_State;

/*****************************************************************************\
| Return values                                                               |
\*****************************************************************************/

enum {
  RC_OK = 0,
  RC_INVALID_LUA_OPERAND = -1,
  RC_INVALID_MEMORY_OPERAND = -2,
  RC_INVALID_CONST_OPERAND = -3,
  RC_INVALID_FP_OPERAND = -4,
  RC_INVALID_CONDITION_TYPE = -5,
  RC_INVALID_OPERATOR = -6,
  RC_INVALID_REQUIRED_HITS = -7,
  RC_DUPLICATED_START = -8,
  RC_DUPLICATED_CANCEL = -9,
  RC_DUPLICATED_SUBMIT = -10,
  RC_DUPLICATED_VALUE = -11,
  RC_DUPLICATED_PROGRESS = -12,
  RC_MISSING_START = -13,
  RC_MISSING_CANCEL = -14,
  RC_MISSING_SUBMIT = -15,
  RC_MISSING_VALUE = -16,
  RC_INVALID_LBOARD_FIELD = -17,
  RC_MISSING_DISPLAY_STRING = -18,
  RC_OUT_OF_MEMORY = -19,
  RC_INVALID_VALUE_FLAG = -20,
  RC_MISSING_VALUE_MEASURED = -21,
  RC_MULTIPLE_MEASURED = -22,
  RC_INVALID_MEASURED_TARGET = -23
};

/*****************************************************************************\
| Console identifiers                                                         |
\*****************************************************************************/

enum {
  RC_CONSOLE_MEGA_DRIVE = 1,
  RC_CONSOLE_NINTENDO_64 = 2,
  RC_CONSOLE_SUPER_NINTENDO = 3,
  RC_CONSOLE_GAMEBOY = 4,
  RC_CONSOLE_GAMEBOY_ADVANCE = 5,
  RC_CONSOLE_GAMEBOY_COLOR = 6,
  RC_CONSOLE_NINTENDO = 7,
  RC_CONSOLE_PC_ENGINE = 8,
  RC_CONSOLE_SEGA_CD = 9,
  RC_CONSOLE_SEGA_32X = 10,
  RC_CONSOLE_MASTER_SYSTEM = 11,
  RC_CONSOLE_PLAYSTATION = 12,
  RC_CONSOLE_ATARI_LYNX = 13,
  RC_CONSOLE_NEOGEO_POCKET = 14,
  RC_CONSOLE_GAME_GEAR = 15,
  RC_CONSOLE_GAMECUBE = 16,
  RC_CONSOLE_ATARI_JAGUAR = 17,
  RC_CONSOLE_NINTENDO_DS = 18,
  RC_CONSOLE_WII = 19,
  RC_CONSOLE_WII_U = 20,
  RC_CONSOLE_PLAYSTATION_2 = 21,
  RC_CONSOLE_XBOX = 22,
  RC_CONSOLE_SKYNET = 23,
  RC_CONSOLE_XBOX_ONE = 24,
  RC_CONSOLE_ATARI_2600 = 25,
  RC_CONSOLE_MS_DOS = 26,
  RC_CONSOLE_ARCADE = 27,
  RC_CONSOLE_VIRTUAL_BOY = 28,
  RC_CONSOLE_MSX = 29,
  RC_CONSOLE_COMMODORE_64 = 30,
  RC_CONSOLE_ZX81 = 31
};

/*****************************************************************************\
| Callbacks                                                                   |
\*****************************************************************************/

/**
 * Callback used to read num_bytes bytes from memory starting at address. If
 * num_bytes is greater than 1, the value is read in little-endian from
 * memory.
 */
typedef unsigned (*rc_peek_t)(unsigned address, unsigned num_bytes, void* ud);

/*****************************************************************************\
| Memory References                                                           |
\*****************************************************************************/

/* Sizes. */
enum {
  RC_MEMSIZE_BIT_0,
  RC_MEMSIZE_BIT_1,
  RC_MEMSIZE_BIT_2,
  RC_MEMSIZE_BIT_3,
  RC_MEMSIZE_BIT_4,
  RC_MEMSIZE_BIT_5,
  RC_MEMSIZE_BIT_6,
  RC_MEMSIZE_BIT_7,
  RC_MEMSIZE_LOW,
  RC_MEMSIZE_HIGH,
  RC_MEMSIZE_8_BITS,
  RC_MEMSIZE_16_BITS,
  RC_MEMSIZE_24_BITS,
  RC_MEMSIZE_32_BITS
};

typedef struct {
  /* The memory address of this variable. */
  unsigned address;
  /* The size of the variable. */
  char size;
  /* True if the value is in BCD. */
  char is_bcd;
  /* True if the reference will be used in indirection */
  char is_indirect;
} rc_memref_t;

typedef struct rc_memref_value_t rc_memref_value_t;

struct rc_memref_value_t {
  /* The value of this memory reference. */
  unsigned value;
  /* The previous value of this memory reference. */
  unsigned previous;
  /* The last differing value of this memory reference. */
  unsigned prior;

  /* The referenced memory */
  rc_memref_t memref;

  /* The next memory reference in the chain */
  rc_memref_value_t* next;
};

/*****************************************************************************\
| Operands                                                                    |
\*****************************************************************************/

/* types */
enum {
  RC_OPERAND_ADDRESS, /* Compare to the value of a live address in RAM. */
  RC_OPERAND_DELTA,   /* The value last known at this address. */
  RC_OPERAND_CONST,   /* A 32-bit unsigned integer. */
  RC_OPERAND_FP,      /* A floating point value. */
  RC_OPERAND_LUA,     /* A Lua function that provides the value. */
  RC_OPERAND_PRIOR    /* The last differing value at this address. */
};

typedef struct {
  union {
    /* A value read from memory. */
    rc_memref_value_t* memref;

    /* An integer value. */
    unsigned num;

    /* A floating point value. */
    double dbl;

    /* A reference to the Lua function that provides the value. */
    int luafunc;
  } value;

  char type;
}
rc_operand_t;

/*****************************************************************************\
| Conditions                                                                  |
\*****************************************************************************/

/* types */
enum {
  RC_CONDITION_STANDARD,
  RC_CONDITION_PAUSE_IF,
  RC_CONDITION_RESET_IF,
  RC_CONDITION_ADD_SOURCE,
  RC_CONDITION_SUB_SOURCE,
  RC_CONDITION_ADD_HITS,
  RC_CONDITION_AND_NEXT,
  RC_CONDITION_MEASURED,
  RC_CONDITION_ADD_ADDRESS
};

/* operators */
enum {
  RC_CONDITION_EQ,
  RC_CONDITION_LT,
  RC_CONDITION_LE,
  RC_CONDITION_GT,
  RC_CONDITION_GE,
  RC_CONDITION_NE,
  RC_CONDITION_NONE
};

typedef struct rc_condition_t rc_condition_t;

struct rc_condition_t {
  /* The condition's operands. */
  rc_operand_t operand1;
  rc_operand_t operand2;

  /* Required hits to fire this condition. */
  unsigned required_hits;
  /* Number of hits so far. */
  unsigned current_hits;

  /* The next condition in the chain. */
  rc_condition_t* next;

  /* The type of the condition. */
  char type;

  /* The comparison operator to use. */
  char oper; /* operator is a reserved word in C++. */

  /* Set if the condition needs to processed as part of the "check if paused" pass. */
  char pause;

  /* Whether or not the condition evaluated true on the last check */
  char is_true;
};

/*****************************************************************************\
| Condition sets                                                              |
\*****************************************************************************/

typedef struct rc_condset_t rc_condset_t;

struct rc_condset_t {
  /* The next condition set in the chain. */
  rc_condset_t* next;

  /* The list of conditions in this condition set. */
  rc_condition_t* conditions;

  /* True if any condition in the set is a pause condition. */
  char has_pause;

  /* True if the set is currently paused. */
  char is_paused;
};

/*****************************************************************************\
| Trigger                                                                     |
\*****************************************************************************/

enum {
  RC_TRIGGER_STATE_INACTIVE,   /* achievement is not being processed */
  RC_TRIGGER_STATE_WAITING,    /* achievement cannot trigger until it has been false for at least one frame */
  RC_TRIGGER_STATE_ACTIVE,     /* achievement is active and may trigger */
  RC_TRIGGER_STATE_PAUSED,     /* achievement is currently paused and will not trigger */
  RC_TRIGGER_STATE_RESET,      /* achievement hit counts were reset */
  RC_TRIGGER_STATE_TRIGGERED   /* achievement has triggered */
};

typedef struct {
  /* The main condition set. */
  rc_condset_t* requirement;

  /* The list of sub condition sets in this test. */
  rc_condset_t* alternative;

  /* The memory references required by the trigger. */
  rc_memref_value_t* memrefs;

  /* The current state of the MEASURED condition. */
  unsigned measured_value;

  /* The target state of the MEASURED condition */
  unsigned measured_target;

  /* The current state of the trigger */
  char state;

  /* True if at least one condition has a non-zero hit count */
  char has_hits;
}
rc_trigger_t;

int rc_trigger_size(const char* memaddr);
rc_trigger_t* rc_parse_trigger(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx);
int rc_evaluate_trigger(rc_trigger_t* trigger, rc_peek_t peek, void* ud, lua_State* L);
int rc_test_trigger(rc_trigger_t* trigger, rc_peek_t peek, void* ud, lua_State* L);
void rc_reset_trigger(rc_trigger_t* self);

/*****************************************************************************\
| Expressions and values                                                      |
\*****************************************************************************/

typedef struct rc_term_t rc_term_t;

struct rc_term_t {
  /* The next term in this chain. */
  rc_term_t* next;

  /* The first operand. */
  rc_operand_t operand1;
  /* The second operand. */
  rc_operand_t operand2;

  /* A value that is applied to the second variable to invert its bits. */
  unsigned invert;
};

typedef struct rc_expression_t rc_expression_t;

struct rc_expression_t {
  /* The next expression in this chain. */
  rc_expression_t* next;

  /* The list of terms in this expression. */
  rc_term_t* terms;
};

typedef struct {
  /* The list of expression to evaluate. */
  rc_expression_t* expressions;

  /* The list of conditions to evaluate. */
  rc_condset_t* conditions;

  /* The memory references required by the value. */
  rc_memref_value_t* memrefs;
}
rc_value_t;

int rc_value_size(const char* memaddr);
rc_value_t* rc_parse_value(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx);
int rc_evaluate_value(rc_value_t* value, rc_peek_t peek, void* ud, lua_State* L);

/*****************************************************************************\
| Leaderboards                                                                |
\*****************************************************************************/

/* Return values for rc_evaluate_lboard. */
enum {
  RC_LBOARD_INACTIVE,
  RC_LBOARD_ACTIVE,
  RC_LBOARD_STARTED,
  RC_LBOARD_CANCELED,
  RC_LBOARD_TRIGGERED
};

typedef struct {
  rc_trigger_t start;
  rc_trigger_t submit;
  rc_trigger_t cancel;
  rc_value_t value;
  rc_value_t* progress;
  rc_memref_value_t* memrefs;

  char started;
  char submitted;
}
rc_lboard_t;

int rc_lboard_size(const char* memaddr);
rc_lboard_t* rc_parse_lboard(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx);
int rc_evaluate_lboard(rc_lboard_t* lboard, int* value, rc_peek_t peek, void* peek_ud, lua_State* L);
void rc_reset_lboard(rc_lboard_t* lboard);

/*****************************************************************************\
| Value formatting                                                            |
\*****************************************************************************/

/* Supported formats. */
enum {
  RC_FORMAT_FRAMES,
  RC_FORMAT_SECONDS,
  RC_FORMAT_CENTISECS,
  RC_FORMAT_SCORE,
  RC_FORMAT_VALUE,
  RC_FORMAT_MINUTES,
  RC_FORMAT_SECONDS_AS_MINUTES
};

int rc_parse_format(const char* format_str);
int rc_format_value(char* buffer, int size, int value, int format);

/*****************************************************************************\
| Rich Presence                                                               |
\*****************************************************************************/

typedef struct rc_richpresence_lookup_item_t rc_richpresence_lookup_item_t;

struct rc_richpresence_lookup_item_t {
  unsigned value;
  rc_richpresence_lookup_item_t* next_item;
  const char* label;
};

typedef struct rc_richpresence_lookup_t rc_richpresence_lookup_t;

struct rc_richpresence_lookup_t {
  rc_richpresence_lookup_item_t* first_item;
  rc_richpresence_lookup_t* next;
  const char* name;
  unsigned short format;
};

typedef struct rc_richpresence_display_part_t rc_richpresence_display_part_t;

struct rc_richpresence_display_part_t {
  rc_richpresence_display_part_t* next;
  const char* text;
  rc_richpresence_lookup_item_t* first_lookup_item;
  rc_value_t value;
  unsigned short display_type;
};

typedef struct rc_richpresence_display_t rc_richpresence_display_t;

struct rc_richpresence_display_t {
  rc_trigger_t trigger;
  rc_richpresence_display_t* next;
  rc_richpresence_display_part_t* display;
};

typedef struct {
  rc_richpresence_display_t* first_display;
  rc_richpresence_lookup_t* first_lookup;
  rc_memref_value_t* memrefs;
}
rc_richpresence_t;

int rc_richpresence_size(const char* script);
rc_richpresence_t* rc_parse_richpresence(void* buffer, const char* script, lua_State* L, int funcs_ndx);
int rc_evaluate_richpresence(rc_richpresence_t* richpresence, char* buffer, unsigned buffersize, rc_peek_t peek, void* peek_ud, lua_State* L);

#ifdef __cplusplus
}
#endif

#endif /* RCHEEVOS_H */
