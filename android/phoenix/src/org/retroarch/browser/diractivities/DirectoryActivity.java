package org.retroarch.browser.diractivities;

import org.retroarch.R;
import org.retroarch.browser.FileWrapper;
import org.retroarch.browser.IconAdapter;
import org.retroarch.browser.preferences.util.UserPreferences;

import java.util.*;
import java.io.*;

import android.content.*;
import android.app.*;
import android.media.AudioManager;
import android.os.*;
import android.widget.*;
import android.view.*;


public class DirectoryActivity extends ListActivity {
	private IconAdapter<FileWrapper> adapter;
	private File listedDirectory;

	public static class BackStackItem implements Parcelable {
		private final String path;
		private final boolean parentIsBack;

		public BackStackItem(String path, boolean parentIsBack) {
			this.path = path;
			this.parentIsBack = parentIsBack;
		}

		private BackStackItem(Parcel in) {
			this.path = in.readString();
			this.parentIsBack = in.readInt() != 0;
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
		setListAdapter(adapter);

		// Load Directory
		if (savedInstanceState != null) {
			backStack = savedInstanceState.getParcelableArrayList("BACKSTACK");
		}

		if (backStack == null || backStack.isEmpty()) {
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
			SharedPreferences settings = UserPreferences.getPreferences(this);
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
	public void onListItemClick(ListView listView, View aView, int position, long id) {
		final FileWrapper item = adapter.getItem(position);

		if (item.isParentItem() && backStack.get(backStack.size() - 1).parentIsBack) {
			backStack.remove(backStack.size() - 1);
			wrapFiles();
			return;
		} else if (item.isDirSelectItem()) {
			finishWithPath(listedDirectory.getAbsolutePath());
			return;
		}

		final File selected = item.isParentItem() ? listedDirectory.getParentFile() : item.getFile();

		if (selected.isDirectory()) {
			backStack.add(new BackStackItem(selected.getAbsolutePath(),
					!item.isParentItem()));
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
					adapter.add(new FileWrapper(file, FileWrapper.FILE, true));
			}
		}

		// Sort items
		adapter.sort(new Comparator<FileWrapper>() {
			@Override
			public int compare(FileWrapper left, FileWrapper right) {
				return left.compareTo(right);
			};
		});

		// Update
		adapter.notifyDataSetChanged();
	}
}
