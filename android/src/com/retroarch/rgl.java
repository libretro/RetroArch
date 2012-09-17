/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

package com.retroarch;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.ByteOrder;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.opengl.GLES20;
import android.opengl.GLSurfaceView;

public class rgl implements GLSurfaceView.Renderer
{
	private int cprg;
	private int v_position_handle;
	private FloatBuffer triangle_vbo;
	
	private void triangles_init()
	{
		float triangle_coords[] = {
				// X, Y, Z
				-0.5f, -0.25f, 0,
				0.5f,  -0.25f, 0,
				0.0f,  0.559016994f, 0
		};
		
		ByteBuffer vbb = ByteBuffer.allocateDirect(triangle_coords.length * 4);
		vbb.order(ByteOrder.nativeOrder());
		triangle_vbo = vbb.asFloatBuffer();
		triangle_vbo.put(triangle_coords);
		triangle_vbo.position(0);
	}
	
	private void shader_init()
	{
	    final String vprg = 
	            "attribute vec4 vPosition; \n" +
	            "void main(){              \n" +
	            " gl_Position = vPosition; \n" +
	            "}                         \n";
	    final String fprg = 
	            "precision mediump float;  \n" +
	            "void main(){              \n" +
	            " gl_FragColor = vec4 (0.63671875, 0.76953125, 0.22265625, 1.0); \n" +
	            "}                         \n";
	    
	    int vertex_shader, fragment_shader;
	    
	    vertex_shader = GLES20.glCreateShader(GLES20.GL_VERTEX_SHADER);
	    
	    GLES20.glShaderSource(vertex_shader, vprg);
	    GLES20.glCompileShader(vertex_shader);
	    
	    fragment_shader = GLES20.glCreateShader(GLES20.GL_FRAGMENT_SHADER);
	    
	    GLES20.glShaderSource(fragment_shader,  fprg);
	    GLES20.glCompileShader(fragment_shader);
	    
	    cprg = GLES20.glCreateProgram();
	    GLES20.glAttachShader(cprg,  vertex_shader);
	    GLES20.glAttachShader(cprg, fragment_shader);
	    GLES20.glLinkProgram(cprg);
	    
	    //get handle to the vertex shader's vPosition member
	    v_position_handle = GLES20.glGetAttribLocation(cprg, "vPosition");
;	}
	
	public void onSurfaceCreated(GL10 unused, EGLConfig config)
	{
		//background color
		GLES20.glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

		triangles_init();
		shader_init();
	}
	
	public void onDrawFrame(GL10 unused)
	{
		GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

		GLES20.glUseProgram(cprg);

		// Triangle
		GLES20.glVertexAttribPointer(v_position_handle, 3, GLES20.GL_FLOAT, false, 12, triangle_vbo);
		GLES20.glEnableVertexAttribArray(v_position_handle);

		GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, 3);
	}
	
	public void onSurfaceChanged(GL10 unused, int width, int height)
	{
		GLES20.glViewport(0,  0, width, height);
	}
}
