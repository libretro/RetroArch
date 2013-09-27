package org.retroarch.browser;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.provider.Settings;

public final class ReportIME extends Activity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		String current_ime = Settings.Secure.getString(getContentResolver(),
				Settings.Secure.DEFAULT_INPUT_METHOD);

		final Activity ctx = this;
		AlertDialog.Builder dialog = new AlertDialog.Builder(this)
				.setMessage(current_ime)
				.setNeutralButton("Close",
						new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface dialog,
									int which) {
								ctx.finish();
							}
						}).setCancelable(true)
				.setOnCancelListener(new DialogInterface.OnCancelListener() {
					@Override
					public void onCancel(DialogInterface dialog) {
						ctx.finish();
					}
				});

		dialog.show();
	}
}
