package com.retroarch;

import android.app.Activity;
import android.content.Context;
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