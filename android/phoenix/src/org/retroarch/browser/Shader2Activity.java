package org.retroarch.browser;

import java.io.File;

import android.os.Bundle;

public class Shader2Activity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		File shaderDir = new File(getCacheDir(), "Shaders");
		if (shaderDir.exists())
			super.setStartDirectory(shaderDir.getAbsolutePath());
		
		super.addAllowedExt(".shader");
		super.setPathSettingKey("video_second_pass_shader");
		super.onCreate(savedInstanceState);
	}
}
