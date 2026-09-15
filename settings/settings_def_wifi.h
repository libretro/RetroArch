/* Single-source definitions: Wi-Fi group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL_EX_NS(wifi_enabled, WIFI_ENABLED,
      "wifi_enabled",
      DEFAULT_WIFI_ENABLE, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Enable Wi-Fi")
S_ACTION_NS(WIFI_NETWORK_SCAN,
      "wifi_network_scan",
      "Connect to Network")
S_ACTION_NS(WIFI_DISCONNECT,
      "disconnect_wifi",
      "Disconnect")
