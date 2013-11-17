package com.retroarch.browser;

import com.retroarch.browser.preferences.util.UserPreferences;

import android.app.NativeActivity;

public final class RetroActivity extends NativeActivity
{
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
}
