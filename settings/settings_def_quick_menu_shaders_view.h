/* Single-source definitions: quick menu shaders visibility setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(quick_menu_show_shaders, QUICK_MENU_SHOW_SHADERS,
      "quick_menu_show_shaders",
      DEFAULT_QUICK_MENU_SHOW_SHADERS, SD_FLAG_NONE, 0, 0,
      "Show 'Shaders'",
      "Show the 'Shaders' option.")
#endif
