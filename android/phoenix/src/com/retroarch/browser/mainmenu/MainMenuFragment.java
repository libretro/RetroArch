package com.retroarch.browser.mainmenu;

import android.os.Bundle;

import com.retroarch.R;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

/**
 * Fragment that represents the main menu.
 */
public class MainMenuFragment extends PreferenceListFragment
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Load the XML for the main menu.
		this.addPreferencesFromResource(R.xml.main_menu);
	}
}
