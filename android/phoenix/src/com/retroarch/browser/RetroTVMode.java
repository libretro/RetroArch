package com.retroarch.browser;

import com.retroarch.browser.preferences.util.UserPreferences;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

/**
 * The {@link Activity} derivative responsible for displaying the TV Mode feature.
 */
public final class RetroTVMode extends Activity
{
	// Need to do this wonky logic as we have to keep this activity alive until
	// RetroArch is done running.
	// Have to readback config right after RetroArch has run to avoid potential
	// broken config state.
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		if (savedInstanceState == null || !savedInstanceState.getBoolean("started", false))
		{
			UserPreferences.updateConfigFile(this);

			Intent myIntent = new Intent(this, RetroActivity.class);
			String current_ime = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
			myIntent.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(this));
			myIntent.putExtra("IME", current_ime);
			startActivity(myIntent);
		}
	}

	@Override
	protected void onSaveInstanceState(Bundle savedInstanceState)
	{
		savedInstanceState.putBoolean("started", true);
	}
}
