package com.retroarch.browser.retroactivity;

import com.retroarch.browser.mainmenu.MainMenuActivity;

import android.content.Intent;
import android.util.Log;

public class RetroActivityIntent extends RetroActivityCommon {
	private Intent pendingIntent = null;
	private static final String TAG = "RetroArch";
	
	@Override
	public void onBackPressed()
	{
		Log.i("RetroActivity", "onBackKeyPressed");
		Intent retro = new Intent(this, MainMenuActivity.class);
		retro.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
		startActivity(retro);
	}
	
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

	public String getPendingIntentStorageLocation()
	{
		return pendingIntent.getStringExtra("SDCARD");
	}
	
	public String getPendingIntentDownloadLocation()
	{
		return pendingIntent.getStringExtra("DOWNLOADS");
	}

	public String getPendingIntentScreenshotsLocation()
	{
		return pendingIntent.getStringExtra("SCREENSHOTS");
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
}
