package org.andretro;
import org.andretro.browser.*;
import org.andretro.emulator.*;
import org.andretro.settings.*;
import org.andretro.system.*;
import org.andretro.system.video.*;

import java.io.*;

import android.view.*;
import android.view.inputmethod.*;
import android.opengl.*;
import android.os.*;
import android.content.*;
import android.widget.*;
import android.graphics.*;

public class RetroDisplay extends android.support.v4.app.FragmentActivity implements QuestionDialog.QuestionHandler
{
	private static final int CLOSE_GAME_QUESTION = 1;
	
	private WindowManager windowManager;
	GLSurfaceView view;
	private boolean questionOpen;
	private boolean isPaused;
	private boolean showActionBar;
	private String moduleName;
	ModuleInfo moduleInfo;
	
    @Override public void onCreate(Bundle aState)
    {	
        super.onCreate(aState);

        // Setup window
        windowManager = WindowManager.createBest();
        windowManager.setActivity(this);
        windowManager.windowCreating();
        
        // Read state
        questionOpen = (null == aState) ? false : aState.getBoolean("questionOpen", false);
        windowManager.setOnScreenInput((null == aState) ? true : aState.getBoolean("onScreenInput", true));
        isPaused = (null == aState) ? false : aState.getBoolean("isPaused", false);
        showActionBar = (null == aState) ? true : aState.getBoolean("showActionBar", true);
        moduleName = getIntent().getStringExtra("moduleName");
        moduleInfo = ModuleInfo.getInfoAbout(this, new File(moduleName)); 
        
		// Setup the view
        setContentView(R.layout.retro_display);

        view = (GLSurfaceView)findViewById(R.id.renderer);
        view.setEGLContextClientVersion(2);
        view.setRenderer(Present.createRenderer(this));
		view.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
		view.setKeepScreenOn(true);
		
		if(!Game.hasGame())
		{
			Game.queueCommand(new Commands.LoadGame(this, moduleName, new File(getIntent().getStringExtra("path"))));
		}
		
        windowManager.windowCreated();
    }
    
    @Override public void onResume()
    {
    	super.onResume();
		windowManager.setupDisplay();
		view.onResume();
    }
    
    @Override public void onPause()
    {
    	super.onPause();
    	view.onPause();
    }
            
    // QuestionDialog.QuestionHandler	
    @Override public void onAnswer(int aID, QuestionDialog aDialog, boolean aPositive)
    {
    	if(CLOSE_GAME_QUESTION == aID && questionOpen)
    	{
    		Game.queueCommand(new Commands.Pause(false));
    		if(aPositive)
    		{
    			Game.queueCommand(new Commands.CloseGame().setCallback(new CommandQueue.Callback(this, new Runnable()
    			{
    				@Override public void run()
    				{
    					System.exit(0);
    				}
    			})));
    		}
    		
    		questionOpen = false;
    		windowManager.setupDisplay();
    	}
    }
    
    @Override public boolean dispatchTouchEvent(MotionEvent aEvent)
    {	
    	if(aEvent.getActionMasked() == MotionEvent.ACTION_DOWN)
    	{
    		windowManager.screenTouched(aEvent);
    	}
    	
//    	if(aEvent.getActionMasked() == MotionEvent.ACTION_DOWN || aEvent.getActionMasked() == MotionEvent.ACTION_MOVE)
//		{
//   		final Rect imageArea = VertexData.getImageArea();
//    		
//    		final float xBase = (float)(aEvent.getX() - imageArea.left) / (float)imageArea.width();
//    		final float yBase = (float)(aEvent.getY() - imageArea.top) / (float)imageArea.height();
//  
//    		Input.setTouchData((short)(((xBase - .5f) * 2.0f) * 32767.0f), (short)(((yBase - .5f) * 2.0f) * 32767.0f), true);
//		}
//   	else if(aEvent.getActionMasked() == MotionEvent.ACTION_UP)
//    	{
//    		Input.setTouchData((short)0, (short)0, false);
//    	}
    	
		return super.dispatchTouchEvent(aEvent);
    }
    
    @Override public boolean dispatchKeyEvent(KeyEvent aEvent)
    {   
    	int keyCode = aEvent.getKeyCode();
    	
    	// Keys to never handle as game input
		if(keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_MENU)
		{
			return super.dispatchKeyEvent(aEvent);
		}
		
		// Update game input structure
		Input.processEvent(aEvent);
		return true;
    }
        
    @Override public void onBackPressed()
    {	
        if(Game.hasGame())
        {
        	if(!questionOpen)
        	{
        		Game.queueCommand(new Commands.Pause(true));
        		QuestionDialog.newInstance(CLOSE_GAME_QUESTION, "Really Close Game?", "All unsaved data will be lost.", "Yes", "No", null).show(getSupportFragmentManager(), "mainfragment");
        		
        		questionOpen = true;
        	}
        }
        else
        {
        	System.exit(0);
        }
    }
    
    @Override protected void onSaveInstanceState(Bundle aState)
    {
    	super.onSaveInstanceState(aState);
    	aState.putBoolean("questionOpen", questionOpen);
    	aState.putBoolean("onScreenInput", windowManager.getOnScreenInput());
    	aState.putBoolean("onPause", isPaused);
    	aState.putBoolean("showActionBar", showActionBar);
    }
    
    // Menu
    @Override public boolean onCreateOptionsMenu(Menu aMenu)
    {
    	super.onCreateOptionsMenu(aMenu);
    	getMenuInflater().inflate(R.menu.retro_display, aMenu);
    	
    	aMenu.findItem(R.id.show_on_screen_input).setChecked(windowManager.getOnScreenInput());
    	aMenu.findItem(R.id.pause).setChecked(isPaused);

    	
        return true;
    }
    
    private void runCommandWithText(CommandQueue.BaseCommand aCommand, final String aText)
    {
    	Game.queueCommand(aCommand.setCallback(new CommandQueue.Callback(this, new Runnable()
    	{
    		@Override public void run()
    		{
    			Toast.makeText(getApplicationContext(), aText, Toast.LENGTH_SHORT).show();
    		}
    	}
    	)));
    }
    
    @Override public boolean onOptionsItemSelected(MenuItem aItem)
    {
    	int itemID = aItem.getItemId();
    	
    	switch(itemID)
    	{	
    		case R.id.show_on_screen_input:
    		{
            	windowManager.setOnScreenInput(!aItem.isChecked());
            	aItem.setChecked(windowManager.getOnScreenInput());
            	return true;
    		}
    		
    		case R.id.pause:
    		{
            	isPaused = !isPaused;
            	aItem.setChecked(isPaused);
            	
            	runCommandWithText(new Commands.Pause(isPaused), "Game " + (isPaused ? "Paused" : "Unpaused"));
            	return true;
    		}
    		
    		case R.id.input_method_select: ((InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE)).showInputMethodPicker(); return true;
    		
    		case R.id.screenshot: runCommandWithText(new Commands.TakeScreenShot(), "Screenshot Saved"); return true;
    		case R.id.reset: runCommandWithText(new Commands.Reset(), "Game Reset"); return true;
    		
    		case R.id.save_state: startActivity(new Intent(this, StateList.class).putExtra("moduleName", moduleName).putExtra("loading",  false)); return true;
    		case R.id.load_state: startActivity(new Intent(this, StateList.class).putExtra("moduleName", moduleName).putExtra("loading",  true)); return true;
    		case R.id.system_settings: startActivity(new Intent(this, SettingActivity.class).putExtra("moduleName", moduleName)); return true;
    		
    		default: return super.onOptionsItemSelected(aItem);
    	}
    }
}

