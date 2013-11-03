package com.retroarch.browser.diractivities;

import android.os.Bundle;

/**
 * {@link DirectoryActivity} subclass used for the sole
 * purpose of navigating the Android filesystem to select
 * a custom save state directory.
 */
public final class StateDirActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.setPathSettingKey("savestate_directory");
		super.setIsDirectoryTarget(true);
		super.onCreate(savedInstanceState);
	}
}
