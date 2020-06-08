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
  RC_INVALID_MEASURED_TARGET = -23,
  RC_INVALID_COMPARISON = -24
};
```

To convert the return code into something human-readable, pass it to:
```c
const char* rc_error_str(int ret);
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
  RC_CONSOLE_POKEMON_MINI = 24,
  RC_CONSOLE_ATARI_2600 = 25,
  RC_CONSOLE_MS_DOS = 26,
  RC_CONSOLE_ARCADE = 27,
  RC_CONSOLE_VIRTUAL_BOY = 28,
  RC_CONSOLE_MSX = 29,
  RC_CONSOLE_COMMODORE_64 = 30,
  RC_CONSOLE_ZX81 = 31,
  RC_CONSOLE_ORIC = 32,
  RC_CONSOLE_SG1000 = 33,
  RC_CONSOLE_VIC20 = 34,
  RC_CONSOLE_AMIGA = 35,
  RC_CONSOLE_AMIGA_ST = 36,
  RC_CONSOLE_AMSTRAD_PC = 37,
  RC_CONSOLE_APPLE_II = 38,
  RC_CONSOLE_SATURN = 39,
  RC_CONSOLE_DREAMCAST = 40,
  RC_CONSOLE_PSP = 41,
  RC_CONSOLE_CDI = 42,
  RC_CONSOLE_3DO = 43,
  RC_CONSOLE_COLECOVISION = 44,
  RC_CONSOLE_INTELLIVISION = 45,
  RC_CONSOLE_VECTREX = 46,
  RC_CONSOLE_PC8800 = 47,
  RC_CONSOLE_PC9800 = 48,
  RC_CONSOLE_PCFX = 49,
  RC_CONSOLE_ATARI_5200 = 50,
  RC_CONSOLE_ATARI_7800 = 51,
  RC_CONSOLE_X68K = 52,
  RC_CONSOLE_WONDERSWAN = 53,
  RC_CONSOLE_CASSETTEVISION = 54,
  RC_CONSOLE_SUPER_CASSETTEVISION = 55
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
    rc_memref_value_t* memref;

    /* An integer value. */
    unsigned num;

    /* A floating point value. */
    double dbl;

    /* A reference to the Lua function that provides the value. */
    int luafunc;
  };

  /* specifies which member of the value union is being used */
  char type;

  /* the actual RC_MEMSIZE of the operand - memref.size may differ */
  char size;
}
rc_operand_t;
```

The `size` field, when applicable, holds one of these values:

```c
enum {
  RC_MEMSIZE_8_BITS,
  RC_MEMSIZE_16_BITS,
  RC_MEMSIZE_24_BITS,
  RC_MEMSIZE_32_BITS,
  RC_MEMSIZE_LOW,
  RC_MEMSIZE_HIGH,
  RC_MEMSIZE_BIT_0,
  RC_MEMSIZE_BIT_1,
  RC_MEMSIZE_BIT_2,
  RC_MEMSIZE_BIT_3,
  RC_MEMSIZE_BIT_4,
  RC_MEMSIZE_BIT_5,
  RC_MEMSIZE_BIT_6,
  RC_MEMSIZE_BIT_7,
  RC_MEMSIZE_BITCOUNT
};
```

The `type` field is always valid, and holds one of these values:

```c
enum {
  RC_OPERAND_ADDRESS,        /* The value of a live address in RAM. */
  RC_OPERAND_DELTA,          /* The value last known at this address. */
  RC_OPERAND_CONST,          /* A 32-bit unsigned integer. */
  RC_OPERAND_FP,             /* A floating point value. */
  RC_OPERAND_LUA,            /* A Lua function that provides the value. */
  RC_OPERAND_PRIOR,          /* The last differing value at this address. */
  RC_OPERAND_BCD,            /* The BCD-decoded value of a live address in RAM */
  RC_OPERAND_INVERTED        /* The twos-complement value of a live address in RAM */
};
```

`RC_OPERAND_ADDRESS`, `RC_OPERAND_DELTA`, `RC_OPERAND_PRIOR`, `RC_OPERAND_BCD`, and `RC_OPERAND_INVERTED` mean that `memref` is active. `RC_OPERAND_CONST` means that `num` is active. `RC_OPERAND_FP` means that `dbl` is active. `RC_OPERAND_LUA` means `luafunc` is active.


### `rc_condition_t`

A condition compares its two operands according to the defined operator. It also keeps track of other things to make it possible to code more advanced achievements.

```c
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
  RC_CONDITION_ADD_ADDRESS,
  RC_CONDITION_TRIGGER,
  RC_CONDITION_MEASURED_IF
};
```

`oper` is the comparison operator to be used when comparing the two operands:

```c
enum {
  RC_OPERATOR_EQ,
  RC_OPERATOR_LT,
  RC_OPERATOR_LE,
  RC_OPERATOR_GT,
  RC_OPERATOR_GE,
  RC_OPERATOR_NE,
  RC_OPERATOR_NONE,
  RC_OPERATOR_MULT,
  RC_OPERATOR_DIV,
  RC_OPERATOR_AND
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

  /* The memory references required by the trigger. */
  rc_memref_value_t* memrefs;
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
  RC_TRIGGER_STATE_TRIGGERED,  /* achievement has triggered */
  RC_TRIGGER_STATE_PRIMED      /* all non-Trigger conditions are true */
};
```

Finally, `rc_reset_trigger` can be used to reset the internal state of a trigger.

```c
void rc_reset_trigger(rc_trigger_t* self);
```

### `rc_value_t`

A value is a collection of conditions that result in a single RC_CONDITION_MEASURED expression. It's used to calculate the value for a leaderboard and for lookups in rich presence.

```c
typedef struct {
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
  rc_memref_value_t* memrefs;

  char state;
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
  RC_LBOARD_STATE_INACTIVE,  /* leaderboard is not being processed */
  RC_LBOARD_STATE_WAITING,   /* leaderboard cannot activate until the start condition has been false for at least one frame */
  RC_LBOARD_STATE_ACTIVE,    /* leaderboard is active and may start */
  RC_LBOARD_STATE_STARTED,   /* leaderboard attempt in progress */
  RC_LBOARD_STATE_CANCELED,  /* leaderboard attempt canceled */
  RC_LBOARD_STATE_TRIGGERED  /* leaderboard attempt complete, value should be submitted */
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

### `rc_runtime_t`

The runtime encapsulates a set of achievements and leaderboards and manages processing them for each frame. When important things occur, events are raised for the caller via a callback.

```c
typedef struct rc_runtime_t {
  rc_runtime_trigger_t* triggers;
  unsigned trigger_count;
  unsigned trigger_capacity;

  rc_runtime_lboard_t* lboards;
  unsigned lboard_count;
  unsigned lboard_capacity;

  rc_runtime_richpresence_t* richpresence;
  char* richpresence_display_buffer;
  char  richpresence_update_timer;

  rc_memref_value_t* memrefs;
  rc_memref_value_t** next_memref;
}
rc_runtime_t;
```

The runtime must first be initialized.
```c
void rc_runtime_init(rc_runtime_t* runtime);
```

Then individual achievements, leaderboards, and even rich presence can be loaded into the runtime. These functions return RC_OK, or one of the negative value error codes listed above.
```c
int rc_runtime_activate_achievement(rc_runtime_t* runtime, unsigned id, const char* memaddr, lua_State* L, int funcs_idx);
int rc_runtime_activate_lboard(rc_runtime_t* runtime, unsigned id, const char* memaddr, lua_State* L, int funcs_idx);
int rc_runtime_activate_richpresence(rc_runtime_t* runtime, const char* script, lua_State* L, int funcs_idx);
```

The runtime should be called once per frame to evaluate the state of the active achievements/leaderboards:
```c
void rc_runtime_do_frame(rc_runtime_t* runtime, rc_runtime_event_handler_t event_handler, rc_peek_t peek, void* ud, lua_State* L);
```

The `event_handler` is a callback function that is called for each event that occurs when processing the frame.
```c
typedef struct rc_runtime_event_t {
  unsigned id;
  int value;
  char type;
}
rc_runtime_event_t;

typedef void (*rc_runtime_event_handler_t)(const rc_runtime_event_t* runtime_event);
```

The `event.type` field will be one of the following:
* RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED (id=achievement id)
  An achievement starts in the RC_TRIGGER_STATE_WAITING state and cannot trigger until it has been false for at least one frame. This event indicates the achievement is no longer waiting and may trigger on a future frame.
* RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED (id=achievement id)
  One or more conditions in the achievement have disabled the achievement.
* RC_RUNTIME_EVENT_ACHIEVEMENT_RESET (id=achievement id)
  One or more conditions in the achievement have reset any progress captured in the achievement.
* RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED (id=achievement id)
  All conditions for the achievement have been met and the user should be informed.
  NOTE: If `rc_runtime_reset` is called without deactivating the achievement, it may trigger again.
* RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED (id=achievement id)
  All non-trigger conditions for the achievement have been met. This typically indicates the achievement is a challenge achievement and the challenge is active.
* RC_RUNTIME_EVENT_LBOARD_STARTED (id=leaderboard id, value=leaderboard value)
  The leaderboard's start condition has been met and the user should be informed that a leaderboard attempt has started.
* RC_RUNTIME_EVENT_LBOARD_CANCELED (id=leaderboard id, value=leaderboard value)
  The leaderboard's cancel condition has been met and the user should be informed that a leaderboard attempt has failed.
* RC_RUNTIME_EVENT_LBOARD_UPDATED (id=leaderboard id, value=leaderboard value)
  The leaderboard value has changed.
* RC_RUNTIME_EVENT_LBOARD_TRIGGERED (id=leaderboard id, value=leaderboard value)
  The leaderboard's submit condition has been met and the user should be informed that a leaderboard attempt was successful. The value should be submitted.

When an achievement triggers, it should be deactivated so it won't trigger again:
```c
void rc_runtime_deactivate_achievement(rc_runtime_t* runtime, unsigned id);
```
Additionally, the unlock should be submitted to the server.

When a leaderboard triggers, it should not be deactivated in case the player wants to try again for a better score. The value should be submitted to the server.

`rc_runtime_do_frame` also periodically updates the rich presense string (every 60 frames). To get the current value, call
```c
const char* rc_runtime_get_richpresence(const rc_runtime_t* runtime);
```

When the game is reset, the runtime should also be reset:
```c
void rc_runtime_reset(rc_runtime_t* runtime);
```

This ensures any active achievements/leaderboards are set back to their initial states and prevents unexpected triggers when the memory changes in atypical way.


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
