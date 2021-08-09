## Adding 'enhanced' core options to a core

The basic steps for updating a core to support core options v1 are as follows:

- Copy `example_default/libretro_core_options.h` to the same directory as `libretro.c/.cpp`

- Copy `example_default/libretro_core_options_intl.h` to the same directory as `libretro.c/.cpp`

- Add `#include "libretro_core_options.h"` to `libretro.c/.cpp`

- Replace any existing calls of `RETRO_ENVIRONMENT_SET_VARIABLES` with `libretro_set_core_options(retro_environment_t environ_cb)`  
  (Note: `libretro_set_core_options()` should be called as early as possible - preferably in `retro_set_environment()`  
  and no later than `retro_load_game()`)

- Open `libretro_core_options.h` and replace the contents of the existing `option_defs_us` struct array with all required core option parameters.  

## Adding core option translations

To add a translation, simply:

- Copy the contents of `option_defs_us` *from* `libretro_core_options.h` *to* `libretro_core_options_intl.h` into a new struct array with the appropriate language suffix

- Translate all human-readable strings

- Add the new struct array to the appropriate location in the `option_defs_intl` array inside `libretro_core_options.h`

This is most easily understood by considering the example in `example_translation/`. Here a French translation has been added (`option_defs_fr`), with comments explaining the appropriate formatting requirements.

NOTE: Since translations involve the use of UTF-8 characters, `libretro_core_options_intl.h` must include a BOM marker. *This renders translations incompatible with c89 builds*. When performing a c89 build, the flag `HAVE_NO_LANGEXTRA` *must* be defined (e.g. `-DHAVE_NO_LANGEXTRA`). This will disable all translations.

## Disabling core options on unsupported frontends

Sometimes it is desirable to only show a particular core option if the frontend supports the new core options v1 API. For example:

- The API v1 allows cores to hide options dynamically

- We can therefore create a specific 'toggle display' option to hide or show a number of other options (e.g. advanced settings)

- On frontends that do not support API v1, this 'toggle display' option will have no function - it should therefore be omitted

This can be handled easily by editing the `libretro_set_core_options()` function to ignore certain core name (key) values when generating option variable arrays for old-style frontends. Again, this is most easily understood by considering the example in `example_hide_option/libretro_core_options.h`:

- Here, a `mycore_show_speedhacks` option is added to `option_defs_us`

- On line 227, the following comparison allows the option to be skipped:  
  (Note that another `strcmp()` may be added for each option to be omitted)

```c
if (strcmp(key, "mycore_show_speedhacks") == 0)
	continue;
```

For any cores that require this functionality, `example_hide_option/libretro_core_options.h` should be used as a template in place of `example_default/libretro_core_options.h`

## Adding core option categories

Core options v2 adds a mechanism for assigning categories to options. On supported fontends, options of a particular category will be displayed in a submenu/subsection of the main core options menu. This functionality may be used to reduce visual clutter, or to effectively 'hide' advanced settings without requiring a dedicated 'toggle display' option.

A template for enabling categories via the core options v2 interface is provided in `example_categories`. The usage of this code is identical to that described in the `Adding 'enhanced' core options to a core` section, with one addition: the `libretro_set_core_options()` function here includes an additional argument identifying whether the frontend has option category support (a core may wish to selectively hide or reorganise options based upon this variable).
