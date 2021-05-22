#include "rcheevos.h"
#include "rc_consoles.h"

#include <ctype.h>

const char* rc_console_name(int console_id)
{
  switch (console_id)
  {
    case RC_CONSOLE_3DO:
      return "3DO";

    case RC_CONSOLE_AMIGA:
      return "Amiga";

    case RC_CONSOLE_AMSTRAD_PC:
      return "Amstrad CPC";

    case RC_CONSOLE_APPLE_II:
      return "Apple II";

    case RC_CONSOLE_ARCADE:
      return "Arcade";

    case RC_CONSOLE_ATARI_2600:
      return "Atari 2600";

    case RC_CONSOLE_ATARI_5200:
      return "Atari 5200";

    case RC_CONSOLE_ATARI_7800:
      return "Atari 7800";

    case RC_CONSOLE_ATARI_JAGUAR:
      return "Atari Jaguar";

    case RC_CONSOLE_ATARI_LYNX:
      return "Atari Lynx";

    case RC_CONSOLE_ATARI_ST:
        return "Atari ST";

    case RC_CONSOLE_CASSETTEVISION:
      return "CassetteVision";

    case RC_CONSOLE_CDI:
      return "CD-I";

    case RC_CONSOLE_COLECOVISION:
      return "ColecoVision";

    case RC_CONSOLE_COMMODORE_64:
      return "Commodore 64";

    case RC_CONSOLE_DREAMCAST:
      return "Dreamcast";

    case RC_CONSOLE_EVENTS:
      return "Events";

    case RC_CONSOLE_FAIRCHILD_CHANNEL_F:
      return "Fairchild Channel F";

    case RC_CONSOLE_FM_TOWNS:
      return "FM Towns";

    case RC_CONSOLE_GAME_AND_WATCH:
      return "Game & Watch";

    case RC_CONSOLE_GAMEBOY:
      return "GameBoy";

    case RC_CONSOLE_GAMEBOY_ADVANCE:
      return "GameBoy Advance";

    case RC_CONSOLE_GAMEBOY_COLOR:
      return "GameBoy Color";

    case RC_CONSOLE_GAMECUBE:
      return "GameCube";

    case RC_CONSOLE_GAME_GEAR:
      return "Game Gear";

    case RC_CONSOLE_HUBS:
      return "Hubs";

    case RC_CONSOLE_INTELLIVISION:
      return "Intellivision";

    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
      return "Magnavox Odyssey 2";

    case RC_CONSOLE_MASTER_SYSTEM:
      return "Master System";

    case RC_CONSOLE_MEGA_DRIVE:
      return "Sega Genesis";

    case RC_CONSOLE_MS_DOS:
      return "MS-DOS";

    case RC_CONSOLE_MSX:
      return "MSX";

    case RC_CONSOLE_NEO_GEO_CD:
      return "Neo Geo CD";

    case RC_CONSOLE_NEOGEO_POCKET:
      return "Neo Geo Pocket";

    case RC_CONSOLE_NINTENDO:
      return "Nintendo Entertainment System";

    case RC_CONSOLE_NINTENDO_64:
      return "Nintendo 64";

    case RC_CONSOLE_NINTENDO_DS:
      return "Nintendo DS";

    case RC_CONSOLE_NINTENDO_3DS:
      return "Nintendo 3DS";

    case RC_CONSOLE_NOKIA_NGAGE:
      return "Nokia N-Gage";

    case RC_CONSOLE_ORIC:
      return "Oric";

    case RC_CONSOLE_PC8800:
      return "PC-8000/8800";

    case RC_CONSOLE_PC9800:
      return "PC-9800";

    case RC_CONSOLE_PCFX:
      return "PC-FX";

    case RC_CONSOLE_PC_ENGINE:
      return "PCEngine";

    case RC_CONSOLE_PLAYSTATION:
      return "PlayStation";

    case RC_CONSOLE_PLAYSTATION_2:
      return "PlayStation 2";

    case RC_CONSOLE_PSP:
      return "PlayStation Portable";

    case RC_CONSOLE_POKEMON_MINI:
      return "Pokemon Mini";

    case RC_CONSOLE_SEGA_32X:
      return "Sega 32X";

    case RC_CONSOLE_SEGA_CD:
      return "Sega CD";

    case RC_CONSOLE_SATURN:
      return "Sega Saturn";

    case RC_CONSOLE_SG1000:
      return "SG-1000";

    case RC_CONSOLE_SHARPX1:
      return "Sharp X1";

    case RC_CONSOLE_SUPER_NINTENDO:
      return "Super Nintendo Entertainment System";

    case RC_CONSOLE_SUPER_CASSETTEVISION:
      return "Super CassetteVision";

    case RC_CONSOLE_SUPERVISION:
      return "Watara Supervision";

    case RC_CONSOLE_THOMSONTO8:
      return "Thomson TO8";

    case RC_CONSOLE_TIC80:
      return "TIC-80";

    case RC_CONSOLE_VECTREX:
      return "Vectrex";

    case RC_CONSOLE_VIC20:
      return "VIC-20";

    case RC_CONSOLE_VIRTUAL_BOY:
      return "Virtual Boy";

    case RC_CONSOLE_WII:
      return "Wii";

    case RC_CONSOLE_WII_U:
      return "Wii-U";

    case RC_CONSOLE_WONDERSWAN:
      return "WonderSwan";

    case RC_CONSOLE_X68K:
      return "X68K";

    case RC_CONSOLE_XBOX:
      return "XBOX";

    case RC_CONSOLE_ZX81:
      return "ZX-81";

    case RC_CONSOLE_ZX_SPECTRUM:
      return "ZX Spectrum";

    default:
      return "Unknown";
  }
}

/* ===== 3DO ===== */
/* http://www.arcaderestoration.com/memorymap/48/3DO+Bios.aspx */
/* NOTE: the Opera core attempts to expose the NVRAM as RETRO_SAVE_RAM, but the 3DO documentation
 * says that applications should only access NVRAM through API calls as it's shared across mulitple
 * games. This suggests that even if the core does expose it, it may change depending on which other
 * games the user has played - so ignore it.
 */
static const rc_memory_region_t _rc_memory_regions_3do[] = {
    { 0x000000U, 0x1FFFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Main RAM" },
};
static const rc_memory_regions_t rc_memory_regions_3do = { _rc_memory_regions_3do, 1 };

/* ===== Apple II ===== */
static const rc_memory_region_t _rc_memory_regions_appleii[] = {
    { 0x000000U, 0x00FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Main RAM" },
    { 0x010000U, 0x01FFFFU, 0x010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Auxillary RAM" }
};
static const rc_memory_regions_t rc_memory_regions_appleii = { _rc_memory_regions_appleii, 2 };

/* ===== Atari 2600 ===== */
static const rc_memory_region_t _rc_memory_regions_atari2600[] = {
    { 0x000000U, 0x00007FU, 0x000080U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_atari2600 = { _rc_memory_regions_atari2600, 1 };

/* ===== Atari 7800 ===== */
/* http://www.atarihq.com/danb/files/78map.txt */
/* http://pdf.textfiles.com/technical/7800_devkit.pdf */
static const rc_memory_region_t _rc_memory_regions_atari7800[] = {
    { 0x000000U, 0x0017FFU, 0x000000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Hardware Interface" },
    { 0x001800U, 0x0027FFU, 0x001800U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x002800U, 0x002FFFU, 0x002800U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirrored RAM" },
    { 0x003000U, 0x0037FFU, 0x003000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirrored RAM" },
    { 0x003800U, 0x003FFFU, 0x003800U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirrored RAM" },
    { 0x004000U, 0x007FFFU, 0x004000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" },
    { 0x008000U, 0x00FFFFU, 0x008000U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM" }
};
static const rc_memory_regions_t rc_memory_regions_atari7800 = { _rc_memory_regions_atari7800, 7 };

/* ===== Atari Jaguar ===== */
/* https://www.mulle-kybernetik.com/jagdox/memorymap.html */
static const rc_memory_region_t _rc_memory_regions_atari_jaguar[] = {
    { 0x000000U, 0x1FFFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_atari_jaguar = { _rc_memory_regions_atari_jaguar, 1 };

/* ===== Atari Lynx ===== */
/* http://www.retroisle.com/atari/lynx/Technical/Programming/lynxprgdumm.php */
static const rc_memory_region_t _rc_memory_regions_atari_lynx[] = {
    { 0x000000U, 0x0000FFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Zero Page" },
    { 0x000100U, 0x0001FFU, 0x000100U, RC_MEMORY_TYPE_SYSTEM_RAM, "Stack" },
    { 0x000200U, 0x00FBFFU, 0x000200U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x00FC00U, 0x00FCFFU, 0x00FC00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "SUZY hardware access" },
    { 0x00FD00U, 0x00FDFFU, 0x00FD00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "MIKEY hardware access" },
    { 0x00FE00U, 0x00FFF7U, 0x00FE00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Boot ROM" },
    { 0x00FFF8U, 0x00FFFFU, 0x00FFF8U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Hardware vectors" }
};
static const rc_memory_regions_t rc_memory_regions_atari_lynx = { _rc_memory_regions_atari_lynx, 7 };

/* ===== ColecoVision ===== */
static const rc_memory_region_t _rc_memory_regions_colecovision[] = {
    { 0x000000U, 0x0003FFU, 0x006000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_colecovision = { _rc_memory_regions_colecovision, 1 };

/* ===== GameBoy / GameBoy Color ===== */
static const rc_memory_region_t _rc_memory_regions_gameboy[] = {
    { 0x000000U, 0x0000FFU, 0x000000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Interrupt vector" },
    { 0x000100U, 0x00014FU, 0x000100U, RC_MEMORY_TYPE_READONLY, "Cartridge header" },
    { 0x000150U, 0x003FFFU, 0x000150U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM (fixed)" }, /* bank 0 */
    { 0x004000U, 0x007FFFU, 0x004000U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM (paged)" }, /* bank 1-XX (switchable) */
    { 0x008000U, 0x0097FFU, 0x008000U, RC_MEMORY_TYPE_VIDEO_RAM, "Tile RAM" },
    { 0x009800U, 0x009BFFU, 0x009800U, RC_MEMORY_TYPE_VIDEO_RAM, "BG1 map data" },
    { 0x009C00U, 0x009FFFU, 0x009C00U, RC_MEMORY_TYPE_VIDEO_RAM, "BG2 map data" },
    { 0x00A000U, 0x00BFFFU, 0x00A000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM"},
    { 0x00C000U, 0x00CFFFU, 0x00C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM (fixed)" },
    { 0x00D000U, 0x00DFFFU, 0x00D000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM (bank 1)" },
    { 0x00E000U, 0x00FDFFU, 0x00C000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Echo RAM" },
    { 0x00FE00U, 0x00FE9FU, 0x00FE00U, RC_MEMORY_TYPE_VIDEO_RAM, "Sprite RAM"},
    { 0x00FEA0U, 0x00FEFFU, 0x00FEA0U, RC_MEMORY_TYPE_UNUSED, ""},
    { 0x00FF00U, 0x00FF7FU, 0x00FF00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Hardware I/O"},
    { 0x00FF80U, 0x00FFFEU, 0x00FF80U, RC_MEMORY_TYPE_SYSTEM_RAM, "Quick RAM"},
    { 0x00FFFFU, 0x00FFFFU, 0x00FFFFU, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Interrupt enable"},

    /* GameBoy Color provides six extra banks of memory that can be paged out through the $DXXX 
     * memory space, but the timing of that does not correspond with blanks, which is when achievements 
     * are processed. As such, it is desirable to always have access to these extra banks. We do this
     * by expecting the extra banks to be addressable at addresses not supported by the native system. */
    { 0x010000U, 0x015FFFU, 0x010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM (banks 2-7, GBC only)" }
};
static const rc_memory_regions_t rc_memory_regions_gameboy = { _rc_memory_regions_gameboy, 16 };
static const rc_memory_regions_t rc_memory_regions_gameboy_color = { _rc_memory_regions_gameboy, 17 };

/* ===== GameBoy Advance ===== */
static const rc_memory_region_t _rc_memory_regions_gameboy_advance[] = {
    { 0x000000U, 0x007FFFU, 0x03000000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" },
    { 0x008000U, 0x047FFFU, 0x02000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_gameboy_advance = { _rc_memory_regions_gameboy_advance, 2 };

/* ===== Game Gear ===== */
/* http://www.smspower.org/Development/MemoryMap */
static const rc_memory_region_t _rc_memory_regions_game_gear[] = {
    { 0x000000U, 0x001FFFU, 0x00C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_game_gear = { _rc_memory_regions_game_gear, 1 };

/* ===== Intellivision ===== */
/* http://wiki.intellivision.us/index.php%3Ftitle%3DMemory_Map */
static const rc_memory_region_t _rc_memory_regions_intellivision[] = {
    { 0x000000U, 0x00007FU, 0x000000U, RC_MEMORY_TYPE_VIDEO_RAM, "STIC Registers" },
    { 0x000080U, 0x0000FFU, 0x000080U, RC_MEMORY_TYPE_UNUSED, "" },
    { 0x000100U, 0x00035FU, 0x000100U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x000360U, 0x0003FFU, 0x000360U, RC_MEMORY_TYPE_UNUSED, "" },
    { 0x000400U, 0x000FFFU, 0x000400U, RC_MEMORY_TYPE_SYSTEM_RAM, "Cartridge RAM" },
    { 0x001000U, 0x001FFFU, 0x001000U, RC_MEMORY_TYPE_UNUSED, "" },
    { 0x002000U, 0x002FFFU, 0x002000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Cartridge RAM" },
    { 0x003000U, 0x003FFFU, 0x003000U, RC_MEMORY_TYPE_VIDEO_RAM, "Video RAM" },
    { 0x004000U, 0x00FFFFU, 0x004000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Cartridge RAM" },
};
static const rc_memory_regions_t rc_memory_regions_intellivision = { _rc_memory_regions_intellivision, 9 };

/* ===== Magnavox Odyssey 2 ===== */
static const rc_memory_region_t _rc_memory_regions_magnavox_odyssey_2[] = {
    { 0x000000U, 0x00003FU, 0x000040U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_magnavox_odyssey_2 = { _rc_memory_regions_magnavox_odyssey_2, 1 };

/* ===== Master System ===== */
/* http://www.smspower.org/Development/MemoryMap */
static const rc_memory_region_t _rc_memory_regions_master_system[] = {
    { 0x000000U, 0x001FFFU, 0x00C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_master_system = { _rc_memory_regions_master_system, 1 };

/* ===== MegaDrive (Genesis) ===== */
/* http://www.smspower.org/Development/MemoryMap */
static const rc_memory_region_t _rc_memory_regions_megadrive[] = {
    { 0x000000U, 0x00FFFFU, 0xFF0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x010000U, 0x01FFFFU, 0x000000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_megadrive = { _rc_memory_regions_megadrive, 2 };

/* ===== MSX ===== */
/* https://www.msx.org/wiki/The_Memory */
/* MSX only has 64KB of addressable RAM, of which 32KB is reserved for the system/BIOS.
 * However, the system has up to 512KB of RAM, which is paged into the addressable RAM
 * We expect the raw RAM to be exposed, rather than force the devs to worry about the
 * paging system. The entire RAM is expected to appear starting at $10000, which is not
 * addressable by the system itself.
 */
static const rc_memory_region_t _rc_memory_regions_msx[] = {
    { 0x000000U, 0x07FFFFU, 0x010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
};
static const rc_memory_regions_t rc_memory_regions_msx = { _rc_memory_regions_msx, 1 };

/* ===== Neo Geo Pocket ===== */
/* http://neopocott.emuunlim.com/docs/tech-11.txt */
static const rc_memory_region_t _rc_memory_regions_neo_geo_pocket[] = {
    /* MednafenNGP exposes 16KB, but the doc suggests there's 24-32KB */
    { 0x000000U, 0x003FFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_neo_geo_pocket = { _rc_memory_regions_neo_geo_pocket, 1 };

/* ===== Nintendo Entertainment System ===== */
/* https://wiki.nesdev.com/w/index.php/CPU_memory_map */
static const rc_memory_region_t _rc_memory_regions_nes[] = {
    { 0x0000U, 0x07FFU, 0x0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x0800U, 0x0FFFU, 0x0000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirror RAM" }, /* duplicates memory from $0000-$07FF */
    { 0x1000U, 0x17FFU, 0x0000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirror RAM" }, /* duplicates memory from $0000-$07FF */
    { 0x1800U, 0x1FFFU, 0x0000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirror RAM" }, /* duplicates memory from $0000-$07FF */
    { 0x2000U, 0x2007U, 0x2000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "PPU Register" },
    { 0x2008U, 0x3FFFU, 0x2008U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirrored PPU Register" }, /* repeats every 8 bytes */
    { 0x4000U, 0x4017U, 0x4000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "APU and I/O register" },
    { 0x4018U, 0x401FU, 0x4018U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "APU and I/O test register" },

    /* NOTE: these are for the original NES/Famicom */
    { 0x4020U, 0x5FFFU, 0x4020U, RC_MEMORY_TYPE_READONLY, "Cartridge data"}, /* varies by mapper */
    { 0x6000U, 0x7FFFU, 0x6000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM"},
    { 0x8000U, 0xFFFFU, 0x8000U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM"},

    /* NOTE: these are the correct mappings for FDS: https://fms.komkon.org/EMUL8/NES.html
     * 0x6000-0xDFFF is RAM on the FDS system and 0xE000-0xFFFF is FDS BIOS.
     * If the core implements a memory map, we should still be able to translate the addresses
     * correctly as we only use the classifications when a memory map is not provided

    { 0x4020U, 0x40FFU, 0x4020U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "FDS I/O registers"},
    { 0x4100U, 0x5FFFU, 0x4100U, RC_MEMORY_TYPE_READONLY, "Cartridge data"}, // varies by mapper
    { 0x6000U, 0xDFFFU, 0x6000U, RC_MEMORY_TYPE_SYSTEM_RAM, "FDS RAM"},
    { 0xE000U, 0xFFFFU, 0xE000U, RC_MEMORY_TYPE_READONLY, "FDS BIOS ROM"},

     */
};
static const rc_memory_regions_t rc_memory_regions_nes = { _rc_memory_regions_nes, 11 };

/* ===== Nintendo 64 ===== */
/* https://raw.githubusercontent.com/mikeryan/n64dev/master/docs/n64ops/n64ops%23h.txt */
static const rc_memory_region_t _rc_memory_regions_n64[] = {
    { 0x000000U, 0x1FFFFFU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RDRAM 1 */
    { 0x200000U, 0x3FFFFFU, 0x00200000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RDRAM 2 */
    { 0x400000U, 0x7FFFFFU, 0x80000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }  /* expansion pak - cannot find any details for real address */
};
static const rc_memory_regions_t rc_memory_regions_n64 = { _rc_memory_regions_n64, 3 };

/* ===== Nintendo DS ===== */
/* https://www.akkit.org/info/gbatek.htm#dsmemorymaps */
static const rc_memory_region_t _rc_memory_regions_nintendo_ds[] = {
    { 0x000000U, 0x3FFFFFU, 0x02000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_nintendo_ds = { _rc_memory_regions_nintendo_ds, 1 };

/* ===== Oric ===== */
static const rc_memory_region_t _rc_memory_regions_oric[] = {
    /* actual size depends on machine type - up to 64KB */
    { 0x000000U, 0x00FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_oric = { _rc_memory_regions_oric, 1 };

/* ===== PC-8800 ===== */
static const rc_memory_region_t _rc_memory_regions_pc8800[] = {
    { 0x000000U, 0x00FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Main RAM" },
    { 0x010000U, 0x010FFFU, 0x010000U, RC_MEMORY_TYPE_VIDEO_RAM, "Text VRAM" } /* technically VRAM, but often used as system RAM */
};
static const rc_memory_regions_t rc_memory_regions_pc8800 = { _rc_memory_regions_pc8800, 2 };

/* ===== PC Engine ===== */
/* http://www.archaicpixels.com/Memory_Map */
static const rc_memory_region_t _rc_memory_regions_pcengine[] = {
    { 0x000000U, 0x001FFFU, 0x1F0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x002000U, 0x011FFFU, 0x100000U, RC_MEMORY_TYPE_SYSTEM_RAM, "CD RAM" },
    { 0x012000U, 0x041FFFU, 0x0D0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Super System Card RAM" },
    { 0x042000U, 0x0427FFU, 0x1EE000U, RC_MEMORY_TYPE_SAVE_RAM,   "CD Battery-backed RAM" }
};
static const rc_memory_regions_t rc_memory_regions_pcengine = { _rc_memory_regions_pcengine, 4 };

/* ===== PC-FX ===== */
/* http://daifukkat.su/pcfx/data/memmap.html */
static const rc_memory_region_t _rc_memory_regions_pcfx[] = {
    { 0x000000U, 0x1FFFFFU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x200000U, 0x207FFFU, 0xE0000000U, RC_MEMORY_TYPE_SAVE_RAM, "Internal Backup Memory" },
    { 0x208000U, 0x20FFFFU, 0xE8000000U, RC_MEMORY_TYPE_SAVE_RAM, "External Backup Memory" },
};
static const rc_memory_regions_t rc_memory_regions_pcfx = { _rc_memory_regions_pcfx, 3 };

/* ===== PlayStation ===== */
/* http://www.raphnet.net/electronique/psx_adaptor/Playstation.txt */
static const rc_memory_region_t _rc_memory_regions_playstation[] = {
    { 0x000000U, 0x00FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Kernel RAM" },
    { 0x010000U, 0x1FFFFFU, 0x010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_playstation = { _rc_memory_regions_playstation, 2 };

/* ===== PlayStation 2 ===== */
/* https://psi-rockin.github.io/ps2tek/ */
static const rc_memory_region_t _rc_memory_regions_playstation2[] = {
    { 0x00000000U, 0x000FFFFFU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Kernel RAM" },
    { 0x00100000U, 0x01FFFFFFU, 0x00100000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_playstation2 = { _rc_memory_regions_playstation2, 2 };

/* ===== Pokemon Mini ===== */
/* https://www.pokemon-mini.net/documentation/memory-map/ */
static const rc_memory_region_t _rc_memory_regions_pokemini[] = {
    { 0x000000U, 0x000FFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "BIOS RAM" },
    { 0x001000U, 0x001FFFU, 0x001000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_pokemini = { _rc_memory_regions_pokemini, 2 };

/* ===== Sega CD ===== */
/* https://en.wikibooks.org/wiki/Genesis_Programming#MegaCD_Changes */
static const rc_memory_region_t _rc_memory_regions_segacd[] = {
    { 0x000000U, 0x00FFFFU, 0x00FF0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "68000 RAM" },
    { 0x010000U, 0x08FFFFU, 0x80020000U, RC_MEMORY_TYPE_SAVE_RAM, "CD PRG RAM" } /* normally banked into $020000-$03FFFF */
};
static const rc_memory_regions_t rc_memory_regions_segacd = { _rc_memory_regions_segacd, 2 };

/* ===== Sega Saturn ===== */
/* https://segaretro.org/Sega_Saturn_hardware_notes_(2004-04-27) */
static const rc_memory_region_t _rc_memory_regions_saturn[] = {
    { 0x000000U, 0x0FFFFFU, 0x00200000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Work RAM Low" },
    { 0x100000U, 0x1FFFFFU, 0x06000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Work RAM High" }
};
static const rc_memory_regions_t rc_memory_regions_saturn = { _rc_memory_regions_saturn, 2 };

/* ===== SG-1000 ===== */
/* http://www.smspower.org/Development/MemoryMap */
static const rc_memory_region_t _rc_memory_regions_sg1000[] = {
    { 0x000000U, 0x0003FFU, 0xC000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* https://github.com/libretro/FBNeo/blob/697801c6262be6ca91615cf905444d3e039bc06f/src/burn/drv/sg1000/d_sg1000.cpp#L210-L237 */
    /* Expansion mode B exposes 8KB at $C000. The first 2KB hides the System RAM, but since the address matches,
       we'll leverage that definition and expand it another 6KB */
    { 0x000400U, 0x001FFFU, 0xC400U, RC_MEMORY_TYPE_SYSTEM_RAM, "Extended RAM" },
    /* Expansion mode A exposes 8KB at $2000 */
    { 0x002000U, 0x003FFFU, 0x2000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Extended RAM" },
    /* Othello exposes 2KB at $8000, and The Castle exposes 8KB at $8000 */
    { 0x004000U, 0x005FFFU, 0x8000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Extended RAM" }
};
static const rc_memory_regions_t rc_memory_regions_sg1000 = { _rc_memory_regions_sg1000, 4 };

/* ===== Super Cassette Vision ===== */
static const rc_memory_region_t _rc_memory_regions_scv[] = {
    { 0x000000U, 0x000FFFU, 0x000000U, RC_MEMORY_TYPE_READONLY, "System ROM" },
    { 0x001000U, 0x001FFFU, 0x001000U, RC_MEMORY_TYPE_UNUSED, "" },
    { 0x002000U, 0x003FFFU, 0x002000U, RC_MEMORY_TYPE_VIDEO_RAM, "Video RAM" },
    { 0x004000U, 0x007FFFU, 0x004000U, RC_MEMORY_TYPE_UNUSED, "" },
    { 0x008000U, 0x00DFFFU, 0x008000U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM" },
    { 0x00E000U, 0x00FF7FU, 0x00E000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" },
    { 0x00FF80U, 0x00FFFFU, 0x00FF80U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_scv = { _rc_memory_regions_scv, 7 };

/* ===== Super Nintendo ===== */
/* https://en.wikibooks.org/wiki/Super_NES_Programming/SNES_memory_map#LoROM */
static const rc_memory_region_t _rc_memory_regions_snes[] = {
    { 0x000000U, 0x01FFFFU, 0x7E0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x020000U, 0x03FFFFU, 0xFE0000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_snes = { _rc_memory_regions_snes, 2 };

/* ===== Thomson TO8 ===== */
/* https://github.com/mamedev/mame/blob/master/src/mame/drivers/thomson.cpp#L1617 */
static const rc_memory_region_t _rc_memory_regions_thomson_to8[] = {
    { 0x000000U, 0x07FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_thomson_to8 = { _rc_memory_regions_thomson_to8, 1 };

/* ===== TIC-80 ===== */
/* https://github.com/nesbox/TIC-80/wiki/RAM */
static const rc_memory_region_t _rc_memory_regions_tic80[] = {
    { 0x000000U, 0x003FFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Video RAM" }, /* have to classify this as system RAM because the core exposes it as part of the RETRO_MEMORY_SYSTEM_RAM */
    { 0x004000U, 0x005FFFU, 0x004000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Tile RAM" },
    { 0x006000U, 0x007FFFU, 0x006000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Sprite RAM" },
    { 0x008000U, 0x00FF7FU, 0x008000U, RC_MEMORY_TYPE_SYSTEM_RAM, "MAP RAM" },
    { 0x00FF80U, 0x00FF8BU, 0x00FF80U, RC_MEMORY_TYPE_SYSTEM_RAM, "Input State" },
    { 0x00FF8CU, 0x014003U, 0x00FF8CU, RC_MEMORY_TYPE_SYSTEM_RAM, "Sound RAM" },
    { 0x014004U, 0x014403U, 0x014004U, RC_MEMORY_TYPE_SAVE_RAM, "Persistent Memory" }, /* this is also returned as part of RETRO_MEMORY_SYSTEM_RAM, but can be extrapolated correctly because the pointer starts at the first SYSTEM_RAM region */
    { 0x014404U, 0x014603U, 0x014404U, RC_MEMORY_TYPE_SYSTEM_RAM, "Sprite Flags" },
    { 0x014604U, 0x014E03U, 0x014604U, RC_MEMORY_TYPE_SYSTEM_RAM, "System Font" },
    { 0x014E04U, 0x017FFFU, 0x014E04U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM"}
};
static const rc_memory_regions_t rc_memory_regions_tic80 = { _rc_memory_regions_tic80, 10 };

/* ===== Vectrex ===== */
/* https://roadsidethoughts.com/vectrex/vectrex-memory-map.htm */
static const rc_memory_region_t _rc_memory_regions_vectrex[] = {
    { 0x000000U, 0x0003FFU, 0x00C800U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_vectrex = { _rc_memory_regions_vectrex, 1 };

/* ===== Virtual Boy ===== */
static const rc_memory_region_t _rc_memory_regions_virtualboy[] = {
    { 0x000000U, 0x00FFFFU, 0x05000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x010000U, 0x01FFFFU, 0x06000000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_virtualboy = { _rc_memory_regions_virtualboy, 2 };

/* ===== Watara Supervision ===== */
/* https://github.com/libretro/potator/blob/b5e5ba02914fcdf4a8128072dbc709da28e08832/common/memorymap.c#L231-L259 */
static const rc_memory_region_t _rc_memory_regions_watara_supervision[] = {
    { 0x0000U, 0x001FFFU, 0x0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x2000U, 0x003FFFU, 0x2000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Registers" },
    { 0x4000U, 0x005FFFU, 0x4000U, RC_MEMORY_TYPE_VIDEO_RAM, "Video RAM" }
};
static const rc_memory_regions_t rc_memory_regions_watara_supervision = { _rc_memory_regions_watara_supervision, 3 };

/* ===== WonderSwan ===== */
/* http://daifukkat.su/docs/wsman/#ovr_memmap */
static const rc_memory_region_t _rc_memory_regions_wonderswan[] = {
    /* RAM ends at 0x3FFF for WonderSwan, WonderSwan color uses all 64KB */
    { 0x000000U, 0x00FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* Only 64KB of SRAM is accessible via the addressing scheme, but the cartridge
     * may have up to 512KB of SRAM. http://daifukkat.su/docs/wsman/#cart_meta
     * Since beetle_wswan exposes it as a contiguous block, assume its contiguous
     * even though the documentation says $20000-$FFFFF is ROM data. If this causes
     * a conflict in the future, we can revisit. A new region with a virtual address
     * could be added to pick up the additional SRAM data. As long as it immediately
     * follows the 64KB at $10000, all existing achievements should be unaffected.
     */
    { 0x010000U, 0x08FFFFU, 0x010000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_wonderswan = { _rc_memory_regions_wonderswan, 2 };

/* ===== default ===== */
static const rc_memory_regions_t rc_memory_regions_none = { 0, 0 };

const rc_memory_regions_t* rc_console_memory_regions(int console_id)
{
  switch (console_id)
  {
    case RC_CONSOLE_3DO:
      return &rc_memory_regions_3do;

    case RC_CONSOLE_APPLE_II:
      return &rc_memory_regions_appleii;

    case RC_CONSOLE_ATARI_2600:
      return &rc_memory_regions_atari2600;

    case RC_CONSOLE_ATARI_7800:
      return &rc_memory_regions_atari7800;

    case RC_CONSOLE_ATARI_JAGUAR:
      return &rc_memory_regions_atari_jaguar;

    case RC_CONSOLE_ATARI_LYNX:
      return &rc_memory_regions_atari_lynx;

    case RC_CONSOLE_COLECOVISION:
      return &rc_memory_regions_colecovision;

    case RC_CONSOLE_GAMEBOY:
      return &rc_memory_regions_gameboy;

    case RC_CONSOLE_GAMEBOY_COLOR:
      return &rc_memory_regions_gameboy_color;

    case RC_CONSOLE_GAMEBOY_ADVANCE:
      return &rc_memory_regions_gameboy_advance;

    case RC_CONSOLE_GAME_GEAR:
      return &rc_memory_regions_game_gear;

    case RC_CONSOLE_INTELLIVISION:
      return &rc_memory_regions_intellivision;

    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
      return &rc_memory_regions_magnavox_odyssey_2;

    case RC_CONSOLE_MASTER_SYSTEM:
      return &rc_memory_regions_master_system;

    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_SEGA_32X:
      /* NOTE: 32x adds an extra 512KB of memory (256KB RAM + 256KB VRAM) to the 
       *       Genesis, but we currently don't support it. */
      return &rc_memory_regions_megadrive;

    case RC_CONSOLE_MSX:
      return &rc_memory_regions_msx;

    case RC_CONSOLE_NEOGEO_POCKET:
      return &rc_memory_regions_neo_geo_pocket;

    case RC_CONSOLE_NINTENDO:
      return &rc_memory_regions_nes;

    case RC_CONSOLE_NINTENDO_64:
      return &rc_memory_regions_n64;

    case RC_CONSOLE_NINTENDO_DS:
      return &rc_memory_regions_nintendo_ds;

    case RC_CONSOLE_ORIC:
      return &rc_memory_regions_oric;

    case RC_CONSOLE_PC8800:
      return &rc_memory_regions_pc8800;

    case RC_CONSOLE_PC_ENGINE:
      return &rc_memory_regions_pcengine;

    case RC_CONSOLE_PCFX:
      return &rc_memory_regions_pcfx;

    case RC_CONSOLE_PLAYSTATION:
      return &rc_memory_regions_playstation;

    case RC_CONSOLE_PLAYSTATION_2:
      return &rc_memory_regions_playstation2;

    case RC_CONSOLE_POKEMON_MINI:
      return &rc_memory_regions_pokemini;

    case RC_CONSOLE_SATURN:
      return &rc_memory_regions_saturn;

    case RC_CONSOLE_SEGA_CD:
      return &rc_memory_regions_segacd;

    case RC_CONSOLE_SG1000:
      return &rc_memory_regions_sg1000;

    case RC_CONSOLE_SUPER_CASSETTEVISION:
      return &rc_memory_regions_scv;

    case RC_CONSOLE_SUPER_NINTENDO:
      return &rc_memory_regions_snes;

    case RC_CONSOLE_SUPERVISION:
      return &rc_memory_regions_watara_supervision;

    case RC_CONSOLE_THOMSONTO8:
      return &rc_memory_regions_thomson_to8;

    case RC_CONSOLE_TIC80:
      return &rc_memory_regions_tic80;

    case RC_CONSOLE_VECTREX:
      return &rc_memory_regions_vectrex;

    case RC_CONSOLE_VIRTUAL_BOY:
      return &rc_memory_regions_virtualboy;

    case RC_CONSOLE_WONDERSWAN:
      return &rc_memory_regions_wonderswan;

    default:
      return &rc_memory_regions_none;
  }
}
