#if defined(_XBOX1)
#define HARDCODE_FONT_SIZE 21
#define FONT_SIZE_VARIABLE FONT_SIZE

#define POSITION_X 60
#define POSITION_X_CENTER (POSITION_X + 350)
#define POSITION_Y_START 80
#define Y_POSITION 430
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define POSITION_Y_INCREMENT 20
#define COMMENT_POSITION_Y (Y_POSITION - ((POSITION_Y_INCREMENT/2) * 3))
#define CORE_MSG_POSITION_X FONT_SIZE
#define CORE_MSG_POSITION_Y (MSG_PREV_NEXT_Y_POSITION + 0.01f)
#define CORE_MSG_FONT_SIZE FONT_SIZE
#define MSG_QUEUE_X_POSITION POSITION_X
#define MSG_QUEUE_Y_POSITION (Y_POSITION - ((POSITION_Y_INCREMENT/2) * 7) + 10)
#define MSG_QUEUE_FONT_SIZE HARDCODE_FONT_SIZE
#define MSG_PREV_NEXT_Y_POSITION 24
#define CURRENT_PATH_Y_POSITION (POSITION_Y_START - ((POSITION_Y_INCREMENT/2)))
#define CURRENT_PATH_FONT_SIZE 21

#define FONT_SIZE 21 

#define NUM_ENTRY_PER_PAGE 15
#elif defined(__CELLOS_LV2__)
#define HARDCODE_FONT_SIZE 0.91f
#define FONT_SIZE_VARIABLE g_settings.video.font_size
#define POSITION_X 0.09f
#define POSITION_X_CENTER 0.5f
#define POSITION_Y_START 0.17f
#define POSITION_Y_INCREMENT 0.035f
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define COMMENT_POSITION_Y 0.82f
#define CORE_MSG_POSITION_X 0.3f
#define CORE_MSG_POSITION_Y 0.06f
#define CORE_MSG_FONT_SIZE COMMENT_POSITION_Y

#define MSG_QUEUE_X_POSITION g_settings.video.msg_pos_x
#define MSG_QUEUE_Y_POSITION 0.90f
#define MSG_QUEUE_FONT_SIZE 1.03f

#define MSG_PREV_NEXT_Y_POSITION 0.03f
#define CURRENT_PATH_Y_POSITION 0.15f
#define CURRENT_PATH_FONT_SIZE (g_settings.video.font_size)

#define NUM_ENTRY_PER_PAGE 18
#endif

static void render_text(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   font_params_t font_parms = {0};

   char msg[128];
   char label[64];

   font_parms.x = POSITION_X;
   font_parms.y = CURRENT_PATH_Y_POSITION;
   font_parms.scale = CURRENT_PATH_FONT_SIZE;
   font_parms.color = WHITE;

   switch(rgui->menu_type)
   {
#ifdef HAVE_SHADER_MANAGER
      case SHADER_CHOICE:
      case CGP_CHOICE:
#endif
      case BORDER_CHOICE:
      case LIBRETRO_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SRAM_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
      case CONFIG_CHOICE:
      case FILE_BROWSER_MENU:
         if (rgui->menu_type == LIBRETRO_CHOICE)
            strlcpy(label, "CORE SELECTION", sizeof(label));
         else if (rgui->menu_type == CONFIG_CHOICE)
            strlcpy(label, "CONFIG", sizeof(label));
         else
            strlcpy(label, "PATH", sizeof(label));
         snprintf(msg, sizeof(msg), "%s %s", label, rgui->browser->current_dir.directory_path);
         break;
      case INGAME_MENU_LOAD_GAME_HISTORY:
         strlcpy(msg, "LOAD HISTORY", sizeof(msg));
         break;
      case INGAME_MENU:
         strlcpy(msg, "MENU", sizeof(msg));
         break;
      case INGAME_MENU_CORE_OPTIONS:
         strlcpy(msg, "CORE OPTIONS", sizeof(msg));
         break;
      case INGAME_MENU_VIDEO_OPTIONS:
      case INGAME_MENU_VIDEO_OPTIONS_MODE:
         strlcpy(msg, "VIDEO OPTIONS", sizeof(msg));
         break;
#ifdef HAVE_SHADER_MANAGER
      case INGAME_MENU_SHADER_OPTIONS:
      case INGAME_MENU_SHADER_OPTIONS_MODE:
         strlcpy(msg, "SHADER OPTIONS", sizeof(msg));
         break;
#endif
      case INGAME_MENU_INPUT_OPTIONS:
      case INGAME_MENU_INPUT_OPTIONS_MODE:
         strlcpy(msg, "INPUT OPTIONS", sizeof(msg));
         break;
      case INGAME_MENU_CUSTOM_RATIO:
         strlcpy(msg, "CUSTOM RATIO", sizeof(msg));
         break;
      case INGAME_MENU_SETTINGS:
      case INGAME_MENU_SETTINGS_MODE:
         strlcpy(msg, "MENU SETTINGS", sizeof(msg));
         break;
      case INGAME_MENU_AUDIO_OPTIONS:
      case INGAME_MENU_AUDIO_OPTIONS_MODE:
         strlcpy(msg, "AUDIO OPTIONS", sizeof(msg));
         break;
      case INGAME_MENU_PATH_OPTIONS:
      case INGAME_MENU_PATH_OPTIONS_MODE:
         strlcpy(msg, "PATH OPTIONS", sizeof(msg));
         break;
   }

   if (driver.video_poke->set_osd_msg && msg[0] != '\0')
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   font_parms.x = POSITION_X;
   font_parms.y = CORE_MSG_POSITION_Y;
   font_parms.scale = CORE_MSG_FONT_SIZE;
   font_parms.color = WHITE;

   snprintf(msg, sizeof(msg), "%s - %s %s", PACKAGE_VERSION, rgui->info.library_name, rgui->info.library_version);

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   bool render_browser = false;
   bool render_settings = false;
   bool render_history = false;
   bool render_ingame_menu_resize = false;
   bool render_core_options = false;

   switch(rgui->menu_type)
   {
      case FILE_BROWSER_MENU:
      case LIBRETRO_CHOICE:
      case CONFIG_CHOICE:
#ifdef HAVE_SHADER_MANAGER
      case CGP_CHOICE:
      case SHADER_CHOICE:
#endif
      case BORDER_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SYSTEM_DIR_CHOICE:
         render_browser = true;
         break;
      case INGAME_MENU_CUSTOM_RATIO:
         render_ingame_menu_resize = true;
         break;
      case INGAME_MENU:
      case INGAME_MENU_SETTINGS:
      case INGAME_MENU_VIDEO_OPTIONS:
      case INGAME_MENU_SHADER_OPTIONS:
      case INGAME_MENU_AUDIO_OPTIONS:
      case INGAME_MENU_INPUT_OPTIONS:
      case INGAME_MENU_PATH_OPTIONS:
         render_settings = true;
         break;
      case INGAME_MENU_LOAD_GAME_HISTORY:
         render_history = true;
         break;
      case INGAME_MENU_CORE_OPTIONS:
         render_core_options = true;
         break;
   }

   if (render_browser)
   {
      font_params_t font_parms = {0};
      font_parms.scale = FONT_SIZE_VARIABLE;

      if (rgui->browser->list->size)
      {
         unsigned file_count = rgui->browser->list->size;
         unsigned current_index = 0;
         unsigned page_number = 0;
         unsigned page_base = 0;
         unsigned i;
         float y_increment = POSITION_Y_START;

         current_index = rgui->browser->current_dir.ptr;
         page_number = current_index / NUM_ENTRY_PER_PAGE;
         page_base = page_number * NUM_ENTRY_PER_PAGE;


         for (i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
         {
            char fname[128];
            fill_pathname_base(fname, rgui->browser->list->elems[i].data, sizeof(fname));
            y_increment += POSITION_Y_INCREMENT;

#ifdef HAVE_MENU_PANEL
            //check if this is the currently selected file
            if (strcmp(rgui->browser->current_dir.path, rgui->browser->list->elems[i].data) == 0)
               menu_panel->y = y_increment;
#endif

            font_parms.x = POSITION_X; 
            font_parms.y = y_increment;
            font_parms.color = i == current_index ? YELLOW : rgui->browser->list->elems[i].attr.b ? GREEN : WHITE;

            if (driver.video_poke->set_osd_msg)
               driver.video_poke->set_osd_msg(driver.video_data, fname, &font_parms);
         }
      }
      else
      {
         char entry[128];
         font_parms.x = POSITION_X; 
         font_parms.y = POSITION_Y_START + POSITION_Y_INCREMENT;
         font_parms.color = WHITE;
         strlcpy(entry, "No entries available.", sizeof(entry));

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, entry, &font_parms);
      }
   }

   if (render_core_options)
   {
      float y_increment = POSITION_Y_START;

      y_increment += POSITION_Y_INCREMENT;

      font_parms.x = POSITION_X; 
      font_parms.y = y_increment;
      font_parms.scale = CURRENT_PATH_FONT_SIZE;
      font_parms.color = WHITE;

      if (g_extern.system.core_options)
      {
         size_t opts = core_option_size(g_extern.system.core_options);
         for (size_t i = 0; i < opts; i++, font_parms.y += POSITION_Y_INCREMENT)
         {
            char type_str[256];

            /* not on same page? */
            if ((i / NUM_ENTRY_PER_PAGE) != (core_opt_selected / NUM_ENTRY_PER_PAGE))
               continue;

#ifdef HAVE_MENU_PANEL
            //check if this is the currently selected option
            if (i == core_opt_selected)
               menu_panel->y = font_parms.y;
#endif

            font_parms.x = POSITION_X; 
            font_parms.color = (core_opt_selected == i) ? YELLOW : WHITE;

            if (driver.video_poke->set_osd_msg)
               driver.video_poke->set_osd_msg(driver.video_data,
                     core_option_get_desc(g_extern.system.core_options, i), &font_parms);

            font_parms.x = POSITION_X_CENTER;
            font_parms.color = WHITE;

            strlcpy(type_str, core_option_get_val(g_extern.system.core_options, i), sizeof(type_str));

            if (driver.video_poke->set_osd_msg)
               driver.video_poke->set_osd_msg(driver.video_data, type_str, &font_parms);
         }
      }
      else if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "No options available.", &font_parms);
   }

   if (render_settings)
   {
      float y_increment = POSITION_Y_START;
      uint8_t i = 0;
      uint8_t j = 0;
      uint8_t item_page = 0;

      for(i = first_setting; i < max_settings; i++)
      {
         char text[PATH_MAX];
         char setting_text[PATH_MAX];
         unsigned w;

         strlcpy(setting_text, "", sizeof(setting_text));

         switch (i)
         {
#ifdef __CELLOS_LV2__
            case SETTING_CHANGE_RESOLUTION:
               strlcpy(text, "Resolution", sizeof(text));
               break;
            case SETTING_PAL60_MODE:
               strlcpy(text, "PAL60 Mode", sizeof(text));
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
                  strlcpy(setting_text, "ON", sizeof(setting_text));
               else
                  strlcpy(setting_text, "OFF", sizeof(setting_text));
               break;
#endif
            case SETTING_EMU_SKIN:
               strlcpy(text, "Menu Skin", sizeof(text));
               fill_pathname_base(setting_text, g_extern.menu_texture_path, sizeof(setting_text));
               break;
            case SETTING_HW_TEXTURE_FILTER:
               strlcpy(text, "Default Filter", sizeof(text));
               break;
#ifdef _XBOX1
            case SETTING_FLICKER_FILTER:
               strlcpy(text, "Flicker Filter", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), "%d", g_extern.console.screen.flicker_filter_index);
               break;
            case SETTING_SOFT_DISPLAY_FILTER:
               strlcpy(text, "Soft Display Filter", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
               break;
#endif
            case SETTING_REFRESH_RATE:
               strlcpy(text, "Estimated Monitor FPS", sizeof(text));
               break;
            case SETTING_VIDEO_VSYNC:
               strlcpy(text, "VSync", sizeof(text));
               break;
            case SETTING_VIDEO_CROP_OVERSCAN:
               strlcpy(text, "Crop Overscan (reload)", sizeof(text));
               break;
            case SETTING_TRIPLE_BUFFERING:
               strlcpy(text, "Triple Buffering", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE)) ? "ON" : "OFF");
               break;
            case SETTING_SOUND_MODE:
               strlcpy(text, "Sound Output", sizeof(text));
               switch(g_extern.console.sound.mode)
               {
                  case SOUND_MODE_NORMAL:
                     strlcpy(setting_text, "Normal", sizeof(setting_text));
                     break;
#ifdef HAVE_RSOUND
                  case SOUND_MODE_RSOUND:
                     strlcpy(setting_text, "RSound", sizeof(setting_text));
                     break;
#endif
#ifdef HAVE_HEADSET
                  case SOUND_MODE_HEADSET:
                     strlcpy(setting_text, "USB/Bluetooth Headset", sizeof(setting_text));
                     break;
#endif
                  default:
                     break;
               }
               break;
#ifdef HAVE_RSOUND
            case SETTING_RSOUND_SERVER_IP_ADDRESS:
               strlcpy(text, "RSound Server IP Address", sizeof(text));
               strlcpy(setting_text, g_settings.audio.device, sizeof(setting_text));
               break;
#endif
            case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
               strlcpy(text, "Debug Info Messages", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? "ON" : "OFF");
               break;
            case SETTING_EMU_SHOW_INFO_MSG:
               strlcpy(text, "Info Messages", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW)) ? "ON" : "OFF");
               break;
            case SETTING_REWIND_ENABLED:
               strlcpy(text, "Rewind", sizeof(text));
               break;
            case SETTING_REWIND_GRANULARITY:
               strlcpy(text, "Rewind Granularity", sizeof(text));
               break;
            case SETTING_EMU_AUDIO_MUTE:
               strlcpy(text, "Mute Audio", sizeof(text));
               break;
            case SETTING_AUDIO_CONTROL_RATE_DELTA:
               strlcpy(text, "Rate Control Delta", sizeof(text));
               break;
#ifdef _XBOX1
            case SETTING_EMU_AUDIO_SOUND_VOLUME_LEVEL:
               strlcpy(text, "Volume Level", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), g_extern.console.sound.volume_level ? "Loud" : "Normal");
               break;
#endif
            case SETTING_ENABLE_CUSTOM_BGM:
               strlcpy(text, "Custom BGM Option", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE)) ? "ON" : "OFF");
               break;
            case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
               strlcpy(text, "Browser Directory", sizeof(text));
               break;
            case SETTING_PATH_SAVESTATES_DIRECTORY:
               strlcpy(text, "Savestate Directory", sizeof(text));
               break;
            case SETTING_PATH_SRAM_DIRECTORY:
               strlcpy(text, "Savefile Directory", sizeof(text));
               break;
#ifdef HAVE_XML
            case SETTING_PATH_CHEATS:
               strlcpy(text, "Cheatfile Directory", sizeof(text));
               strlcpy(setting_text, g_settings.cheat_database, sizeof(setting_text));
               break;
#endif
            case SETTING_PATH_SYSTEM:
               strlcpy(text, "System Directory", sizeof(text));
               break;
            case SETTING_CONTROLS_NUMBER:
               strlcpy(text, "Player", sizeof(text));
               break;
            case SETTING_CONTROLS_BIND_DEVICE_TYPE:
               strlcpy(text, "Device Type", sizeof(text));
               break;
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3:
            case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3:
               {
                  unsigned id = i - FIRST_CONTROL_BIND;
                  struct platform_bind key_label;
                  strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
                  key_label.joykey = g_settings.input.binds[rgui->current_pad][id].joykey;

                  if (driver.input->set_keybinds)
                     driver.input->set_keybinds(&key_label, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
                  strlcpy(text, g_settings.input.binds[rgui->current_pad][id].desc, sizeof(text));
                  strlcpy(setting_text, key_label.desc, sizeof(setting_text));
               }
               break;
            case SETTING_CONTROLS_DEFAULT_ALL:
               strlcpy(text, "DEFAULTS", sizeof(text));
               break;
            case INGAME_MENU_LOAD_STATE:
               strlcpy(text, "Load State", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), "%d", g_extern.state_slot);
               break;
            case INGAME_MENU_SAVE_STATE:
               strlcpy(text, "Save State", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), "%d", g_extern.state_slot);
               break;
            case SETTING_ASPECT_RATIO:
               strlcpy(text, "Aspect Ratio", sizeof(text));
               break;
            case SETTING_ROTATION:
               strlcpy(text, "Rotation", sizeof(text));
               break;
            case SETTING_CUSTOM_VIEWPORT:
               strlcpy(text, "Custom Ratio", sizeof(text));
               break;
            case INGAME_MENU_CORE_OPTIONS_MODE:
               strlcpy(text, "Core Options", sizeof(text));
               break;
#ifdef HAVE_SHADER_MANAGER
            case INGAME_MENU_SHADER_OPTIONS_MODE:
               strlcpy(text, "Shader Options", sizeof(text));
               break;
#endif
            case INGAME_MENU_LOAD_GAME_HISTORY_MODE:
               strlcpy(text, "Load Game (History)", sizeof(text));
               break;
            case INGAME_MENU_VIDEO_OPTIONS_MODE:
               strlcpy(text, "Video Options", sizeof(text));
               break;
            case INGAME_MENU_AUDIO_OPTIONS_MODE:
               strlcpy(text, "Audio Options", sizeof(text));
               break;
            case INGAME_MENU_INPUT_OPTIONS_MODE:
               strlcpy(text, "Input Options", sizeof(text));
               break;
            case INGAME_MENU_PATH_OPTIONS_MODE:
               strlcpy(text, "Path Options", sizeof(text));
               break;
            case INGAME_MENU_SETTINGS_MODE:
               strlcpy(text, "Settings", sizeof(text));
               break;
            case INGAME_MENU_SCREENSHOT_MODE:
               strlcpy(text, "Take Screenshot", sizeof(text));
               break;
            case INGAME_MENU_RESET:
               strlcpy(text, "Restart Game", sizeof(text));
               break;
            case INGAME_MENU_RETURN_TO_GAME:
               strlcpy(text, "Resume Game", sizeof(text));
               break;
            case INGAME_MENU_CHANGE_GAME:
               snprintf(text, sizeof(text), "Load Game (%s)",
                     rgui->info.library_name ? rgui->info.library_name : g_extern.system.info.library_name);
               break;
            case INGAME_MENU_CHANGE_LIBRETRO_CORE:
               strlcpy(text, "Core", sizeof(text));
               break;
#ifdef HAVE_MULTIMAN
            case INGAME_MENU_RETURN_TO_MULTIMAN:
               strlcpy(text, "Return to multiMAN", sizeof(text));
               break;
#endif
            case INGAME_MENU_CONFIG:
               strlcpy(text, "RetroArch Config", sizeof(text));
               break;
            case INGAME_MENU_SAVE_CONFIG:
               strlcpy(text, "Save Config", sizeof(text));
               break;
            case INGAME_MENU_QUIT_RETROARCH:
               strlcpy(text, "Quit RetroArch", sizeof(text));
               break;
            default:
               break;
#ifdef HAVE_SHADER_MANAGER
            case SHADERMAN_LOAD_CGP:
               strlcpy(text, "Load Shader Preset", sizeof(text));
               break;
            case SHADERMAN_SAVE_CGP:
               strlcpy(text, "Save Shader Preset", sizeof(text));
               break;
            case SHADERMAN_SHADER_PASSES:
               strlcpy(text, "Shader Passes", sizeof(text));
               snprintf(setting_text, sizeof(setting_text), "%u", rgui->shader.passes);
               break;
            case SHADERMAN_APPLY_CHANGES:
               strlcpy(text, "Apply Shader Changes", sizeof(text));
               break;
            case SHADERMAN_SHADER_0:
            case SHADERMAN_SHADER_1:
            case SHADERMAN_SHADER_2:
            case SHADERMAN_SHADER_3:
            case SHADERMAN_SHADER_4:
            case SHADERMAN_SHADER_5:
            case SHADERMAN_SHADER_6:
            case SHADERMAN_SHADER_7:
               {
                  char type_str[256];
                  uint8_t index = (i - SHADERMAN_SHADER_0) / 3;
                  if (*rgui->shader.pass[index].source.cg)
                     fill_pathname_base(type_str,
                           rgui->shader.pass[index].source.cg, sizeof(type_str));
                  else
                     strlcpy(type_str, "N/A", sizeof(type_str));
                  snprintf(text, sizeof(text), "Shader #%d", index);
                  strlcpy(setting_text, type_str, sizeof(setting_text));
               }
               break;
            case SHADERMAN_SHADER_0_FILTER:
            case SHADERMAN_SHADER_1_FILTER:
            case SHADERMAN_SHADER_2_FILTER:
            case SHADERMAN_SHADER_3_FILTER:
            case SHADERMAN_SHADER_4_FILTER:
            case SHADERMAN_SHADER_5_FILTER:
            case SHADERMAN_SHADER_6_FILTER:
            case SHADERMAN_SHADER_7_FILTER:
               {
                  char type_str[256];
                  uint8_t index = (i - SHADERMAN_SHADER_0) / 3;
                  snprintf(text, sizeof(text), "Shader #%d filter", index);
                  shader_manager_get_str_filter(type_str, sizeof(type_str), index);
                  strlcpy(setting_text, type_str, sizeof(setting_text));
               }
               break;
            case SHADERMAN_SHADER_0_SCALE:
            case SHADERMAN_SHADER_1_SCALE:
            case SHADERMAN_SHADER_2_SCALE:
            case SHADERMAN_SHADER_3_SCALE:
            case SHADERMAN_SHADER_4_SCALE:
            case SHADERMAN_SHADER_5_SCALE:
            case SHADERMAN_SHADER_6_SCALE:
            case SHADERMAN_SHADER_7_SCALE:
               {
                  char type_str[256];
                  uint8_t index = (i - SHADERMAN_SHADER_0) / 3;
                  unsigned scale = rgui->shader.pass[index].fbo.scale_x;

                  snprintf(text, sizeof(text), "Shader #%d scale", index);

                  if (!scale)
                     strlcpy(type_str, "Don't care", sizeof(type_str));
                  else
                     snprintf(type_str, sizeof(type_str), "%ux", scale);

                  strlcpy(setting_text, type_str, sizeof(setting_text));
               }
               break;
#endif
         }

         switch (i)
         {
#ifdef __CELLOS_LV2__
            case SETTING_CHANGE_RESOLUTION:
#endif
            case INGAME_MENU_CONFIG:
            case SETTING_ROTATION:
            case SETTING_ASPECT_RATIO:
            case SETTING_CONTROLS_BIND_DEVICE_TYPE:
            case SETTING_CONTROLS_NUMBER:
            case SETTING_PATH_SYSTEM:
            case SETTING_PATH_SRAM_DIRECTORY:
            case SETTING_PATH_SAVESTATES_DIRECTORY:
            case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
            case SETTING_AUDIO_CONTROL_RATE_DELTA:
            case SETTING_EMU_AUDIO_MUTE:
            case SETTING_REWIND_GRANULARITY:
            case SETTING_REWIND_ENABLED:
            case SETTING_VIDEO_CROP_OVERSCAN:
            case SETTING_VIDEO_VSYNC:
            case SETTING_REFRESH_RATE:
            case SETTING_HW_TEXTURE_FILTER:
               menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
               break;
            case SETTING_CUSTOM_VIEWPORT:
            case INGAME_MENU_SAVE_CONFIG:
            case INGAME_MENU_CHANGE_LIBRETRO_CORE:
            case INGAME_MENU_CHANGE_GAME:
            case INGAME_MENU_SETTINGS_MODE:
            case INGAME_MENU_PATH_OPTIONS_MODE:
            case INGAME_MENU_INPUT_OPTIONS_MODE:
            case INGAME_MENU_AUDIO_OPTIONS_MODE:
            case INGAME_MENU_VIDEO_OPTIONS_MODE:
            case INGAME_MENU_LOAD_GAME_HISTORY_MODE:
#ifdef HAVE_SHADER_MANAGER
            case INGAME_MENU_SHADER_OPTIONS_MODE:
#endif
            case INGAME_MENU_CORE_OPTIONS_MODE:
               strlcpy(setting_text, "...", sizeof(setting_text));
               break;
         }

         char setting_text_buf[256];
         menu_ticker_line(setting_text_buf, TICKER_LABEL_CHARS_MAX_PER_LINE, g_extern.frame_count / 15, setting_text, i == rgui->selection_ptr);

         if (!(j < NUM_ENTRY_PER_PAGE))
         {
            j = 0;
            item_page++;
         }

         items_pages[i] = item_page;
         j++;

         if (item_page != setting_page_number)
            continue;

         y_increment += POSITION_Y_INCREMENT;

         font_parms.x = POSITION_X; 
         font_parms.y = y_increment;
         font_parms.scale = FONT_SIZE_VARIABLE;
         font_parms.color = (i == rgui->selection_ptr) ? YELLOW : WHITE;

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, text, &font_parms);

         font_parms.x = POSITION_X_CENTER;
         font_parms.color = WHITE;

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, setting_text_buf, &font_parms);

         if (i != rgui->selection_ptr)
            continue;

#ifdef HAVE_MENU_PANEL
         menu_panel->y = y_increment;
#endif
      }
   }

   if (render_history)
   {
      if (rom_history_size(rgui->history))
      {
         float y_increment = POSITION_Y_START;

         y_increment += POSITION_Y_INCREMENT;

         font_parms.x = POSITION_X; 
         font_parms.y = y_increment;
         font_parms.scale = CURRENT_PATH_FONT_SIZE;
         font_parms.color = WHITE;
         size_t opts = rom_history_size(rgui->history);

         for (size_t i = 0; i < opts; i++)
         {
            const char *path = NULL;
            const char *core_path = NULL;
            const char *core_name = NULL;

            rom_history_get_index(rgui->history, i,
                  &path, &core_path, &core_name);

            char path_short[PATH_MAX];
            fill_pathname(path_short, path_basename(path), "", sizeof(path_short));

            char fill_buf[PATH_MAX];
            snprintf(fill_buf, sizeof(fill_buf), "%s (%s)",
                  path_short, core_name);

            /* not on same page? */
            if ((i / NUM_ENTRY_PER_PAGE) != (hist_opt_selected / NUM_ENTRY_PER_PAGE))
               continue;

#ifdef HAVE_MENU_PANEL
            //check if this is the currently selected option
            if (i == hist_opt_selected)
               menu_panel->y = font_parms.y;
#endif

            font_parms.x = POSITION_X; 
            font_parms.color = (hist_opt_selected == i) ? YELLOW : WHITE;

            if (driver.video_poke->set_osd_msg)
               driver.video_poke->set_osd_msg(driver.video_data,
                     fill_buf, &font_parms);

            font_parms.y += POSITION_Y_INCREMENT;
         }
      }
      else if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "No history available.", &font_parms);
   }

   if (render_ingame_menu_resize && rgui->frame_buf_show)
   {
      char viewport[32];
      snprintf(viewport, sizeof(viewport), "Viewport X: #%d Y: %d (%dx%d)", g_extern.console.screen.viewports.custom_vp.x, g_extern.console.screen.viewports.custom_vp.y, g_extern.console.screen.viewports.custom_vp.width,
            g_extern.console.screen.viewports.custom_vp.height);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN;
      font_parms.scale = HARDCODE_FONT_SIZE;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, viewport, &font_parms);
   }
}
