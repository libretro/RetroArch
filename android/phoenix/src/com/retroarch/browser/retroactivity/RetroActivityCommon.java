package com.retroarch.browser.retroactivity;

import com.retroarch.browser.preferences.util.UserPreferences;

/**
 * Class which provides common methods for RetroActivity related classes.
 */
public class RetroActivityCommon extends RetroActivityLocation
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
