/* Single-source definitions: netplay stateless mode setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING) #if defined(HAVE_NETWORK_CMD); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORK_CMD) || defined(SETTINGS_DEF_STRINGS_PASS) || (defined(SETTINGS_DEF_CONFIG_PASS) && defined(HAVE_NETWORKGAMEPAD))
/* The configuration table registers this row by hand in
 * configuration.c because it carries no default there; the
 * generated row is for the other passes. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_EX(network_remote_enable, NETWORK_REMOTE_ENABLE,
      "network_remote_enable",
      false, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Network RetroPad",
      "Let another device be a controller over the network: a RetroArch running 'Start Remote RetroPad' sends its buttons and sticks here by UDP. Each user enabled below listens on its own port, counted up from the base port. Nothing is asked of the sender, so use it on a network you trust.")
#endif
#endif
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(network_remote_first_sender, NETWORK_REMOTE_FIRST_SENDER,
      "network_remote_first_sender",
      false, SD_FLAG_ADVANCED, 0, 0,
      "Network RetroPad: First Sender Only",
      "Each user's Network RetroPad listens to the first device that sends to it and ignores every other, until RetroArch is restarted. A second device cannot take the controller over. It does not stop a device that forges its address.")
#endif
