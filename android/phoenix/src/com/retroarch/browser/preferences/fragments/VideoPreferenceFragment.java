package com.retroarch.browser.preferences.fragments;

import com.retroarch.R;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceManager;
import android.preference.Preference.OnPreferenceClickListener;
import android.view.Display;
import android.view.WindowManager;
import android.widget.Toast;

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

		// Set OS-reported refresh rate preference.
		final Preference osReportedRatePref = findPreference("set_os_reported_ref_rate_pref");
		osReportedRatePref.setOnPreferenceClickListener(new OnPreferenceClickListener()
		{
			@Override
			public boolean onPreferenceClick(Preference preference)
			{
				final WindowManager wm = getActivity().getWindowManager();
				final Display display = wm.getDefaultDisplay();
				final double rate = display.getRefreshRate();

				final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
				final SharedPreferences.Editor edit = prefs.edit();
				edit.putString("video_refresh_rate", Double.toString(rate));
				edit.commit();

				Toast.makeText(getActivity(), String.format(getString(R.string.using_os_reported_refresh_rate), rate), Toast.LENGTH_LONG).show();
				return true;
			}
		});
	}
}
