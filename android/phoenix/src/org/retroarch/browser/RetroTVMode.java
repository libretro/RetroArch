package org.retroarch.browser;

import org.retroarch.browser.preferences.UserPreferences;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

public final class RetroTVMode extends Activity {

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		UserPreferences.updateConfigFile(this);

		Intent myIntent = new Intent(this, RetroActivity.class);
		String current_ime = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
		myIntent.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(this));
		myIntent.putExtra("IME", current_ime);
		startActivity(myIntent);
		finish();
	}
}


