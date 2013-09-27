package org.retroarch.browser;

import java.io.File;

import android.content.SharedPreferences;
import android.os.Bundle;

public final class ROMActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		
		SharedPreferences prefs = MainMenuActivity.getPreferences();
		String startPath = prefs.getString("rgui_browser_directory", "");
		if (!startPath.isEmpty() && new File(startPath).exists())
			super.setStartDirectory(startPath);

		super.addDisallowedExt(".state");
		super.addDisallowedExt(".srm");
		super.addDisallowedExt(".state.auto");
		super.addDisallowedExt(".rtc");
		super.onCreate(savedInstanceState);
	}
}
