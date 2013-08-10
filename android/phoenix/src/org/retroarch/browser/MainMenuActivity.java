package org.retroarch.browser;

import org.retroarch.R;

import android.media.AudioManager;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;

public class MainMenuActivity extends PreferenceActivity {
	@SuppressWarnings("deprecation")
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.xml.prefs);
		PreferenceManager.setDefaultValues(this, R.xml.prefs, false);
		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);
	}
}
