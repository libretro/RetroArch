package org.retroarch.browser;

import org.retroarch.R;

import android.app.Activity;
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
