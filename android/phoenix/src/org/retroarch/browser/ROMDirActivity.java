package org.retroarch.browser;

import android.os.Bundle;

public class ROMDirActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.setPathSettingKey("phoenix_rom_dir");
		super.setIsDirectoryTarget(true);
		super.onCreate(savedInstanceState);
	}
}
