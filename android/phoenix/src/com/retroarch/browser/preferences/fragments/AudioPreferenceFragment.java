package com.retroarch.browser.preferences.fragments;

import com.retroarch.R;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.os.Build;
import android.os.Bundle;
import android.preference.CheckBoxPreference;

/**
 * A {@link PreferenceListFragment} responsible for handling the audio preferences.
 */
public final class AudioPreferenceFragment extends PreferenceListFragment
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		
		// Add audio preferences from the XML.
		addPreferencesFromResource(R.xml.audio_preferences);
	}
}
