package org.retroarch.browser;

import org.retroarch.R;

import android.app.Activity;
import android.media.AudioManager;
import android.os.Bundle;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;

public class SettingsActivity extends Activity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		getFragmentManager().beginTransaction().
			replace(android.R.id.content, new SettingsFragment()).commit();
		PreferenceManager.setDefaultValues(this, R.xml.prefs, false);
		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);
	}
	
	public static class SettingsFragment extends PreferenceFragment {
		
	    @Override
	    public void onCreate(Bundle savedInstanceState) {
	        super.onCreate(savedInstanceState);

	        // Load the preferences from an XML resource
	        addPreferencesFromResource(R.xml.prefs);
	    }
	}
}
