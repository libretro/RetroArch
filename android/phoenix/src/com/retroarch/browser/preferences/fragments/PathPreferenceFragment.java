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
		// Custom savefile directory
		else if (prefKey.equals("srmDirPref"))
		{
			final DirectoryFragment srmDirBrowser = DirectoryFragment.newInstance(R.string.savefile_directory_select);
			srmDirBrowser.setPathSettingKey("savefile_directory");
			srmDirBrowser.setIsDirectoryTarget(true);
			srmDirBrowser.show(getFragmentManager(), "srmDirBrowser");
		}
		// Custom save state directory
		else if (prefKey.equals("saveStateDirPref"))
		{
			final DirectoryFragment saveStateDirBrowser = DirectoryFragment.newInstance(R.string.save_state_directory_select);
			saveStateDirBrowser.setPathSettingKey("savestate_directory");
			saveStateDirBrowser.setIsDirectoryTarget(true);
			saveStateDirBrowser.show(getFragmentManager(), "saveStateDirBrowser");
		}
		// Custom system directory
		else if (prefKey.equals("systemDirPref"))
		{
			final DirectoryFragment systemDirBrowser = DirectoryFragment.newInstance(R.string.system_directory_select);
			systemDirBrowser.setPathSettingKey("system_directory");
			systemDirBrowser.setIsDirectoryTarget(true);
			systemDirBrowser.show(getFragmentManager(), "systemDirBrowser");
		}

		return true;
	}
}
