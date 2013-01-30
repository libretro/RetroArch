package org.retroarch.browser;

import org.retroarch.R;

import java.util.*;
import java.io.*;

import android.content.*;
import android.app.*;
import android.media.AudioManager;
import android.os.*;
import android.preference.PreferenceManager;
import android.widget.*;
import android.view.*;
import android.graphics.drawable.*;

class FileWrapper implements IconAdapterItem {
	public final File file;
	public final boolean parentItem;
	public final boolean dirSelectItem;
	
	protected final boolean enabled;
	
	public static final int DIRSELECT = 0;
	public static final int PARENT = 1;
	public static final int FILE = 2;
	
	protected final int typeIndex;

	public FileWrapper(File aFile, int type, boolean aIsEnabled) {
		file = aFile;
		
		parentItem = type == PARENT;
		dirSelectItem = type == DIRSELECT;		
		typeIndex = type == FILE ? (FILE + (file.isDirectory() ? 0 : 1)) : type;	
		
		enabled = parentItem || dirSelectItem || aIsEnabled;
	}

	@Override
	public boolean isEnabled() {
		return enabled;
	}

	@Override
	public String getText() {
		if (dirSelectItem)
			return "[[Use this directory]]";
		else if (parentItem)
			return "[Parent Directory]";
		else
			return file.getName();
	}

	@Override
	public int getIconResourceId() {
		if (!parentItem && !dirSelectItem) {
			return file.isFile() ? R.drawable.ic_file : R.drawable.ic_dir;
		} else {
			return R.drawable.ic_dir;
		}
	}

	@Override
	public Drawable getIconDrawable() {
		return null;
	}

	public int compareTo(FileWrapper aOther) {
		if (aOther != null) {
			// Who says ternary is hard to follow
			if (isEnabled() == aOther.isEnabled()) {
				return (typeIndex == aOther.typeIndex) ? file
						.compareTo(aOther.file)
						: ((typeIndex < aOther.typeIndex) ? -1 : 1);
			} else {
				return isEnabled() ? -1 : 1;
			}
		}

		return -1;
	}
}

public class DirectoryActivity extends Activity implements
		AdapterView.OnItemClickListener {
	private IconAdapter<FileWrapper> adapter;
	private File listedDirectory;

	public static class BackStackItem implements Parcelable {
		public String path;
		public boolean parentIsBack;

		public BackStackItem(String aPath, boolean aParentIsBack) {
			path = aPath;
			parentIsBack = aParentIsBack;
		}

		private BackStackItem(Parcel aIn) {
			path = aIn.readString();
			parentIsBack = aIn.readInt() != 0;
		}

		public int describeContents() {
			return 0;
		}

		public void writeToParcel(Parcel out, int flags) {
			out.writeString(path);
			out.writeInt(parentIsBack ? 1 : 0);
		}

		public static final Parcelable.Creator<BackStackItem> CREATOR = new Parcelable.Creator<BackStackItem>() {
			public BackStackItem createFromParcel(Parcel in) {
				return new BackStackItem(in);
			}

			public BackStackItem[] newArray(int size) {
				return new BackStackItem[size];
			}
		};

	}

	private ArrayList<BackStackItem> backStack;
	
	protected String startDirectory;
	protected String pathSettingKey;
	
	protected void setStartDirectory(String path) {
		startDirectory = path;
	}
	
	protected void setPathSettingKey(String key) {
		pathSettingKey = key;
	}
	
	private boolean isDirectoryTarget;
	protected void setIsDirectoryTarget(boolean enable) {
		isDirectoryTarget = enable;
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.line_list);

		// Setup the list
		adapter = new IconAdapter<FileWrapper>(this, R.layout.line_list_item);
		ListView list = (ListView) findViewById(R.id.list);
		list.setAdapter(adapter);
		list.setOnItemClickListener(this);
		list.setFastScrollEnabled(true);

		// Load Directory
		if (savedInstanceState != null) {
			backStack = savedInstanceState.getParcelableArrayList("BACKSTACK");
		}

		if (backStack == null || backStack.size() == 0) {
			backStack = new ArrayList<BackStackItem>();
			String startPath = (startDirectory == null || startDirectory.isEmpty()) ? Environment
					.getExternalStorageDirectory().getPath() : startDirectory;
			backStack.add(new BackStackItem(startPath, false));
		}

		wrapFiles();
		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);
	}

	@Override
	protected void onSaveInstanceState(Bundle aState) {
		super.onSaveInstanceState(aState);
		aState.putParcelableArrayList("BACKSTACK", backStack);
	}
	
	private void finishWithPath(String path) {
		if (pathSettingKey != null && !pathSettingKey.isEmpty()) {
			SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
			SharedPreferences.Editor editor = settings.edit();
			editor.putString(pathSettingKey, path);
			editor.commit();
		}
		
		Intent intent = new Intent();			
		intent.putExtra("PATH", path);
		setResult(RESULT_OK, intent);
		finish();
	}

	@Override
	public void onItemClick(AdapterView<?> aListView, View aView,
			int aPosition, long aID) {
		final FileWrapper item = adapter.getItem(aPosition);

		if (item.parentItem && backStack.get(backStack.size() - 1).parentIsBack) {
			backStack.remove(backStack.size() - 1);
			wrapFiles();
			return;
		} else if (item.dirSelectItem) {
			finishWithPath(listedDirectory.getAbsolutePath());
			return;
		}

		final File selected = item.parentItem ? listedDirectory.getParentFile()
				: item.file;

		if (selected.isDirectory()) {
			backStack.add(new BackStackItem(selected.getAbsolutePath(),
					!item.parentItem));
			wrapFiles();
		} else {
			String filePath = selected.getAbsolutePath();
			finishWithPath(filePath);
		}
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			if (backStack.size() > 1) {
				backStack.remove(backStack.size() - 1);
				wrapFiles();
			} else {
				Intent intent = new Intent();
				setResult(RESULT_CANCELED, intent);
				finish();
			}
			return true;
		}

		return super.onKeyDown(keyCode, event);
	}
	
	private ArrayList<String> allowedExt;
	private ArrayList<String> disallowedExt;
	
	private boolean filterPath(String path) {
		if (disallowedExt != null) {
			for (String ext : disallowedExt)
				if (path.endsWith(ext))
					return false;
		}
		
		if (allowedExt != null) {
			for (String ext : allowedExt)
				if (path.endsWith(ext))
					return true;
			
			return false;
		}

		return true;
	}
	
	protected void addAllowedExt(String ext) {
		if (allowedExt == null)
			allowedExt = new ArrayList<String>();
		
		allowedExt.add(ext);
	}
	
	protected void addDisallowedExt(String ext) {
		if (disallowedExt == null)
			disallowedExt = new ArrayList<String>();
		
		disallowedExt.add(ext);
	}

	private void wrapFiles() {
		listedDirectory = new File(backStack.get(backStack.size() - 1).path);

		if (!listedDirectory.isDirectory()) {
			throw new IllegalArgumentException("Directory is not valid.");
		}

		adapter.clear();
		setTitle(listedDirectory.getAbsolutePath());
		
		if (isDirectoryTarget)
			adapter.add(new FileWrapper(null, FileWrapper.DIRSELECT, true));

		if (listedDirectory.getParentFile() != null)
			adapter.add(new FileWrapper(null, FileWrapper.PARENT, true));

		// Copy new items
		final File[] files = listedDirectory.listFiles();
		if (files != null) {
			for (File file : files) {
				String path = file.getName();

				boolean allowFile = file.isDirectory() || (filterPath(path) && !isDirectoryTarget);

				if (allowFile)
					adapter.add(new FileWrapper(file, FileWrapper.FILE,
							file.isDirectory() || true));
			}
		}

		// Sort items
		adapter.sort(new Comparator<FileWrapper>() {
			@Override
			public int compare(FileWrapper aLeft, FileWrapper aRight) {
				return aLeft.compareTo(aRight);
			};
		});

		// Update
		adapter.notifyDataSetChanged();
	}
}
