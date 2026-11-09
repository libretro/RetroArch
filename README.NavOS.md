# NavOS RetroArch

NavOS RetroArch is a modified build of [RetroArch](https://github.com/libretro/RetroArch)
used as the emulation engine for the NavOS frontend. It is a derivative work of
RetroArch and is licensed under GPLv3 (see `COPYING`). The original upstream
`README.md` is unchanged.

## What differs from upstream

The patches touch these files (from the project history, not a feature promise):

- `cheat_manager.c`, `cheevos/cheevos.c`, `command.c`, `retroarch.c`, `runloop.c`
- `input/bsv/bsvmovie.c`, `menu/menu_displaylist.c`, `menu/menu_driver.c`
- `network/netplay/netplay_frontend.c`, `tasks/task_content.c`,
  `tasks/task_netplay_find_content.c`, `tasks/task_save.c`, `tasks/task_screenshot.c`
- `libretro-common/formats/vp8/rvp8.c` (variable rename to avoid a macro clash on Android)

According to `Navin/CHANGELOG.md`, the changes cover Hardcore-mode handling of
save states and auto states, entry-state loading, rewind and cheats, plus
netplay, badge-task and screenshot-memory fixes. Details are in the `Navin/`
documents. Behaviour described there has been tested by the maintainer only;
nothing here is independently verified.

## Policy notice

Hardcore Mode is a RetroAchievements feature that restricts save states, rewind
and cheats. These patches remove those restrictions in the emulator.
RetroAchievements may not accept achievements earned this way, and using such a
build with a RetroAchievements account may violate their rules. Check their
current policy before using it with an account. This project is not affiliated
with RetroAchievements.

## Platforms

- Windows 64-bit, built with MSYS2 MinGW-w64: the maintained target.
- Android (`pkg/android/phoenix`): experimental, not a completed milestone.

## Building on Windows (MSYS2 MinGW-w64)

Prerequisites per `Navin/How to Modify Rev2.md`: Windows 10+ 64-bit, MSYS2 with
`mingw-w64-x86_64-gcc`, `mingw-w64-x86_64-make` and `git`. Follow
section 9 "Exact Build Instructions" of that document. It is the authoritative
procedure; it is not duplicated here to avoid drift.

## Building on Android

Use the Gradle project in `pkg/android/phoenix`. Notes:

- Use a Java 21 JDK; a Java 25 JDK has been seen to break the Gradle build on the
  maintainer's machine.
- Provide `local.properties` (SDK path) and any release-signing properties
  locally. Neither is part of this repository.

## Known limitations

- Android support is experimental.
- The Windows procedure has been run on one maintainer machine.
- Prebuilt cores and DLLs are not included; see `NOTICE`.

## Attribution

RetroArch is developed by the libretro team and contributors. See `COPYING`,
`AUTHORS.h` and `NOTICE`.
