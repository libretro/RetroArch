package com.retroarch.browser.retroactivity;

import android.view.View;
import android.view.WindowManager;
import android.content.Intent;

public final class RetroActivityFuture extends RetroActivityCamera {

	@Override
	public void onResume() {
		super.onResume();

		if (android.os.Build.VERSION.SDK_INT >= 19) {
			// Immersive mode

			// Constants from API > 14
			final int API_SYSTEM_UI_FLAG_LAYOUT_STABLE = 0x00000100;
			final int API_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 0x00000200;
			final int API_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 0x00000400;
			final int API_SYSTEM_UI_FLAG_FULLSCREEN = 0x00000004;
			final int API_SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 0x00001000;

			View thisView = getWindow().getDecorView();
			thisView.setSystemUiVisibility(API_SYSTEM_UI_FLAG_LAYOUT_STABLE
					| API_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
					| API_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
					| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
					| API_SYSTEM_UI_FLAG_FULLSCREEN
					| API_SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

			// Check for REFRESH parameter
			Intent retro = getIntent();
			String refresh = retro.getStringExtra("REFRESH");

			// If REFRESH parameter is provided then try to set refreshrate accordingly
			if(refresh != null) {
				WindowManager.LayoutParams params = getWindow().getAttributes();
				params.preferredRefreshRate = Integer.parseInt(refresh);
				getWindow().setAttributes(params);
			}
		}
	}

}
