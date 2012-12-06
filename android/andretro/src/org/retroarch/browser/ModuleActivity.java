package org.retroarch.browser;
import org.retroarch.R;

import java.io.*;

import android.content.*;
import android.app.*;
import android.os.*;
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
    
    @Override public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
               
        setContentView(R.layout.line_list);
        
        // Setup the list
        adapter = new IconAdapter<ModuleWrapper>(this, R.layout.line_list_item);
        ListView list = (ListView)findViewById(R.id.list);
        list.setAdapter(adapter);
        list.setOnItemClickListener(this);
        
        setTitle("Select Emulator");
    	
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

		startActivity(new Intent(ModuleActivity.this, DirectoryActivity.class)
			.putExtra("LIBRETRO", item.file.getAbsolutePath()));
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
