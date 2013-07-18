package org.andretro.browser;

import org.andretro.*;
import org.andretro.emulator.*;
import org.andretro.system.*;

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
	public final ModuleInfo info;
	
    public ModuleWrapper(Context aContext, File aFile) throws IOException
    {
		file = aFile;
		info = ModuleInfo.getInfoAbout(aContext, aFile);
    }
    
    @Override public boolean isEnabled()
    {
    	return true;
    }
    
    @Override public String getText()
    {
    	return info.name;
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
	// HACK: Hard path
	private static final String modulePath = "/data/data/org.andretro/lib/";
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
        			Logger.d("Couldn't add module: " + lib.getPath());
        		}
        	}
        }
    }
    
	@Override public void onItemClick(AdapterView<?> aListView, View aView, int aPosition, long aID)
	{
		final ModuleWrapper item = adapter.getItem(aPosition);

		startActivity(new Intent(ModuleActivity.this, DirectoryActivity.class)
			.putExtra("path", item.info.getDataPath() + "/Games")
			.putExtra("moduleName", item.file.getAbsolutePath()));
	}
		
    @Override public boolean onCreateOptionsMenu(Menu aMenu)
    {
    	super.onCreateOptionsMenu(aMenu);
		getMenuInflater().inflate(R.menu.directory_list, aMenu);
    	
		aMenu.removeItem(R.id.goto_root);
		aMenu.removeItem(R.id.system_settings);
    	
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
