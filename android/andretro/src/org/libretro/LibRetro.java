package org.libretro;

import java.nio.*;

public final class LibRetro
{
	public static final int  RETRO_API_VERSION = 1;

	public static final int  RETRO_DEVICE_MASK = 0xff;
	public static final int  RETRO_DEVICE_NONE = 0;
	public static final int  RETRO_DEVICE_JOYPAD = 1;
	public static final int  RETRO_DEVICE_MOUSE = 2;
	public static final int  RETRO_DEVICE_KEYBOARD = 3;
	public static final int  RETRO_DEVICE_LIGHTGUN = 4;
	public static final int  RETRO_DEVICE_ANALOG = 5;
	public static final int  RETRO_DEVICE_POINTER = 6;

	public static final int  RETRO_DEVICE_JOYPAD_MULTITAP = ((1 << 8) | RETRO_DEVICE_JOYPAD);
	public static final int  RETRO_DEVICE_LIGHTGUN_SUPER_SCOPE = ((1 << 8) | RETRO_DEVICE_LIGHTGUN);
	public static final int  RETRO_DEVICE_LIGHTGUN_JUSTIFIER = ((2 << 8) | RETRO_DEVICE_LIGHTGUN);
	public static final int  RETRO_DEVICE_LIGHTGUN_JUSTIFIERS = ((3 << 8) | RETRO_DEVICE_LIGHTGUN);

	public static final int  RETRO_DEVICE_ID_JOYPAD_B = 0;
	public static final int  RETRO_DEVICE_ID_JOYPAD_Y = 1;
	public static final int  RETRO_DEVICE_ID_JOYPAD_SELECT = 2;
	public static final int  RETRO_DEVICE_ID_JOYPAD_START = 3;
	public static final int  RETRO_DEVICE_ID_JOYPAD_UP = 4;
	public static final int  RETRO_DEVICE_ID_JOYPAD_DOWN = 5;
	public static final int  RETRO_DEVICE_ID_JOYPAD_LEFT = 6;
	public static final int  RETRO_DEVICE_ID_JOYPAD_RIGHT = 7;
	public static final int  RETRO_DEVICE_ID_JOYPAD_A = 8;
	public static final int  RETRO_DEVICE_ID_JOYPAD_X = 9;
	public static final int  RETRO_DEVICE_ID_JOYPAD_L = 10;
	public static final int  RETRO_DEVICE_ID_JOYPAD_R = 11;
	public static final int  RETRO_DEVICE_ID_JOYPAD_L2 = 12;
	public static final int  RETRO_DEVICE_ID_JOYPAD_R2 = 13;
	public static final int  RETRO_DEVICE_ID_JOYPAD_L3 = 14;
	public static final int  RETRO_DEVICE_ID_JOYPAD_R3 = 15;

	public static final int  RETRO_DEVICE_INDEX_ANALOG_LEFT = 0;
	public static final int  RETRO_DEVICE_INDEX_ANALOG_RIGHT = 1;
	public static final int  RETRO_DEVICE_ID_ANALOG_X = 0;
	public static final int  RETRO_DEVICE_ID_ANALOG_Y = 1;

	public static final int  RETRO_DEVICE_ID_MOUSE_X = 0;
	public static final int  RETRO_DEVICE_ID_MOUSE_Y = 1;
	public static final int  RETRO_DEVICE_ID_MOUSE_LEFT = 2;
	public static final int  RETRO_DEVICE_ID_MOUSE_RIGHT = 3;

	public static final int  RETRO_DEVICE_ID_LIGHTGUN_X = 0;
	public static final int  RETRO_DEVICE_ID_LIGHTGUN_Y = 1;
	public static final int  RETRO_DEVICE_ID_LIGHTGUN_TRIGGER = 2;
	public static final int  RETRO_DEVICE_ID_LIGHTGUN_CURSOR = 3;
	public static final int  RETRO_DEVICE_ID_LIGHTGUN_TURBO = 4;
	public static final int  RETRO_DEVICE_ID_LIGHTGUN_PAUSE = 5;
	public static final int  RETRO_DEVICE_ID_LIGHTGUN_START = 6;

	public static final int  RETRO_REGION_NTSC = 0;
	public static final int  RETRO_REGION_PAL = 1;

	public static final int  RETRO_MEMORY_MASK = 0xff;
	public static final int  RETRO_MEMORY_SAVE_RAM = 0;
	public static final int  RETRO_MEMORY_RTC = 1;
	public static final int  RETRO_MEMORY_SYSTEM_RAM = 2;
	public static final int  RETRO_MEMORY_VIDEO_RAM = 3;

	public static final int  RETRO_MEMORY_SNES_BSX_RAM = ((1 << 8) | RETRO_MEMORY_SAVE_RAM);
	public static final int  RETRO_MEMORY_SNES_BSX_PRAM = ((2 << 8) | RETRO_MEMORY_SAVE_RAM);
	public static final int  RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM = ((3 << 8) | RETRO_MEMORY_SAVE_RAM);
	public static final int  RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM = ((4 << 8) | RETRO_MEMORY_SAVE_RAM);
	public static final int  RETRO_MEMORY_SNES_GAME_BOY_RAM = ((5 << 8) | RETRO_MEMORY_SAVE_RAM);
	public static final int  RETRO_MEMORY_SNES_GAME_BOY_RTC = ((6 << 8) | RETRO_MEMORY_RTC);

	public static final int  RETRO_GAME_TYPE_BSX = 0x101;
	public static final int  RETRO_GAME_TYPE_BSX_SLOTTED = 0x102;
	public static final int  RETRO_GAME_TYPE_SUFAMI_TURBO = 0x103;
	public static final int  RETRO_GAME_TYPE_SUPER_GAME_BOY = 0x104;

	public static final int  RETRO_PIXEL_FORMAT_0RGB1555 = 0;
	public static final int  RETRO_PIXEL_FORMAT_XRGB8888 = 1;
	public static final int  RETRO_PIXEL_FORMAT_RGB565 = 2;
	
	public static final int  RETROK_UNKNOWN        = 0;
	public static final int  RETROK_FIRST          = 0;
	public static final int  RETROK_BACKSPACE      = 8;
	public static final int  RETROK_TAB            = 9;
	public static final int  RETROK_CLEAR          = 12;
	public static final int  RETROK_RETURN         = 13;
	public static final int  RETROK_PAUSE          = 19;
	public static final int  RETROK_ESCAPE         = 27;
	public static final int  RETROK_SPACE          = 32;
	public static final int  RETROK_EXCLAIM        = 33;
	public static final int  RETROK_QUOTEDBL       = 34;
	public static final int  RETROK_HASH           = 35;
	public static final int  RETROK_DOLLAR         = 36;
	public static final int  RETROK_AMPERSAND      = 38;
	public static final int  RETROK_QUOTE          = 39;
	public static final int  RETROK_LEFTPAREN      = 40;
	public static final int  RETROK_RIGHTPAREN     = 41;
	public static final int  RETROK_ASTERISK       = 42;
	public static final int  RETROK_PLUS           = 43;
	public static final int  RETROK_COMMA          = 44;
	public static final int  RETROK_MINUS          = 45;
	public static final int  RETROK_PERIOD         = 46;
	public static final int  RETROK_SLASH          = 47;
	public static final int  RETROK_0              = 48;
	public static final int  RETROK_1              = 49;
	public static final int  RETROK_2              = 50;
	public static final int  RETROK_3              = 51;
	public static final int  RETROK_4              = 52;
	public static final int  RETROK_5              = 53;
	public static final int  RETROK_6              = 54;
	public static final int  RETROK_7              = 55;
	public static final int  RETROK_8              = 56;
	public static final int  RETROK_9              = 57;
	public static final int  RETROK_COLON          = 58;
	public static final int  RETROK_SEMICOLON      = 59;
	public static final int  RETROK_LESS           = 60;
	public static final int  RETROK_EQUALS         = 61;
	public static final int  RETROK_GREATER        = 62;
	public static final int  RETROK_QUESTION       = 63;
	public static final int  RETROK_AT             = 64;
	public static final int  RETROK_LEFTBRACKET    = 91;
	public static final int  RETROK_BACKSLASH      = 92;
	public static final int  RETROK_RIGHTBRACKET   = 93;
	public static final int  RETROK_CARET          = 94;
	public static final int  RETROK_UNDERSCORE     = 95;
	public static final int  RETROK_BACKQUOTE      = 96;
	public static final int  RETROK_a              = 97;
	public static final int  RETROK_b              = 98;
	public static final int  RETROK_c              = 99;
	public static final int  RETROK_d              = 100;
	public static final int  RETROK_e              = 101;
	public static final int  RETROK_f              = 102;
	public static final int  RETROK_g              = 103;
	public static final int  RETROK_h              = 104;
	public static final int  RETROK_i              = 105;
	public static final int  RETROK_j              = 106;
	public static final int  RETROK_k              = 107;
	public static final int  RETROK_l              = 108;
	public static final int  RETROK_m              = 109;
	public static final int  RETROK_n              = 110;
	public static final int  RETROK_o              = 111;
	public static final int  RETROK_p              = 112;
	public static final int  RETROK_q              = 113;
	public static final int  RETROK_r              = 114;
	public static final int  RETROK_s              = 115;
	public static final int  RETROK_t              = 116;
	public static final int  RETROK_u              = 117;
	public static final int  RETROK_v              = 118;
	public static final int  RETROK_w              = 119;
	public static final int  RETROK_x              = 120;
	public static final int  RETROK_y              = 121;
	public static final int  RETROK_z              = 122;
	public static final int  RETROK_DELETE         = 127;

	public static final int  RETROK_KP0            = 256;
	public static final int  RETROK_KP1            = 257;
	public static final int  RETROK_KP2            = 258;
	public static final int  RETROK_KP3            = 259;
	public static final int  RETROK_KP4            = 260;
	public static final int  RETROK_KP5            = 261;
	public static final int  RETROK_KP6            = 262;
	public static final int  RETROK_KP7            = 263;
	public static final int  RETROK_KP8            = 264;
	public static final int  RETROK_KP9            = 265;
	public static final int  RETROK_KP_PERIOD      = 266;
	public static final int  RETROK_KP_DIVIDE      = 267;
	public static final int  RETROK_KP_MULTIPLY    = 268;
	public static final int  RETROK_KP_MINUS       = 269;
	public static final int  RETROK_KP_PLUS        = 270;
	public static final int  RETROK_KP_ENTER       = 271;
	public static final int  RETROK_KP_EQUALS      = 272;

	public static final int  RETROK_UP             = 273;
	public static final int  RETROK_DOWN           = 274;
	public static final int  RETROK_RIGHT          = 275;
	public static final int  RETROK_LEFT           = 276;
	public static final int  RETROK_INSERT         = 277;
	public static final int  RETROK_HOME           = 278;
	public static final int  RETROK_END            = 279;
	public static final int  RETROK_PAGEUP         = 280;
	public static final int  RETROK_PAGEDOWN       = 281;

	public static final int  RETROK_F1             = 282;
	public static final int  RETROK_F2             = 283;
	public static final int  RETROK_F3             = 284;
	public static final int  RETROK_F4             = 285;
	public static final int  RETROK_F5             = 286;
	public static final int  RETROK_F6             = 287;
	public static final int  RETROK_F7             = 288;
	public static final int  RETROK_F8             = 289;
	public static final int  RETROK_F9             = 290;
	public static final int  RETROK_F10            = 291;
	public static final int  RETROK_F11            = 292;
	public static final int  RETROK_F12            = 293;
	public static final int  RETROK_F13            = 294;
	public static final int  RETROK_F14            = 295;
	public static final int  RETROK_F15            = 296;

	public static final int  RETROK_NUMLOCK        = 300;
	public static final int  RETROK_CAPSLOCK       = 301;
	public static final int  RETROK_SCROLLOCK      = 302;
	public static final int  RETROK_RSHIFT         = 303;
	public static final int  RETROK_LSHIFT         = 304;
	public static final int  RETROK_RCTRL          = 305;
	public static final int  RETROK_LCTRL          = 306;
	public static final int  RETROK_RALT           = 307;
	public static final int  RETROK_LALT           = 308;
	public static final int  RETROK_RMETA          = 309;
	public static final int  RETROK_LMETA          = 310;
	public static final int  RETROK_LSUPER         = 311;
	public static final int  RETROK_RSUPER         = 312;
	public static final int  RETROK_MODE           = 313;
	public static final int  RETROK_COMPOSE        = 314;

	public static final int  RETROK_HELP           = 315;
	public static final int  RETROK_PRINT          = 316;
	public static final int  RETROK_SYSREQ         = 317;
	public static final int  RETROK_BREAK          = 318;
	public static final int  RETROK_MENU           = 319;
	public static final int  RETROK_POWER          = 320;
	public static final int  RETROK_EURO           = 321;
	public static final int  RETROK_UNDO           = 322;

	public static final int  RETROK_LAST           = 323;


	public static final int  RETRO_ENVIRONMENT_SET_ROTATION = 1;  // const unsigned * --
	public static final int  RETRO_ENVIRONMENT_GET_OVERSCAN = 2;  // bool * --
	public static final int  RETRO_ENVIRONMENT_GET_CAN_DUPE = 3;  // bool * --
	public static final int  RETRO_ENVIRONMENT_GET_VARIABLE = 4; // struct retro_variable * --
	public static final int  RETRO_ENVIRONMENT_SET_VARIABLES = 5;  // const struct retro_variable * --
	public static final int  RETRO_ENVIRONMENT_SET_MESSAGE = 6;  // const struct retro_message * --
	public static final int  RETRO_ENVIRONMENT_SHUTDOWN = 7;  // N/A (NULL) --
	public static final int  RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL = 8;
	public static final int  RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY = 9;
	public static final int  RETRO_ENVIRONMENT_SET_PIXEL_FORMAT = 10;
	
	public static class SystemInfo
	{
	    public String libraryName;
	    public String libraryVersion;
	    public String validExtensions;
	    
	    public boolean needFullPath;
	    public boolean blockExtract;
	}
	
	public static class AVInfo
	{
	    public int baseWidth;
	    public int baseHeight;
	    
	    public int maxWidth;
	    public int maxHeight;
	    
	    public float aspectRatio;
	    
	    public double fps;
	    public double sampleRate;
	}
	
    public static class VideoFrame
    {
    	public boolean restarted = true;
    	
    	public int framesToRun;
    	public boolean rewind;
    	
    	public int width;
    	public int height;
    	public int pixelFormat;
    	public int rotation;
    	public float aspect;
    	
    	public final int[] keyboard = new int[RETROK_LAST];
    	public final int[] buttons = new int[8];
    	
    	public short touchX;
    	public short touchY;
    	public boolean touched;
    	
    	public final short[] audio = new short[48000];
    	public int audioSamples;
    }
	
	public static native boolean loadLibrary(String aPath, String aSystemDirectory);
	public static native void unloadLibrary();
	public static native void init();
	public static native void deinit();
	public static native int apiVersion();
	public static native void getSystemInfo(SystemInfo aInfo);
	public static native void getSystemAVInfo(AVInfo aInfo);
	public static native void setControllerPortDevice(int aPort, int aDevice);
	public static native void reset();
	public static native void run(VideoFrame aVideo);
	public static native int serializeSize();
//	public static native boolean serialize(byte[] aData, int aSize);
//	public static native boolean unserialize(byte[] aData, int aSize);
	public static native void cheatReset();
	public static native void cheatSet(int aIndex, boolean aEnabled, String aCode);
	public static native boolean loadGame(String aPath);
	public static native void unloadGame();
	public static native int getRegion();
	public static native int getMemorySize(int aID);
	public static native ByteBuffer getMemoryData(int aID);
	
	// Helpers
	public static native void setupRewinder(int aDataSize); // 0 or less disables
	
	public static native boolean writeMemoryRegion(int aID, String aFileName);
	public static native boolean readMemoryRegion(int aID, String aFileBase);

	public static native boolean serializeToFile(String aPath);
	public static native boolean unserializeFromFile(String aPath);
	
	public static native boolean nativeInit();
}
