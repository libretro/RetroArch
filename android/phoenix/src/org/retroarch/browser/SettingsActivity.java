package org.retroarch.browser;

import org.retroarch.R;

import android.app.Activity;
import android.os.Bundle;
import android.preference.PreferenceFragment;

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
		
		setTitle(getIntent().getStringExtra("TITLE"));
		
		getFragmentManager().beginTransaction().
			replace(android.R.id.content, new SettingsFragment()).commit();
	}
}
