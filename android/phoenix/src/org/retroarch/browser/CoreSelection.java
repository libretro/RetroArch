package org.retroarch.browser;

import org.retroarch.R;

import java.io.*;

import android.app.*;
import android.media.AudioManager;
import android.os.*;
import android.widget.*;
import android.util.Log;
import android.view.*;

// JELLY_BEAN_MR1 = 17

public final class CoreSelection extends Activity implements
		AdapterView.OnItemClickListener {
	private IconAdapter<ModuleWrapper> adapter;
	static private final String TAG = "CoreSelection";

	@Override
	public void onCreate(Bundle savedInstanceState) {
		ConfigFile core_config;
		super.onCreate(savedInstanceState);

		core_config = new ConfigFile();
		try {
			core_config.append(getAssets().open("libretro_cores.cfg"));
		} catch (IOException e) {
			Log.e(TAG, "Failed to load libretro_cores.cfg from assets.");
		}

		String cpuInfo = MainMenuActivity.readCPUInfo();
		boolean cpuIsNeon = cpuInfo.contains("neon");

		setContentView(R.layout.line_list);

		// Setup the list
		adapter = new IconAdapter<ModuleWrapper>(this, R.layout.line_list_item);
		ListView list = (ListView) findViewById(R.id.list);
		list.setAdapter(adapter);
		list.setOnItemClickListener(this);

		setTitle("Select Libretro core");

		// Populate the list
		final String modulePath = MainMenuActivity.getInstance()
				.getApplicationInfo().nativeLibraryDir;
		final File[] libs = new File(modulePath).listFiles();
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
	public void onItemClick(AdapterView<?> aListView, View aView,
			int aPosition, long aID) {
		final ModuleWrapper item = adapter.getItem(aPosition);
		MainMenuActivity.getInstance().setModule(item.file.getAbsolutePath(), item.getText());
		MainMenuActivity.getInstance().updateConfigFile();
		finish();
	}
}
