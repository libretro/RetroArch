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
import android.app.Activity;
import android.content.pm.PackageManager;
import android.app.AlertDialog.Builder;

/**
 * {@link PreferenceActivity} subclass that provides all of the
 * functionality of the main menu screen.
 */
public final class MainMenuActivity extends PreferenceActivity
{
int MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE = 0; 

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
		
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
		{
		   if (this.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)!= PackageManager.PERMISSION_GRANTED) {

		      // Should we show an explanation?
		      if (this.shouldShowRequestPermissionRationale(Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
		         // Show an explanation to the user *asynchronously* -- don't block
		         // this thread waiting for the user's response! After the user
		         // sees the explanation, try again to request the permission.

		      } else {
		         // No explanation needed, we can request the permission.
		         this.requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
		            MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE);

		         // MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE is an
		         // app-defined int constant. The callback method gets the
		         // result of the request.
	              }
	           }
		}

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
	
	@Override
	public void onRequestPermissionsResult(int requestCode,
	      String permissions[], int[] grantResults) {
	   switch (requestCode) {
	      case MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE: {
	         // If request is cancelled, the result arrays are empty.
            	 if (grantResults.length > 0
	               && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
		    // permission granted
            	 } else {
		    // permission denied
		    AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
		    builder.setMessage("External SDCard acesss will not be available.") //hardcoded because the rest of retroarch is...
		    .setNeutralButton("Ok", new DialogInterface.OnClickListener() {
		       public void onClick(DialogInterface dialog, int id) {
                          // Nothing lul
                       }
		    })
		    builder.create();
		 }
		 return;
	      }
	   }
	}
}
