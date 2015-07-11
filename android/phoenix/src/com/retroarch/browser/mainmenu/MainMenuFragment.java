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
import android.os.Environment;
import android.content.Context;

import com.retroarch.R;
import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivityFuture;
import com.retroarch.browser.retroactivity.RetroActivityPast;

/**
 * Represents the fragment that handles the layout of the main menu.
 */
public final class MainMenuFragment extends PreferenceListFragment implements OnPreferenceClickListener
{
	private static final String TAG = "MainMenuFragment";
	private Context ctx;
	
	public Intent getRetroActivity()
	{
		if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB))
		{
			return new Intent(ctx, RetroActivityFuture.class);
		}
		return new Intent(ctx, RetroActivityPast.class);
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Cache the context
		this.ctx = getActivity();

		// Add the layout through the XML.
		addPreferencesFromResource(R.xml.main_menu);

		// Set the listeners for the menu items
		findPreference("resumeContentPref").setOnPreferenceClickListener(this);
		findPreference("quitRetroArch").setOnPreferenceClickListener(this);

		// Extract assets. 
		extractAssets();
	}

	private void extractAssets()
	{
		if (areAssetsExtracted())
			return;

		final Dialog dialog = new Dialog(ctx);
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
			final String dataDir = ctx.getApplicationInfo().dataDir;
			final String apk = ctx.getApplicationInfo().sourceDir;

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
			String dataDir = ctx.getApplicationInfo().dataDir;
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
			version = ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).versionCode;
		}
		catch (NameNotFoundException ignored)
		{
		}

		return version;
	}
	
	@Override
	public boolean onPreferenceClick(Preference preference)
	{
		final String prefKey = preference.getKey();

		// Resume Content
		if (prefKey.equals("resumeContentPref"))
		{
			UserPreferences.updateConfigFile(ctx);
			final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);
			final Intent retro = getRetroActivity();
			
			MainMenuFragment.startRetroActivity(
					retro,
					null,
					prefs.getString("libretro_path", ctx.getApplicationInfo().dataDir + "/cores/"),
					UserPreferences.getDefaultConfigPath(ctx),
					Settings.Secure.getString(ctx.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD),
               ctx.getApplicationInfo().dataDir,
               ctx.getApplicationInfo().sourceDir);
			startActivity(retro);
		}
		// Quit RetroArch preference
		else if (prefKey.equals("quitRetroArch"))
		{
			// TODO - needs to close entire app gracefully - including
			// NativeActivity if possible
			getActivity().finish();
		}

		return true;
	}

	public static void startRetroActivity(Intent retro, String contentPath, String corePath,
			String configFilePath, String imePath, String dataDirPath, String dataSourcePath)
	{
		if (contentPath != null) {
			retro.putExtra("ROM", contentPath);
		}
		retro.putExtra("LIBRETRO", corePath);
		retro.putExtra("CONFIGFILE", configFilePath);
		retro.putExtra("IME", imePath);
		retro.putExtra("DATADIR", dataDirPath);
		retro.putExtra("APK", dataSourcePath);
		retro.putExtra("SDCARD", Environment.getExternalStorageDirectory().getAbsolutePath());
		retro.putExtra("DOWNLOADS", Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).getAbsolutePath());
		retro.putExtra("SCREENSHOTS", Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES).getAbsolutePath());
        String external = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Android/data/com.retroarch/files";
        retro.putExtra("EXTERNAL", external);
	}
}



