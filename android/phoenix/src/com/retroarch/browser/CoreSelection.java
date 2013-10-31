package com.retroarch.browser;

import com.retroarch.R;
import com.retroarch.browser.preferences.util.ConfigFile;
import com.retroarch.browser.preferences.util.UserPreferences;

import java.io.*;

import android.app.*;
import android.media.AudioManager;
import android.os.*;
import android.widget.*;
import android.util.Log;
import android.view.*;

// JELLY_BEAN_MR1 = 17

public final class CoreSelection extends ListActivity {
	private IconAdapter<ModuleWrapper> adapter;
	private static final String TAG = "CoreSelection";

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		ConfigFile core_config = new ConfigFile();
		try {
			core_config.append(getAssets().open("libretro_cores.cfg"));
		} catch (IOException e) {
			Log.e(TAG, "Failed to load libretro_cores.cfg from assets.");
		}

		final String cpuInfo = UserPreferences.readCPUInfo();
		final boolean cpuIsNeon = cpuInfo.contains("neon");

		// Setup the layout
		setContentView(R.layout.line_list);

		// Setup the list
		adapter = new IconAdapter<ModuleWrapper>(this, R.layout.line_list_item);
		setListAdapter(adapter);

		// Set the activity title.
		setTitle(R.string.select_libretro_core);

		// Populate the list
		final File[] libs = new File(getApplicationInfo().dataDir, "/cores").listFiles();
		for (final File lib : libs) {
			String libName = lib.getName();

			Log.i(TAG, "Libretro core: " + libName);
			// Never append a NEON lib if we don't have NEON.
			if (libName.contains("neon") && !cpuIsNeon)
				continue;

			// If we have a NEON version with NEON capable CPU,
			// never append a non-NEON version.
			if (cpuIsNeon && !libName.contains("neon")) {
				boolean hasNeonVersion = false;
				for (final File lib_ : libs) {
					String otherName = lib_.getName();
					String baseName = libName.replace(".so", "");
					if (otherName.contains("neon")
							&& otherName.startsWith(baseName)) {
						hasNeonVersion = true;
						break;
					}
				}

				if (hasNeonVersion)
					continue;
			}
			
			adapter.add(new ModuleWrapper(this, lib));
		}

		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);
	}

	@Override
	public void onListItemClick(ListView listView, View view, int position, long id) {
		final ModuleWrapper item = adapter.getItem(position);
		MainMenuActivity.getInstance().setModule(item.getUnderlyingFile().getAbsolutePath(), item.getText());
		UserPreferences.updateConfigFile(this);
		finish();
	}
}
