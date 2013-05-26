package org.retroarch.browser;

import android.os.Bundle;

public class SystemDirActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.setPathSettingKey("system_directory");
		super.setIsDirectoryTarget(true);
		super.onCreate(savedInstanceState);
	}
}
