package org.retroarch.browser.diractivities;

import android.os.Bundle;

public final class ROMDirActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.setPathSettingKey("rgui_browser_directory");
		super.setIsDirectoryTarget(true);
		super.onCreate(savedInstanceState);
	}
}
