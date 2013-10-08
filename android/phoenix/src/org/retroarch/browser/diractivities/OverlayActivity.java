package org.retroarch.browser.diractivities;

import java.io.File;

import android.os.Bundle;

public final class OverlayActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		File overlayDir = new File(getBaseContext().getApplicationInfo().dataDir, "overlays");
		if (overlayDir.exists())
			super.setStartDirectory(overlayDir.getAbsolutePath());
		
		super.addAllowedExt(".cfg");
		super.setPathSettingKey("input_overlay");
		super.onCreate(savedInstanceState);
	}
}
