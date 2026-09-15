package com.retroarch.browser.preferences.util;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

/**
 * Utility class for retrieving the application preferences.
 */
public final class UserPreferences
{
	// Disallow explicit instantiation.
	private UserPreferences()
	{
	}

	/**
	 * Retrieves the {@link SharedPreferences} instance that contains
	 * the default application preferences.
	 *
	 * @param ctx the current {@link Context}.
	 *
	 * @return the default {@link SharedPreferences} instance.
	 */
	public static SharedPreferences getPreferences(Context ctx)
	{
		return PreferenceManager.getDefaultSharedPreferences(ctx);
	}
}
