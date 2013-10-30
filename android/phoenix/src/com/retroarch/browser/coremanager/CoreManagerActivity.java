package com.retroarch.browser.coremanager;

import com.retroarch.R;
import com.retroarch.browser.coremanager.fragments.DownloadableCoresFragment;
import com.retroarch.browser.coremanager.fragments.InstalledCoresFragment;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.view.ViewPager;
import android.support.v7.app.ActionBar;
import android.support.v7.app.ActionBar.Tab;
import android.support.v7.app.ActionBar.TabListener;
import android.support.v7.app.ActionBarActivity;

/**
 * Activity which provides the base for viewing installed cores,
 * as well as the ability to download other cores.
 */
public final class CoreManagerActivity extends ActionBarActivity implements TabListener
{
	// ViewPager for the fragments
	private ViewPager viewPager;

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Set the ViewPager
		setContentView(R.layout.coremanager_viewpager);
		viewPager = (ViewPager) findViewById(R.id.coreviewer_viewPager);

		// Set the ViewPager adapter.
		final ViewPagerAdapter adapter = new ViewPagerAdapter(getSupportFragmentManager());
		viewPager.setAdapter(adapter);

		// Initialize the ActionBar.
		final ActionBar actionBar = getSupportActionBar();
		actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);
		actionBar.addTab(actionBar.newTab().setText(R.string.installed_cores).setTabListener(this));
		actionBar.addTab(actionBar.newTab().setText(R.string.downloadable_cores).setTabListener(this));

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
		});
	}
	
	@Override
	public void onTabSelected(Tab tab, FragmentTransaction ft)
	{
		// Switch to the fragment indicated by the tab's position.
		viewPager.setCurrentItem(tab.getPosition());
	}

	@Override
	public void onTabReselected(Tab tab, FragmentTransaction ft)
	{
		// Do nothing. Not used.
	}

	@Override
	public void onTabUnselected(Tab tab, FragmentTransaction ft)
	{
		// Do nothing. Not used.
	}

	// Adapter for the CoreView ViewPager class.
	private final class ViewPagerAdapter extends FragmentPagerAdapter
	{
		/**
		 * Constructor
		 * 
		 * @param fm The {@link FragmentManager} for this adapter.
		 */
		public ViewPagerAdapter(FragmentManager fm)
		{
			super(fm);
		}

		@Override
		public Fragment getItem(int position)
		{
			switch (position)
			{
				case 0:
					return new InstalledCoresFragment();

				case 1:
					return new DownloadableCoresFragment();

				default: // Should never happen.
					return null;
			}
		}

		@Override
		public int getCount()
		{
			return 2;
		}

		@Override
		public CharSequence getPageTitle(int position)
		{
			switch (position)
			{
				case 0:
					return getString(R.string.installed_cores);

				case 1:
					return getString(R.string.downloadable_cores);

				default: // Should never happen.
					return null;
			}
		}
	}
}
