package org.retroarch.browser;

import java.util.prefs.Preferences;

import org.retroarch.R;

import android.app.Activity;
import android.os.Bundle;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;

class SettingsFragment extends PreferenceFragment {
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Load the preferences from an XML resource
        addPreferencesFromResource(R.xml.prefs);
    }
}

public class SettingsActivity extends Activity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		getFragmentManager().beginTransaction().
			replace(android.R.id.content, new SettingsFragment()).commit();
		PreferenceManager.setDefaultValues(this, R.xml.prefs, false);
	}
}
