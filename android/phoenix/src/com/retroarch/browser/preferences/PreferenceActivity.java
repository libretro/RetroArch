package com.retroarch.browser.preferences;

import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.support.v4.view.ViewPager;
import android.support.v7.app.ActionBarActivity;

import com.retroarch.R;
import com.retroarch.browser.preferences.fragments.AudioPreferenceFragment;
import com.retroarch.browser.preferences.fragments.GeneralPreferenceFragment;
import com.retroarch.browser.preferences.fragments.InputPreferenceFragment;
import com.retroarch.browser.preferences.fragments.PathPreferenceFragment;
import com.retroarch.browser.preferences.fragments.VideoPreferenceFragment;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarch.browser.preferences.util.UserPreferences;

/**
 * {@link ActionBarActivity} responsible for handling all of the {@link PreferenceListFragment}s.
 * <p>
 * This class can be considered the central activity for the settings, as this class
 * provides the backbone for the {@link ViewPager} that handles all of the fragments being used.
 */
public final class PreferenceActivity extends ActionBarActivity implements OnSharedPreferenceChangeListener
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Set the ViewPager.
		setContentView(R.layout.preference_viewpager);

		// Initialize the ViewPager.
		final ViewPager viewPager = (ViewPager) findViewById(R.id.viewPager);
		viewPager.setAdapter(new PreferencesAdapter(getSupportFragmentManager()));

		// Register the preference change listener.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(this);
		sPrefs.registerOnSharedPreferenceChangeListener(this);
	}

	@Override
	public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key)
	{
		// Update the config file immediately when a preference has changed.
		UserPreferences.updateConfigFile(this);
	}

	/**
	 * The {@link FragmentPagerAdapter} that will back
	 * the view pager of this {@link PreferenceActivity}.
	 */
	private final class PreferencesAdapter extends FragmentPagerAdapter
	{
		private final String[] pageTitles = {
			getString(R.string.general_options),
			getString(R.string.audio_options),
			getString(R.string.input_options),
			getString(R.string.video_options),
			getString(R.string.path_options)
		};

		/**
		 * Constructor
		 * 
		 * @param fm the {@link FragmentManager} for this adapter.
		 */
		public PreferencesAdapter(FragmentManager fm)
		{
			super(fm);
		}

		@Override
		public Fragment getItem(int fragmentId)
		{
			switch (fragmentId)
			{
				case 0:
					return new GeneralPreferenceFragment();

				case 1:
					return new AudioPreferenceFragment();

				case 2:
					return new InputPreferenceFragment();
					
				case 3:
					return new VideoPreferenceFragment();

				case 4:
					return new PathPreferenceFragment();

				default: // Should never happen
					return null;
			}
		}

		@Override
		public CharSequence getPageTitle(int position)
		{
			return pageTitles[position];
		}

		@Override
		public int getCount()
		{
			return 5;
		}
	}
}
