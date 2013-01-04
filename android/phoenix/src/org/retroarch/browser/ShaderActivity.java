package org.retroarch.browser;

import android.os.Bundle;

public class ShaderActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.addAllowedExt(".shader");
		super.setPathSettingKey("video_bsnes_shader");
		super.onCreate(savedInstanceState);
	}
}
