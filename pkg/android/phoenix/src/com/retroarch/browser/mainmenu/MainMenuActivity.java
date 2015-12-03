package com.retroarch.browser.mainmenu;

import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivityFuture;
import com.retroarch.browser.retroactivity.RetroActivityPast;

import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.provider.Settings;

/**
 * {@link PreferenceActivity} subclass that provides all of the
 * functionality of the main menu screen.
 */
public final class MainMenuActivity extends PreferenceActivity
{
	public static void startRetroActivity(Intent retro, String contentPath, String corePath,
			String configFilePath, String imePath, String dataDirPath, String dataSourcePath)
	{
		if (contentPath != null) {
			retro.putExtra("ROM", contentPath);
		}
		retro.putExtra("LIBRETRO", corePath);
		retro.putExtra("CONFIGFILE", configFilePath);
		retro.putExtra("IME", imePath);
		retro.putExtra("DATADIR", dataDirPath);
		retro.putExtra("APK", dataSourcePath);
		retro.putExtra("SDCARD", Environment.getExternalStorageDirectory().getAbsolutePath());
		retro.putExtra("DOWNLOADS", Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).getAbsolutePath());
		retro.putExtra("SCREENSHOTS", Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES).getAbsolutePath());
        String external = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Android/data/com.retroarch/files";
        retro.putExtra("EXTERNAL", external);
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Bind audio stream to hardware controls.
		setVolumeControlStream(AudioManager.STREAM_MUSIC);

      UserPreferences.updateConfigFile(this);
      final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
      Intent retro;
      
      if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB))
      {
         retro = new Intent(this, RetroActivityFuture.class);
      }
      else
      {
         retro = new Intent(this, RetroActivityPast.class);
      }
      retro.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);

      startRetroActivity(
            retro,
            null,
            prefs.getString("libretro_path", getApplicationInfo().dataDir + "/cores/"),
            UserPreferences.getDefaultConfigPath(this),
            Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD),
            getApplicationInfo().dataDir,
            getApplicationInfo().sourceDir);
      startActivity(retro);
      finish();
	}
}
