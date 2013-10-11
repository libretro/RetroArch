package org.retroarch.browser;

import org.retroarch.R;
import org.retroarch.browser.preferences.ConfigFile;
import org.retroarch.browser.preferences.UserPreferences;

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
		final String modulePath = getApplicationInfo().nativeLibraryDir;
		final File[] libs = new File(modulePath).listFiles();
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

			// Allow both libretro-core.so and libretro_core.so.
			if (libName.startsWith("libretro")
					&& !libName.startsWith("libretroarch")) {
				try {
					adapter.add(new ModuleWrapper(this, lib, core_config));
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}

		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);
	}

	@Override
	public void onListItemClick(ListView listView, View view, int position, long id) {
		final ModuleWrapper item = adapter.getItem(position);
		MainMenuActivity.getInstance().setModule(item.file.getAbsolutePath(), item.getText());
		UserPreferences.updateConfigFile(this);
		finish();
	}
}
