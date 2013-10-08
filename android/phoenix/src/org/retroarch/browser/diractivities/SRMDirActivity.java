package org.retroarch.browser.diractivities;

import android.os.Bundle;

public final class SRMDirActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.setPathSettingKey("savefile_directory");
		super.setIsDirectoryTarget(true);
		super.onCreate(savedInstanceState);
	}
}
