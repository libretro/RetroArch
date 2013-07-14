package org.andretro;

import org.andretro.input.view.*;
import org.andretro.system.*;

import android.annotation.*;
import android.app.*;
import android.view.*;

abstract class WindowManager
{
	protected RetroDisplay activity;
	protected boolean onScreenInput = true;
	private boolean windowCreated;
	
	public void setOnScreenInput(boolean aState)
	{
		if(onScreenInput != aState)
		{
			onScreenInput = aState;
			
			if(windowCreated)
			{
				setupDisplay();
			}
		}
	}

	public boolean getOnScreenInput()
	{
		return onScreenInput;
	}
	
	public void setActivity(RetroDisplay aActivity)
	{
		activity = aActivity;
	}
	
	
	/**
	 * Called to set any paramters that need to be set before the window is created.
	 */
	public void windowCreating() { }
	public void windowCreated()
	{
		windowCreated = true;
	}
	
	public void screenTouched(MotionEvent aEvent) {	}	
	
	public void setupDisplay()
	{
		InputGroup inputBase = (InputGroup)activity.findViewById(R.id.base);
    	inputBase.removeChildren();
		
    	if(onScreenInput)
    	{
			inputBase.loadInputLayout(activity, activity.moduleInfo, activity.getResources().openRawResource(R.raw.default_retro_pad));
    	}
    	
    	Input.setOnScreenInput(onScreenInput ? inputBase : null);
	}
	
	static public WindowManager createBest()
	{
		if(android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.JELLY_BEAN)
		{
			return new WindowManager_JellyBean();
		}
		else if(android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.ICE_CREAM_SANDWICH)
		{
			return new WindowManager_IceCreamSandwich();
		}
		else
		{
			return new WindowManager_GingerBread();
		}
	}
}

class WindowManager_GingerBread extends WindowManager
{
	@Override public void windowCreating()
	{
		super.windowCreating();
		activity.requestWindowFeature(Window.FEATURE_NO_TITLE);
	}
}

@TargetApi(11) class WindowManager_ICSBase extends WindowManager
{
	protected boolean uiFullscreen = false;
	protected int uiFlags = View.SYSTEM_UI_FLAG_LOW_PROFILE;

	@Override public void windowCreating()
	{
		super.windowCreating();
		
		activity.requestWindowFeature(Window.FEATURE_ACTION_BAR_OVERLAY);
	}
	
	@Override public void setupDisplay()
	{
		super.setupDisplay();
		
		activity.view.setSystemUiVisibility(uiFlags);
	}
	
	protected void setActionBarState()
	{
    	final ActionBar bar = activity.getActionBar();
    	
        if(null != bar && !uiFullscreen)
        {
        	bar.show();
        }
        else if(null != bar && uiFullscreen)
        {
        	bar.hide();
        }		
	}
	
	protected boolean touchedOverActionBar(float aY)
	{
    	final float barSize = 82.0f;
    	return aY <= barSize;
	}
	
	protected boolean toggleFullscreenOverActionBar(float aY)
	{
    	final ActionBar bar = activity.getActionBar();
    	
    	if(bar != null)
    	{
    		final boolean top = touchedOverActionBar(aY);
    		
    		if(top == uiFullscreen)
    		{
    			uiFullscreen = !uiFullscreen;
    			return true;
    		}
    	}

    	return false;
	}
}

@TargetApi(11) class WindowManager_IceCreamSandwich extends WindowManager_ICSBase
{
	@Override public void setupDisplay()
	{
		super.setupDisplay();		
		
		super.setActionBarState();
	}	
	
	@Override public void screenTouched(MotionEvent aEvent)
	{
		super.screenTouched(aEvent);

		if(toggleFullscreenOverActionBar(aEvent.getY()))
		{
			setupDisplay();
		}
	}
}

@TargetApi(11) class WindowManager_JellyBean extends WindowManager_ICSBase
{
	@Override public void windowCreated()
	{
		super.windowCreated();
		
		activity.view.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener()
		{	
			@Override public void onSystemUiVisibilityChange(int aVisibility)
			{
				if(!onScreenInput)
				{
					uiFullscreen = (aVisibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) != 0;
				}
			}
		});
	}
		
	@Override public void setupDisplay()
	{
		uiFlags = View.SYSTEM_UI_FLAG_LOW_PROFILE;		
		
		if(!onScreenInput)
		{
			uiFlags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
			
			if(uiFullscreen)
			{
				uiFlags |= View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION; 
			}
		}
		else
		{
			super.setActionBarState();
		}
				
		super.setupDisplay();
	}
	
	@Override public void screenTouched(MotionEvent aEvent)
	{
		super.screenTouched(aEvent);
		
		if(!onScreenInput)
		{
			if(!touchedOverActionBar(aEvent.getY()))
			{
				uiFullscreen = true;
				setupDisplay();
			}
		}
		else if(toggleFullscreenOverActionBar(aEvent.getY()))
		{
			setupDisplay();
		}
	}
}
