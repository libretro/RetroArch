package org.retroarch.browser;

import org.retroarch.R;
import org.retroarch.browser.preferences.UserPreferences;

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
		
		final WindowManager wm = getWindowManager();
		final Display display = wm.getDefaultDisplay();
		double rate = display.getRefreshRate();
		SharedPreferences prefs = UserPreferences.getPreferences(this);
		SharedPreferences.Editor edit = prefs.edit();
		edit.putString("video_refresh_rate", Double.toString(rate));
		edit.commit();
		
		Toast.makeText(this, String.format(getString(R.string.using_os_reported_refresh_rate), rate), Toast.LENGTH_LONG).show();
		finish();
	}
}
