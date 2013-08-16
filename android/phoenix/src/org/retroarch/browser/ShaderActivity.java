package org.retroarch.browser;

import java.io.File;

import android.os.Bundle;

public class ShaderActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		MainMenuActivity.waitAssetThread();
		File shaderDir = new File(getBaseContext().getApplicationInfo().dataDir, "shaders_glsl");
		if (shaderDir.exists())
			super.setStartDirectory(shaderDir.getAbsolutePath());
		
		super.addAllowedExt(".glsl");
		super.setPathSettingKey("video_shader");
		super.onCreate(savedInstanceState);
	}
}
