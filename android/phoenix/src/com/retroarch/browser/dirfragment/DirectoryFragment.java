package com.retroarch.browser.dirfragment;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.v4.app.DialogFragment;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import com.retroarch.R;
import com.retroarch.browser.FileWrapper;
import com.retroarch.browser.IconAdapter;
import com.retroarch.browser.preferences.util.UserPreferences;

import java.util.*;
import java.io.*;


/**
 * {@link DialogFragment} subclass that provides a file-browser
 * like UI for browsing for specific files.
 * <p>
 * This file browser also allows for custom filtering
 * depending on the type of class that inherits it.
 * <p>
 * This file browser also uses an implementation of a 
 * backstack for remembering previously browsed folders
 * within this DirectoryFragment.
 * <p>
 * To instantiate a new instance of this class
 * you must use the {@code newInstance} method.
 */
public class DirectoryFragment extends DialogFragment
{
	protected IconAdapter<FileWrapper> adapter;
	protected File listedDirectory;

	public static final class BackStackItem implements Parcelable
	{
		protected final String path;
		protected boolean parentIsBack;

		public BackStackItem(String path, boolean parentIsBack)
		{
			this.path = path;
			this.parentIsBack = parentIsBack;
		}

		private BackStackItem(Parcel in)
		{
			this.path = in.readString();
			this.parentIsBack = in.readInt() != 0;
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
			public BackStackItem createFromParcel(Parcel in)
			{
				return new BackStackItem(in);
			}

			public BackStackItem[] newArray(int size)
			{
				return new BackStackItem[size];
			}
		};
	}

	/**
	 * Listener interface for executing ROMs or performing
	 * other things upon the DirectoryFragment instance closing.
	 */
	public interface OnDirectoryFragmentClosedListener
	{
		/**
		 * Performs some arbitrary action after the 
		 * {@link DirectoryFragment} closes.
		 * 
		 * @param path The path to the file chosen within the {@link DirectoryFragment}
		 */
		void onDirectoryFragmentClosed(String path);
	}


	protected ArrayList<BackStackItem> backStack;
	protected String startDirectory;
	protected String pathSettingKey;
	protected boolean isDirectoryTarget;
	protected OnDirectoryFragmentClosedListener onClosedListener;

	/**
	 * Sets the starting directory for this DirectoryFragment
	 * when it is shown to the user.
	 * 
	 * @param path the initial directory to show to the user
	 *             when this DirectoryFragment is shown.
	 */
	public void setStartDirectory(String path)
	{
		startDirectory = path;
	}

	/**
	 * Sets the key to save the selected item in the DialogFragment
	 * into the application SharedPreferences at.
	 * 
	 * @param key the key to save the selected item's path to in
	 *            the application's SharedPreferences.
	 */
	public void setPathSettingKey(String key)
	{
		pathSettingKey = key;
	}

	/**
	 * Sets whether or not we are browsing for a specific
	 * directory or not. If enabled, it will allow the user
	 * to select a specific directory, rather than a file.
	 * 
	 * @param enable Whether or not to enable this.
	 */
	public void setIsDirectoryTarget(boolean enable)
	{
		isDirectoryTarget = enable;
	}

	/**
	 * Sets the listener for an action to perform upon the
	 * closing of this DirectoryFragment.
	 * 
	 * @param onClosedListener the OnDirectoryFragmentClosedListener to set.
	 */
	public void setOnDirectoryFragmentClosedListener(OnDirectoryFragmentClosedListener onClosedListener)
	{
		this.onClosedListener = onClosedListener;
	}

	/**
	 * Retrieves a new instance of a DirectoryFragment
	 * with a title specified by the given resource ID.
	 * 
	 * @param titleResId String resource ID for the title
	 *                   of this DirectoryFragment.
	 * 
	 * @return A new instance of a DirectoryFragment.
	 */
	public static DirectoryFragment newInstance(int titleResId)
	{
		final DirectoryFragment dFrag = new DirectoryFragment();
		final Bundle bundle = new Bundle();
		bundle.putInt("titleResId", titleResId);
		dFrag.setArguments(bundle);

		return dFrag;
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		ListView rootView = (ListView) inflater.inflate(R.layout.line_list, container, false);
		rootView.setOnItemClickListener(onItemClickListener);
		
		// Set the dialog title.
		getDialog().setTitle(getArguments().getInt("titleResId"));

		// Setup the list
		adapter = new IconAdapter<FileWrapper>(getActivity(), R.layout.line_list_item);
		rootView.setAdapter(adapter);

		// Load Directory
		if (savedInstanceState != null)
		{
			backStack = savedInstanceState.getParcelableArrayList("BACKSTACK");
		}

		if (backStack == null || backStack.isEmpty())
		{
			backStack = new ArrayList<BackStackItem>();
			String startPath = (startDirectory == null || startDirectory.isEmpty()) ? Environment
					.getExternalStorageDirectory().getPath() : startDirectory;
			backStack.add(new BackStackItem(startPath, false));
		}

		wrapFiles();
		return rootView;
	}

	private final OnItemClickListener onItemClickListener = new OnItemClickListener()
	{
		@Override
		public void onItemClick(AdapterView<?> parent, View view, int position, long id)
		{
			final FileWrapper item = adapter.getItem(position);

			if (item.isParentItem() && backStack.get(backStack.size() - 1).parentIsBack)
			{
				backStack.remove(backStack.size() - 1);
				wrapFiles();
				return;
			}
			else if (item.isDirSelectItem())
			{
				finishWithPath(listedDirectory.getAbsolutePath());
				return;
			}

			final File selected = item.isParentItem() ? listedDirectory.getParentFile() : item.getFile();

			if (selected.isDirectory())
			{
				backStack.add(new BackStackItem(selected.getAbsolutePath(), !item.isParentItem()));
				wrapFiles();
			}
			else
			{
				String filePath = selected.getAbsolutePath();
				finishWithPath(filePath);
			}
		}
	};

	@Override
	public void onSaveInstanceState(Bundle outState)
	{
		super.onSaveInstanceState(outState);

		outState.putParcelableArrayList("BACKSTACK", backStack);
	}

	private void finishWithPath(String path)
	{
		if (pathSettingKey != null && !pathSettingKey.isEmpty())
		{
			SharedPreferences settings = UserPreferences.getPreferences(getActivity());
			SharedPreferences.Editor editor = settings.edit();
			editor.putString(pathSettingKey, path);
			editor.commit();
		}

		if (onClosedListener != null)
		{
			onClosedListener.onDirectoryFragmentClosed(path);
		}

		dismiss();
	}

	// TODO: Hook this up to a callable interface (if backstack is desirable).
	public boolean onKeyDown(int keyCode, KeyEvent event)
	{
		if (keyCode == KeyEvent.KEYCODE_BACK)
		{
			if (backStack.size() > 1)
			{
				backStack.remove(backStack.size() - 1);
				wrapFiles();
			}

			return true;
		}

		return false;
	}

	private ArrayList<String> allowedExt;
	private ArrayList<String> disallowedExt;

	private boolean filterPath(String path)
	{
		if (disallowedExt != null)
		{
			for (String ext : disallowedExt)
			{
				if (path.endsWith(ext))
					return false;
			}
		}
		
		if (allowedExt != null)
		{
			for (String ext : allowedExt)
			{
				if (path.endsWith(ext))
					return true;
			}
			
			return false;
		}

		return true;
	}

	/**
	 * Allows specifying an allowed file extension.
	 * <p>
	 * Any files that contain this file extension will be shown
	 * within the DirectoryFragment file browser. Those that don't
	 * contain this extension will not be shows.
	 * <p>
	 * It is possible to specify more than one allowed extension by
	 * simply calling this method with a different file extension specified.
	 * 
	 * @param ext The file extension(s) to allow being shown in this DirectoryFragment.
	 */
	public void addAllowedExts(String... exts)
	{
		if (allowedExt == null)
			allowedExt = new ArrayList<String>();

		allowedExt.addAll(Arrays.asList(exts));
	}

	/**
	 * Allows specifying a disallowed file extension.
	 * <p>
	 * Any files that contain this file extension will not be shown
	 * within the DirectoryFragment file browser.
	 * <p>
	 * It is possible to specify more than one disallowed extension by
	 * simply calling this method with a different file extension specified.
	 * 
	 * @param exts The file extension(s) to hide from being shown in this DirectoryFragment.
	 */
	public void addDisallowedExts(String... exts)
	{
		if (disallowedExt == null)
			disallowedExt = new ArrayList<String>();

		disallowedExt.addAll(Arrays.asList(exts));
	}

	protected void wrapFiles()
	{
		listedDirectory = new File(backStack.get(backStack.size() - 1).path);

		if (!listedDirectory.isDirectory())
		{
			throw new IllegalArgumentException("Directory is not valid.");
		}

		adapter.clear();
		
		if (isDirectoryTarget)
			adapter.add(new FileWrapper(null, FileWrapper.DIRSELECT, true));

		if (listedDirectory.getParentFile() != null)
			adapter.add(new FileWrapper(null, FileWrapper.PARENT, true));

		// Copy new items
		final File[] files = listedDirectory.listFiles();
		if (files != null)
		{
			for (File file : files)
			{
				String path = file.getName();

				boolean allowFile = file.isDirectory() || (filterPath(path) && !isDirectoryTarget);
				if (allowFile)
				{
					adapter.add(new FileWrapper(file, FileWrapper.FILE, true));
				}
			}
		}

		// Sort items
		adapter.sort(new Comparator<FileWrapper>()
		{
			@Override
			public int compare(FileWrapper left, FileWrapper right)
			{
				return left.compareTo(right);
			};
		});

		// Update
		adapter.notifyDataSetChanged();
	}
}
