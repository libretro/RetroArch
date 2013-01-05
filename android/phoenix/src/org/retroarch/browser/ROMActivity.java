package org.retroarch.browser;

import java.io.File;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;

public class ROMActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
		String startPath = prefs.getString("phoenix_rom_dir", "");
		if (!startPath.isEmpty() && new File(startPath).exists())
			super.setStartDirectory(startPath);

		super.addDisallowedExt(".state");
		super.addDisallowedExt(".srm");
		super.addDisallowedExt(".state.auto");
		super.addDisallowedExt(".rtc");
		super.onCreate(savedInstanceState);
	}
}
