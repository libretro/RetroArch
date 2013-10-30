package com.retroarch.browser.preferences.fragments;

import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.view.ViewPager;
import android.support.v7.app.ActionBar;
import android.support.v7.app.ActionBar.Tab;
import android.support.v7.app.ActionBar.TabListener;
import android.support.v7.app.ActionBarActivity;

import com.retroarch.R;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarch.browser.preferences.util.UserPreferences;

/**
 * {@link ActionBarActivity} responsible for handling all of the {@link PreferenceListFragment}s.
 * <p>
 * This class can be considered the central activity for the settings, as this class
 * provides the backbone for the {@link ViewPager} that handles all of the fragments being used.
 */
public final class PreferenceActivity extends ActionBarActivity implements TabListener, OnSharedPreferenceChangeListener
{
	// ViewPager for the fragments.
	private ViewPager viewPager;

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Set the ViewPager.
		setContentView(R.layout.preference_viewpager);
		viewPager = (ViewPager) findViewById(R.id.viewPager);

		// Initialize the ViewPager adapter.
		final PreferencesAdapter adapter = new PreferencesAdapter(getSupportFragmentManager());
		viewPager.setAdapter(adapter);

		// Register the preference change listener.
		final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(this);
		sPrefs.registerOnSharedPreferenceChangeListener(this);

		// Initialize the ActionBar.
		final ActionBar actionBar = getSupportActionBar();
		actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);
		actionBar.addTab(actionBar.newTab().setText(R.string.general_options).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.audio_options).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.input_options).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.video_options).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.path_options).setTabListener(this));

		// When swiping between different sections, select the corresponding
		// tab. We can also use ActionBar.Tab#select() to do this if we have
		// a reference to the Tab.
		viewPager.setOnPageChangeListener(new ViewPager.SimpleOnPageChangeListener()
		{
			@Override
			public void onPageSelected(int position)
			{
				actionBar.setSelectedNavigationItem(position);
			}
		} );
	}

	@Override
	public void onTabSelected(Tab tab, FragmentTransaction ft)
	{
		// Switch to the fragment indicated by the tab's position.
		viewPager.setCurrentItem(tab.getPosition());
	}

	@Override
	public void onTabUnselected(Tab tab, FragmentTransaction ft)
	{
		// Do nothing.
	}

	@Override
	public void onTabReselected(Tab tab, FragmentTransaction ft)
	{
		// Do nothing
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
			switch (position)
			{
				case 0:
					return getString(R.string.general_options);

				case 1:
					return getString(R.string.audio_options);

				case 2:
					return getString(R.string.input_options);

				case 3:
					return getString(R.string.video_options);

				case 4:
					return getString(R.string.path_options);

				default: // Should never happen
					return null;
			}
		}

		@Override
		public int getCount()
		{
			return 5;
		}
	}
}
