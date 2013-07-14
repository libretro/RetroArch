package org.andretro.system;
import org.andretro.R;
import org.andretro.emulator.*;
import org.libretro.*;
import org.andretro.system.video.*;

import java.io.*;

import javax.microedition.khronos.opengles.*;

import android.content.*;
import android.opengl.*;

import static android.opengl.GLES20.*;

// NOT THREAD SAFE
public final class Present implements GLSurfaceView.Renderer
{	
	final LibRetro.VideoFrame frame = new LibRetro.VideoFrame();
	
	public static class Texture
	{
		private static final int id[] = new int[1];
		private static boolean smoothMode = true;
		
		private static void create()
		{
	    	glGenTextures(1, id, 0);
	        glBindTexture(GL_TEXTURE_2D, id[0]);
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	        
	        setSmoothMode(smoothMode);
		}
			    	    
	    public static void setSmoothMode(boolean aEnable)
	    {
	    	smoothMode = aEnable;
	    	
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, aEnable ? GL_LINEAR : GL_NEAREST);
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, aEnable ? GL_LINEAR : GL_NEAREST);	    	
	    }
	}
	
    private static int programID;
    private static String vertexShader;
    private static String fragmentShader;
        
    // OpenGL Renderer
    private static String getShaderString(final InputStream aSource)
    {
    	String source = "";
    	
    	try
    	{
    		source = new java.util.Scanner(aSource).useDelimiter("\\A").next();
    		aSource.close();
    	}
    	catch(IOException e)
    	{
    	
    	}

    	return source;
    }
    
    private static int buildShader(int aType, final String aSource)
    {
    	int result = glCreateShader(aType);
    	glShaderSource(result, aSource);
    	glCompileShader(result);
    	
    	int[] state = new int[1];
    	glGetShaderiv(result, GL_COMPILE_STATUS, state, 0);
    	if(0 == state[0])
    	{
    		System.out.println(glGetShaderInfoLog(result));
    	}
    	
    	return result;
    }
    
    public static GLSurfaceView.Renderer createRenderer(Context aContext)
    {
    	vertexShader = getShaderString(aContext.getResources().openRawResource(R.raw.vertex_shader));
    	fragmentShader = getShaderString(aContext.getResources().openRawResource(R.raw.fragment_shader));
    	return new Present();
    }
    
    @Override public void onSurfaceCreated(GL10 gl, javax.microedition.khronos.egl.EGLConfig config)
    {    	
    	// Program
    	programID = glCreateProgram();
    	glAttachShader(programID, buildShader(GL_VERTEX_SHADER, vertexShader));
    	glAttachShader(programID, buildShader(GL_FRAGMENT_SHADER, fragmentShader));
    	glBindAttribLocation(programID, 0, "pos");
    	glBindAttribLocation(programID, 1, "tex");
    	
    	glLinkProgram(programID);
    	glUseProgram(programID);
    	
        // Objects
        Texture.create();
        VertexData.create();
    }
    
    @Override public void onSurfaceChanged(GL10 gl, int aWidth, int aHeight)
    {
        glViewport(0, 0, aWidth, aHeight);
        
        VertexData.setScreenSize(aWidth, aHeight);        
        frame.restarted = true;
    }
        
    @Override public void onDrawFrame(GL10 gl)
    {
    	if(Game.doFrame(frame))
    	{
    		VertexData.setImageData(frame.width, frame.height, frame.aspect, frame.rotation);
    	}
    	
		VertexData.draw();
    }
}
