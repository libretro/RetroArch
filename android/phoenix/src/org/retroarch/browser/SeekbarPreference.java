package org.retroarch.browser;

import org.retroarch.R;

import android.content.Context;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.SeekBar;
import android.widget.TextView;

public final class SeekbarPreference extends DialogPreference implements SeekBar.OnSeekBarChangeListener {
	float seek_value;
	SeekBar bar;
	TextView text;
	public SeekbarPreference(Context context, AttributeSet attrs) {
		super(context, attrs);
	}
	

	@Override
	protected View onCreateDialogView()
	{
		LayoutInflater inflater = ((LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE));
		View view = inflater.inflate(R.layout.seek_dialog, null);
		bar = (SeekBar) view.findViewById(R.id.seekbar_bar);
		text = (TextView) view.findViewById(R.id.seekbar_text);
		seek_value = getPersistedFloat(1.0f);
		int prog = (int) (seek_value * 100);
		bar.setProgress(prog);
		text.setText(prog + "%");
		bar.setOnSeekBarChangeListener(this);
		return view;
	}
	
	@Override
	protected void onDialogClosed(boolean positiveResult) {
	   super.onDialogClosed(positiveResult);
	
	    if (positiveResult) {
	    	persistFloat(seek_value);
	    }
	}

	@Override
	public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
		seek_value = (float) progress / 100.0f;
		text.setText((int) (seek_value * 100) + "%");
	}

	@Override
	public void onStartTrackingTouch(SeekBar seekBar) {
	}

	@Override
	public void onStopTrackingTouch(SeekBar seekBar) {	
	}
}
