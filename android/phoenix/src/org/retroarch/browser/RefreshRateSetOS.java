package org.retroarch.browser;

import org.retroarch.R;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.Display;
import android.view.WindowManager;
import android.widget.Toast;

public final class RefreshRateSetOS extends Activity {

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		final WindowManager wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
		final Display display = wm.getDefaultDisplay();
		double rate = display.getRefreshRate();
		SharedPreferences prefs = MainMenuActivity.getPreferences();
		SharedPreferences.Editor edit = prefs.edit();
		edit.putString("video_refresh_rate", Double.valueOf(rate).toString());
		edit.commit();
		
		Toast.makeText(this, String.format(getString(R.string.using_os_reported_refresh_rate), rate), Toast.LENGTH_LONG).show();
		finish();
	}
}
