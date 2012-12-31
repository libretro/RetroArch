package org.retroarch.browser;

import org.retroarch.R;

import java.io.*;

import android.content.*;
import android.app.*;
import android.os.*;
import android.widget.*;
import android.util.Log;
import android.view.*;
import android.view.inputmethod.*;
import android.graphics.drawable.*;

class ModuleWrapper implements IconAdapterItem {
	public final File file;

	public ModuleWrapper(Context aContext, File aFile) throws IOException {
		file = aFile;
	}

	@Override
	public boolean isEnabled() {
		return true;
	}

	@Override
	public String getText() {
		return file.getName();
	}

	@Override
	public int getIconResourceId() {
		return 0;
	}

	@Override
	public Drawable getIconDrawable() {
		return null;
	}
}

public class ModuleActivity extends Activity implements
		AdapterView.OnItemClickListener, PopupMenu.OnMenuItemClickListener {
	private IconAdapter<ModuleWrapper> adapter;
	static private final int ACTIVITY_LOAD_ROM = 0;
	static private String libretro_path;
	static private final String TAG = "RetroArch";
	private ConfigFile config;

	public float getRefreshRate() {
		final WindowManager wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
		final Display display = wm.getDefaultDisplay();
		float rate = display.getRefreshRate();
		return rate;
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		try {
			config = new ConfigFile(new File(getDefaultConfigPath()));
		} catch (IOException e) {
			config = new ConfigFile();
		}
		
		setContentView(R.layout.line_list);

		// Setup the list
		adapter = new IconAdapter<ModuleWrapper>(this, R.layout.line_list_item);
		ListView list = (ListView) findViewById(R.id.list);
		list.setAdapter(adapter);
		list.setOnItemClickListener(this);

		setTitle("Select Libretro core");

		// Populate the list
		final String modulePath = getApplicationInfo().nativeLibraryDir;
		for (final File lib : new File(modulePath).listFiles()) {
			String libName = lib.getName();
			
			// Allow both libretro-core.so and libretro_core.so.
			if (libName.startsWith("libretro") && !libName.startsWith("libretroarch")) {
				try {
					adapter.add(new ModuleWrapper(this, lib));
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}
	}

	@Override
	public void onItemClick(AdapterView<?> aListView, View aView,
			int aPosition, long aID) {
		final ModuleWrapper item = adapter.getItem(aPosition);
		libretro_path = item.file.getAbsolutePath();

		Intent myIntent;
		myIntent = new Intent(this, DirectoryActivity.class);
		startActivityForResult(myIntent, ACTIVITY_LOAD_ROM);
	}
	
	private String getDefaultConfigPath() {
		String internal = System.getenv("INTERNAL_STORAGE");
		String external = System.getenv("EXTERNAL_STORAGE");
		
		if (external != null) {
			String confPath = external + File.separator + "retroarch.cfg";
			if (new File(confPath).exists())
				return confPath;
		} else if (internal != null) {
			String confPath = internal + File.separator + "retroarch.cfg";
			if (new File(confPath).exists())
				return confPath;
		} else {
			String confPath = "/mnt/extsd/retroarch.cfg";
			if (new File(confPath).exists())
				return confPath;
		}
		
		if (internal != null && new File(internal + File.separator + "retroarch.cfg").canWrite())
			return internal + File.separator + "retroarch.cfg";
		else if (external != null && new File(internal + File.separator + "retroarch.cfg").canWrite())
			return external + File.separator + "retroarch.cfg";
		else
			return getCacheDir().getAbsolutePath() + File.separator + "retroarch.cfg";
	}

	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		Intent myIntent;

		switch (requestCode) {
		case ACTIVITY_LOAD_ROM:
			if (data.getStringExtra("PATH") != null) {
				Toast.makeText(this,
						"Loading: [" + data.getStringExtra("PATH") + "]...",
						Toast.LENGTH_SHORT).show();
				myIntent = new Intent(this, NativeActivity.class);
				myIntent.putExtra("ROM", data.getStringExtra("PATH"));
				myIntent.putExtra("LIBRETRO", libretro_path);
				myIntent.putExtra("REFRESHRATE",
						Float.toString(getRefreshRate()));
				myIntent.putExtra("CONFIGFILE", getDefaultConfigPath());
				startActivity(myIntent);
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
		PopupMenu menu = new PopupMenu(this, v);
		MenuInflater inflater = menu.getMenuInflater();
		inflater.inflate(R.menu.context_menu, menu.getMenu());
		menu.setOnMenuItemClickListener(this);
		menu.show();
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem aItem) {
		switch (aItem.getItemId()) {
		case R.id.settings:
			showPopup(findViewById(R.id.settings));
			Log.i(TAG, "Got settings ...");
			return true;

		default:
			return super.onOptionsItemSelected(aItem);
		}
	}

	@Override
	public boolean onMenuItemClick(MenuItem item) {
		switch (item.getItemId()) {
		case R.id.input_method_select:
			InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
			imm.showInputMethodPicker();
			return true;
			
		case R.id.video_settings:
			Log.i(TAG, "Video settings clicked!");
			
			Intent vset = new Intent(this, SettingsActivity.class);
			vset.putExtra("TITLE", "Video Config");
			startActivity(vset);
			return true;
			
		case R.id.audio_settings:
			Log.i(TAG, "Audio settings clicked!");
			Intent aset = new Intent(this, SettingsActivity.class);
			aset.putExtra("TITLE", "Audio Config");
			startActivity(aset);
			return true;
			
		case R.id.general_settings:
			Log.i(TAG, "General settings clicked!");
			Intent gset = new Intent(this, SettingsActivity.class);
			gset.putExtra("TITLE", "General Config");
			startActivity(gset);
			return true;
		default:
			return false;
		}
	}
}