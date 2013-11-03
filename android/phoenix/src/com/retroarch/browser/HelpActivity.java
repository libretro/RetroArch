package com.retroarch.browser;

import com.retroarch.R;

import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;

/**
 * Basic {@link PreferenceActivity} responsible for displaying the help articles.
 */
public final class HelpActivity extends PreferenceActivity {
	@SuppressWarnings("deprecation")
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.xml.help);
		PreferenceManager.setDefaultValues(this, R.xml.help, false);
	}
}