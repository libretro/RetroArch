package org.retroarch.browser;

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
		
		Toast.makeText(this, "Using OS-reported refresh rate of: " + rate + " Hz.", Toast.LENGTH_LONG).show();
		finish();
	}
}
