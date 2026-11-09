# NavOS — RetroAchievement Manager
**User Manual (Revision 2)**

## Table of Contents
1. [Introduction](#1-introduction)
2. [System Requirements](#2-system-requirements)
3. [Installation](#3-installation)
4. [First-Time Setup](#4-first-time-setup)
5. [Understanding Hardcore Mode](#5-understanding-hardcore-mode)
6. [Save States](#6-save-states)
7. [Rewind](#7-rewind)
8. [Cheats](#8-cheats)
9. [External Memory Commands](#9-external-memory-commands)
10. [Netplay](#10-netplay)
11. [Video Features](#11-video-features)
12. [Frame/Playback Controls](#12-frameplayback-controls)
13. [RetroAchievements](#13-retroachievements)
14. [Understanding What NavOS Does NOT Change](#14-understanding-what-navos-does-not-change)
15. [Troubleshooting](#15-troubleshooting)
16. [Backup and Recovery](#16-backup-and-recovery)
17. [Updating NavOS](#17-updating-navos)
18. [Advanced Users](#18-advanced-users)
19. [Glossary](#19-glossary)
20. [Final Quick Reference](#20-final-quick-reference)

---

## 1. Introduction
Welcome to NavOS. 
**What NavOS is:** NavOS is a premium, custom-built emulation frontend heavily based on RetroArch, the leading unified emulator interface. 
**What it is designed for:** NavOS is designed specifically for enthusiasts who want to earn RetroAchievements in Hardcore Mode without sacrificing quality-of-life emulator features (like Save States, Rewind, and Fast Forward). 
**RetroAchievements:** An online community that adds achievements/trophies to classic games. 
**Hardcore Mode:** A mode on RetroAchievements that typically forces players to play games exactly as they were originally released (no saves, no rewinding, no cheats). **NavOS uniquely bypasses these restrictions for the user, allowing full emulator functionality while keeping Hardcore Mode active.**

## 2. System Requirements
- **OS:** Windows 10 or later (64-bit).
- **CPU:** Intel Core i3 / AMD Ryzen 3 or better.
- **GPU:** OpenGL 2.0 or DirectX 11 compatible graphics card.
- **RAM:** 4GB minimum.
- **Storage:** 500MB minimum for NavOS, plus additional space for ROMs and Cores.

## 3. Installation
1. Extract the downloaded `NavOS.zip` into a dedicated folder on your PC (e.g., `C:\Emulation\NavOS\`).
2. Make sure you do not overwrite an existing standard RetroArch installation, to avoid configuration conflicts.
3. Launch `retroarch.exe` to begin.
4. **Important:** NavOS relies on `libwinpthread-1.dll` and other MinGW-w64 libraries included in the folder. Do not delete them.

## 4. First-Time Setup
When you first launch NavOS:
1. **Accounts:** Go to `Settings > Achievements`. Enter your RetroAchievements Username and Password.
2. **Cores:** Go to `Main Menu > Online Updater > Core Downloader` to download emulation cores (e.g., FCEUmm for NES).
3. **Directories:** Go to `Settings > Directory`. Configure your paths for `Downloads`, `Savefiles`, `Savestates`, and `System/BIOS`.
4. **Controllers:** Plug in your gamepad. NavOS will typically auto-configure it. If not, go to `Settings > Input > Port 1 Controls`.

## 5. Understanding Hardcore Mode
**Hardcore OFF (Softcore):** You earn achievements normally, but they are not counted towards "Hardcore Mastery" on the RetroAchievements website.
**Hardcore ON:** You earn prestigious Hardcore achievements. 
**The NavOS Difference:** In standard RetroArch, turning Hardcore ON locks you out of saving, rewinding, and fast-forwarding. **In NavOS, these features remain completely unlocked while Hardcore Mode is ON.** You are free to play how you want while still earning Hardcore points.

## 6. Save States
A **Save State** is a snapshot of the emulator's exact memory at a specific moment in time. This is different from a normal in-game save (SRAM).
- **Auto Save / Auto Load:** If enabled in `Settings > Saving`, NavOS will automatically create an `.auto` save state when you close a game, and load it when you restart.
- **Automatic Save Interval:** You can set NavOS to silently save a state every X seconds.
- **Undo Load State:** If you accidentally load a state and lose your progress, use this menu option to reverse the load.
- **NavOS Advantage:** All of these save state features function flawlessly during Hardcore Mode.

## 7. Rewind
Rewind allows you to seamlessly reverse gameplay time.
- **Enabling:** Go to `Settings > Frame Throttle > Rewind` and turn it ON.
- **Using:** Map a hotkey for Rewind in `Settings > Input > Hotkeys`. Hold the button during gameplay to reverse time.
- **Paused Rewind:** You can pause the game, press Rewind to find the exact frame you want, and release pause to continue.
- **NavOS Advantage:** Rewind works fully and unconditionally during Hardcore mode. 

## 8. Cheats
NavOS supports standard RetroArch `.cht` files.
- **Finding Cheats:** Use the `Online Updater` to download cheat databases.
- **Applying:** Load a game, open the Quick Menu, select `Cheats`, load a cheat file, toggle desired cheats to ON, and click `Apply Changes`.
- **Cheat Search:** Use the Cheat Search menu to find your own memory addresses and freeze values.
- **NavOS Advantage:** You can apply and use EMU and RETRO cheats without Hardcore Mode deactivating.

## 9. External Memory Commands
> [!WARNING]
> This is an advanced feature intended for external tools and developers.

NavOS supports the Network Command Interface. External tools can connect via UDP to read and write to the emulator's memory using commands like `WRITE_CORE_RAM` and `WRITE_CORE_MEMORY`.
- **Enabling:** `Settings > Network > Network Commands`.
- **NavOS Advantage:** External memory manipulation does not pause or disable Hardcore mode.

## 10. Netplay
Netplay allows you to play local multiplayer games over the internet.
- **Joining a Host:** Use the `Netplay` menu to browse or enter a host IP.
- **NavOS Advantage:** NavOS will never force your Hardcore mode to turn off just because the host is not using Hardcore, or because of a protocol mismatch.
- **Reloading:** If you join a Netplay session while already playing a game, NavOS will cleanly unload your current game (temporarily suppressing Auto-Save so you don't lose your local state) and reload into the multiplayer session safely.

## 11. Video Features
Advanced video timings allow for lower latency and smoother gameplay.
- **Frame Delay:** Delays the core execution to sit closer to VSync.
- **Swap Interval / VSync:** Controls frame pacing.
- **Black Frame Insertion (BFI):** Reduces motion blur on modern displays.
- **NavOS Advantage:** You can tune all of these settings for maximum performance without triggering a Hardcore penalty.

## 12. Frame/Playback Controls
- **Fast Forward:** Speeds up the game. Map a hotkey in the Input settings.
- **Slow Motion:** Slows the game down.
- **Frame Advance:** Pauses the game and allows you to move forward exactly one frame at a time.
- **Replay Seeking:** For BSV movie recordings.
- **NavOS Advantage:** All frame controls are fully unlocked in Hardcore Mode.

## 13. RetroAchievements
- **Login:** Enter credentials in `Settings > Achievements`.
- **Game Identification:** When a game boots, NavOS generates a checksum (hash) and compares it against the RetroAchievements server to identify the game.
- **Mastery:** Earning all Hardcore achievements in a game grants you a Mastery Badge.
- **Rich Presence:** Shows what you are doing in-game to other users on the RetroAchievements website.

## 14. Understanding What NavOS Does NOT Change
NavOS unlocks quality-of-life emulator tools, but **it strictly enforces the underlying integrity of the game.**
NavOS does **not** bypass:
- **Game Validation:** You must use the correct, verified ROM. Bad dumps or hacked ROMs will not grant achievements.
- **Entry States:** You cannot boot a game with conditions pre-met. NavOS ensures the achievement system initializes correctly on start.
- **Core Compatibility:** You must use a supported core for the achievements to register.
- **Server Authentication:** You must be legitimately logged in to your account. NavOS does not fake server responses.

## 15. Troubleshooting
| Symptom | Cause | Solution |
| :--- | :--- | :--- |
| **NavOS won't launch** | Missing MinGW DLLs. | Ensure `libwinpthread-1.dll` and others remain in the NavOS folder. |
| **Achievements don't load** | Wrong ROM version. | Check the RetroAchievements website for the required game hash/version. |
| **Save State won't load** | Corrupted state file. | Try using "Undo Load State" or loading a backup state index. |
| **Netplay crashes on join** | Usually network timeout. | Ensure your ports are forwarded (default 55435) if hosting. NavOS includes a patch to prevent core-transition deadlocks, so the UI will remain responsive. |
| **Rewind is stuttering** | High RAM usage / Heavy core. | Reduce "Rewind Buffer Size" in settings, or disable rewind for 3D heavy systems (PS1/N64). |
| **Cheats not working** | Wrong region code. | Ensure the cheat file matches your specific ROM's region (USA/EUR/JPN). |

## 16. Backup and Recovery
Regularly backup these folders located in your NavOS directory:
- `saves/` (Standard in-game saves)
- `states/` (Save States and `.auto` files)
- `retroarch.cfg` (Your master configuration file)

## 17. Updating NavOS
> [!CAUTION]
> Do NOT overwrite `retroarch.exe` with a standard version from the official Libretro website. Doing so will remove NavOS features and restore the strict Hardcore restrictions.

Updates for NavOS will be provided as complete packages. To update, copy your `saves/`, `states/`, `system/`, and `retroarch.cfg` into the new NavOS update folder.

## 18. Advanced Users
- **Logging:** Enable logging in `Settings > Logging`. Output is saved to `retroarch-log.txt`. Use this to debug achievement logic.
- **Command Interface:** Advanced users can utilize UDP scripts via port 55355 to send `READ_CORE_RAM` commands.

## 19. Glossary
- **Core:** The emulator plugin (e.g., Snes9x, Genesis Plus GX).
- **ROM/Content:** The game file.
- **.auto:** An automatic save state file.
- **BIOS:** System files required by certain consoles (e.g., PS1) placed in the `system/` directory.

## 20. Final Quick Reference
- **F2:** Save State
- **F4:** Load State
- **F1:** Quick Menu
- **Spacebar:** Fast Forward (Hold)
- **R (key):** Rewind (Hold)
- **P:** Pause
- **K:** Frame Advance (while paused)
