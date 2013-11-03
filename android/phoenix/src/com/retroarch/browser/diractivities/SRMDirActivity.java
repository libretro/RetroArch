package com.retroarch.browser.diractivities;

import android.os.Bundle;

/**
 * {@link DirectoryActivity} subclass used for the sole
 * purpose of navigating the Android filesystem for selecting
 * a custom save file directory.
 */
public final class SRMDirActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.setPathSettingKey("savefile_directory");
		super.setIsDirectoryTarget(true);
		super.onCreate(savedInstanceState);
	}
}
