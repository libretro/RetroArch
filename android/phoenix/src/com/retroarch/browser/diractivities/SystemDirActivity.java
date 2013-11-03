package com.retroarch.browser.diractivities;

import android.os.Bundle;

/**
 * {@link DirectoryActivity} subclass used for the sole
 * purpose of navigating the Android filesystem for selecting
 * a custom 'System' directory that the cores will look in for
 * required files, such as BIOS files and other miscellaneous
 * system-required files.
 */
public final class SystemDirActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.setPathSettingKey("system_directory");
		super.setIsDirectoryTarget(true);
		super.onCreate(savedInstanceState);
	}
}
