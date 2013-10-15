package org.retroarch.browser.preferences.fragments;

import org.retroarch.R;
import org.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.app.AlertDialog;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.provider.Settings;

/**
 * A {@link PreferenceListFragment} responsible for handling the input preferences.
 */
public final class InputPreferenceFragment extends PreferenceListFragment
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Add input preferences from the XML.
		addPreferencesFromResource(R.xml.input_preferences);

		// Report IME preference
		final Preference reportImePref = findPreference("report_ime_pref");
		reportImePref.setOnPreferenceClickListener(new OnPreferenceClickListener()
		{
			@Override
			public boolean onPreferenceClick(Preference preference)
			{
				final String currentIme = Settings.Secure.getString(getActivity().getContentResolver(),
						Settings.Secure.DEFAULT_INPUT_METHOD);
				
				AlertDialog.Builder reportImeDialog = new AlertDialog.Builder(getActivity());
				reportImeDialog.setTitle(R.string.current_ime);
				reportImeDialog.setMessage(currentIme);
				reportImeDialog.setNegativeButton(R.string.close, null);
				reportImeDialog.show();
				return true;
			}
		});
	}
}
