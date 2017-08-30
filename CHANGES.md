# 1.6.8 (future)
- GUI: (MaterialUI) Skip querying and drawing items that are not visible; Cache content height and bbox calculation.
- GUI: (XMB) Skip drawing the fading list when it is already transparent. Optimization.
- GUI: (XMB) Comment out visible item calculation in xmb_draw_items().
- GUI: (RGUI) Prevent crashes when using a non-English language reliant on UTF8.
- LOCALIZATION: Update Italian translation.
- LOCALIZATION: Update Portuguese-Brazilian translation.
- LOCALIZATION: Update Russian translation.
- LINUX/PI: Broadcom VC4: Add Videocore config option
- NETPLAY: Fix disconnection not fully deinitializing Netplay.
- COMMON: Fix clear/free loop conditionals in playlists.
- WINDOWS/GDI: Fix flickering of text.
- WINDOWS/WGL: Try to use wglSwapLayerBuffers instead of SwapBuffers if possible (for more optimal performance).
- WII: Use custom, embedded libogc SDK.
- WIIU: Initial touchscreen support for WiiU gamepad.

# 1.6.7
- SCANNER: Fix directory scanning.
- SCANNER: Fix file scanning.
- COMMON: Fix 'Disk Image Append' option.
- FREEBSD: Compatibility fixes for Video4Linux2 camera driver.
- GUI: (MaterialUI) Add disk image append icons.
- GUI: (MaterialUI) Improve word wrapping when menu icons are enabled.
- GUI: (MaterialUI) Add User Interface -> Appearance -> Menu Icons Enable. You can turn on/off the icons on the lefthand side of the menu entries.
- GUI: Performance optimizations for XMB menu driver - only calculates visible items.
- LOCALIZATION: Update Italian translation.

# 1.6.6 (future)
- 3DS: Fixes serious performance regression that affected every core; rewind was always implicitly enabled.
- AUDIO: MOD/S3M/XM sound should now be properly mixed in with the core's sound.
- GUI: Visual makeover of MaterialUI.
- GUI: Added 'Music', 'Images' and 'Video' collection options to RGUI/MaterialUI.
- GUI: Allow the user to add 'Favorites'.
- GUI: Allow the user to rename entries.
- GUI: Performance optimizations for XMB menu driver.
- LOCALIZATION: Update Italian translation
- INPUT: Overlay controller response - when we press buttons on the gamepad or keyboard, the corresponding buttons on the overlay will be highlighted as well.
- NETBSD: Silence some compilation warnings.
- COMMON: Fixed bug 'Deleting an entry from a playlist would not update the list view inside XMB'.
- COMMON: Fix inet_ntop_compat on *nix
- LOBBY: Add skeleton to add help descriptions to lobbies

# 1.6.5
Skipped this one.

# 1.6.4

- ANDROID: Fire Stick & Fire TV remote overrides gamepad port 0 on button press and viceversa like SHIELD devices
- ANDROID: Provide default save / system / state / screenshot locations
- AUDIO: Audio mixer supports MOD/S3M/XM file types now!
- INPUT: input swap override flag (for remotes) is cleared correctly
- INPUT: allow specifying libretro device in remap files
- INPUT: allow specifying analog dpad mode in remap files
- INPUT: allow saving libretro device to remap files
- INPUT: allow saving analog dpad mode to remap files
- INPUT: allow removing core and game remap files from the menu
- COMMON: Cores can now request to set a 'shared context'. You no longer need to explicitly enable 'Shared Hardware Context' for Citra/OpenLara/Dolphin.
- COMMON: Add 'Delete Core' option to Core Information menu.
- COMMON: Allow Max Timing Skew to be set to 0.
- COMMON: Change the "content dir" behavior so it works on either a flag or an empty directory setting, now platform drivers can provide defaults for save / system / state / screenshot dirs and still allow the content dir functionality, these settings are under settings / saving and flagged as advanced
- GUI: You can turn on/off 'Horizontal Animation' now for the XMB menu. Turning animations off can result in a performance boost.
- GUI: Fix sublabel word-wrapping in XMB where multi-byte languages were cut off too soon
- LOCALIZATION: Update Dutch translation
- LOCALIZATION: Update Traditional Chinese translation
- LOCALIZATION: Update Italian translation
- LOCALIZATION: Update Russian translation
- WINDOWS: Provide default save / system / state / screenshot locations
- LOBBIES: Show what country the host is in
- MENU: Enable OSD text rendering for gdi and libcaca drivers
- WINDOWS 98/ME/2K: Set default directory for MSVC 2005 RetroArch version.
- WII: Better V-Sync handling, backported from SuperrSonic.
- WIIU: Exception handler rewritten.

# 1.6.3
- IOS: Fix GL regression - 32bit color format cores were no longer rendering
- CHEEVOS: Add support for N64 cheevos and other small fixes.
- CHEEVOS: Add 'Achievements -> Achievements Verbose Mode'. Ability to display cheevos related messages in OSD, useful for RetroAchievements users.
- AUDIO: Audio mixer's volume can now be independently increased/decreased, and muted.
- AUDIO: Mute now no longer disables/enables audio but instead properly mutes the audio volume. Mute is also independent from the audio mixer volume.
- INPUT: Add mouse index selection; ability now to select between different mice
- INPUT: Fix 'All Users Control Menu' setting
- LINUX: Add a tinyalsa audio driver. Doesn't require asoundlib, should be self-contained and lower-level.
- LOBBIES: Announce the RetroArch version too
- LOCALIZATION: Add Traditional Chinese translation
- LOCALIZATION: Update French translation
- LOCALIZATION: Update Italian translation
- LOCALIZATION: Update Japanese translation
- LOCALIZATION: Update Russian translation
- MENU: Add 'User Interface -> Views'. Ability to display/hide online updater and core updater options.
- NETPLAY: Disconnecting one client shouldn't cause everyone to disconnect anymore
- NETWORK: SSL/TLS support, disabled by default
- SCANNER: Fix PS1 game scanning
- SCANNER: Move content list builder into scanner task with progress, fixes menu freeze with large playlists
- SDL2: Fix 'SDL2 driver does not see the hat on wired Xbox 360 controller"
- SETTINGS: Fix regression 'Custom Viewport is no longer overridable per-core or per-game'
- VITA: Add cheevos support
- VITA: Add support for external USB if mounted
- WAYLAND: Fix menu mouse input
- WII: Add support for single-port 'PS1/PS2 to USB controller adapter'

# 1.6.0
- ANDROID: Allow remotes to retain OK/Cancel position when menu_swap_ok_cancel is enabled
- ANDROID: Improve autoconf fallback
- ANDROID: Improve shield portable/gamepad device grouping workaround
- ANDROID: Runtime permission checking
- AUDIO: Audio mixer support. Mix up to 8 streams with the game's audio.
- AUTOSAVE/SRAM - Fix bug #3829 / #4820 (https://github.com/libretro/RetroArch/issues/3829)
- ENDIANNESS: Fixed database scanning. Should fix scanning on PS3/WiiU/Wii, etc.
- LOBBIES: Fallback to filename based matching if no CRC matches are found (for people making playlists by hand)
- LOBBIES: GUI refinement, show stop hosting when a host has been started, show disconnect when playing as client
- LOBBIES: if the game is already loaded it will try to connect directly instead of re-loading content (non-fullpath cores only)
- LOBBIES: unify both netplay menus
- LOCALIZATION/GUI: Korean font should display properly now with XMB/MaterialUI's default font
- LOCALIZATION: Update German translation
- LOCALIZATION: Update Japanese translation
- LOCALIZATION: Update Russian translation
- LOCALIZATION: Update/finish French translation
- MENU: Improved rendering for XMB ribbon; using additive blending (Vulkan/GL)
- MISC: Various frontend optimizations.
- NET: Fix bug #4703 (https://github.com/libretro/RetroArch/issues/4703)
- OSX/MACOS: Fixes serious memory leak
- THUMBNAILS: Thumbnails show up now in Load Content -> Collection, Information -> Database 
- VIDEO: Fix threaded video regression; tickering of menu entries would no longer work.
- VITA: Fix 30fps menu (poke into input now instead of reading the entire input buffer which apparently is slow)
- VITA: Fix frame throttle
- VITA: Fix slow I/O
- VULKAN: Fix some crashes on loading some thumbnails
- VULKAN: Unicode font rendering support. Should fix bad character encoding for French characters, etc.
- WII: Fix crashing issues which could occur with the dummy core
- WIIU: HID Controller support
- WIIU: Initial network/netplay support
- WIIU: XMB/MaterialUI menu driver support
- WINDOWS: Added RawInput input driver for low-latency, low-level input.
- WINDOWS: Added WASAPI audio driver for low-latency audio. Both shared and exclusive mode.
- WINDOWS: Core mouse input should be relative again in cores

# 1.5.0
- ANDROID: Autoconf fallback
- ANDROID: Mouse support / Emulated mouse support
- AUTOCONF: Fix partial matches for pad name
- CHEEVOS: Fix crashes in the cheevos description menu
- CHEEVOS: WIP leaderboards support
- COMMON: 9-slice texture drawing support
- COMMON: Threading fixes
- CORETEXT/APPLE: Ability to load menu display font drivers and loading of custom font.
- DOS: Add keyboard driver
- DOS: Improve color accuracy and scaling
- GUI: Add a symbol page in the OSK
- GUI: Allow changing icon theme on the fly
- GUI: Better dialogs for XMB
- GUI: Various settings are now only visible when advanced settings is enabled
- LOCALIZATION: Add/update Korean translation
- LOCALIZATION: Rewrite German translation
- LOCALIZATION: Update several English sublabels
- LOCALIZATION: Update several Japanese labels
- MOBILE: Long-tap a setting to reset to default
- MOBILE: Single-tap for menu entry selection
- NET: Allow manual netplay content loading
- NET: Announcing network games to the public lobby is optional now
- NET: Bake in miniupnpc
- NET: Fix netplay join for contentless cores
- NET: Fix netplay rooms being pushed on the wrong tab
- NET: Lan games show next to lobbies with (lan) and connect via the private IP address
- NET: Use new lobby system with MITM support
- NUKLEAR: Update to current version
- SCANNER: Always add 7z & zip to supported extensions
- VULKAN: Add snow/bokeh shader pipeline effects - at parity with GL now
- VULKAN: Find supported composite alpha in swapchain
- WIIU: Keyboard support
- WINDOWS: Fix loading of core/content via file menu
- WINDOWS: Logging to file no longer spawns an empty window

# 1.4.1
