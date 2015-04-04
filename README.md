# SAKUJ0's repository

This is a sandbox for development purposes only. Under no circumstances use this repository for any purposes other
than to test communicated changes. Go to the [official GitHub repository](https://github.com/libretro/RetroArch)
and find support on the [official Libretro web page](http://libretro.com).

## Game-specific and core-specific overrides

Tries to append game-specific and core-specific configuration.
These settings will always have precedence, thus this feature
can be used to enforce overrides.

Let $RETROARCH_CFG be the directory that contains retroarch.cfg,
$CORE_NAME be the name of the libretro core library in use and
$ROM_NAME the basename of the ROM (without the extension and
directory).

This function only has an effect if a game-specific or core-specific
configuration file exists at the folling locations respectively.

   - core-specific: $RETROARCH_CFG/$CORE_NAME/$CORE_NAME.cfg
   - game-specific: $RETROARCH_CFG/$CORE_NAME/$ROM_NAME.cfg

