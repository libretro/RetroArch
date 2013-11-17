package com.retroarch.browser.mainmenu;

import com.retroarch.R;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.content.SharedPreferences;
import android.media.AudioManager;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentTransaction;

/**
 * {@link PreferenceActivity} subclass that provides all of the
 * functionality of the main menu screen.
 */
public final class MainMenuActivity extends FragmentActivity
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Load the main menu layout
		setContentView(R.layout.mainmenu_activity_layout);
		if (savedInstanceState == null)
		{
			final MainMenuFragment mmf = new MainMenuFragment();
			final FragmentTransaction ft = getSupportFragmentManager().beginTransaction();

			// Add the base main menu fragment to the content view.
			ft.replace(R.id.content_frame, mmf);
			ft.commit();
		}

		// Bind audio stream to hardware controls.
		setVolumeControlStream(AudioManager.STREAM_MUSIC);
	}

	public void setModule(String core_path, String core_name)
	{
		UserPreferences.updateConfigFile(this);

		SharedPreferences prefs = UserPreferences.getPreferences(this);
		SharedPreferences.Editor edit = prefs.edit();
		edit.putString("libretro_path", core_path);
		edit.putString("libretro_name", core_name);
		edit.commit();

		// Set the title section to contain the name of the selected core.
		setCoreTitle(core_name);
	}

	public void setCoreTitle(String core_name)
	{
		setTitle("RetroArch : " + core_name);
	}

	@Override
	protected void onSaveInstanceState(Bundle data)
	{
		super.onSaveInstanceState(data);

		data.putCharSequence("title", getTitle());
	}

	@Override
	protected void onRestoreInstanceState(Bundle savedInstanceState)
	{
		super.onRestoreInstanceState(savedInstanceState);

		setTitle(savedInstanceState.getCharSequence("title"));
	}
}