package org.andretro.settings;

import org.andretro.*;

import android.annotation.*;
import android.content.*;
import android.os.*;
import android.preference.*;
import android.view.*;
import android.widget.*;
import android.util.*;

public class ButtonSetting extends DialogPreference
{	
	@TargetApi(12) public ButtonSetting(Context aContext, AttributeSet aAttributes)
	{
		super(aContext, aAttributes);
		
		// HACK: Set a layout that forces the dialog to get key focus
		// TODO: Make the layout better looking!
		setDialogLayoutResource(R.layout.dialog_focus_hack);
		
		setDialogTitle("Waiting for input");
		setDialogMessage(getTitle());
	}
	
	@TargetApi(12) public ButtonSetting(Context aContext, String aKey, String aName, int aDefault)
	{
		super(aContext, null);

		setKey(aKey);
		setTitle(aName);
		setPersistent(true);
		setDefaultValue(aDefault);
		
		// HACK: Set a layout that forces the dialog to get key focus
		// TODO: Make the layout better looking!
		setDialogLayoutResource(R.layout.dialog_focus_hack);
		
		setDialogTitle("Waiting for input");
		setDialogMessage(aName);
	}
	
	@Override protected void onAttachedToActivity()
	{
		super.onAttachedToActivity();
		refreshSummary();
	}
	
	@Override protected void showDialog(Bundle aState)
	{
		super.showDialog(aState);
	
		// HACK: Set the message in the hacked layout
		((EditText)getDialog().findViewById(R.id.hack_message)).setText(getDialogMessage());
		
		getDialog().setOnKeyListener(new DialogInterface.OnKeyListener()
		{	
			@Override public boolean onKey(DialogInterface aDialog, int aKeyCode, KeyEvent aEvent)
			{
				persistInt(aKeyCode);
				valueChanged(aKeyCode);
				refreshSummary();					
				getDialog().dismiss();
				return false;
			}
		});
	}
	
	@TargetApi(12) private void refreshSummary()
	{
        if(android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.HONEYCOMB_MR1)
        {
        	setSummary(KeyEvent.keyCodeToString(getPersistedInt(0)));
        }
        else
        {
        	setSummary(Integer.toString(getPersistedInt(0)));
        }			
	}
	
	protected void valueChanged(int aKeyCode)
	{
		
	}
}