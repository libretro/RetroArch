package org.andretro.settings;

import org.andretro.*;
import org.andretro.emulator.*;

import android.preference.*;
import android.content.*;
import android.os.*;

import java.io.*;

@SuppressWarnings("deprecation")
public class SettingActivity extends PreferenceActivity
{
	private String moduleName;
	private ModuleInfo moduleInfo; 
	
	@Override public void onCreate(Bundle aState)
	{
		super.onCreate(aState);
		
		moduleName = getIntent().getStringExtra("moduleName");
		moduleInfo = ModuleInfo.getInfoAbout(this, new File(moduleName));
		
		getPreferenceManager().setSharedPreferencesName(moduleInfo.getDataName());
		
		// Add the input subsection
		PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(this);
		PreferenceCategory inputCategory = new PreferenceCategory(this);
		inputCategory.setTitle("Input");
		screen.addPreference(inputCategory);
		
		// Add all pads (TODO: Just port 1 for now)
		PreferenceScreen inputScreen = getPreferenceManager().createPreferenceScreen(this);
		inputScreen.setTitle("Port 1");

		Doodads.Device device = moduleInfo.inputData.getDevice(0,  0);
		for(Doodads.Button i: device.getAll())
		{
			inputScreen.addPreference(new Button(this, i));
		}
		
		inputCategory.addPreference(inputScreen);

		// Add the rest of the system settings
		setPreferenceScreen(screen);
        addPreferencesFromResource(R.xml.preferences);
	}
	
	@Override public void onPause()
	{
		super.onPause();

		if(Game.hasGame())
		{
			Game.queueCommand(new Commands.RefreshSettings(getPreferenceManager().getSharedPreferences()));
		}
	}
	
	private static class Button extends ButtonSetting
	{
		final Doodads.Button button;
		
		public Button(Context aContext, final Doodads.Button aButton)
		{
			super(aContext, aButton.configKey, aButton.fullName, 0);
			
			button = aButton;
		}
				
		@Override protected void valueChanged(int aKeyCode)
		{
			button.setKeyCode(aKeyCode);
		}
	}
}
