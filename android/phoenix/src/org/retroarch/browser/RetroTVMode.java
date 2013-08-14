package org.retroarch.browser;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

public class RetroTVMode extends Activity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		Intent myIntent = new Intent(this, RetroActivity.class);
		startActivity(myIntent);
		finish();
	}
}


