package com.retroarch.browser;

import java.io.IOException;

import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.annotation.SuppressLint;
import android.app.NativeActivity;
import android.content.Intent;
import android.util.Log;

// For Android 2.3.x

public final class RetroActivityPast extends NativeActivity
{
	private Intent pendingIntent = null;

	public void onCameraStart()
	{
	}

	public void onCameraStop()
	{
	}

	public void onCameraInit()
	{
	}

	public boolean onCameraPoll()
	{
		return false;
	}

	public void onCameraFree()
	{
	}
	
	@SuppressLint("NewApi")
	public void onCameraTextureInit(int gl_texid)
	{
	}

	@SuppressLint("NewApi")
	public void onCameraSetTexture(int gl_texid) throws IOException
	{
	}

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
	
	@Override
	public void onBackPressed()
	{
		Log.i("RetroActivity", "onBackKeyPressed");
		Intent retro = new Intent(this, MainMenuActivity.class);
		retro.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
		startActivity(retro);
	}
}
