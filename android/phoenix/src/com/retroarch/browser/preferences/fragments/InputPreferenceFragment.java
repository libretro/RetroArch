package com.retroarch.browser.preferences.fragments;

import java.io.File;

import com.retroarch.R;
import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.app.AlertDialog;
import android.content.Context;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.provider.Settings;
import android.view.inputmethod.InputMethodManager;

/**
 * A {@link PreferenceListFragment} responsible for handling the input preferences.
 */
public final class InputPreferenceFragment extends PreferenceListFragment implements OnPreferenceClickListener
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		
		// Add input preferences from the XML.
		addPreferencesFromResource(R.xml.input_preferences);

		// Set preference listeners
		findPreference("set_ime_pref").setOnPreferenceClickListener(this);
		findPreference("report_ime_pref").setOnPreferenceClickListener(this);
		findPreference("inputOverlayDirPref").setOnPreferenceClickListener(this);
	}

	@Override
	public boolean onPreferenceClick(Preference preference)
	{
		final String prefKey = preference.getKey();

		// Set Input Method preference
		if (prefKey.equals("set_ime_pref"))
		{
			// Show an IME picker so the user can change their set IME.
			InputMethodManager imm = (InputMethodManager) getActivity().getSystemService(Context.INPUT_METHOD_SERVICE);
			imm.showInputMethodPicker();
		}
		// Report IME preference
		else if (prefKey.equals("report_ime_pref"))
		{
			final String currentIme = Settings.Secure.getString(getActivity().getContentResolver(),
					Settings.Secure.DEFAULT_INPUT_METHOD);
			
			AlertDialog.Builder reportImeDialog = new AlertDialog.Builder(getActivity());
			reportImeDialog.setTitle(R.string.current_ime);
			reportImeDialog.setMessage(currentIme);
			reportImeDialog.setNegativeButton(R.string.close, null);
			reportImeDialog.show();
		}
		// Input Overlay selection
		else if (prefKey.equals("inputOverlayDirPref"))
		{
			final DirectoryFragment overlayBrowser = DirectoryFragment.newInstance(R.string.input_overlay_select);
			File overlayDir = new File(getActivity().getApplicationInfo().dataDir, "overlays");
			if (overlayDir.exists())
				overlayBrowser.setStartDirectory(overlayDir.getAbsolutePath());
			
			overlayBrowser.addAllowedExts(".cfg");
			overlayBrowser.setPathSettingKey("input_overlay");
			overlayBrowser.show(getFragmentManager(), "overlayBrowser");
		}

		return true;
	}
}
