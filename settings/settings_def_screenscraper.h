/* Single-source definitions: ScreenScraper scraper preferences group.
 * Account credentials live in settings_def_screenscraper_account.h.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(screenscraper_primary_scraper, SCREENSCRAPER_PRIMARY_SCRAPER,
      "screenscraper_primary_scraper",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_screenscraper_primary, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Primary Scraper",
      "Which scraper provides boxarts, screenshots and title screens first; the other one is used as the per-item fallback. ScreenScraper is the primary while signed in unless changed here.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(screenscraper_quota_action, SCREENSCRAPER_QUOTA_ACTION,
      "screenscraper_quota_action",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_screenscraper_quota, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "When Daily Requests Run Out",
      "ScreenScraper allows a limited number of requests per day. 'Pause And Resume Later' stops the scrape and picks it up automatically once the allowance refreshes; 'Continue With libretro' finishes the run using the libretro thumbnail server alone.")
#endif
/* Range constants for the region/language selection settings; must
 * match the ss_regions/ss_languages tables in network/screenscraper.c */
#ifndef SCREENSCRAPER_REGION_LAST
#define SCREENSCRAPER_REGION_LAST 18
#endif
#ifndef SCREENSCRAPER_LANGUAGE_LAST
#define SCREENSCRAPER_LANGUAGE_LAST 16
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(screenscraper_region, SCREENSCRAPER_REGION,
      "screenscraper_region",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, SCREENSCRAPER_REGION_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_screenscraper_region, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Preferred Media Region",
      "Region whose artwork and release dates are preferred when a game has several regional variants.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(screenscraper_language, SCREENSCRAPER_LANGUAGE,
      "screenscraper_language",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, SCREENSCRAPER_LANGUAGE_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_screenscraper_language, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Preferred Text Language",
      "Language preferred for the synopsis and genre text stored in the metadata sidecar.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_boxarts, SCREENSCRAPER_MEDIA_BOXARTS,
      "screenscraper_media_boxarts",
      DEFAULT_SCREENSCRAPER_MEDIA_ON, SD_FLAG_NONE, 0, 0,
      "Scrape Boxarts",
      "Download 2D box art into 'Named_Boxarts'.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_snaps, SCREENSCRAPER_MEDIA_SNAPS,
      "screenscraper_media_snaps",
      DEFAULT_SCREENSCRAPER_MEDIA_ON, SD_FLAG_NONE, 0, 0,
      "Scrape Screenshots",
      "Download in-game screenshots into 'Named_Snaps'.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_titles, SCREENSCRAPER_MEDIA_TITLES,
      "screenscraper_media_titles",
      DEFAULT_SCREENSCRAPER_MEDIA_ON, SD_FLAG_NONE, 0, 0,
      "Scrape Title Screens",
      "Download title screens into 'Named_Titles'.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_logos, SCREENSCRAPER_MEDIA_LOGOS,
      "screenscraper_media_logos",
      DEFAULT_SCREENSCRAPER_MEDIA_ON, SD_FLAG_NONE, 0, 0,
      "Scrape Logos (Wheels)",
      "Download game logo art into 'Named_Logos'.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_boxarts3d, SCREENSCRAPER_MEDIA_BOXARTS3D,
      "screenscraper_media_boxarts3d",
      DEFAULT_SCREENSCRAPER_MEDIA_OFF, SD_FLAG_NONE, 0, 0,
      "Scrape 3D Boxarts",
      "Download 3D box art into 'Named_Boxarts3D'.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_fanarts, SCREENSCRAPER_MEDIA_FANARTS,
      "screenscraper_media_fanarts",
      DEFAULT_SCREENSCRAPER_MEDIA_OFF, SD_FLAG_NONE, 0, 0,
      "Scrape Fan Art",
      "Download fan art backgrounds into 'Named_Fanarts'.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_marquees, SCREENSCRAPER_MEDIA_MARQUEES,
      "screenscraper_media_marquees",
      DEFAULT_SCREENSCRAPER_MEDIA_OFF, SD_FLAG_NONE, 0, 0,
      "Scrape Marquees",
      "Download marquee art into 'Named_Marquees'.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_videos, SCREENSCRAPER_MEDIA_VIDEOS,
      "screenscraper_media_videos",
      DEFAULT_SCREENSCRAPER_MEDIA_OFF, SD_FLAG_NONE, 0, 0,
      "Scrape Videos",
      "Download video snaps into 'Named_Videos'. Significantly increases scraping time and disk usage.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_manuals, SCREENSCRAPER_MEDIA_MANUALS,
      "screenscraper_media_manuals",
      DEFAULT_SCREENSCRAPER_MEDIA_OFF, SD_FLAG_NONE, 0, 0,
      "Scrape Manuals",
      "Download game manuals (PDF) into 'Named_Manuals'.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_media_bezels, SCREENSCRAPER_MEDIA_BEZELS,
      "screenscraper_media_bezels",
      DEFAULT_SCREENSCRAPER_MEDIA_OFF, SD_FLAG_NONE, 0, 0,
      "Scrape Bezels",
      "Download 16:9 bezel art into 'Named_Bezels'.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_metadata, SCREENSCRAPER_METADATA,
      "screenscraper_metadata",
      DEFAULT_SCREENSCRAPER_MEDIA_ON, SD_FLAG_NONE, 0, 0,
      "Scrape Metadata",
      "Save synopsis, genre, developer, publisher, players, rating and release date to a per-game JSON file in the 'Metadata' thumbnail directory.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_overwrite, SCREENSCRAPER_OVERWRITE,
      "screenscraper_overwrite",
      DEFAULT_SCREENSCRAPER_MEDIA_OFF, SD_FLAG_NONE, 0, 0,
      "Overwrite Existing Media",
      "Redownload media and metadata that already exist locally.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(screenscraper_use_crc, SCREENSCRAPER_USE_CRC,
      "screenscraper_use_crc",
      DEFAULT_SCREENSCRAPER_MEDIA_ON, SD_FLAG_NONE, 0, 0,
      "Match Using CRC Hashes",
      "Identify games by their CRC checksum in addition to the file name. More accurate; disable only if lookups fail.")
#endif
