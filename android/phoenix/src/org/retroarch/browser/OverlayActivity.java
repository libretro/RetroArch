package org.retroarch.browser;

import java.io.File;

import android.os.Bundle;

public class OverlayActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		File overlayDir = new File(getCacheDir(), "Overlays");
		if (overlayDir.exists())
			super.setStartDirectory(overlayDir.getAbsolutePath());
		
		super.addAllowedExt(".cfg");
		super.setPathSettingKey("input_overlay");
		super.onCreate(savedInstanceState);
	}
}
