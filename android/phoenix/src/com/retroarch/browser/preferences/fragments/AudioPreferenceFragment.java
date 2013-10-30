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
		
		// Disable automatic detection of optimal audio latency if a device is below Android 4.1
		final CheckBoxPreference autoDetectAudioLatency = (CheckBoxPreference) findPreference("audio_latency_auto");
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN)
		{
			autoDetectAudioLatency.setChecked(false);
			autoDetectAudioLatency.setEnabled(false);
		}
	}
}
