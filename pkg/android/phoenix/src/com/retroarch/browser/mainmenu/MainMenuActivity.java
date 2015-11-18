package com.retroarch.browser.mainmenu;

import java.io.File;

import com.retroarch.R;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
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

		// Ensure resource directories are created.
		final ApplicationInfo info = getApplicationInfo();
		final File coresDir = new File(info.dataDir, "cores");
		final File infoDir = new File(info.dataDir, "info");
		if (!coresDir.exists())
			coresDir.mkdir();
		if (!infoDir.exists())
			infoDir.mkdir();

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
