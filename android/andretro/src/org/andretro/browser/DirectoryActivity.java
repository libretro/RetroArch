package org.andretro.browser;

import org.andretro.*;
import org.andretro.settings.*;
import org.andretro.emulator.*;

import java.util.*;
import java.io.*;

import android.content.*;
import android.app.*;
import android.os.*;
import android.widget.*;
import android.view.*;
import android.view.inputmethod.*;
import android.graphics.drawable.*;

class FileWrapper implements IconAdapterItem
{
    public final File file;
    protected final int typeIndex;
    protected final boolean enabled;

    public FileWrapper(File aFile, boolean aIsEnabled)
    {
        file = aFile;
        typeIndex = (file.isDirectory() ? 1 : 0) + (file.isFile() ? 2 : 0);
        enabled = aIsEnabled;
    }
        
    @Override public boolean isEnabled()
    {
    	return enabled;
    }
    
    @Override public String getText()
    {
    	return file.getName();
    }
    
    @Override public int getIconResourceId()
    {
    	return file.isFile() ? R.drawable.file : R.drawable.folder;
    }
    
    @Override public Drawable getIconDrawable()
    {
    	return null;
    }
    
    public int compareTo(FileWrapper aOther)
    {	
    	if(null != aOther)
    	{
    		// Who says ternary is hard to follow
    		if(isEnabled() == aOther.isEnabled())
    		{
    			return (typeIndex == aOther.typeIndex) ? file.compareTo(aOther.file) : ((typeIndex < aOther.typeIndex) ? -1 : 1);
    		}
    		else
    		{
    			return isEnabled() ? -1 : 1;
    		}
    	}
    	
    	return -1;
    }
}

public class DirectoryActivity extends Activity implements AdapterView.OnItemClickListener
{
    private IconAdapter<FileWrapper> adapter;
    private boolean inRoot;
    private String moduleName;
    private ModuleInfo moduleInfo;
    
    @Override public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        setContentView(R.layout.line_list);
        
        inRoot = getIntent().getBooleanExtra("inRoot", false);
        moduleName = getIntent().getStringExtra("moduleName");
        moduleInfo = ModuleInfo.getInfoAbout(this, new File(moduleName));
        
        // Setup the list
        adapter = new IconAdapter<FileWrapper>(this, R.layout.line_list_item);
        ListView list = (ListView)findViewById(R.id.list);
        list.setAdapter(adapter);
        list.setOnItemClickListener(this);
        
        // Load Directory
        final String path = getIntent().getStringExtra("path");;
        wrapFiles(new File(path));
        setTitle(path);
    }
    
	@Override public void onItemClick(AdapterView<?> aListView, View aView, int aPosition, long aID)
	{
		final File selected = adapter.getItem(aPosition).file;
		
		final Intent intent = new Intent(this, selected.isFile() ? RetroDisplay.class : DirectoryActivity.class)
				.putExtra("path", selected.getAbsolutePath())
				.putExtra("moduleName", moduleName)
				.putExtra("inRoot", inRoot);

		startActivity(intent);
	}
	
    @Override public boolean onCreateOptionsMenu(Menu aMenu)
    {
    	super.onCreateOptionsMenu(aMenu);
		getMenuInflater().inflate(R.menu.directory_list, aMenu);
    	
    	if(inRoot)
    	{
    		aMenu.removeItem(R.id.goto_root);
    	}
    	
    	return true;
    }

    @Override public boolean onOptionsItemSelected(MenuItem aItem)
    {
    	if(R.id.goto_root == aItem.getItemId())
    	{
    		startActivity(new Intent(this, DirectoryActivity.class)
    				.putExtra("inRoot", true)
    				.putExtra("path", "/mnt/")
    				.putExtra("moduleName", moduleName));
    		return true;
    	}
    	
        if(R.id.input_method_select == aItem.getItemId())
        {
        	InputMethodManager imm = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        	imm.showInputMethodPicker();
        	return true;
        }
    	
        if(aItem.getItemId() == R.id.system_settings)
        {
        	startActivity(new Intent(this, SettingActivity.class)
        		.putExtra("moduleName", moduleName));
    		return true;
        }
        
        return super.onOptionsItemSelected(aItem);
    }
        
    private void wrapFiles(File aDirectory)
    {
    	if(null == aDirectory || !aDirectory.isDirectory())
    	{
    		throw new IllegalArgumentException("Directory is not valid.");
    	}

        // Copy new items
        for(File file: aDirectory.listFiles())
        {
        	adapter.add(new FileWrapper(file, file.isDirectory() || moduleInfo.isFileValid(file)));
        }
        
        // Sort items
        adapter.sort(new Comparator<FileWrapper>()
        {
            @Override public int compare(FileWrapper aLeft, FileWrapper aRight)
            {
                return aLeft.compareTo(aRight);
            };
        });

        // Update
        adapter.notifyDataSetChanged();    	
    }
}
