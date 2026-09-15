/* Single-source definitions: AI service group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_TRANSLATE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_TRANSLATE) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(ai_service_mode, AI_SERVICE_MODE,
      "ai_service_mode",
      DEFAULT_AI_SERVICE_MODE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_ai_service_mode, NULL, NULL, NULL, NULL, 0,
      "AI Service Output",
      "Show translation as a text overlay (Image Mode), play as Text-To-Speech (Speech), or use a system narrator like NVDA (Narrator).")
#endif
