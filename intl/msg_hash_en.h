#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

/* Top-Level Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Favourites"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Net-play"
   )

/* Main Menu */

#ifdef HAVE_LAKKA
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Net-play"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Join or host a net-play session."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Quit RetroArch. Killing the program in any hard way (SIGKILL, etc.) will terminate RetroArch without saving the configuration in any case. On Unix-likes, SIGINT/SIGTERM allows a clean deinitialisation which includes configuration save if enabled."
   )

/* Main Menu > Load Core */


/* Main Menu > Load Content */


/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Favourites"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Content added to 'Favourites' will appear here."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Browse all content matching the database with a categorised search interface."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Switch Cores to the Play Store Versions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Download complete thumbnail package for the selected system."
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "View information about the currently inserted media discs."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Licence"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Serialised (Save/Load, Rewind)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Deterministic (Save/Load, Rewind, Run-Ahead, Net-play)"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Front-end Identifier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Front-end OS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Net-play (Peer-to-Peer) Support"
   )

/* Main Menu > Information > Database Manager */


/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Art-style"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analogue Supported"
   )

/* Main Menu > Configuration File */


/* Main Menu > Help */


/* Main Menu > Help > Basic Menu Controls */


/* Settings */


/* Core option category placeholders for icons */

#ifdef HAVE_MIST
#endif

/* Settings > Drivers */


#ifdef HAVE_MICROPHONE
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Audio Re-sampler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Audio re-sampler driver to use."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Recording driver to use."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Full-screen Mode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Change full-screen mode settings."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Change video synchronisation settings."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Suspends the screensaver. Is a hint that does not necessarily have to be honoured by the video driver."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Inserts black frame(s) in between frames for enhanced motion clarity. Only use option designated for your current display refresh rate. Not for use at refresh rates that are non-multiples of 60Hz such as 144Hz, 165Hz, etc. Do not combine with Swap Interval > 1, sub-frames, Frame Delay, or Sync to Exact Content Framerate. Leaving system VRR on is ok, just not that setting. If you notice -any- temporary image retention, you should disable at 120Hz, and for higher Hz adjust the dark frames setting [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Adjusts the number of frames displayed in the BFI sequence that are black. More black frames increases motion clarity but reduces brightness. Not applicable at 120hz as there is only one total extra 60hz frame, so it must be black otherwise BFI would not be active at all."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Specify image interpolation method when scaling content with the internal IPU. 'Bi-cubic' or 'Bilinear' is recommended when using CPU-powered video filters. This option has no performance impact."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bi-cubic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Nearest Neighbour"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Specify image interpolation method when 'Integer Scale' is disabled. 'Nearest Neighbour' has the least performance impact."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Nearest Neighbour"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Delay autoloading shaders (in ms). Can work around graphical glitches when using 'screen grabbing' software."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Apply a CPU-powered video filter. Might come at a high performance cost. Some video filters might only work for cores that use 32-bit or 16-bit colour."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Apply a CPU-powered video filter. Might come at a high performance cost. Some video filters might only work for cores that use 32-bit or 16-bit colour. Dynamically linked video filter libraries can be selected."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Apply a CPU-powered video filter. Might come at a high performance cost. Some video filters might only work for cores that use 32-bit or 16-bit colour. Built-in video filter libraries can be selected."
   )

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Switch among native and ultra-wide super resolutions."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "X-Axis Centring"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Cycle through these options if the image is not centred properly on the display."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Switch to high resolution mode-line for use with high-resolution menus when no content is loaded."
   )

/* Settings > Video > Output */

#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Optimise for Wii U GamePad (Restart required)"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Only in Exclusive Full-screen Mode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Only in Windowed Full-screen Mode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "All Full-screen Modes"
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
#endif

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Start in Full-screen Mode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Start in full-screen. Can be changed at runtime. Can be overridden by a command line switch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Windowed Full-screen Mode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "If full-screen, prefer using a full-screen window to prevent display mode switching."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Full-screen Width"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Set the custom width size for the non-windowed full-screen mode. Leaving it unset will use the desktop resolution."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Full-screen Height"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Set the custom height size for the non-windowed full-screen mode. Leaving it unset will use the desktop resolution."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Force the resolution to the full-screen size, if set to 0, a fixed value of 3840 x 2160 will be used."
   )

/* Settings > Video > Windowed Mode */


/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Configuration Aspect Ratio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Configured"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Maintain 1:1 pixel aspect ratios when scaling content with the internal IPU. If it's disabled, images will then be stretched to fill the entire display."
   )
#endif
#if defined(RARCH_MOBILE)
#endif
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Cut off a few pixels around the edges of the image customarily left blank by developers which sometimes also contain rubbish pixels."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "Gamma/contrast control for HDR. Takes the colours and increases the overall range between the brightest parts and the darkest parts of the image. The higher HDR Contrast is, the larger this difference becomes, while the lower the contrast is, the more washed out the image becomes. Helps users tune the image to their liking and what they feel looks best on their display."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Once the colour space is converted to linear space, decide whether we should use an expanded colour gamut to get to HDR10."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Vertical Sync (V-Sync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Synchronise the output video of the graphics card to the refresh rate of the screen. Recommended."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "V-Sync Swap Interval"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Use a custom swap interval for V-Sync. Effectively reduces monitor refresh rate by the specified factor. 'Auto' sets factor based on core-reported frame rate, providing improved frame pacing when running e.g. 30 fps content on a 60 Hz display or 60 fps content on a 120 Hz display."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Adaptive V-Sync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "V-Sync is enabled until performance falls below the target refresh rate. Can minimize stuttering when performance falls below real time, and be more energy efficient."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Hard-synchronise the CPU and GPU. Reduces latency at the cost of performance."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Sync to Exact Content Frame-rate (G-Sync, Free-Sync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "No deviation from core requested timing. Use for Variable Refresh Rate screens (G-Sync, Free-Sync, HDMI 2.1 VRR)."
   )

/* Settings > Audio */

#ifdef HAVE_MICROPHONE
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "Re-sampler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Change audio re-sampler settings."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Change audio synchronisation settings."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "DSP Plug-in"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Audio DSP plug-in that processes audio before it's sent to the driver."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Remove DSP Plug-in"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Unload any active audio DSP plug-in."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Maximum audio latency in milliseconds. The driver aims to keep actual latency at 50% of this value. Might not be honoured if the audio driver can't provide given latency."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Re-sampler Quality"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Lower this value to favour performance/lower latency over audio quality, increase for better audio quality at the expense of performance/lower latency."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Desired audio input latency in milliseconds. Might not be honoured if the microphone driver can't provide given latency."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Re-sampler Quality"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Lower this value to favour performance/lower latency over audio quality, increase for better audio quality at the expense of performance/lower latency."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Synchronise the audio. Recommended."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Helps smooth out imperfections in timing when synchronising audio and video. Be aware that if disabled, proper synchronisation is nearly impossible to obtain."
   )

/* Settings > Audio > MIDI */


/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Will start playback of the audio stream. Once finished, it will jump to the next audio stream in sequential order and repeat this behaviour. Useful as an album playback mode."
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Enable 'Okay' Sound"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Polling Behaviour (Restart required)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Remap the Controls for This Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Autoconfiguration"
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
#endif
#ifdef ANDROID
#endif
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Always enable 'Game Focus' mode when launching and resuming content. When set to 'Detect', option will be enabled if current core implements front-end keyboard callback functionality."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "How far an axis must be tilted to result in a button press when using 'Analogue to Digital'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Analogue Dead-zone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Ignore analogue stick movements below dead-zone value."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Analogue Sensitivity"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Adjust the sensitivity of analogue sticks."
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Select the general behaviour of turbo mode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro uses a virtual gamepad abstraction known as the 'RetroPad' to communicate from frontends (like RetroArch) to cores and vice versa. This menu determines how the virtual RetroPad is mapped to the physical input devices and which virtual input ports these devices occupy.\nIf a physical input device is recognised and autoconfigured correctly, users probably do not need to use this menu at all, and for core-specific input changes, should use the Quick Menu's 'Controls' submenu instead."
   )

/* Settings > Input > Haptic Feedback/Vibration */


/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "If enabled, Info button presses will be ignored."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "If enabled, Search button presses will be ignored."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Disable Left Analogue in Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Prevent Left Analogue stick from navigating in menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Disable Right Analogue in Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Prevent Right Analogue stick from navigating in menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Menu Swap Okay and Cancel Buttons"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Swap buttons for Okay/Cancel. When disabled, the Japanese button orientation is on by default, when this is enabled, it is the western orientation instead."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "When assigned, the 'Hotkey Enable' key must be held before any other hotkeys are recognised. Allows controller buttons to be mapped to hotkey functions without affecting normal input. Assigning the modifier to controller only will not require it for keyboard hotkeys, and vice versa, but both modifiers work for both devices."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Add a delay in frames before normal input is blocked after pressing the assigned 'Hot-key Enable' key. Allows normal input from the 'Hot-key Enable' key to be captured when it is mapped to another action (e.g. RetroPad 'Select')."
   )



MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "If virtual disc tray is closed, this opens it and removes the loaded disc. Otherwise, it inserts the currently selected disc and closes the tray."
   )



MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Switches 'Game Focus' mode on/off. When content has focus, hot-keys are disabled (full keyboard input is passed to the running core) and mouse is grabbed."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Full-screen (Toggle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Switches between full-screen and windowed display modes."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Sync to Exact Content Frame-rate (Toggle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Toggles sync to exact content frame-rate on/off."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Pre-emptive Frames (Toggle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Switches Pre-emptive Frames on/off."
   )


MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Net-play Ping (Toggle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Switches the ping counter for the current net-play room on/off."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Net-play Hosting (Toggle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Switches net-play hosting on/off."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Net-play Play/Spectate Mode (Toggle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Switches current net-play session between 'play' and 'spectate' modes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Net-play Player Chat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Sends a chat message to the current net-play session."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Toggle between fading and static net-play chat messages."
   )


/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Analogue to Digital Type"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Use specified analogue stick for D-Pad input. 'Forced' modes override core native analogue input."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Map specified analogue stick for D-Pad input.\nIf core has native analogue support, D-Pad mapping will be disabled unless a '(Forced)' option is selected.\nIf D-Pad mapping is forced, core will receive no analogue input from specified stick."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "The physical controller as recognised by RetroArch."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Specifies which core port will receive input from front-end controller port %u."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "The physical mouse as recognised by RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Left Analogue X+ (Right)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Left Analogue X- (Left)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Left Analogue Y+ (Down)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Left Analogue Y- (Up)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Right Analogue X+ (Right)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Right Analogue X- (Left)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Right Analogue Y+ (Down)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Right Analogue Y- (Up)"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Run-Ahead is Unavailable]"
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Number of Pre-emptive Frames"
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "Some cores might have a shut down feature. If this option is left disabled, selecting the shut down procedure would trigger RetroArch being shut down.\nEnabling this option will load a dummy core instead so that we remain inside the menu and RetroArch won't shut down."
   )
#ifndef HAVE_DYNAMIC
#endif
#ifdef HAVE_MIST







#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Load customised core options by default at start-up."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Load customised configuration at start-up."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Load customised controls at start-up."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Save all core options to a common settings file (retroarch-core-options.cfg). When disabled, options for each core will be saved to a separate core-specific folder/file in RetroArch's 'Configurations' directory."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Autosaves the non-volatile SRAM at a regular interval. This is disabled by default unless set otherwise. The interval is measured in seconds. A value of 0 disables autosaves."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Automatically load the auto save state on start-up."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Write non-volatile SaveRAM files in an archived format. Dramatically reduces file size at the expense of (negligibly) increased saving/loading times.\nOnly applies to cores that enable saving with the standard libretro SaveRAM interface."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Front-end Logging Level"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Set log level for the front-end. If a log level issued by the front-end is below this value, it is ignored."
   )

/* Settings > File Browser */


/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Fast-Forward Frame-skip"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Throttle Menu Frame-rate"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Makes sure the frame-rate is capped while inside the menu."
   )

/* Settings > Frame Throttle > Rewind */


/* Settings > Frame Throttle > Frame Time Counter */


/* Settings > Recording */


/* Settings > On-Screen Display */


/* Settings > On-Screen Display > On-Screen Overlay */


#if defined(ANDROID)
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Show the Mouse Cursor With Overlay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Analogue Recentring Zone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Analogue stick input will be relative to first touch if pressed within this zone."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */


/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Select input to send when two pointers are on screen. Trigger Delay should be non-zero to distinguish from other input."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Select input to send when three pointers are on screen. Trigger Delay should be non-zero to distinguish from other input."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Select input to send when four pointers are on screen. Trigger Delay should be non-zero to distinguish from other input."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */


/* Settings > On-Screen Display > Video Layout */


/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Graphical Widgets"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Scale Graphical Widgets Automatically"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Graphical Widgets Scale Override (Full-screen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Apply a manual scaling factor override when drawing display widgets in full-screen mode. Only applies when 'Scale Graphical Widgets Automatically' is disabled. Can be used to increase or decrease the size of decorated notifications, indicators and controls independently from the menu itself."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Graphical Widgets Scale Override (Windowed)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Apply a manual scaling factor override when drawing display widgets in windowed mode. Only applies when 'Scale Graphical Widgets Automatically' is disabled. Can be used to increase or decrease the size of decorated notifications, indicators and controls independently from the menu itself."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Display Frame-rate"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Frame-rate Update Interval (In Frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "Frame-rate display will be updated at the set interval in frames."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Display Net-play Ping"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Display the ping for the current net-play room."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "\"Load Content\" Start-up Notification"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Input (Autoconfigured) Connection Notifications"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Configuration Override Loaded Notifications"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Display an on-screen message when automatically restoring at launch the last used disc of multi-disc content loaded with M3U playlists."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Extra Net-play Notifications"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Display non-essential net-play on-screen messages."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Notification Colour (Red)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Sets the red value of the OSD text colour. Valid values are between 0 and 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Notification Colour (Green)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Sets the green value of the OSD text colour. Valid values are between 0 and 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Notification Colour (Blue)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Sets the blue value of the OSD text colour. Valid values are between 0 and 255."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Enables a background colour for the OSD."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Notification Background Colour (Red)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Sets the red value of the OSD background colour. Valid values are between 0 and 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Notification Background Colour (Green)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Sets the green value of the OSD background colour. Valid values are between 0 and 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Notification Background Colour (Blue)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Sets the blue value of the OSD background colour. Valid values are between 0 and 255."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Sets the opacity of the OSD background colour. Valid values are between 0.0 and 1.0."
   )

/* Settings > User Interface */

#ifdef _3DS
#endif
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Automatically quit RetroArch when closing content. 'CLI' quits only when content is launched with a command line."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Open Desktop Menu on Start-up"
   )

/* Settings > User Interface > Menu Item Visibility */

#ifdef HAVE_LAKKA
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Show 'Favourites'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Show the 'Favourites' menu. (Restart required on Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Show 'Net-play'"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Show start-up screen in menu. This is automatically set to false after the program starts for the first time."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Show 'Add to Favourites'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Show the 'Add to Favourites' option."
   )

/* Settings > User Interface > Views > Settings */



/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Select an image to set as menu background. Manual and dynamic images will override 'Colour Theme'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Use Preferred System Colour Theme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Use operating system's colour theme (if any). Overrides theme settings."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "The animation speed when the long menu text scrolls."
   )

/* Settings > AI Service */


/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Leader-boards"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Periodically send contextual game information to the RetroAchievements website. Has no effect if 'Hardcore Mode' is enabled."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Display badges in the Achievements List."
   )

/* Settings > Achievements > Appearance */


/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "Log-in Messages"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "Show messages related to RetroAchievements account log-in."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Show additional diagnostic and error messages."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Publicly Announce Net-play"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Whether to announce net-play games publicly. If unset, clients must manually connect rather than using the public lobby."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Forward net-play connections through a man-in-the-middle server. Useful if the host is behind a firewall or has NAT/UPnP problems."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "South America (South-east, Brazil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "South-east Asia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Net-play TCP Port"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Maximum Simultaneous Connections"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Net-play Spectator Mode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Start net-play in spectator mode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Whether to start net-play in spectator mode. If set to true, net-play will be in spectator mode on start. It's always possible to change mode later."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Chat Colour (Nickname)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Chat Colour (Message)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Allow players to pause during net-play."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Net-play Check Frames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "The frequency (in frames) that net-play will verify that the host and client are in sync."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "The frequency in frames with which net-play will verify that the host and client are in sync. With most cores, this value will have no visible effect and can be ignored. With nondeterminstic cores, this value determines how often the net-play peers will be brought into sync. With buggy cores, setting this to any non-zero value will cause severe performance issues. Set to zero to perform no checks. This value is only used on the net-play host."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "The number of frames of input latency for net-play to use to hide network latency. Reduces jitter and makes net-play less CPU-intensive, at the expense of noticeable input lag."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "The number of frames of input latency for net-play to use to hide network latency.\nWhen in net-play, this option delays local input, so that the frame being run is closer to the frames being received from the network. This reduces jitter and makes net-play less CPU-intensive, but at the price of noticeable input lag."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "The range of frames of input latency that may be used to hide network latency. Reduces jitter and makes net-play less CPU-intensive, at the expense of unpredictable input lag."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "The range of frames of input latency that may be used by net-play to hide network latency.\nIf set, net-play will adjust the number of frames of input latency dynamically to balance CPU time, input latency and network latency. This reduces jitter and makes net-play less CPU-intensive, but at the price of unpredictable input lag."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Net-play NAT Traversal"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Build-bot Cores URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL to core updater directory on the libretro build-bot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Build-bot Assets URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL to assets updater directory on the libretro build-bot."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Specify how many automatically generated backups to keep for each installed core. When this limit is reached, creating a new backup with an online update will delete the oldest backup. Manual core backups are unaffected by this setting."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Favourites Size"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Limit the number of entries in the 'Favourites' playlist. Once the limit is reached, new additions will be prevented until old entries are removed. Setting a value of -1 allows 'unlimited' entries.\nWARNING: Reducing the value will delete existing entries!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Allow the Renaming of Entries"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Allow the Removal of Entries"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Specify when to tag playlist entries with the currently associated core (if any).\nThis setting is ignored when playlist sub-labels are enabled."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Show Content Specific Icons in History and Favourites"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Show specific icons for each history and favourites playlist entry. Has a variable performance impact."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Select which type of runtime log record to display on playlist sub-labels.\nThe corresponding runtime log must be enabled with the 'Saving' options menu."
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Specify core to use when launching content with a playlist entry that does not have an existing core association."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Remove playlist from file-system."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Input your username here. This will be used for net-play sessions, among other things."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Localises the menu and all on-screen messages according to the language you have selected here. Requires a restart for the changes to take effect.\nTranslation completeness is shown next to each option. In case a language is not implemented for a menu item, we fall back to English."
   )

/* Settings > User > Privacy */


/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Log-in details for your RetroAchievements account. Visit retroachievements.org and sign up for a free account.\nAfter you are done registering, you need to input the username and password into RetroArch."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Input the password of your RetroAchievements account. Maximum length: 255 characters."
   )

/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > User > Accounts > Facebook Gaming */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Recording Configurations"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Favourites Playlist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Save the Favourites playlist to this directory."
   )

#ifdef HAVE_MIST
/* Settings > Steam */



#endif

/* Music */

/* Music > Quick Menu */


/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Connect to Net-play Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Enter net-play server address and connect in client mode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Disconnect From Net-play Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Disconnect an active net-play connection."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Refresh Net-play Host List"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Scan for net-play hosts."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Refresh Net-play LAN List"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Scan for net-play hosts on LAN."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Start Net-play Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Start net-play in host (server) mode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Stop Net-play Host"
   )

/* Import Content */


/* Import Content > Scan File */


/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Specify a 'system name' with which to associate scanned content. Used to name the system to the generated playlist file and to identify playlist thumbnails."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Initialising list..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "By Art-style"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Enter name of the new view"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Add to Favourites"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Add the content to 'Favourites'."
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */

MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Playtime"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Add to Favourites"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Add the content to 'Favourites'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Start streaming to the chosen destination."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "End the stream."
   )

/* Quick Menu > Options */


/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Force current settings to be written to active options file. Ensures options are preserved in the event that a core bug causes improper shutdown of the front-end."
   )

/* - Legacy (unused) */

/* Quick Menu > Controls */


/* Quick Menu > Controls > Manage Remap Files */


/* Quick Menu > Controls > Manage Remap Files > Load Remap File */


/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Load a cheat file and replace the existing cheats."
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Press Left or Right to change the value."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Press Left or Right to change the value."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Press Left or Right to change the value."
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */


/* Quick Menu > Cheats > Load Cheat File (Append) */


/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Address bit-mask when Memory Search Size < 8-bit."
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Open the virtual disc tray and remove the currently loaded disc. If 'Pause Content When Menu Is Active' is enabled, some cores may not register changes unless content is resumed for a few seconds after each disc control action."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Insert the disc corresponding to 'Current Disc Index' and close the virtual disc tray. If 'Pause Content When Menu Is Active' is enabled, some cores may not register changes unless content is resumed for a few seconds after each disc control action."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Eject current disc, select a new disc from the filesystem then insert it and close the virtual disc tray.\nNOTE: This is a legacy feature. It is instead recommended to load multi-disc titles with M3U playlists, which allow disc selection using the 'Eject/Insert Disc' and 'Current Disc Index' options."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Select a new disc from the filesystem and insert it without closing the virtual disc tray.\nNOTE: This is a legacy feature. It is instead recommended to load multi-disc titles with M3U playlists, which allow disc selection using the 'Current Disc Index' option."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Changes to the shader configuration will take effect immediately. Use this if you've changed the amount of shader passes, filtering, FBO scale, etc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "After changing shader settings such as the amount of shader passes, filtering, FBO scale, use this to apply changes.\nChanging these shader settings is a somewhat expensive operation so it has to be done explicitly.\nWhen you apply shaders, the shader settings are saved to a temporary file (retroarch.slangp/.cgp/.glslp) and loaded. The file persists after RetroArch exits and is saved to the Shader Directory."
   )

/* Quick Menu > Shaders > Save */




/* Quick Menu > Shaders > Remove */


/* Quick Menu > Shaders > Shader Parameters */


/* Quick Menu > Overrides */


/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Achievements cannot be activated with this core"
)

/* Quick Menu > Information */


/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "No Favourites Available"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
   "Okay"
   )

/* Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Analogue Input Sharing"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Maximum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "History & Favourites"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "History & Favourites"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad with Analogue"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Box-art"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Left Analogue"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Right Analogue"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Left Analogue (Forced)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Right Analogue (Forced)"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Increase coarseness of the menu background chequerboard pattern."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Increase coarseness of menu border chequerboard."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Resize and position menu entries to make the best use of the available screen space. Disable this to use the classic fixed-width two column layout."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Upscale menu interface before drawing to screen. When used with 'Menu Linear Filter' enabled, removes scaling artefacts (uneven pixels) while maintaining a sharp image. Has a significant performance impact that increases with upscaling level."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Colour Theme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Select a different colour theme. Choosing 'Custom' enables the use of menu theme preset files."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Enable background display of content while Quick Menu is active. Disabling transparency may alter theme colours."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Nearest Neighbour (Fast)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (Centred)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (Centred)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (Centred)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (Centred)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (Centred)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Fit the Screen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Fill the Screen (Stretched)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarised Dark"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarised Light"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Grey Dark"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Grey Light"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Type of thumbnail to display on the left side."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Colour Theme Alpha Factor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Font Colour (Red)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Font Colour (Green)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Font Colour (Blue)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Colour Theme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Select a different background colour theme."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "Grey Dark"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "Grey Light"
   )

/* Ozone: Settings > User Interface > Appearance */


MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Colour Theme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Select a different colour theme."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Solarised Dark"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarised Light"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Grey Dark"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Grey Light"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Optimise Landscape Layout"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Automatically move the navigation bar to the right-hand side of the screen when using landscape display orientations."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Colour Theme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Select a different background colour theme."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarised Dark"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Grey Dark"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Grey Light"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Box-art"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Highlight colour:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Select Colour"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "\"All Playlists\" maximum list entries:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "\"All Playlists\" maximum grid entries:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Please fill out all the required fields."
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "Turbo/Dead-zone"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Front-end Counters"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "No net-play hosts found."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "No net-play clients found."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Maximum Swap-chain Images"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "Waitable Swap-chains"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Hard-synchronise the CPU and GPU. Reduces latency at the cost of performance."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Maximum Frame Latency"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Net-play"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Net-play Delay Frames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Search for and connect to net-play hosts on the local network."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Net-play Client"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Net-play Spectator"
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Left Analogue X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Left Analogue Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Right Analogue X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Right Analogue Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Database - Filter: Maximum Users"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Configured"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Net-play settings"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "OpenGL/Direct3D Render-to-texture (multi-pass shaders) support"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "Confirm/Okay"
   )

/* Discord Status */


/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Net-play will start when content is loaded."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "Content must be loaded before starting net-play."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Couldn't find a suitable core or content file, please try loading it manually."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Net-play scan complete."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Sorry, unimplemented: cores that don't demand content cannot participate in net-play."
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Unknown net-play command received"
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Net-play Port Mapping Successful"
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Net-play UPnP Port Mapping Failed"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "A net-play connection attempt failed because the peer is not running RetroArch, or is running an old version of RetroArch."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "A net-play peer is running an old version of RetroArch. Cannot connect."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "WARNING: A net-play peer is running a different version of RetroArch. If problems occur, use the same version."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "A net-play peer is running a different core. Cannot connect."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "WARNING: A net-play peer is running a different version of the core. If problems occur, use the same version."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "This core does not support net-play between these platforms"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "This core does not support net-play between different platforms"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Enter net-play server password:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Enter net-play chat message:"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "A net-play client has disconnected"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Net-play disconnected"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "Net-play peer \"%s\" paused"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Your nickname was changed to \"%s\""
   )

MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Connecting to net-play host"
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "All installed cores are at their latest version"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "All supported cores are switched to Play Store versions"
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Added to favourites"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Failed to add favourite: playlist full"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Audio disabled."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Audio enabled."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Could not initialise autosave."
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Cannot infer new configuration path. Use the current time."
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Configuration directory not set. Cannot save new configuration."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Could not read film header."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Could not read state from film."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "CRC32 checksum mismatch between content file and saved content checksum in replay file header. Replay highly likely to de-sync on playback."
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Failed saving configuration to"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Failed to load film file"
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Failed to start film record."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Front-end for libretro"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Loading favourites file"
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Input replay film format seems to have a different serialiser version. It will most likely fail."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "Input replay film playback ended."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Stopping film record."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Failed to initialise net-play."
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "Core does not support net-play."
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Restarting recording due to driver re-initiation."
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "Rewind unavailable because this core lacks serialised save state support."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Initialising rewind buffer with size"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Failed to initialise rewind buffer. Rewinding will be disabled."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Saved new configuration to"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Starting film playback."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Starting film record to"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Full-screen thumbnails"
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Unrecognised command \"%s\" received.\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Using core name for new configuration."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Scanning Bluetooth devices..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Scanning for net-play hosts..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Password is correct."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Password is incorrect."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Password is correct."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Password is incorrect."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "Searching has not been initialised/started."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "There's not enough room. The maximum number of simultaneous cheats is 100."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "New cheat added to the top of the list."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "New cheat added to the bottom of the list."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "Sync to exact content frame-rate enabled."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "Sync to exact content frame-rate disabled."
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Front-end name"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Graphical Widgets Scale Override"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Apply a manual scaling factor override when drawing display widgets. Only applies when 'Scale Graphical Widgets Automatically' is disabled. Can be used to increase or decrease the size of decorated notifications, indicators and controls independently from the menu itself."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Scan for Bluetooth devices and connect them."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Scan for wireless networks and establish a connection."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "De-flicker"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Adjust display overscan cropping by reducing image size by specified number of scan lines (taken from top of screen). May introduce scaling artefacts."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Adjust display overscan cropping by reducing image size by specified number of scan lines (taken from bottom of screen). May introduce scaling artefacts."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Allows the choice of what governors to use in menus and during gameplay. Performance, Ondemand or Schedutil are recommended during gameplay."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Use the lowest frequency available to save power. Useful on battery powered devices, but performance will be significantly reduced."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Enabling Linux GameMode can improve latency, fix audio crackling issues and maximise overall performance by automatically configuring your CPU and GPU for best performance.\nThe GameMode software needs to be installed for this to work. See https://github.com/FeralInteractive/gamemode for information on how to install GameMode."
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "CPU Over-clock"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Over-clock the Switch CPU."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "Displays a list of available time zones. After selecting a time zone, time and date is adjusted to the selected time zone. It assumes, that system/hardware clock is set to UTC."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "CPU Over-clock"
   )
#endif
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Adjust x/y scale for Wii-mote light gun speed."
   )
#endif
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Enable along with Mouse to utilise the touch screen as a trackball, adding inertia to the pointer."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA scaling and bi-cubic filtering. May break widgets."
   )
#else
#endif
#ifdef _3DS
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Tap the Touch Screen to go\nto the RetroArch menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "Font Colour Red"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "Adjust bottom screen font red colour."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "Font Colour Green"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "Adjust bottom screen font green colour."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "Font Colour Blue"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "Adjust bottom screen font blue colour."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "Font Colour Opacity"
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Scan Finished.<br><br>\nIn order for content to be correctly scanned, you must:\n<ul><li>have a compatible core already downloaded</li>\n<li>have \"Core Info Files\" updated with the Online Updater</li>\n<li>have \"Databases\" updated with the Online Updater</li>\n<li>restart RetroArch if any of the above was just done</li></ul>\nFinally, the content must match existing databases from <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">here</a>. If it is still not working, consider <a href=\"https://www.github.com/libretro/RetroArch/issues\">submitting a bug report</a>."
   )
#endif
#ifdef HAVE_GAME_AI





#endif
