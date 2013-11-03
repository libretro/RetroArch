package com.retroarch.browser.diractivities;

import java.io.File;

import android.os.Bundle;

/**
 * {@link DirectoryActivity} subclass used for the sole
 * purpose of navigating the Android filesystem for input overlays.
 * @author Lioncash-yay
 *
 */
public final class OverlayActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		File overlayDir = new File(getApplicationInfo().dataDir, "overlays");
		if (overlayDir.exists())
			super.setStartDirectory(overlayDir.getAbsolutePath());
		
		super.addAllowedExt(".cfg");
		super.setPathSettingKey("input_overlay");
		super.onCreate(savedInstanceState);
	}
}
