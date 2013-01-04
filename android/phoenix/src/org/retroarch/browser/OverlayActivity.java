package org.retroarch.browser;

import android.os.Bundle;

public class OverlayActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.addAllowedExt(".cfg");
		super.setPathSettingKey("input_overlay");
		super.onCreate(savedInstanceState);
	}
}
