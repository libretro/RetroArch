package org.retroarch.browser;

import java.io.File;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

public class RetroTVMode extends Activity {
	private String getDefaultConfigPath() {
		String internal = System.getenv("INTERNAL_STORAGE");
		String external = System.getenv("EXTERNAL_STORAGE");
		
		if (external != null) {
			String confPath = external + File.separator + "retroarch.cfg";
			if (new File(confPath).exists())
				return confPath;
		} else if (internal != null) {
			String confPath = internal + File.separator + "retroarch.cfg";
			if (new File(confPath).exists())
				return confPath;
		} else {
			String confPath = "/mnt/extsd/retroarch.cfg";
			if (new File(confPath).exists())
				return confPath;
		}
		
		if (internal != null && new File(internal + File.separator + "retroarch.cfg").canWrite())
			return internal + File.separator + "retroarch.cfg";
		else if (external != null && new File(internal + File.separator + "retroarch.cfg").canWrite())
			return external + File.separator + "retroarch.cfg";
		else if ((getApplicationInfo().dataDir) != null)
			return (getApplicationInfo().dataDir) + File.separator + "retroarch.cfg";
		else // emergency fallback, all else failed
			return "/mnt/sd/retroarch.cfg";
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		Intent myIntent = new Intent(this, RetroActivity.class);
		String current_ime = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
		myIntent.putExtra("CONFIGFILE", getDefaultConfigPath());
		myIntent.putExtra("IME", current_ime);
		startActivity(myIntent);
		finish();
	}
}


