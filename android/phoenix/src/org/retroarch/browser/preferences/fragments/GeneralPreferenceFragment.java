package org.retroarch.browser.preferences.fragments;

import org.retroarch.R;
import org.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import org.retroarch.browser.preferences.util.UserPreferences;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;

/**
 * A {@link PreferenceListFragment} that handles the general settings.
 */
public final class GeneralPreferenceFragment extends PreferenceListFragment
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Add general preferences from the XML.
		addPreferencesFromResource(R.xml.general_preferences);

		// Set a listener for the global configuration checkbox.
		final CheckBoxPreference usingGlobalConfig = (CheckBoxPreference) findPreference("global_config_enable");
		usingGlobalConfig.setOnPreferenceClickListener(new OnPreferenceClickListener(){
			@Override
			public boolean onPreferenceClick(Preference preference)
			{
				UserPreferences.updateConfigFile(getActivity());
				UserPreferences.readbackConfigFile(getActivity());
				return true;
			}
		});
	}
}
