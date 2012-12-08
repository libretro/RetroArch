package org.retroarch.browser;
import org.retroarch.R;

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
    public final boolean parentItem;
    protected final int typeIndex;
    protected final boolean enabled;

    public FileWrapper(File aFile, boolean aIsParentItem, boolean aIsEnabled)
    {
        file = aFile;
        typeIndex = aIsParentItem ? 0 : (file.isDirectory() ? 1 : 0) + (file.isFile() ? 2 : 0);
        parentItem = aIsParentItem;
        enabled = aIsParentItem || aIsEnabled;
    }

    @Override public boolean isEnabled()
    {
    	return enabled;
    }

    @Override public String getText()
    {
    	return parentItem ? "[Parent Directory]" : file.getName();
    }

    @Override public int getIconResourceId()
    {
        if(!parentItem)
        {
    	    return file.isFile() ? R.drawable.ic_file : R.drawable.ic_dir;
        }
        else
        {
            return R.drawable.ic_dir;
        }
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
    private File listedDirectory;

    public static class BackStackItem implements Parcelable
    {
        public String path;
        public boolean parentIsBack;

        public BackStackItem(String aPath, boolean aParentIsBack)
        {
            path = aPath;
            parentIsBack = aParentIsBack;
        }

        private BackStackItem(Parcel aIn)
        {
            path = aIn.readString();
            parentIsBack = aIn.readInt() != 0;
        }

        public int describeContents()
        {
            return 0;
        }

        public void writeToParcel(Parcel out, int flags)
        {
            out.writeString(path);
            out.writeInt(parentIsBack ? 1 : 0);
        }


        public static final Parcelable.Creator<BackStackItem> CREATOR = new Parcelable.Creator<BackStackItem>()
        {
             public BackStackItem createFromParcel(Parcel in) { return new BackStackItem(in); }
             public BackStackItem[] newArray(int size) { return new BackStackItem[size]; }
         };

    }

    private ArrayList<BackStackItem> backStack;

    @Override public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.line_list);

        // Setup the list
        adapter = new IconAdapter<FileWrapper>(this, R.layout.line_list_item);
        ListView list = (ListView)findViewById(R.id.list);
        list.setAdapter(adapter);
        list.setOnItemClickListener(this);

        // Load Directory
        if(savedInstanceState != null)
        {
            backStack = savedInstanceState.getParcelableArrayList("BACKSTACK");
        }

        if(backStack == null || backStack.size() == 0)
        {
            backStack = new ArrayList<BackStackItem>();
            backStack.add(new BackStackItem(Environment.getExternalStorageDirectory().getPath(), false));
        }

        wrapFiles();
    }

    @Override protected void onSaveInstanceState(Bundle aState)
    {
    	super.onSaveInstanceState(aState);
        aState.putParcelableArrayList("BACKSTACK", backStack);
    }

	@Override public void onItemClick(AdapterView<?> aListView, View aView, int aPosition, long aID)
	{
        final FileWrapper item = adapter.getItem(aPosition);

        if(item.parentItem && backStack.get(backStack.size() - 1).parentIsBack)
        {
            backStack.remove(backStack.size() - 1);
            wrapFiles();
            return;
        }

        final File selected = item.parentItem ? listedDirectory.getParentFile() : item.file;

        if(selected.isDirectory())
        {
            backStack.add(new BackStackItem(selected.getAbsolutePath(), !item.parentItem));
            wrapFiles();
        }
        else
        {
            final Intent intent = new Intent(this, selected.isFile() ? NativeActivity.class : DirectoryActivity.class)
                    .putExtra("ROM", selected.getAbsolutePath())
                    .putExtra("LIBRETRO", getIntent().getStringExtra("LIBRETRO"));
            startActivity(intent);
        }
	}

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event)
    {
        if(keyCode == KeyEvent.KEYCODE_BACK && backStack.size() > 1)
        {
            backStack.remove(backStack.size() - 1);
            wrapFiles();
            return true;
        }

        return super.onKeyDown(keyCode, event);
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

    private void wrapFiles()
    {
        listedDirectory = new File(backStack.get(backStack.size() - 1).path);

    	if(!listedDirectory.isDirectory())
    	{
    		throw new IllegalArgumentException("Directory is not valid.");
    	}

        adapter.clear();
        setTitle(listedDirectory.getAbsolutePath());

        if(listedDirectory.getParentFile() != null)
        {
            adapter.add(new FileWrapper(null, true, true));
        }

        // Copy new items
        final File[] files = listedDirectory.listFiles();
        if(files != null)
        {
            for(File file: files)
            {
        	    adapter.add(new FileWrapper(file, false, file.isDirectory() || true));
            }
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
