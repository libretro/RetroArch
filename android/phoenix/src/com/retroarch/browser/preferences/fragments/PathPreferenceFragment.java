package com.retroarch.browser.preferences.fragments;

import com.retroarch.R;
import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;

/**
 * A {@link PreferenceListFragment} that handles the path preferences.
 */
public final class PathPreferenceFragment extends PreferenceListFragment implements OnPreferenceClickListener
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Add path preferences from the XML.
		addPreferencesFromResource(R.xml.path_preferences);

		// Set preference click listeners
		findPreference("romDirPref").setOnPreferenceClickListener(this);
		findPreference("srmDirPref").setOnPreferenceClickListener(this);
		findPreference("saveStateDirPref").setOnPreferenceClickListener(this);
		findPreference("systemDirPref").setOnPreferenceClickListener(this);
	}

	@Override
	public boolean onPreferenceClick(Preference preference)
	{
		final String prefKey = preference.getKey();

		// Custom ROM directory
		if (prefKey.equals("romDirPref"))
		{
			final DirectoryFragment romDirBrowser = DirectoryFragment.newInstance(R.string.rom_directory_select);
			romDirBrowser.setPathSettingKey("rgui_browser_directory");
			romDirBrowser.setIsDirectoryTarget(true);
			romDirBrowser.show(getFragmentManager(), "romDirBrowser");
		}

		return true;
	}
}
