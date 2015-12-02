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



