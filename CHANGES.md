# 1.5.1 (future)
- NET: Fix bug #4703 (https://github.com/libretro/RetroArch/issues/4703)
- ANDROID: Runtime permission checking
- LOCALIZATION: Update/finish French translation
- LOCALIZATION: Update German translation
- LOCALIZATION: Update Japanese translation
- LOCALIZATION/GUI: Korean font should display properly now with XMB/MaterialUI's
default font
- OSX/MACOS: Fixes serious memory leak
- WINDOWS: Added WASAPI audio driver for low-latency audio. Both shared and exclusive mode.
- MISC: Various frontend optimizations.
- VIDEO: Fix threaded video regression; tickering of menu entries would no longer work.

# 1.5.0
- MOBILE: Single-tap for menu entry selection
- MOBILE: Long-tap a setting to reset to default
- ANDROID: Autoconf fallback
- ANDROID: Mouse support / Emulated mouse support
- AUTOCONF: Fix partial matches for pad name
- CHEEVOS: Fix crashes in the cheevos description menu
- CHEEVOS: WIP leaderboards support
- COMMON: Threading fixes
- COMMON: 9-slice texture drawing support
- CORETEXT/APPLE: Ability to load menu display font drivers and loading of custom font.
- DOS: Add keyboard driver
- DOS: Improve color accuracy and scaling
- GUI: Various settings are now only visible when advanced settings is enabled
- GUI: Allow changing icon theme on the fly
- GUI: Add a symbol page in the OSK
- GUI: Better dialogs for XMB
- LOCALIZATION: Add/update Korean translation
- LOCALIZATION: Rewrite German translation
- LOCALIZATION: Update several English sublabels
- LOCALIZATION: Update several Japanese labels
- NET: Allow manual netplay content loading
- NET: Announcing network games to the public lobby is optional now
- NET: Bake in miniupnpc
- NET: Fix netplay join for contentless cores
- NET: Lan games show next to lobbies with (lan) and connect via the private IP address
- NET: Use new lobby system with MITM support
- NET: Fix netplay rooms being pushed on the wrong tab
- NUKLEAR: Update to current version
- SCANNER: Always add 7z & zip to supported extensions
- VULKAN: Find supported composite alpha in swapchain
- VULKAN: Add snow/bokeh shader pipeline effects - at parity with GL now
- WIIU: Keyboard support
- WINDOWS: Logging to file no longer spawns an empty window
- WINDOWS: Fix loading of core/content via file menu

# 1.4.1
