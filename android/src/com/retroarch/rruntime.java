/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

package com.retroarch;

public class rruntime
{	
	static
	{
		System.loadLibrary("retroarch");
	}	
	
	private rruntime() { }

	public static native void load_game(final String j_path, final int j_extract_zip_mode);

	public static native boolean run_frame();

	public static native void startup(String j_config_path);
	
	public static native void deinit(); 
	
	public static native void load_state();

	public static native void save_state();
	
	public static native void settings_change(final int j_setting);

	public static native void settings_set_defaults();
}
