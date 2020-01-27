# **rcheevos**

**rcheevos** is a set of C code, or a library if you will, that tries to make it easier for emulators to process [RetroAchievements](https://retroachievements.org) data, providing support for achievements and leaderboards for their players.

Keep in mind that **rcheevos** does *not* provide HTTP network connections or JSON parsing. Clients must get data from RetroAchievements, parse the JSON payloads and pass the results down to **rcheevos** for processing. (**TODO**: document the server API and JSON schema.)

Not all structures defined by **rcheevos** can be created via the public API, but are exposed to allow interactions beyond just creation, destruction, and testing, such as the ones required by UI code that helps to create them.

Finally, **rcheevos** does *not* allocate or manage memory by itself. All structures that can be returned by it have a function to determine the number of bytes needed to hold the structure, and another one that actually builds the structure using a caller-provided buffer to bake it. However, calls to **rcheevos** may allocate and/or free memory as part of the Lua runtime, which is a dependency.

## Lua

RetroAchievements has decided to support achievements written using the [Lua](https://www.lua.org) language. The current expression-based implementation was too limiting already for the Nintendo 64, and was preventing the support of other systems.

> **rcheevos** does *not* create or maintain a Lua state, you have to create your own state and provide it to **rcheevos** to be used when Lua-coded achievements are found.

Lua functions used in trigger operands receive two parameters: `peek`, which is used to read from the emulated system's memory, and `userdata`, which must be passed to `peek`. `peek`'s signature is the same as its C counterpart:

```lua
function peek(address, num_bytes, userdata)
```

## API

> An understanding about how achievements are developed may be useful, you can read more about it [here](http://docs.retroachievements.org/Developer-docs/).

### User Configuration

There's only one thing that can be configured by users of **rcheevos**: `RC_ALIGNMENT`. This macro holds the alignment of allocations made in the buffer provided to the parsing functions, and the default value is `sizeof(void*)`.

If your platform will benefit from a different value, define a new value for it on your compiler flags before compiling the code. It has to be a power of 2, but no checking is done.

### Return values

The functions that compute the amount of memory that something will take return the number of bytes, or a negative value from the following enumeration:

```c
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
```

### Console identifiers

This enumeration uniquely identifies each of the supported platforms in RetroAchievements.

```c
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
```

### `rc_operand_t`

An operand is the leaf node of RetroAchievements expressions, and can hold one of the following:

* A constant integer or floating-point value
* A memory address of the system being emulated
* A reference to the Lua function that will be called to provide the value

```c
typedef struct {
  union {
    /* A value read from memory. */
    struct {
      /* The memory address or constant value of this variable. */
      unsigned value;
      /* The previous memory contents if RC_OPERAND_DELTA. */
      unsigned previous;

      /* The size of the variable. */
      char size;
      /* True if the value is in BCD. */
      char is_bcd;
      /* The type of the variable. */
    };

    /* A floating point value. */
    double fp_value;

    /* A reference to the Lua function that provides the value. */
    int function_ref;
  };

  char type;
}
rc_operand_t;
```

The `size` field, when applicable, holds one of these values:

```c
enum {
  RC_OPERAND_BIT_0,
  RC_OPERAND_BIT_1,
  RC_OPERAND_BIT_2,
  RC_OPERAND_BIT_3,
  RC_OPERAND_BIT_4,
  RC_OPERAND_BIT_5,
  RC_OPERAND_BIT_6,
  RC_OPERAND_BIT_7,
  RC_OPERAND_LOW,
  RC_OPERAND_HIGH,
  RC_OPERAND_8_BITS,
  RC_OPERAND_16_BITS,
  RC_OPERAND_24_BITS,
  RC_OPERAND_32_BITS,
};
```

The `type` field is always valid, and holds one of these values:

```c
enum {
  RC_OPERAND_ADDRESS, /* Compare to the value of a live address in RAM. */
  RC_OPERAND_DELTA,   /* The value last known at this address. */
  RC_OPERAND_CONST,   /* A 32-bit unsigned integer. */
  RC_OPERAND_FP,      /* A floating point value. */
  RC_OPERAND_LUA      /* A Lua function that provides the value. */
};
```

`RC_OPERAND_ADDRESS`, `RC_OPERAND_DELTA` and `RC_OPERAND_CONST` mean that the anonymous structure in the union is active. `RC_OPERAND_FP` means that `fp_value` is active. `RC_OPERAND_LUA` means `function_ref` is active.


### `rc_condition_t`

A condition compares its two operands according to the defined operator. It also keeps track of other things to make it possible to code more advanced achievements.

```c
typedef struct rc_condition_t rc_condition_t;

struct rc_condition_t {
  /* The next condition in the chain. */
  rc_condition_t* next;

  /* The condition's operands. */
  rc_operand_t operand1;
  rc_operand_t operand2;

  /* Required hits to fire this condition. */
  unsigned required_hits;
  /* Number of hits so far. */
  unsigned current_hits;

  /* The type of the condition. */
  char type;
  /* The comparison operator to use. */
  char oper; /* operator is a reserved word in C++. */
  /* Set if the condition needs to processed as part of the "check if paused" pass. */
  char pause;
  /* Whether or not the condition evaluated as true on the last check. */
  char is_true;
};
```

`type` can be one of these values:

```c
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
```

`oper` is the comparison operator to be used when comparing the two operands:

```c
enum {
  RC_CONDITION_EQ,
  RC_CONDITION_LT,
  RC_CONDITION_LE,
  RC_CONDITION_GT,
  RC_CONDITION_GE,
  RC_CONDITION_NE,
  RC_CONDITION_NONE
};
```

### `rc_condset_t`

Condition sets are an ordered collection of conditions (`rc_condition_t`), which are usually and'ed together to help build complex expressions for achievements.

```c
typedef struct rc_condset_t rc_condset_t;

struct rc_condset_t {
  /* The next condition set in the chain. */
  rc_condset_t* next;

  /* The list of conditions in this condition set. */
  rc_condition_t* conditions;

  /* True if any condition in the set is a pause condition. */
  char has_pause;
};
```

### `rc_trigger_t`

Triggers are the basic blocks of achievements and leaderboards. In fact, achievements are just triggers with some additional information like title, description, a badge, and some state, like whether it has already been awarded or not. All the logic to test if an achievement should be awarded is encapsulated in `rc_trigger_t`.

```c
typedef struct {
  /* The main condition set. */
  rc_condset_t* requirement;

  /* The list of sub condition sets in this test. */
  rc_condset_t* alternative;
}
rc_trigger_t;
```

The size in bytes of memory a trigger needs to be created is given by the `rc_trigger_size` function:

```c
int rc_trigger_size(const char* memaddr);
```

The return value is the size needed for the trigger described by the `memaddr` parameter, or a negative value with an [error code](#return-values).

Once the memory size is known, `rc_parse_trigger` can be called to actually construct a trigger in the caller-provided buffer:

```c
rc_trigger_t* rc_parse_trigger(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx);
```

`buffer` is the caller-allocated buffer, which must have enough space for the trigger. `memaddr` describes the trigger, and must be the same one used to compute the trigger's size with `rc_trigger_size`. `L` must be a valid Lua state, and `funcs_ndx` must be an index to the current Lua stack which contains a table which is a map of names to functions. This map is used to look for operands which are Lua functions.

Once the trigger is created, `rc_evaluate_trigger` can be called to test whether the trigger fires or not.

```c
int rc_evaluate_trigger(rc_trigger_t* trigger, rc_peek_t peek, void* ud, lua_State* L);
```

`trigger` is the trigger to test. `peek` is a callback used to read bytes from the emulated memory. `ud` is an user-provided opaque value that is passed to `peek`. `L` is the Lua state in which context the Lua functions are looked for and called, if necessary.

`rc_peek_t`'s signature is:

```c
typedef unsigned (*rc_peek_t)(unsigned address, unsigned num_bytes, void* ud);
```

where `address` is the starting address to read from, `num_bytes` the number of bytes to read (1, 2, or 4, little-endian), and `ud` is the same value passed to `rc_test_trigger`.

> Addresses passed to `peek` do *not* map 1:1 to the emulated memory. (**TODO**: document the mapping from `peek` addresses to emulated memory for each supported system.)

The return value of `rc_evaluate_trigger` is one of the following:
```c
enum {
  RC_TRIGGER_STATE_INACTIVE,   /* achievement is not being processed */
  RC_TRIGGER_STATE_WAITING,    /* achievement cannot trigger until it has been false for at least one frame */
  RC_TRIGGER_STATE_ACTIVE,     /* achievement is active and may trigger */
  RC_TRIGGER_STATE_PAUSED,     /* achievement is currently paused and will not trigger */
  RC_TRIGGER_STATE_RESET,      /* achievement hit counts were reset */
  RC_TRIGGER_STATE_TRIGGERED   /* achievement has triggered */
};
```

Finally, `rc_reset_trigger` can be used to reset the internal state of a trigger.

```c
void rc_reset_trigger(rc_trigger_t* self);
```

### `rc_term_t`

A term is the leaf node of expressions used to compute values from operands. A term is evaluated by multiplying its two operands. `invert` is used to invert the bits of the second operand of the term, when the unary operator `~` is used.

```c
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
```

### `rc_expression_t`

An expression is a collection of terms. All terms in the collection are added together to give the value of the expression.

```c
typedef struct rc_expression_t rc_expression_t;

struct rc_expression_t {
  /* The next expression in this chain. */
  rc_expression_t* next;

  /* The list of terms in this expression. */
  rc_term_t* terms;
};
```

### `rc_value_t`

A value is a collection of expressions. It's used to give the value for a leaderboard, and it evaluates to value of the expression with the greatest value in the collection.

```c
typedef struct {
  /* The list of expression to evaluate. */
  rc_expression_t* expressions;

  /* The list of conditions to evaluate. */
  rc_condset_t* conditions;

  /* The memory references required by the value. */
  rc_memref_value_t* memrefs;
}
rc_value_t;
```

The size in bytes needed to create a value can be computed by `rc_value_size`:

```c
int rc_value_size(const char* memaddr);
```

With the size at hand, the caller can allocate the necessary memory and pass it to `rc_parse_value` to create the value:

```c
rc_value_t* rc_parse_value(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx);
```

`buffer`, `memaddr`, `L`, and `funcs_ndx` are the same as in [`rc_parse_trigger`](#rc_parse_trigger).

To compute the value, use `rc_evaluate_value`:

```c
int rc_evaluate_value(rc_value_t* value, rc_peek_t peek, void* ud, lua_State* L);
```

`value` is the value to compute the value of, and `peek`, `ud`, and `L`, are as in [`rc_test_trigger`](#rc_test_trigger).

### `rc_lboard_t`

Leaderboards track a value over time, starting when a trigger is fired. The leaderboard can be canceled depending on the value of another trigger, and submitted to the RetroAchievements server depending on a third trigger.

The value submitted comes from the `value` field. Values displayed to the player come from the `progress` field unless it's `NULL`, in which case it's the same as `value`.

```c
typedef struct {
  rc_trigger_t start;
  rc_trigger_t submit;
  rc_trigger_t cancel;
  rc_value_t value;
  rc_value_t* progress;

  char started;
  char submitted;
}
rc_lboard_t;
```

Leaderboards are created and parsed just the same as triggers and values:

```c
int rc_lboard_size(const char* memaddr);
rc_lboard_t* rc_parse_lboard(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx);
```

A leaderboard can be evaluated with the `rc_evaluate_lboard` function:

```c
int rc_evaluate_lboard(rc_lboard_t* lboard, int* value, rc_peek_t peek, void* peek_ud, lua_State* L);
```

The function returns an action that must be performed by the caller, and `value` contains the value to be used for that action when the function returns. The action can be one of:

```c
enum {
  RC_LBOARD_INACTIVE,
  RC_LBOARD_ACTIVE,
  RC_LBOARD_STARTED,
  RC_LBOARD_CANCELED,
  RC_LBOARD_TRIGGERED
};
```

The caller must keep track of these values and do the necessary actions:

* `RC_LBOARD_ACTIVE` and `RC_LBOARD_INACTIVE`: just signal that the leaderboard didn't change its state.
* `RC_LBOARD_STARTED`: indicates that the leaderboard has been started, so the caller can i.e. show a message for the player, and start showing its value in the UI.
* `RC_LBOARD_CANCELED`: the leaderboard has been canceled, and the caller can inform the user and stop showing its value.
* `RC_LBOARD_TRIGGERED`: the leaderboard has been finished, and the value must be submitted to the RetroAchievements server; the caller can also notify the player and stop showing the value in the UI.

`rc_reset_lboard` resets the leaderboard:

```c
void rc_reset_lboard(rc_lboard_t* lboard);
```

### Value Formatting

**rcheevos** includes helper functions to parse formatting strings from RetroAchievements, and format values according to them.

`rc_parse_format` returns the format for the given string:

```c
int rc_parse_format(const char* format_str);
```

The returned value is one of:

```c
enum {
  RC_FORMAT_FRAMES,
  RC_FORMAT_SECONDS,
  RC_FORMAT_CENTISECS,
  RC_FORMAT_SCORE,
  RC_FORMAT_VALUE,
  RC_FORMAT_MINUTES,
  RC_FORMAT_SECONDS_AS_MINUTES
};
```

`RC_FORMAT_VALUE` is returned if `format_str` doesn't contain a valid format.

`rc_format_value` can be used to format the given value into the provided buffer:

```c
int rc_format_value(char* buffer, int size, int value, int format);
```

`buffer` receives `value` formatted according to `format`. No more than `size` characters will be written to `buffer`. 32 characters are enough to hold any valid value with any format. The returned value is the number of characters written.

# **rurl**

**rurl** builds URLs to access many RetroAchievements web services. Its purpose it to just to free the developer from having to URL-encode parameters and build correct URL that are valid for the server.

**rurl** does *not* make HTTP requests.

## API

### Return values

All functions return `0` if successful, or `-1` in case of errors. Errors are usually because the provided buffer is too small to hold the URL. If your buffer is large and you're still receiving errors, please open an issue.

### Functions

All functions take a `buffer`, where the URL will be written into, and `size` with the size of the buffer. The other parameters are particular to the desired URL.

```c
int rc_url_award_cheevo(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned cheevo_id, int hardcore);

int rc_url_submit_lboard(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned lboard_id, unsigned value, unsigned char hash[16]);

int rc_url_get_gameid(char* buffer, size_t size, unsigned char hash[16]);

int rc_url_get_patch(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid);

int rc_url_get_badge_image(char* buffer, size_t size, const char* badge_name);

int rc_url_login_with_password(char* buffer, size_t size, const char* user_name, const char* password);

int rc_url_login_with_token(char* buffer, size_t size, const char* user_name, const char* login_token);

int rc_url_get_unlock_list(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid, int hardcore);

int rc_url_post_playing(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid);
```
