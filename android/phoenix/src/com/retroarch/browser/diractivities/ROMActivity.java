package com.retroarch.browser.diractivities;

import java.io.File;

import com.retroarch.browser.preferences.util.UserPreferences;

import android.content.SharedPreferences;
import android.os.Bundle;

/**
 * {@link DirectoryActivity} subclass used for the sole
 * purpose of navigating the Android filesystem for selecting
 * a ROM file to execute during emulation.
 */
public final class ROMActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		
		SharedPreferences prefs = UserPreferences.getPreferences(this);
		String startPath = prefs.getString("rgui_browser_directory", "");
		if (!startPath.isEmpty() && new File(startPath).exists())
			super.setStartDirectory(startPath);

		super.addDisallowedExt(".state");
		super.addDisallowedExt(".srm");
		super.addDisallowedExt(".state.auto");
		super.addDisallowedExt(".rtc");
		super.onCreate(savedInstanceState);
	}
}
