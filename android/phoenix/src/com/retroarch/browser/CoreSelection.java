package com.retroarch.browser;

import com.retroarch.R;
import com.retroarch.browser.preferences.util.UserPreferences;

import java.io.*;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import android.app.*;
import android.media.AudioManager;
import android.os.*;
import android.widget.*;
import android.view.*;

// JELLY_BEAN_MR1 = 17

public final class CoreSelection extends ListActivity {
	private IconAdapter<ModuleWrapper> adapter;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		final String cpuInfo = UserPreferences.readCPUInfo();
		final boolean cpuIsNeon = cpuInfo.contains("neon");

		// Setup the layout
		setContentView(R.layout.line_list);

		// Set the activity title.
		setTitle(R.string.select_libretro_core);

		// Populate the list
		final List<ModuleWrapper> cores = new ArrayList<ModuleWrapper>();
		final File[] libs = new File(getApplicationInfo().dataDir, "/cores").listFiles();
		for (final File lib : libs) {
			String libName = lib.getName();

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

			cores.add(new ModuleWrapper(this, lib));
		}

		// Sort the list of cores alphabetically
		Collections.sort(cores);

		// Initialize the IconAdapter with the list of cores.
		adapter = new IconAdapter<ModuleWrapper>(this, R.layout.line_list_item, cores);
		setListAdapter(adapter);

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
