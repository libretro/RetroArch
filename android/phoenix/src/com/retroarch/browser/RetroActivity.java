package com.retroarch.browser;

import android.app.NativeActivity;
import android.os.Bundle;

public final class RetroActivity extends NativeActivity {

	@Override
	public void onCreate(Bundle savedInstance) {
		super.onCreate(savedInstance);
	}

	@Override
	public void onLowMemory() {
	}

	@Override
	public void onTrimMemory(int level) {
	}
}
