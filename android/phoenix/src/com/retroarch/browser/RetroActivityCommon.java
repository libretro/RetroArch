package com.retroarch.browser;

import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.content.Intent;
import android.util.Log;

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

	public String getPendingIntentFullPath()
	{
		return pendingIntent.getStringExtra("ROM");
	}

	public String getPendingIntentLibretroPath()
	{
		return pendingIntent.getStringExtra("LIBRETRO");
	}

	public String getPendingIntentConfigPath()
	{
		return pendingIntent.getStringExtra("CONFIGFILE");
	}

	public String getPendingIntentIME()
	{
		return pendingIntent.getStringExtra("IME");
	}

	public boolean hasPendingIntent()
	{
		if (pendingIntent == null)
			return false;
		return true;
	}

	public void clearPendingIntent()
	{
		pendingIntent = null;
	}
	
	
	/* MISC
	 * Other RetroArch functions */
    
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
	
	public int getAndroidOSVersion()
	{
		return android.os.Build.VERSION.SDK_INT;
	}
}
