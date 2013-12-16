package com.retroarch.browser.retroactivity;

import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.content.Intent;
import android.util.Log;

/**
 * Class which provides common methods for RetroActivity related classes.
 */
public class RetroActivityCommon extends RetroActivityLocation
 {
	private Intent pendingIntent = null;
	
	@Override
	public void onNewIntent(Intent intent)
	{
		Log.i("RetroActivity", "onNewIntent invoked.");
		super.onNewIntent(intent);
		setIntent(intent);
		pendingIntent = intent;
	}

	/**
	 * Gets the ROM file specified in the pending intent.
	 * 
	 * @return the ROM file specified in the pending intent.
	 */
	public String getPendingIntentFullPath()
	{
		return pendingIntent.getStringExtra("ROM");
	}

	/**
	 * Gets the specified path to the libretro core in the pending intent.
	 * 
	 * @return the specified path to the libretro core in the pending intent.
	 */
	public String getPendingIntentLibretroPath()
	{
		return pendingIntent.getStringExtra("LIBRETRO");
	}

	/**
	 * Gets the path specified in the pending intent to the retroarch cfg file.
	 * 
	 * @return the path specified in the pending intent to the retroarch cfg file.
	 */
	public String getPendingIntentConfigPath()
	{
		return pendingIntent.getStringExtra("CONFIGFILE");
	}

	/**
	 * Gets the specified IME in the pending intent.
	 * 
	 * @return the specified IME in the pending intent.
	 */
	public String getPendingIntentIME()
	{
		return pendingIntent.getStringExtra("IME");
	}

	/**
	 * Checks whether or not a pending intent exists.
	 * 
	 * @return true if a pending intent exists, false otherwise.
	 */
	public boolean hasPendingIntent()
	{
		if (pendingIntent == null)
			return false;

		return true;
	}

	/**
	 * Clears the current pending intent.
	 */
	public void clearPendingIntent()
	{
		pendingIntent = null;
	}


	/*
	 * MISC
	 * Other RetroArch functions
	 */

	@Override
	public void onDestroy()
	{
		UserPreferences.readbackConfigFile(this);
	}

	@Override
	public void onLowMemory()
	{
	}

	@Override
	public void onTrimMemory(int level)
	{
	}

	@Override
	public void onBackPressed()
	{
		Log.i("RetroActivity", "onBackKeyPressed");
		Intent retro = new Intent(this, MainMenuActivity.class);
		retro.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
		startActivity(retro);
	}

	/**
	 * Gets the current Android version being used on a device.
	 * 
	 * @return the current Android version.
	 */
	public int getAndroidOSVersion()
	{
		return android.os.Build.VERSION.SDK_INT;
	}
}
