package com.retroarch.browser.mainmenu;

import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivityPast;

import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.provider.Settings;

import java.util.List;
import java.util.ArrayList;
import android.content.pm.PackageManager;
import android.Manifest;
import android.content.DialogInterface;
import android.app.AlertDialog;
import android.util.Log;

/**
 * {@link PreferenceActivity} subclass that provides all of the
 * functionality of the main menu screen.
 */
public final class MainMenuActivity extends PreferenceActivity
{
	final private int REQUEST_CODE_ASK_MULTIPLE_PERMISSIONS = 124;
	public static String PACKAGE_NAME;
	boolean checkPermissions = false;

	public void showMessageOKCancel(String message, DialogInterface.OnClickListener onClickListener)
	{
		new AlertDialog.Builder(this).setMessage(message)
			.setPositiveButton("OK", onClickListener).setCancelable(false)
			.setNegativeButton("Cancel", null).create().show();
	}

	private boolean addPermission(List<String> permissionsList, String permission)
	{
		if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED)
		{
			permissionsList.add(permission);

			// Check for Rationale Option
			if (!shouldShowRequestPermissionRationale(permission))
				return false;
		}

		return true;
	}

	public void checkRuntimePermissions()
	{
		if (android.os.Build.VERSION.SDK_INT >= 23)
		{
			// Android 6.0+ needs runtime permission checks
			List<String> permissionsNeeded = new ArrayList<String>();
			final List<String> permissionsList = new ArrayList<String>();

			if (!addPermission(permissionsList, Manifest.permission.READ_EXTERNAL_STORAGE))
				permissionsNeeded.add("Read External Storage");
			if (!addPermission(permissionsList, Manifest.permission.WRITE_EXTERNAL_STORAGE))
				permissionsNeeded.add("Write External Storage");

			if (permissionsList.size() > 0)
			{
				checkPermissions = true;

				if (permissionsNeeded.size() > 0)
				{
					// Need Rationale
					Log.i("MainMenuActivity", "Need to request external storage permissions.");

					String message = "You need to grant access to " + permissionsNeeded.get(0);

					for (int i = 1; i < permissionsNeeded.size(); i++)
						message = message + ", " + permissionsNeeded.get(i);

					showMessageOKCancel(message,
						new DialogInterface.OnClickListener()
						{
							@Override
							public void onClick(DialogInterface dialog, int which)
							{
								if (which == AlertDialog.BUTTON_POSITIVE)
								{
									requestPermissions(permissionsList.toArray(new String[permissionsList.size()]),
										REQUEST_CODE_ASK_MULTIPLE_PERMISSIONS);

									Log.i("MainMenuActivity", "User accepted request for external storage permissions.");
								}
							}
						});
				}
				else
				{
					requestPermissions(permissionsList.toArray(new String[permissionsList.size()]),
						REQUEST_CODE_ASK_MULTIPLE_PERMISSIONS);

					Log.i("MainMenuActivity", "Requested external storage permissions.");
				}
			}
		}

		if (!checkPermissions)
		{
			finalStartup();
		}
	}

	public void finalStartup()
	{
		final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		Intent retro = new Intent(this, RetroActivityPast.class);

		retro.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);

		startRetroActivity(
				retro,
				null,
				prefs.getString("libretro_path", getApplicationInfo().dataDir + "/cores/"),
				UserPreferences.getDefaultConfigPath(this),
				Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD),
				getApplicationInfo().dataDir,
				getApplicationInfo().sourceDir);
		startActivity(retro);
		finish();
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
	{
		switch (requestCode)
		{
			case REQUEST_CODE_ASK_MULTIPLE_PERMISSIONS:
				for (int i = 0; i < permissions.length; i++)
				{
					if(grantResults[i] == PackageManager.PERMISSION_GRANTED)
					{
						Log.i("MainMenuActivity", "Permission: " + permissions[i] + " was granted.");
					}
					else
					{
						Log.i("MainMenuActivity", "Permission: " + permissions[i] + " was not granted.");
					}
				}

				break;
			default:
				super.onRequestPermissionsResult(requestCode, permissions, grantResults);
				break;
		}

		finalStartup();
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
		String external = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Android/data/" + PACKAGE_NAME + "/files";
		retro.putExtra("EXTERNAL", external);
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		PACKAGE_NAME = getPackageName();

		// Bind audio stream to hardware controls.
		setVolumeControlStream(AudioManager.STREAM_MUSIC);

		UserPreferences.updateConfigFile(this);

		checkRuntimePermissions();
	}
}
