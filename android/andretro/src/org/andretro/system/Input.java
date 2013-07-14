package org.andretro.system;

import static android.view.KeyEvent.*;
import static org.libretro.LibRetro.*;

import org.andretro.input.view.*;
import android.view.*;
import java.util.*;

import org.andretro.emulator.Doodads;

/**
 * Static class to manage input state for the emulator view.
 * @author jason
 *
 */
public final class Input
{
	private static Set<Integer> keys = new TreeSet<Integer>();
	private static InputGroup onscreenInput;
	
	private static short touchX;
	private static short touchY;
	private static boolean touched;
	
    // Called by UI
	public synchronized static void processEvent(KeyEvent aEvent)
	{
		if(aEvent.getAction() == KeyEvent.ACTION_DOWN)
		{
			keys.add(aEvent.getKeyCode());
		}
		else if(aEvent.getAction() == KeyEvent.ACTION_UP)
		{
			keys.remove(aEvent.getKeyCode());
		}
	}
	
	public synchronized static void clear()
	{
		keys.clear();
	}

	public synchronized static void setOnScreenInput(InputGroup aInput)
	{
		onscreenInput = aInput;
	}
	
	public synchronized static void setTouchData(short aX, short aY, boolean aTouched)
	{
		touchX = aX;
		touchY = aY;
		touched = aTouched; 
	}
	
	// Called by emulator
	public static synchronized int getBits(Doodads.Device aDevice)
	{
		int result = (null != onscreenInput) ? onscreenInput.getBits() : 0;
		
		for(Doodads.Button i: aDevice.getAll())
		{
			if(keys.contains(i.getKeyCode()))
			{
				result |= (1 << i.bitOffset);
			}	
		}
		
		return result;
	}
	
	public static synchronized boolean isPressed(int aKeyCode)
	{
		return keys.contains(aKeyCode);
	}
	
	public static synchronized void poolKeyboard(int[] aKeyState)
	{
		for(int i = 0; i != mappingTable.length / 2; i ++)
		{
			final int retroID = mappingTable[i * 2 + 0];
			final int androidID = mappingTable[i * 2 + 1];
			aKeyState[retroID] = keys.contains(androidID) ? 1 : 0;
		}
	}
	
	public static synchronized short getTouchX()
	{
		return touchX;
	}
	
	public static synchronized short getTouchY()
	{
		return touchY;
	}

	public static synchronized boolean getTouched()
	{
		return touched;
	}

	

	// KEYBOARD
	private static final int[] mappingTable = 
	{
		RETROK_LEFT, KEYCODE_DPAD_LEFT,
		RETROK_RIGHT, KEYCODE_DPAD_RIGHT,
		RETROK_UP, KEYCODE_DPAD_UP,
		RETROK_DOWN, KEYCODE_DPAD_DOWN,
		RETROK_RETURN, KEYCODE_ENTER,
		RETROK_TAB, KEYCODE_TAB,
		RETROK_DELETE, KEYCODE_DEL,
		RETROK_RSHIFT, KEYCODE_SHIFT_RIGHT,
		RETROK_LSHIFT, KEYCODE_SHIFT_LEFT,
		RETROK_LCTRL, KEYCODE_CTRL_LEFT,
		RETROK_END, KEYCODE_MOVE_END,
		RETROK_HOME, KEYCODE_MOVE_HOME,
		RETROK_PAGEDOWN, KEYCODE_PAGE_DOWN,
		RETROK_PAGEUP, KEYCODE_PAGE_UP,
		RETROK_LALT, KEYCODE_ALT_LEFT,
		RETROK_SPACE, KEYCODE_SPACE,
		RETROK_ESCAPE, KEYCODE_ESCAPE,
//			RETROK_BACKSPACE, KEYCODE,
		RETROK_KP_ENTER, KEYCODE_NUMPAD_ENTER,
		RETROK_KP_PLUS, KEYCODE_NUMPAD_ADD,
		RETROK_KP_MINUS, KEYCODE_NUMPAD_SUBTRACT,
		RETROK_KP_MULTIPLY, KEYCODE_NUMPAD_MULTIPLY,
		RETROK_KP_DIVIDE, KEYCODE_NUMPAD_DIVIDE,
//			RETROK_BACKQUOTE, KEYCODE,
//			RETROK_PAUSE, KEYCODE,
		RETROK_KP0, KEYCODE_NUMPAD_0,
		RETROK_KP1, KEYCODE_NUMPAD_1,
		RETROK_KP2, KEYCODE_NUMPAD_2,
		RETROK_KP3, KEYCODE_NUMPAD_3,
		RETROK_KP4, KEYCODE_NUMPAD_4,
		RETROK_KP5, KEYCODE_NUMPAD_5,
		RETROK_KP6, KEYCODE_NUMPAD_6,
		RETROK_KP7, KEYCODE_NUMPAD_7,
		RETROK_KP8, KEYCODE_NUMPAD_8,
		RETROK_KP9, KEYCODE_NUMPAD_9,
		RETROK_0, KEYCODE_0,
		RETROK_1, KEYCODE_1,
		RETROK_2, KEYCODE_2,
		RETROK_3, KEYCODE_3,
		RETROK_4, KEYCODE_4,
		RETROK_5, KEYCODE_5,
		RETROK_6, KEYCODE_6,
		RETROK_7, KEYCODE_7,
		RETROK_8, KEYCODE_8,
		RETROK_9, KEYCODE_9,
		RETROK_F1, KEYCODE_F1,
		RETROK_F2, KEYCODE_F2,
		RETROK_F3, KEYCODE_F3,
		RETROK_F4, KEYCODE_F4,
		RETROK_F5, KEYCODE_F5,
		RETROK_F6, KEYCODE_F6,
		RETROK_F7, KEYCODE_F7,
		RETROK_F8, KEYCODE_F8,
		RETROK_F9, KEYCODE_F9,
		RETROK_F10, KEYCODE_F10,
		RETROK_F11, KEYCODE_F11,
		RETROK_F12, KEYCODE_F12,
		RETROK_a, KEYCODE_A,
		RETROK_b, KEYCODE_B,
		RETROK_c, KEYCODE_C,
		RETROK_d, KEYCODE_D,
		RETROK_e, KEYCODE_E,
		RETROK_f, KEYCODE_F,
		RETROK_g, KEYCODE_G,
		RETROK_h, KEYCODE_H,
		RETROK_i, KEYCODE_I,
		RETROK_j, KEYCODE_J,
		RETROK_k, KEYCODE_K,
		RETROK_l, KEYCODE_L,
		RETROK_m, KEYCODE_M,
		RETROK_n, KEYCODE_N,
		RETROK_o, KEYCODE_O,
		RETROK_p, KEYCODE_P,
		RETROK_q, KEYCODE_Q,
		RETROK_r, KEYCODE_R,
		RETROK_s, KEYCODE_S,
		RETROK_t, KEYCODE_T,
		RETROK_u, KEYCODE_U,
		RETROK_v, KEYCODE_V,
		RETROK_w, KEYCODE_W,
		RETROK_x, KEYCODE_X,
		RETROK_y, KEYCODE_Y,
		RETROK_z, KEYCODE_Z,
	};
}
