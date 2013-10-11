package org.retroarch.browser.preferences.fragments;

import org.retroarch.R;
import org.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.os.Bundle;

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
	}
}
