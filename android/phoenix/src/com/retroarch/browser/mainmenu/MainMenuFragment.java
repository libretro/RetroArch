package com.retroarch.browser.mainmenu;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;

import com.retroarch.R;
import com.retroarch.browser.CoreSelection;
import com.retroarch.browser.HistorySelection;
import com.retroarch.browser.ModuleWrapper;
import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.RetroActivity;
import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.dirfragment.DirectoryFragment.OnDirectoryFragmentClosedListener;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarch.browser.preferences.util.UserPreferences;

/**
 * Represents the fragment that handles the layout of the main menu.
 */
public final class MainMenuFragment extends PreferenceListFragment implements OnPreferenceClickListener, OnDirectoryFragmentClosedListener
{
	private static final String TAG = "MainMenuFragment";

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Add the layout through the XML.
		addPreferencesFromResource(R.xml.main_menu);

		// Set the listeners for the menu items
		findPreference("loadCorePref").setOnPreferenceClickListener(this);
		findPreference("loadRomPref").setOnPreferenceClickListener(this);
		findPreference("loadRomHistoryPref").setOnPreferenceClickListener(this);

		// Extract assets. 
		extractAssets();

		final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
		if (!prefs.getBoolean("first_time_refreshrate_calculate", false))
		{
			prefs.edit().putBoolean("first_time_refreshrate_calculate", true).commit();

			if (!detectDevice(false))
			{
				AlertDialog.Builder alert = new AlertDialog.Builder(getActivity())
						.setTitle(R.string.welcome_to_retroarch)
						.setMessage(R.string.welcome_to_retroarch_desc)
						.setPositiveButton(R.string.ok, null);
				alert.show();
			}
			
			showGPLWaiver();
		}
	}

	private void showGPLWaiver()
	{
		AlertDialog.Builder alert = new AlertDialog.Builder(getActivity())
				.setTitle(R.string.gpl_waiver)
				.setMessage(R.string.gpl_waiver_desc)
				.setPositiveButton(R.string.keep_cores, null)
				.setNegativeButton(R.string.remove_cores, new DialogInterface.OnClickListener()
				{
					@Override
					public void onClick(DialogInterface dialog, int which)
					{
						final File[] libs = new File(getActivity().getApplicationInfo().dataDir, "/cores").listFiles();
						for (final File lib : libs)
						{
							ModuleWrapper module = new ModuleWrapper(getActivity().getApplicationContext(), lib);
							
							boolean gplv3 = module.getCoreLicense().equals("GPLv3");
							boolean gplv2 = module.getCoreLicense().equals("GPLv2");
							
							if (!gplv3 && !gplv2)
							{
								String libName = lib.getName();
								Log.i("GPL WAIVER", "Deleting non-GPL core" + libName + "...");
								lib.delete();
							}
						}
					}
				});
		alert.show();
	}

	private void extractAssets()
	{
		if (areAssetsExtracted())
			return;

		final Dialog dialog = new Dialog(getActivity());
		final Handler handler = new Handler();
		dialog.setContentView(R.layout.assets);
		dialog.setCancelable(false);
		dialog.setTitle(R.string.asset_extraction);

		// Java is fun :)
		Thread assetsThread = new Thread(new Runnable()
		{
			public void run()
			{
				extractAssetsThread();
				handler.post(new Runnable()
				{
					public void run()
					{
						dialog.dismiss();
					}
				});
			}
		});
		assetsThread.start();

		dialog.show();
	}

	// Extract assets from native code. Doing it from Java side is apparently unbearably slow ...
	private void extractAssetsThread()
	{
		try
		{
			final String dataDir = getActivity().getApplicationInfo().dataDir;
			final String apk = getActivity().getApplicationInfo().sourceDir;

			Log.i(TAG, "Extracting RetroArch assets from: " + apk + " ...");
			boolean success = NativeInterface.extractArchiveTo(apk, "assets", dataDir);
			if (!success) {
				throw new IOException("Failed to extract assets ...");
			}
			Log.i(TAG, "Extracted assets ...");

			File cacheVersion = new File(dataDir, ".cacheversion");
			DataOutputStream outputCacheVersion = new DataOutputStream(new FileOutputStream(cacheVersion, false));
			outputCacheVersion.writeInt(getVersionCode());
			outputCacheVersion.close();
		}
		catch (IOException e)
		{
			Log.e(TAG, "Failed to extract assets to cache.");
		}
	}

	private boolean areAssetsExtracted()
	{
		int version = getVersionCode();

		try
		{
			String dataDir = getActivity().getApplicationInfo().dataDir;
			File cacheVersion = new File(dataDir, ".cacheversion");
			if (cacheVersion.isFile() && cacheVersion.canRead() && cacheVersion.canWrite())
			{
				DataInputStream cacheStream = new DataInputStream(new FileInputStream(cacheVersion));
				int currentCacheVersion = 0;
				try
				{
					currentCacheVersion = cacheStream.readInt();
					cacheStream.close();
				}
				catch (IOException ignored)
				{
				}

				if (currentCacheVersion == version)
				{
					Log.i("ASSETS", "Assets already extracted, skipping...");
					return true;
				}
			}
		}
		catch (IOException e)
		{
			Log.e(TAG, "Failed to extract assets to cache.");
			return false;
		}

		return false;
	}

	private int getVersionCode()
	{
		int version = 0;
		try
		{
			final Context ctx = getActivity();
			version = ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).versionCode;
		}
		catch (NameNotFoundException ignored)
		{
		}

		return version;
	}

	private boolean detectDevice(boolean show_dialog)
	{
		boolean retval = false;

		final Context ctx = getActivity();
		final boolean mentionPlayStore = !Build.MODEL.equals("OUYA Console");
		final String message = (mentionPlayStore ? getString(R.string.detect_device_msg_general) : getString(R.string.detect_device_msg_ouya));

		Log.i("Device MODEL", Build.MODEL);
		if (Build.MODEL.equals("SHIELD"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.nvidia_shield_detected);
			alert.setMessage(message);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
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
		}
		else if (Build.MODEL.equals("GAMEMID_BT"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.game_mid_detected);
			alert.setMessage(message);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
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
		}
		else if (Build.MODEL.equals("OUYA Console"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.ouya_detected);
			alert.setMessage(message);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
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
		}
		else if (Build.MODEL.equals("R800x"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.xperia_play_detected);
			alert.setMessage(message);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
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
		}
		else if (Build.ID.equals("JSS15J"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(ctx);
			alert.setTitle(R.string.nexus_7_2013_detected);
			alert.setMessage(message);
			alert.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					SharedPreferences prefs = UserPreferences.getPreferences(ctx);
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

		if (show_dialog)
		{
			Toast.makeText(ctx, R.string.no_optimal_settings, Toast.LENGTH_SHORT).show();
		}

		return retval;
	}

	@Override
	public boolean onPreferenceClick(Preference preference)
	{
		final String prefKey = preference.getKey();

		// Load Core Preference
		if (prefKey.equals("loadCorePref"))
		{
			final CoreSelection coreSelection = new CoreSelection();
			coreSelection.show(getFragmentManager(), "core_selection");
		}
		// Load ROM Preference
		else if (prefKey.equals("loadRomPref"))
		{
			final Context ctx = getActivity();
			final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);
			final String libretro_path = prefs.getString("libretro_path", ctx.getApplicationInfo().dataDir + "/cores");
			
			if (!new File(libretro_path).isDirectory())
			{
				final DirectoryFragment romBrowser = DirectoryFragment.newInstance(R.string.load_game);
				romBrowser.addDisallowedExts(".state", ".srm", ".state.auto", ".rtc");
				romBrowser.setOnDirectoryFragmentClosedListener(this);
	
				final String startPath = prefs.getString("rgui_browser_directory", "");
				if (!startPath.isEmpty() && new File(startPath).exists())
					romBrowser.setStartDirectory(startPath);
	
				romBrowser.show(getFragmentManager(), "romBrowser");
			}
			else
			{
				Toast.makeText(ctx, R.string.load_a_core_first, Toast.LENGTH_SHORT).show();
			}
		}
		// Load ROM (History) Preference
		else if (prefKey.equals("loadRomHistoryPref"))
		{
			final HistorySelection historySelection = new HistorySelection();
			historySelection.show(getFragmentManager(), "history_selection");
		}

		return true;
	}

	@Override
	public void onDirectoryFragmentClosed(String path)
	{
		final Context ctx = getActivity();
		final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);
		final String libretro_path = prefs.getString("libretro_path", "");

		UserPreferences.updateConfigFile(ctx);
		String current_ime = Settings.Secure.getString(ctx.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
		Toast.makeText(ctx, String.format(getString(R.string.loading_data), path), Toast.LENGTH_SHORT).show();
		Intent myIntent = new Intent(ctx, RetroActivity.class);
		myIntent.putExtra("ROM", path);
		myIntent.putExtra("LIBRETRO", libretro_path);
		myIntent.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(ctx));
		myIntent.putExtra("IME", current_ime);
		startActivity(myIntent);
	}
}
