package org.retroarch.browser;

import android.content.Context;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.retroarch.R;

class KeyBindPreference extends DialogPreference implements View.OnKeyListener {
	private int key_bind_code;
	TextView keyText;
	public KeyBindPreference(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	@Override
	protected void onBindDialogView(View view) {
	    super.onBindDialogView(view);
	}
	
	@Override
	protected View onCreateDialogView()
	{
		LayoutInflater inflater = (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
		View view = inflater.inflate(R.layout.key_bind_dialog, null);
		keyText = (TextView) view.findViewById(R.id.key_bind_value);
		view.setOnKeyListener(this);
		return view;
	}

	@Override
	protected void onDialogClosed(boolean positiveResult) {
	   super.onDialogClosed(positiveResult);
	
	    if (positiveResult) {
	        // deal with persisting your values here
	    }
	}
	
	@Override
	public boolean onKey(View v, int keyCode, KeyEvent event) {
		Log.i("RetroArch", "Key event!");
		if (event.getAction() == KeyEvent.ACTION_DOWN)
		{
			key_bind_code = keyCode;
			keyText.setText(String.valueOf(key_bind_code));
		}
		return false;
	}
}