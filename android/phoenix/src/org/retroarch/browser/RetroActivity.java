package org.retroarch.browser;

import android.app.NativeActivity;
import android.os.Bundle;
import android.widget.Toast;

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

	// We call this function from native to display a toast string
	public void showToastAlert(String text)
	{
		// We need to use a runnable here to ensure that when the spawned
		// native_app_glue thread calls, we actually post the work to the UI
		// thread.  Otherwise, we'll likely get exceptions because there's no
		// prepared Looper on the native_app_glue main thread.
		final String finalText = text;
		runOnUiThread(new Runnable() {
			public void run()
			{
				Toast.makeText(getApplicationContext(), finalText, Toast.LENGTH_SHORT).show();
			}
		});
	}
}
