/* Single-source definitions: fourth main menu list actions.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION_EX(ONSCREEN_DISPLAY_SETTINGS,
      "onscreen_display_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "On-Screen Display",
      "Change display overlay and keyboard overlay, and on-screen notification settings.")
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(ONSCREEN_OVERLAY_SETTINGS,
      "onscreen_overlay_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "On-Screen Overlay",
      "Adjust bezels and on-screen controls.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(OVERLAY_LIGHTGUN_SETTINGS,
      "overlay_lightgun_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Overlay Lightgun",
      "Configure lightgun input sent from the overlay.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(OVERLAY_MOUSE_SETTINGS,
      "overlay_mouse_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Overlay Mouse",
      "Configure mouse input sent from the overlay. Note: 1-, 2-, and 3-finger taps send left, right, and middle button clicks.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(OSK_OVERLAY_SETTINGS,
      "osk_overlay_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Keyboard Overlay",
      "Select and adjust a keyboard overlay.")
#endif
S_ACTION_EX(ONSCREEN_NOTIFICATIONS_SETTINGS,
      "onscreen_notifications_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "On-Screen Notifications",
      "Adjust On-Screen Notifications.")
S_ACTION(ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
      "onscreen_notifications_views_settings",
      "Notification Visibility",
      "Toggle the visibility of specific types of notifications.")
S_ACTION(MENU_SETTINGS,
      "menu_settings",
      "Appearance",
      "Change menu screen appearance settings.")
#ifdef _3DS
S_ACTION(MENU_BOTTOM_SETTINGS,
      "menu_bottom_settings",
      "3DS Bottom Screen Appearance",
      "Change bottom screen appearance settings.")
#endif
S_ACTION(MENU_VIEWS_SETTINGS,
      "menu_views_settings",
      "Menu Item Visibility",
      "Toggle the visibility of menu items in RetroArch.")
S_ACTION(QUICK_MENU_VIEWS_SETTINGS,
      "quick_menu_views_settings",
      "Quick Menu",
      "Toggle the visibility of menu items in the Quick Menu.")
S_ACTION(SETTINGS_VIEWS_SETTINGS,
      "settings_views_settings",
      "Settings",
      "Toggle the visibility of menu items in the Settings menu.")
S_ACTION(USER_INTERFACE_SETTINGS,
      "user_interface_settings",
      "User Interface",
      "Change user interface settings.")
S_ACTION(AI_SERVICE_SETTINGS,
      "ai_service_settings",
      "AI Service",
      "Change settings for the AI Service (Translation/TTS/Misc).")
/* Descriptor and configuration rows are #ifdef HAVE_ACCESSIBILITY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_ACCESSIBILITY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION(ACCESSIBILITY_SETTINGS,
      "accessibility_settings",
      "Accessibility",
      "Change settings for the Accessibility narrator.")
#endif
S_ACTION(POWER_MANAGEMENT_SETTINGS,
      "power_management_settings",
      "Power Management",
      "Change power management settings.")
S_ACTION_EX(MENU_FILE_BROWSER_SETTINGS,
      "menu_file_browser_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "File Browser",
      "Change File Browser settings.")
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION(RETRO_ACHIEVEMENTS_SETTINGS,
      "retro_achievements_settings",
      "Achievements",
      "Change achievement settings.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION(CHEEVOS_APPEARANCE_SETTINGS,
      "cheevos_appearance_settings",
      "Appearance",
      "Change the position and offsets of on-screen achievement notifications.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION(CHEEVOS_VISIBILITY_SETTINGS,
      "cheevos_visibility_settings",
      "Visibility",
      "Change which messages and on-screen elements are shown. Does not disable functionality.")
#endif
S_ACTION(UPDATER_SETTINGS,
      "updater_settings",
      "Updater Settings",
      "Access core updater settings")
