static void producesettingentry(uint64_t switchvalue)
{
	uint64_t state;

	state = cell_pad_input_poll_device(0);

	switch(switchvalue)
	{
		case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
		{
			if(g_extern.state_slot != 0)
				g_extern.state_slot--;

			set_text_message("", 7);
		}
		if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			g_extern.state_slot++;
			set_text_message("", 7);
		}

		if(CTRL_START(state))
			g_extern.state_slot = 0;

		break;
		case SETTING_CHANGE_RESOLUTION:
		if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) )
		{
			//ps3graphics_next_resolution();
			set_text_message("", 7);
		}
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) )
		{
			//ps3graphics_previous_resolution();
			set_text_message("", 7);
		}
		if(CTRL_CROSS(state))
		{
		}
		break;
		/*
		   case SETTING_PAL60_MODE:
		   if(CTRL_RIGHT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state) || CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
		   {
		   if (Graphics->GetCurrentResolution() == CELL_VIDEO_OUT_RESOLUTION_576)
		   {
		   if(Graphics->CheckResolution(CELL_VIDEO_OUT_RESOLUTION_576))
		   {
		   Settings.PS3PALTemporalMode60Hz = !Settings.PS3PALTemporalMode60Hz;
		   Graphics->SetPAL60Hz(Settings.PS3PALTemporalMode60Hz);
		   Graphics->SwitchResolution(Graphics->GetCurrentResolution(), Settings.PS3PALTemporalMode60Hz, Settings.TripleBuffering);
		   }
		   }

		   }
		   break;
		 */
#ifdef HAVE_GAMEAWARE
		case SETTING_GAME_AWARE_SHADER:
		if((CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state)) && Settings.ScaleEnabled)
		{
			menuStackindex++;
			menuStack[menuStackindex] = menu_filebrowser;
			menuStack[menuStackindex].enum_id = GAME_AWARE_SHADER_CHOICE;
			set_initial_dir_tmpbrowser = true;
		}
		if(CTRL_START(state) && Settings.ScaleEnabled)
		{
		}
		break;
#endif
		case SETTING_SHADER_PRESETS:
		if(g_settings.video.render_to_texture)
		{
			if((CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state)) && g_settings.video.render_to_texture)
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = PRESET_CHOICE;
				set_initial_dir_tmpbrowser = true;
			}
		}
		if(CTRL_START(state) && g_settings.video.render_to_texture)
		{
		}
		break;
		case SETTING_BORDER:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			menuStackindex++;
			menuStack[menuStackindex] = menu_filebrowser;
			menuStack[menuStackindex].enum_id = BORDER_CHOICE;
			set_initial_dir_tmpbrowser = true;
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_SHADER:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			menuStackindex++;
			menuStack[menuStackindex] = menu_filebrowser;
			menuStack[menuStackindex].enum_id = SHADER_CHOICE;
			set_shader = 0;
			set_initial_dir_tmpbrowser = true;
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_SHADER_2:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_FONT_SIZE:
		if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
		{
		}
		if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_KEEP_ASPECT_RATIO:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
		{
		}
		if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state))
		{
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_HW_TEXTURE_FILTER:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			g_settings.video.smooth = !g_settings.video.smooth;
			//ps3graphics_set_smooth(Settings.PS3Smooth, 0);
			set_text_message("", 7);
		}
		if(CTRL_START(state))
		{
			g_settings.video.smooth = 1;
			//ps3graphics_set_smooth(Settings.PS3Smooth, 0);
		}
		break;
		case SETTING_HW_TEXTURE_FILTER_2:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			g_settings.video.second_pass_smooth = !g_settings.video.second_pass_smooth;
			set_text_message("", 7);
		}
		if(CTRL_START(state))
		{
			g_settings.video.second_pass_smooth = 1;
		}
		break;
		case SETTING_SCALE_ENABLED:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			g_settings.video.render_to_texture = !g_settings.video.render_to_texture;

			if(g_settings.video.render_to_texture)
			{
			}
			else
			{
			}

			set_text_message("", 7);
		}
		if(CTRL_START(state))
		{
			g_settings.video.fbo_scale_x = 2;
			g_settings.video.fbo_scale_y = 2;
		}
		break;
		case SETTING_SCALE_FACTOR:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
		{
		}
		if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_HW_OVERSCAN_AMOUNT:
		if(CTRL_LEFT(state)  ||  CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
		{
		}
		if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_SOUND_MODE:
		if(CTRL_LEFT(state) ||  CTRL_LSTICK_LEFT(state))
		{
		}
		if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_RSOUND_SERVER_IP_ADDRESS:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_CROSS(state) | CTRL_LSTICK_RIGHT(state) )
		{
		}
		break;
		case SETTING_THROTTLE_MODE:
		if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_CROSS(state) || CTRL_LSTICK_RIGHT(state))
		{
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_TRIPLE_BUFFERING:
		if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
		{
		}
		if(CTRL_START(state))
		{
		}
		break;
		case SETTING_ENABLE_SCREENSHOTS:
		if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
		{
#if(CELL_SDK_VERSION > 0x340000)
			Settings.ScreenshotsEnabled = !Settings.ScreenshotsEnabled;
			if(Settings.ScreenshotsEnabled)
			{
				cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
				CellScreenShotSetParam screenshot_param = {0, 0, 0, 0};

				screenshot_param.photo_title = EMULATOR_NAME;
				screenshot_param.game_title = EMULATOR_NAME;
				cellScreenShotSetParameter (&screenshot_param);
				cellScreenShotEnable();
			}
			else
			{
				cellScreenShotDisable();
				cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
			}

			set_text_message("", 7);
#endif
		}
		if(CTRL_START(state))
		{
#if(CELL_SDK_VERSION > 0x340000)
			Settings.ScreenshotsEnabled = false;
#endif
		}
		break;
		case SETTING_SAVE_SHADER_PRESET:
		if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state)  || CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
		}
		break;
		case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
		if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state)  || CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state) || CTRL_START(state) || CTRL_CROSS(state))
		{
		}
		break;
		case SETTING_DEFAULT_VIDEO_ALL:
		if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state)  || CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state) || CTRL_START(state) || CTRL_CROSS(state))
		{
		}
		break;
		case SETTING_DEFAULT_AUDIO_ALL:
		if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state)  || CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state) || CTRL_START(state) || CTRL_CROSS(state))
		{
		}
		break;
		case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			menuStackindex++;
			menuStack[menuStackindex] = menu_filebrowser;
			menuStack[menuStackindex].enum_id = PATH_DEFAULT_ROM_DIR_CHOICE;
			set_initial_dir_tmpbrowser = true;
		}

		if(CTRL_START(state))
		{
		}

		break;
		case SETTING_PATH_SAVESTATES_DIRECTORY:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			menuStackindex++;
			menuStack[menuStackindex] = menu_filebrowser;
			menuStack[menuStackindex].enum_id = PATH_SAVESTATES_DIR_CHOICE;
			set_initial_dir_tmpbrowser = true;
		}

		if(CTRL_START(state))
		{
			//strcpy(Settings.PS3PathSaveStates, usrDirPath);
		}

		break;
		case SETTING_PATH_SRAM_DIRECTORY:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			menuStackindex++;
			menuStack[menuStackindex] = menu_filebrowser;
			menuStack[menuStackindex].enum_id = PATH_SRAM_DIR_CHOICE;
			set_initial_dir_tmpbrowser = true;
		}

		if(CTRL_START(state))
		{
			//strcpy(Settings.PS3PathSRAM, "");
		}

		break;
#ifdef HAVE_CHEATS
		case SETTING_PATH_CHEATS:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			menuStackindex++;
			menuStack[menuStackindex] = menu_filebrowser;
			menuStack[menuStackindex].enum_id = PATH_CHEATS_DIR_CHOICE;
			set_initial_dir_tmpbrowser = true;
		}
		break;
#endif
		case SETTING_PATH_DEFAULT_ALL:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_START(state))
		{
		}
		break;
		case SETTING_CONTROLS_SCHEME:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state) | CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			menuStackindex++;
			menuStack[menuStackindex] = menu_filebrowser;
			menuStack[menuStackindex].enum_id = INPUT_PRESET_CHOICE;
			set_initial_dir_tmpbrowser = true;
		}
		break;
		case SETTING_CONTROLS_NUMBER:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
		{
			if(currently_selected_controller_menu != 0)
				currently_selected_controller_menu--;
			set_text_message("", 7);
		}

		if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			if(currently_selected_controller_menu < 6)
				currently_selected_controller_menu++;
			set_text_message("", 7);
		}

		if(CTRL_START(state))
			currently_selected_controller_menu = 0;

		break; 
		case SETTING_CONTROLS_DPAD_UP:
		case SETTING_CONTROLS_DPAD_DOWN:
		case SETTING_CONTROLS_DPAD_LEFT:
		case SETTING_CONTROLS_DPAD_RIGHT:
		case SETTING_CONTROLS_BUTTON_CIRCLE:
		case SETTING_CONTROLS_BUTTON_CROSS:
		case SETTING_CONTROLS_BUTTON_TRIANGLE:
		case SETTING_CONTROLS_BUTTON_SQUARE:
		case SETTING_CONTROLS_BUTTON_SELECT:
		case SETTING_CONTROLS_BUTTON_START:
		case SETTING_CONTROLS_BUTTON_L1:
		case SETTING_CONTROLS_BUTTON_R1:
		case SETTING_CONTROLS_BUTTON_L2:
		case SETTING_CONTROLS_BUTTON_R2:
		case SETTING_CONTROLS_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_R3_BUTTON_L3:
		case SETTING_CONTROLS_ANALOG_R_UP:
		case SETTING_CONTROLS_ANALOG_R_DOWN:
		case SETTING_CONTROLS_ANALOG_R_LEFT:
		case SETTING_CONTROLS_ANALOG_R_RIGHT:
		if(CTRL_LEFT(state) | CTRL_LSTICK_LEFT(state))
		{
			//Input_MapButton(control_binds[currently_selected_controller_menu][(switchvalue-SETTING_CONTROLS_DPAD_UP)],false,NULL);
			set_text_message("", 7);
		}
		if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
		{
			//Input_MapButton(control_binds[currently_selected_controller_menu][(switchvalue-SETTING_CONTROLS_DPAD_UP)],true,NULL);
			set_text_message("", 7);
		}
		if(CTRL_START(state))
		{
			//Input_MapButton(control_binds[currently_selected_controller_menu][(switchvalue-SETTING_CONTROLS_DPAD_UP)],true, default_control_binds[switchvalue-SETTING_CONTROLS_DPAD_UP]);
		}
		break;
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
		if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_START(state))
			//emulator_save_settings(INPUT_PRESET_FILE);
		break;
		case SETTING_CONTROLS_DEFAULT_ALL:
		if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_START(state))
		{
			//emulator_set_controls("", SET_ALL_CONTROLS_TO_DEFAULT, "Default");
		}
		break;
	}
}
