package org.retroarch.browser;

import android.app.NativeActivity;
import android.os.Bundle;

public class RetroActivity extends NativeActivity
{
	public RetroActivity()
	{
		super();
	}
	
	@Override
	public void onCreate(Bundle savedInstance)
	{
		super.onCreate(savedInstance);
	}
	
	@Override
	public void onLowMemory()
	{
	}
	
    @Override
    public void onTrimMemory(int level) {
    }
}
