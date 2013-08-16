package org.retroarch.browser;

import org.retroarch.R;

import java.io.*;

import android.content.*;
import android.app.*;
import android.media.AudioManager;
import android.os.*;
import android.provider.Settings;
import android.widget.*;
import android.util.Log;
import android.view.*;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.inputmethod.*;

// JELLY_BEAN_MR1 = 17

public class CoreSelection extends Activity implements
		AdapterView.OnItemClickListener {
	private IconAdapter<ModuleWrapper> adapter;
	static private final int ACTIVITY_LOAD_ROM = 0;
	static private String libretro_path;
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

		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
			this.registerForContextMenu(findViewById(android.R.id.content));
		}
	}

	@Override
	public void onItemClick(AdapterView<?> aListView, View aView,
			int aPosition, long aID) {
		final ModuleWrapper item = adapter.getItem(aPosition);
		libretro_path = item.file.getAbsolutePath();

		Intent myIntent;
		myIntent = new Intent(this, ROMActivity.class);
		startActivityForResult(myIntent, ACTIVITY_LOAD_ROM);
	}

	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		Intent myIntent;
		String current_ime = Settings.Secure.getString(getContentResolver(),
				Settings.Secure.DEFAULT_INPUT_METHOD);

		MainMenuActivity.updateConfigFile();

		switch (requestCode) {
		case ACTIVITY_LOAD_ROM:
			if (data.getStringExtra("PATH") != null) {
				Toast.makeText(this,
						"Loading: [" + data.getStringExtra("PATH") + "]...",
						Toast.LENGTH_SHORT).show();
				myIntent = new Intent(this, RetroActivity.class);
				myIntent.putExtra("ROM", data.getStringExtra("PATH"));
				myIntent.putExtra("LIBRETRO", libretro_path);
				myIntent.putExtra("CONFIGFILE",
						MainMenuActivity.getDefaultConfigPath());
				myIntent.putExtra("IME", current_ime);
				startActivity(myIntent);
				finish();
			}
			break;
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu aMenu) {
		super.onCreateOptionsMenu(aMenu);
		getMenuInflater().inflate(R.menu.directory_list, aMenu);
		return true;
	}

	public void showPopup(View v) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			PopupMenuAbstract menu = new PopupMenuAbstract(this, v);
			MenuInflater inflater = menu.getMenuInflater();
			inflater.inflate(R.menu.context_menu, menu.getMenu());
			menu.setOnMenuItemClickListener(new PopupMenuAbstract.OnMenuItemClickListener() {
				@Override
				public boolean onMenuItemClick(MenuItem item) {
					return onContextItemSelected(item);
				}

			});
			menu.show();
		} else {
			this.openContextMenu(findViewById(android.R.id.content));
		}
	}

	@Override
	public void onCreateContextMenu(ContextMenu menu, View v,
			ContextMenuInfo menuInfo) {
		super.onCreateContextMenu(menu, v, menuInfo);
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.context_menu, menu);
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem aItem) {
		switch (aItem.getItemId()) {
		case R.id.settings:
			showPopup(findViewById(R.id.settings));
			return true;

		default:
			return super.onOptionsItemSelected(aItem);
		}
	}

	@Override
	public boolean onContextItemSelected(MenuItem item) {
		switch (item.getItemId()) {
		case R.id.input_method_select:
			InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
			imm.showInputMethodPicker();
			return true;
		default:
			return false;
		}
	}
}
