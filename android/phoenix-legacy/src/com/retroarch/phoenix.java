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

import com.retroarch.R;
import com.retroarch.fileio.FileChooser;

import android.app.Activity;
import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.view.Display;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.WindowManager;
import android.widget.Toast;
import android.os.Bundle;

public class phoenix extends Activity
{
	static private final int ACTIVITY_LOAD_ROM = 0;
	static private final int ACTIVITY_LOAD_LIBRETRO_CORE = 1;
	
	static private String libretro_path;
	
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        this.setTitle("RetroArch Phoenix");
    }
    
    public boolean onCreateOptionsMenu(Menu menu)
    {
    	MenuInflater inflater = getMenuInflater();
    	inflater.inflate(R.menu.main_menu, menu);
    	return true;
    }
    
    public float getRefreshRate()
    {
    	final WindowManager wm = (WindowManager)getSystemService(Context.WINDOW_SERVICE);
    	final Display display = wm.getDefaultDisplay();
    	float rate = display.getRefreshRate();
    	return rate;
    }
    
    public boolean onOptionsItemSelected(MenuItem item)
    {
    	Intent myIntent;
    	switch (item.getItemId())
    	{
    	    case R.id.main:
    	    	this.finish();
    		    break;
    	    case R.id.open:
    	    	Toast.makeText(this, "Select a ROM image from the Filebrowser.", Toast.LENGTH_SHORT).show();
    	    	myIntent = new Intent(this, FileChooser.class);
    	    	startActivityForResult(myIntent, ACTIVITY_LOAD_ROM);
    		    break;
    	    case R.id.libretro:
    	    	Toast.makeText(this, "Select a libretro core from the Filebrowser.", Toast.LENGTH_SHORT).show();
    	    	myIntent = new Intent(this, FileChooser.class);
    	    	startActivityForResult(myIntent, ACTIVITY_LOAD_LIBRETRO_CORE);
    		    break;   	    	
    		default:
    	    	Toast.makeText(this, "MenuItem " + item.getTitle() + " selected.", Toast.LENGTH_SHORT).show();
    	    	break;
    	}
    	
    	return true;
    }
    
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
    	Intent myIntent;
    	
    	switch(requestCode)
    	{
    	   case ACTIVITY_LOAD_ROM:
    		   if(data.getStringExtra("PATH") != null)
    		   {
    			   if(libretro_path != null)
    			   {
    			       Toast.makeText(this, "Loading: ["+ data.getStringExtra("PATH") + "]...", Toast.LENGTH_SHORT).show();
    				   myIntent = new Intent(this, NativeActivity.class);
    				   myIntent.putExtra("ROM", data.getStringExtra("PATH"));
    				   myIntent.putExtra("LIBRETRO", libretro_path);
    				   myIntent.putExtra("REFRESHRATE", Float.toString(getRefreshRate()));
    				   startActivity(myIntent);
    			   }
    			   else
    	    	    	Toast.makeText(this, "ERROR - No libretro core has been selected, cannot start RetroArch.", Toast.LENGTH_SHORT).show();
    		   }
    		   break;
    	   case ACTIVITY_LOAD_LIBRETRO_CORE:
    		   if(data.getStringExtra("PATH") != null)
    		   {
			       Toast.makeText(this, "Libretro core path set to: ["+ data.getStringExtra("PATH") + "].", Toast.LENGTH_SHORT).show();
    			   libretro_path = data.getStringExtra("PATH");
    		   }
    		   break;
    	}
    }
    
    @Override
    protected void onPause()
    {
    	super.onPause();
    }
    
    @Override
    protected void onResume()
    {
    	super.onResume();
    }
}
