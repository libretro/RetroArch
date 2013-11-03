package com.retroarch.browser.diractivities;

import java.io.File;

import android.os.Bundle;

/**
 * {@link DirectoryActivity} subclass used for the sole
 * purpose of navigating the Android filesystem for selecting
 * a shader to use during emulation.
 */
public final class ShaderActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		File shaderDir = new File(getApplicationInfo().dataDir, "shaders_glsl");
		if (shaderDir.exists())
			super.setStartDirectory(shaderDir.getAbsolutePath());
		
		super.addAllowedExt(".glsl");
		super.setPathSettingKey("video_shader");
		super.onCreate(savedInstanceState);
	}
}
