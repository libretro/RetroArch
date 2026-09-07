/* Single-source definitions: power management action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_LAKKA; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_LAKKA) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_NS(CPU_PERFPOWER,
      "cpu_perfpower_list",
      "CPU Performance and Power")
#endif
