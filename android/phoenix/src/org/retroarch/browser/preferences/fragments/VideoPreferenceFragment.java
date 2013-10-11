package org.retroarch.browser.preferences.fragments;

import org.retroarch.R;
import org.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.os.Bundle;

/**
 * A {@link PreferenceListFragment} responsible for handling the video preferences.
 */
public final class VideoPreferenceFragment extends PreferenceListFragment
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Add preferences from the resources
		addPreferencesFromResource(R.xml.video_preferences);
	}
}
