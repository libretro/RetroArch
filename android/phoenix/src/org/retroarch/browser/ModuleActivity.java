package org.retroarch.browser;
import org.retroarch.R;


import java.io.*;

import android.content.*;
import android.app.*;
import android.os.*;
import android.provider.Settings;
import android.widget.*;
import android.view.*;
import android.view.inputmethod.*;
import android.graphics.drawable.*;

class ModuleWrapper implements IconAdapterItem
{
	public final File file;
	
    public ModuleWrapper(Context aContext, File aFile) throws IOException
    {
		file = aFile;
    }
    
    @Override public boolean isEnabled()
    {
    	return true;
    }
    
    @Override public String getText()
    {
    	return file.getName();
    }
    
    @Override public int getIconResourceId()
    {
    	return 0;
    }
    
    @Override public Drawable getIconDrawable()
    {
    	return null;
    }
}

public class ModuleActivity extends Activity implements AdapterView.OnItemClickListener
{
    private IconAdapter<ModuleWrapper> adapter;
	static private final int ACTIVITY_LOAD_ROM = 0;
	static private String libretro_path;
	
    private final float getRefreshRate()
    {
    	final WindowManager wm = (WindowManager)getSystemService(Context.WINDOW_SERVICE);
    	final Display display = wm.getDefaultDisplay();
    	float rate = display.getRefreshRate();
    	return rate;
    }
    
    @Override public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
               
        setContentView(R.layout.line_list);
        
        // Setup the list
        adapter = new IconAdapter<ModuleWrapper>(this, R.layout.line_list_item);
        ListView list = (ListView)findViewById(R.id.list);
        list.setAdapter(adapter);
        list.setOnItemClickListener(this);
        
        setTitle("Select Libretro core");
    	
    	// Populate the list
    	final String modulePath = getApplicationInfo().nativeLibraryDir;
        for(final File lib: new File(modulePath).listFiles())
        {
        	if(lib.getName().startsWith("libretro_"))
        	{
        		try
        		{
        			adapter.add(new ModuleWrapper(this, lib));;
        		}
        		catch(Exception e)
        		{
        			//Logger.d("Couldn't add module: " + lib.getPath());
        		}
        	}
        }
    }
    
	@Override public void onItemClick(AdapterView<?> aListView, View aView, int aPosition, long aID)
	{
		final ModuleWrapper item = adapter.getItem(aPosition);
    	libretro_path = item.file.getAbsolutePath();
		
    	Intent myIntent;
    	myIntent = new Intent(this, DirectoryActivity.class);
    	startActivityForResult(myIntent, ACTIVITY_LOAD_ROM);
	}
	
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
    	Intent myIntent;
    	
    	switch(requestCode)
    	{
    	   case ACTIVITY_LOAD_ROM:
    		   if(data.getStringExtra("PATH") != null)
    		   {
    			   String current_ime = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
    			      Toast.makeText(this, "Loading: ["+ data.getStringExtra("PATH") + "]...", Toast.LENGTH_SHORT).show();
    				   myIntent = new Intent(this, NativeActivity.class);
    				   myIntent.putExtra("ROM", data.getStringExtra("PATH"));
    				   myIntent.putExtra("LIBRETRO", libretro_path);
    				   myIntent.putExtra("REFRESHRATE", Float.toString(getRefreshRate()));
    				   myIntent.putExtra("CONFIGFILE", "");
    				   myIntent.putExtra("IME", current_ime);
    				   startActivity(myIntent);
    		   }
    		   break;
    	}
    }
		
    @Override public boolean onCreateOptionsMenu(Menu aMenu)
    {
    	super.onCreateOptionsMenu(aMenu);
		getMenuInflater().inflate(R.menu.directory_list, aMenu);
    	return true;
    }

    @Override public boolean onOptionsItemSelected(MenuItem aItem)
    {
        if(R.id.input_method_select == aItem.getItemId())
        {
        	InputMethodManager imm = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        	imm.showInputMethodPicker();
        	return true;
        }
    	
        return super.onOptionsItemSelected(aItem);
    }
}
