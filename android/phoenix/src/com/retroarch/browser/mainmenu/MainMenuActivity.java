package com.retroarch.browser.mainmenu;

import java.io.*;

import com.retroarch.R;
import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.RetroActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.util.Log;
import android.widget.Toast;

/**
 * Class representing the {@link FragmentActivity} for the main menu.
 */
public final class MainMenuActivity extends FragmentActivity {
	private static MainMenuActivity instance = null;
	private static final String TAG = "MainMenu";

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// Set the content view. This will give us the FrameLayout to switch fragments in and out of.
		setContentView(R.layout.main_menu_layout);

		// Show the main UI for this FragmentActivity.
		final Fragment mainMenuFragment = new MainMenuFragment();
		final FragmentManager fm = getSupportFragmentManager();
		fm.beginTransaction().replace(R.id.content_frame, mainMenuFragment).commit();

		// Cache an instance of this class (TODO: Bad practice, kill this somehow).
		instance = this;

		// Get libretro path and name.
		SharedPreferences prefs = UserPreferences.getPreferences(this);

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
				loadRomExternal(startedByIntent.getStringExtra("ROM"), startedByIntent.getStringExtra("LIBRETRO"));
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
					Log.i(TAG, "Assets already extracted, skipping...");
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
			boolean success = NativeInterface.extractArchiveTo(apk, "assets", dataDir);
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
		dialog.setTitle(R.string.asset_extraction);

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
		
		String libretro_path = core_path;
		String libretro_name = core_name;

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

		final Context ctx = this;
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
									UserPreferences.updateConfigFile(ctx);
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
									UserPreferences.updateConfigFile(ctx);
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
									UserPreferences.updateConfigFile(ctx);
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
											UserPreferences.updateConfigFile(ctx);
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
									UserPreferences.updateConfigFile(ctx);
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
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(this);
		final String corePath = sPrefs.getString("libretro_path", "");

		// If ROMActivity is attempting to be accessed.
		if (intent.getComponent().getClassName().equals("com.retroarch.browser.diractivities.ROMActivity")) {
			// If the path for a core hasn't been set yet, prompt the user to do so.
			// otherwise, launch the activity to browse for a ROM to load.
			if (!new File(corePath).isDirectory()) {
				startActivity(intent);
			} else {
				Toast.makeText(this, R.string.load_a_core_first, Toast.LENGTH_SHORT).show();
			}
		} else {
			super.startActivity(intent);
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
