package org.andretro.browser;

import org.andretro.*;
import org.andretro.emulator.*;

import android.app.*;
import android.graphics.drawable.*;
import android.os.*;
import android.view.*;
import android.widget.*;

import java.io.*;
import java.util.*;
import java.text.*;

class SlotWrapper implements IconAdapterItem
{
    public final int slot;
    public final boolean enabled;
    public final String text;
    public final Drawable thumbNail;

    public SlotWrapper(int aSlot, boolean aLoading)
    {
        slot = aSlot;
        
    	final File slotFile = new File(Game.getGameDataName("SaveStates", "st" + aSlot));
    	final boolean hasSlotFile = slotFile.isFile() && slotFile.exists();
    	final String slotFileDate = hasSlotFile ? DateFormat.getDateTimeInstance().format(new Date(slotFile.lastModified())) : "EMPTY";

    	enabled = !aLoading || hasSlotFile;
    	text = "Slot " + aSlot + " (" + slotFileDate + ")";
    	
    	// Get thumb
    	final File thumbFile = new File(Game.getGameDataName("SaveStates", "tb" + aSlot));
    	thumbNail = thumbFile.isFile() ? Drawable.createFromPath(thumbFile.getAbsolutePath()) : null;
    }
        
    @Override public boolean isEnabled()
    {
    	return enabled;
    }
    
    @Override public String getText()
    {
    	return text;
    }
    
    @Override public int getIconResourceId()
    {
    	return 0;
    }
    
    @Override public Drawable getIconDrawable()
    {
    	return thumbNail;
    }
}

public class StateList extends Activity implements AdapterView.OnItemClickListener
{
    private IconAdapter<SlotWrapper> adapter;
    private boolean loading;
    
    @Override public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        setContentView(R.layout.icon_grid_list);
        
        loading = getIntent().getBooleanExtra("loading", false);
        
        
        // Setup the list
        adapter = new IconAdapter<SlotWrapper>(this, R.layout.icon_grid_item);
        GridView list = (GridView)findViewById(R.id.list);
        list.setAdapter(adapter);
        list.setOnItemClickListener(this);
        
        setTitle(loading ? "Load State" : "Save State");
        
        // Add data
        for(int i = 0; i != 10; i ++)
        {
        	adapter.add(new SlotWrapper(i, loading));
        }
    }
    
	@Override public void onItemClick(AdapterView<?> aListView, View aView, int aPosition, long aID)
	{
		Game.queueCommand(new Commands.StateAction(loading, aPosition));
		finish();
	}
}
