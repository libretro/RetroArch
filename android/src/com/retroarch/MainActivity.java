package com.retroarch;

import android.app.ActionBar;
import android.app.Activity;
import android.content.Context;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.Toast;
import android.opengl.GLSurfaceView;
import android.os.Bundle;

public class MainActivity extends Activity
{
	private GLSurfaceView ctx_gl;
	
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        ctx_gl = new rgl_context(this);
        setContentView(ctx_gl);
    }
    
    public boolean onCreateOptionsMenu(Menu menu)
    {
    	MenuInflater inflater = getMenuInflater();
    	inflater.inflate(R.menu.main_menu, menu);
    	return true;
    }
    
    public boolean onOptionsItemSelected(MenuItem item)
    {
    	switch (item.getItemId())
    	{
    		case R.id.quit:
    			android.os.Process.killProcess(android.os.Process.myPid());
    			return true;
    		default:
    	    	Toast.makeText(this, "MenuItem " + item.getTitle() + " selected.", Toast.LENGTH_SHORT).show();
    	    	return true;
    	}
    }
    
    @Override
    protected void onPause()
    {
    	super.onPause();
    	ctx_gl.onPause();
    }
    
    @Override
    protected void onResume()
    {
    	super.onResume();
    	ctx_gl.onResume();
    }
}

class rgl_context extends GLSurfaceView
{
	public rgl_context(Context context)
	{
		super(context);
		setEGLContextClientVersion(2);
		setRenderer(new rgl());
	}
}