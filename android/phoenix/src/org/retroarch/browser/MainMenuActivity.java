package org.retroarch.browser;

import java.io.*;

import org.retroarch.R;
import org.retroarch.browser.preferences.util.UserPreferences;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceActivity;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;

public final class MainMenuActivity extends PreferenceActivity {
	private static MainMenuActivity instance = null;
	private static final int ACTIVITY_LOAD_ROM = 0;
	private static final String TAG = "MainMenu";
	private static String libretro_path;
	private static String libretro_name;

	@Override
	@SuppressWarnings("deprecation")
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// Load the main menu XML.
		addPreferencesFromResource(R.xml.main_menu);

		// Cache an instance of this class (TODO: Bad practice, kill this somehow).
		instance = this;

		// Get libretro path and name.
		SharedPreferences prefs = UserPreferences.getPreferences(this);
		libretro_path = prefs.getString("libretro_path", getApplicationInfo().nativeLibraryDir);
		libretro_name = prefs.getString("libretro_name", getString(R.string.no_core));

		// Bind audio stream to hardware controls.
		setVolumeControlStream(AudioManager.STREAM_MUSIC);

		// Extract assets. 
		extractAssets();

		if (!prefs.getBoolean("first_time_refreshrate_calculate", false)) {
			prefs.edit().putBoolean("first_time_refreshrate_calculate", true).commit();

			if (!detectDevice(false)) {
				AlertDialog.Builder alert = new AlertDialog.Builder(this)
						.setTitle(R.string.welcome_to_retroarch)
						.setMessage(R.string.welcome_to_retroarch_desc)
						.setPositiveButton("OK", null);
				alert.show();
			}
		}

		Intent startedByIntent = getIntent();
		if (startedByIntent.getStringExtra("ROM") != null && startedByIntent.getStringExtra("LIBRETRO") != null) {
			if (savedInstanceState == null || !savedInstanceState.getBoolean("romexec"))
				loadRomExternal(startedByIntent.getStringExtra("ROM"),
						startedByIntent.getStringExtra("LIBRETRO"));
			else
				finish();
		}
	}

	public static MainMenuActivity getInstance() {
		return instance;
	}

	private int getVersionCode() {
		int version = 0;
		try {
			version = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
		} catch (NameNotFoundException e) {
		}

		return version;
	}

	private boolean areAssetsExtracted() {
		int version = getVersionCode();

		try {
			String dataDir = getApplicationInfo().dataDir;
			File cacheVersion = new File(dataDir, ".cacheversion");
			if (cacheVersion.isFile() && cacheVersion.canRead() && cacheVersion.canWrite()) {
				DataInputStream cacheStream = new DataInputStream(
						new FileInputStream(cacheVersion));

				int currentCacheVersion = 0;
				try {
					currentCacheVersion = cacheStream.readInt();
				} catch (IOException e) {
				}
				cacheStream.close();

				if (currentCacheVersion == version) {
					Log.i("ASSETS", "Assets already extracted, skipping...");
					return true;
				}
			}
		} catch (IOException e) {
			Log.e(TAG, "Failed to extract assets to cache.");
			return false;
		}

		return false;
	}

	// Extract assets from native code. Doing it from Java side is apparently unbearably slow ...
	private void extractAssetsThread() {
		try {
			String dataDir = getApplicationInfo().dataDir;

			String apk = getApplicationInfo().sourceDir;
			Log.i(TAG, "Extracting RetroArch assets from: " + apk + " ...");
			AssetExtractor asset = new AssetExtractor(apk);
			boolean success = asset.extractTo("assets", dataDir);
			if (!success) {
				throw new IOException("Failed to extract assets ...");
			}
			Log.i(TAG, "Extracted assets ...");

			File cacheVersion = new File(dataDir, ".cacheversion");
			DataOutputStream outputCacheVersion = new DataOutputStream(
					new FileOutputStream(cacheVersion, false));
			outputCacheVersion.writeInt(getVersionCode());
			outputCacheVersion.close();
		} catch (IOException e) {
			Log.e(TAG, "Failed to extract assets to cache.");
		}
	}

	private void extractAssets() {
		if (areAssetsExtracted())
			return;

		final Dialog dialog = new Dialog(this);
		final Handler handler = new Handler();
		dialog.setContentView(R.layout.assets);
		dialog.setCancelable(false);
		dialog.setTitle("Asset extraction");

		// Java is fun :)
		Thread assetsThread = new Thread(new Runnable() {
			public void run() {
				extractAssetsThread();
				handler.post(new Runnable() {
					public void run() {
						dialog.dismiss();
					}
				});
			}
		});
		assetsThread.start();

		dialog.show();
	}

	public void setModule(String core_path, String core_name) {
		UserPreferences.updateConfigFile(this);
		
		libretro_path = core_path;
		libretro_name = core_name;

		SharedPreferences prefs = UserPreferences.getPreferences(this);
		SharedPreferences.Editor edit = prefs.edit();
		edit.putString("libretro_path", libretro_path);
		edit.putString("libretro_name", libretro_name);
		edit.commit();

		// Set the title section to contain the name of the selected core.
		setCoreTitle(libretro_name);
		
	}

	public void setCoreTitle(String core_name) {
		setTitle("RetroArch : " + core_name);
	}

	private boolean detectDevice(boolean show_dialog) {
		boolean retval = false;

		final boolean mentionPlayStore = !Build.MODEL.equals("OUYA Console");
		final String message = (mentionPlayStore ? getString(R.string.detect_device_msg_general) : getString(R.string.detect_device_msg_ouya));

		Log.i("Device MODEL", Build.MODEL);
		if (Build.MODEL.equals("SHIELD")) {
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
					.setTitle(R.string.nvidia_shield_detected)
					.setMessage(message)
					.setPositiveButton(R.string.ok,
							new DialogInterface.OnClickListener() {
								@Override
								public void onClick(DialogInterface dialog, int which) {
									SharedPreferences prefs = UserPreferences.getPreferences(MainMenuActivity.this);
									SharedPreferences.Editor edit = prefs.edit();
									edit.putString("video_refresh_rate", Double.toString(60.00d));
									edit.putBoolean("input_overlay_enable", false);
									edit.putBoolean("input_autodetect_enable", true);
									edit.putString("audio_latency", "64");
									edit.putBoolean("audio_latency_auto", true);
									edit.commit();
								}
							});
			alert.show();
			retval = true;
		} else if (Build.MODEL.equals("GAMEMID_BT")) {
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
					.setTitle(R.string.game_mid_detected)
					.setMessage(message)
					.setPositiveButton(R.string.ok,
							new DialogInterface.OnClickListener() {
								@Override
								public void onClick(DialogInterface dialog, int which) {
									SharedPreferences prefs = UserPreferences.getPreferences(MainMenuActivity.this);
									SharedPreferences.Editor edit = prefs.edit();
									edit.putBoolean("input_overlay_enable", false);
									edit.putBoolean("input_autodetect_enable", true);
									edit.putString("audio_latency", "160");
									edit.putBoolean("audio_latency_auto", false);
									edit.commit();
								}
							});
			alert.show();
			retval = true;
		} else if (Build.MODEL.equals("OUYA Console")) {
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
					.setTitle(R.string.ouya_detected)
					.setMessage(message)
					.setPositiveButton(R.string.ok,
							new DialogInterface.OnClickListener() {
								@Override
								public void onClick(DialogInterface dialog, int which) {
									SharedPreferences prefs = UserPreferences.getPreferences(MainMenuActivity.this);
									SharedPreferences.Editor edit = prefs.edit();
									edit.putBoolean("input_overlay_enable", false);
									edit.putBoolean("input_autodetect_enable", true);
									edit.putString("audio_latency", "64");
									edit.putBoolean("audio_latency_auto", true);
									edit.commit();
								}
							});
			alert.show();
			retval = true;
		} else if (Build.MODEL.equals("R800x")) {
					AlertDialog.Builder alert = new AlertDialog.Builder(this)
							.setTitle(R.string.xperia_play_detected)
							.setMessage(message)
							.setPositiveButton(R.string.ok,
									new DialogInterface.OnClickListener() {
										@Override
										public void onClick(DialogInterface dialog, int which) {
											SharedPreferences prefs = UserPreferences.getPreferences(MainMenuActivity.this);
											SharedPreferences.Editor edit = prefs.edit();
											edit.putBoolean("video_threaded", false);
											edit.putBoolean("input_overlay_enable", false);
											edit.putBoolean("input_autodetect_enable", true);
											edit.putString("video_refresh_rate", Double.toString(59.19132938771038));
											edit.putString("audio_latency", "128");
											edit.putBoolean("audio_latency_auto", false);
											edit.commit();
										}
									});
					alert.show();
					retval = true;
		} else if (Build.ID.equals("JSS15J")) {
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
					.setTitle(R.string.nexus_7_2013_detected)
					.setMessage(message)
					.setPositiveButton(R.string.ok,
							new DialogInterface.OnClickListener() {
								@Override
								public void onClick(DialogInterface dialog, int which) {
									SharedPreferences prefs = UserPreferences.getPreferences(MainMenuActivity.this);
									SharedPreferences.Editor edit = prefs.edit();
									edit.putString("video_refresh_rate", Double.toString(59.65));
									edit.putString("audio_latency", "64");
									edit.putBoolean("audio_latency_auto", false);
									edit.commit();
								}
							});
			alert.show();
			retval = true;
		}

		if (show_dialog) {
			Toast.makeText(this, R.string.no_optimal_settings, Toast.LENGTH_SHORT).show();
		}

		return retval;
	}

	@Override
	public void startActivity(Intent intent) {
		if (intent.getComponent().getClassName()
				.equals("org.retroarch.browser.diractivities.ROMActivity")) {
			if (!new File(libretro_path).isDirectory()) {
				super.startActivityForResult(intent, ACTIVITY_LOAD_ROM);
			} else {
				Toast.makeText(this, R.string.load_a_core_first, Toast.LENGTH_SHORT).show();
			}
		} else {
			super.startActivity(intent);
		}
	}

	@Override
	protected void onActivityResult(int reqCode, int resCode, Intent data) {
		switch (reqCode) {
		case ACTIVITY_LOAD_ROM: {
			if (data.getStringExtra("PATH") != null) {
				UserPreferences.updateConfigFile(this);
				String current_ime = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
				Toast.makeText(this,String.format(getString(R.string.loading_data), data.getStringExtra("PATH")), Toast.LENGTH_SHORT).show();
				Intent myIntent = new Intent(this, RetroActivity.class);
				myIntent.putExtra("ROM", data.getStringExtra("PATH"));
				myIntent.putExtra("LIBRETRO", libretro_path);
				myIntent.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(this));
				myIntent.putExtra("IME", current_ime);
				startActivity(myIntent);
			}
		}
			break;
		}
	}

	@Override
	protected void onSaveInstanceState(Bundle data) {
		super.onSaveInstanceState(data);
		data.putBoolean("romexec", true);
	}

	private void loadRomExternal(String rom, String core) {
		UserPreferences.updateConfigFile(this);
		String current_ime = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
		Toast.makeText(this, String.format(getString(R.string.loading_data), rom), Toast.LENGTH_SHORT).show();
		Intent myIntent = new Intent(this, RetroActivity.class);
		myIntent.putExtra("ROM", rom);
		myIntent.putExtra("LIBRETRO", core);
		myIntent.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(this));
		myIntent.putExtra("IME", current_ime);
		startActivity(myIntent);
	}
}
