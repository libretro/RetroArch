package com.retroarch.browser.retroactivity;

import com.retroarch.browser.preferences.util.UserPreferences;

/**
 * Class which provides common methods for RetroActivity related classes.
 */
public class RetroActivityCommon extends RetroActivityLocation
{
	// Exiting cleanly from NDK seems to be nearly impossible.
	// Have to use exit(0) to avoid weird things happening, even with runOnUiThread() approaches.
	// Use a separate JNI function to explicitly trigger the readback.
	public void onRetroArchExit()
	{
      finish();
	}
}
