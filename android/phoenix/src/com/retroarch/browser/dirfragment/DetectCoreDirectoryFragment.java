package com.retroarch.browser.dirfragment;

import java.io.File;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import com.retroarch.R;
import com.retroarch.browser.FileWrapper;
import com.retroarch.browser.IconAdapter;
import com.retroarch.browser.ModuleWrapper;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivityFuture;
import com.retroarch.browser.retroactivity.RetroActivityPast;

/**
 * {@link DirectoryFragment} that implements core autodetect.
 * <p>
 * Basically, how it works is the user selects a file.
 * Then, we iterate over all the cores and check what their supported extensions are.
 * Then, if any cores contain the supported extension, they are added to a list and
 * displayed to the user to choose from.
 * <p>
 * The only exception is if only one core matches the extension of the chosen file.
 * In this case, we just attempt to launch the core with that file directly.
 */
// TODO: This is ugly as hell. Clean this up sometime.
//       For example, maybe breaking this out into two fragments
//       to handle the behavior would be better. One for browsing,
//       one for handling the list of selectable cores.
public final class DetectCoreDirectoryFragment extends DirectoryFragment
{
	private ListView backingListView = null;
	private boolean inFileBrowser = true;
	private ArrayList<String> supportedCorePaths = new ArrayList<String>();

	/**
	 * Retrieves a new instance of a DetectCoreDirectoryFragment
	 * with a title specified by the given resource ID.
	 * 
	 * @param titleResId String resource ID for the title
	 *                   of this DetectCoreDirectoryFragment.
	 * 
	 * @return A new instance of a DetectCoreDirectoryFragment.
	 */
	public static DetectCoreDirectoryFragment newInstance(int titleResId)
	{
		final DetectCoreDirectoryFragment dFrag = new DetectCoreDirectoryFragment();
		final Bundle bundle = new Bundle();
		bundle.putInt("titleResId", titleResId);
		dFrag.setArguments(bundle);

		return dFrag;
	}

	/**
	 * Returns everything after the last ‘.’ of the given file name or path.
	 */
	public static String getFileExt(String filePath)
	{
        	int i = filePath.lastIndexOf('.');
        	if (i >= 0)
                	return filePath.substring(i+1).toLowerCase();
        	return ""; // No extension
	}
	
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		backingListView = (ListView) inflater.inflate(R.layout.line_list, container, false);
		backingListView.setOnItemClickListener(onItemClickListener);

		// Get whether or not we were in the file browser prior to recreation.
		if (savedInstanceState != null)
		{
			inFileBrowser = savedInstanceState.getBoolean("inFileBrowser");
			
			if (inFileBrowser)
				backStack = savedInstanceState.getParcelableArrayList("BACKSTACK");
		}

		// Set the dialog title.
		if (inFileBrowser)
			getDialog().setTitle(getArguments().getInt("titleResId"));
		else
			getDialog().setTitle(R.string.multiple_cores_detected);

		// If we're in the file browser, reinitialize the file list adapter.
		if (savedInstanceState == null || inFileBrowser)
		{
			// Setup the list
			adapter = new IconAdapter<FileWrapper>(getActivity(), R.layout.line_list_item);
			backingListView.setAdapter(adapter);
		}

		if (inFileBrowser)
		{
			if (backStack == null || backStack.isEmpty())
			{
				backStack = new ArrayList<BackStackItem>();
				String startPath = (startDirectory == null || startDirectory.isEmpty()) ? Environment
						.getExternalStorageDirectory().getPath() : startDirectory;
						backStack.add(new BackStackItem(startPath, false));
			}

			wrapFiles();
		}
		else // Rebuild the core adapter.
		{
			supportedCorePaths = savedInstanceState.getStringArrayList("coreFilePaths");
			CoreSelectionAdapter adapter = new CoreSelectionAdapter(getActivity(), android.R.layout.simple_list_item_2);

			for (String path : supportedCorePaths)
			{
				ModuleWrapper mw = new ModuleWrapper(getActivity(), path);
				adapter.add(new CoreItem(mw.getInternalName(), mw.getEmulatedSystemName()));
			}

			backingListView.setAdapter(adapter);
		}

		return backingListView;
	}

	@Override
	public void onSaveInstanceState(Bundle outState)
	{
		// Save whether or not we're in core selection or the file browser.
		outState.putBoolean("inFileBrowser", inFileBrowser);

		if (!inFileBrowser)
			outState.putStringArrayList("coreFilePaths", supportedCorePaths);
	}

	private File chosenFile = null;
	private final OnItemClickListener onItemClickListener = new OnItemClickListener()
	{
		@Override
		public void onItemClick(AdapterView<?> parent, View view, int position, long id)
		{
			final FileWrapper item = adapter.getItem(position);

			if (inFileBrowser && item.isParentItem() && backStack.get(backStack.size() - 1).parentIsBack)
			{
				backStack.remove(backStack.size() - 1);
				wrapFiles();
				return;
			}

			final File selected = item.isParentItem() ? listedDirectory.getParentFile() : item.getFile();
			if (inFileBrowser && selected.isDirectory())
			{
				Log.d("DirectoryFrag", "Is Directory.");
				backStack.add(new BackStackItem(selected.getAbsolutePath(), !item.isParentItem()));
				wrapFiles();
				return;
			}
			else if (inFileBrowser && selected.isFile())
			{
				String filePath = selected.getAbsolutePath();
				chosenFile = selected;

				// Attempt to get the file extension.
				String fileExt = getFileExt(filePath);

				if (fileExt.equals(“zip”))
				{
        				ZipFile zipFile = new ZipFile(chosenFile);
        				Enumeration<? extends ZipEntry> entries = zipFile.entries();

        				// Try to handle the case of small text files bundles with ROMs.
        				long largestEntry = Long.MIN_VALUE;

        				while (entries.hasMoreElements())
        				{
                				ZipEntry zipEntry = entries.nextElement();
                				if (zipEntry.getCompressedSize()) >= largestEntry)
                				{
                        				largestEntry = zipEntry.getCompressedSize();
                        				fileExt = getFileExt(zipEntry.getName());
                				}
        				}
				}

				// Enumerate the cores and check for the extension
				File coreDir = new File(getActivity().getApplicationInfo().dataDir + File.separator + "cores");
				File[] coreFiles = coreDir.listFiles();
				List<ModuleWrapper>supportedCores = new ArrayList<ModuleWrapper>();

				for (File core : coreFiles)
				{
					ModuleWrapper mw = new ModuleWrapper(getActivity(), core);

					if (mw.getSupportedExtensions().contains(fileExt))
					{
						supportedCores.add(mw);
						supportedCorePaths.add(mw.getUnderlyingFile().getAbsolutePath());
					}
				}

				// Display a toast if no cores are detected.
				if (supportedCores.isEmpty())
				{
					Toast.makeText(getActivity(), R.string.no_cores_detected, Toast.LENGTH_SHORT).show();
				}
				// If only one core is supported, launch the content directly.
				else if (supportedCores.size() == 1)
				{
					launchCore(selected.getPath(), supportedCores.get(0).getUnderlyingFile().getPath());
				}
				// Otherwise build the list for the user to choose from.
				else if (supportedCores.size() > 1)
				{
					// Not in the file browser any more.
					inFileBrowser = false;

					// Modify the title to notify of multiple cores.
					getDialog().setTitle(R.string.multiple_cores_detected);

					// Add all the cores to the adapter and swap it with the one in the ListView.
					final CoreSelectionAdapter csa = new CoreSelectionAdapter(getActivity(), android.R.layout.simple_list_item_2);

					for (ModuleWrapper core : supportedCores)
					{
						csa.add(new CoreItem(core.getInternalName(), core.getEmulatedSystemName()));
					}

					backingListView.setAdapter(csa);
				}
			}
			else // Selection made
			{
				launchCore(chosenFile.getPath(), DetectCoreDirectoryFragment.this.supportedCorePaths.get(position));
			}
		}
	};

	private void launchCore(String contentPath, String corePath)
	{
		Intent retro;
		if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB))
			retro = new Intent(getActivity(), RetroActivityFuture.class);
		else
			retro = new Intent(getActivity(), RetroActivityPast.class);

		UserPreferences.updateConfigFile(getActivity());
		String current_ime = Settings.Secure.getString(getActivity().getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
		retro.putExtra("ROM", contentPath);
		retro.putExtra("LIBRETRO", corePath);
		retro.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(getActivity()));
		retro.putExtra("IME", current_ime);
		startActivity(retro);
		dismiss();
	}

	// Used to represent data in the ListView after the user chooses an item.
	private static final class CoreItem
	{
		public final String Title;
		public final String Subtitle;

		public CoreItem(String title, String subtitle)
		{
			this.Title    = title;
			this.Subtitle = subtitle;
		}
	}

	// Adapter that the ListView is flipped to after choosing a file to launch.
	private static final class CoreSelectionAdapter extends ArrayAdapter<CoreItem>
	{
		private final int resourceId;

		/**
		 * Constructor
		 * 
		 * @param context    The current {@link Context}.
		 * @param resourceId The resource ID for a layout file.
		 */
		public CoreSelectionAdapter(Context context, int resourceId)
		{
			super(context, resourceId);

			this.resourceId = resourceId;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent) 
		{
			if (convertView == null)
			{
				LayoutInflater inflater = LayoutInflater.from(getContext());
				convertView = inflater.inflate(resourceId, parent, false);
			}

			final CoreItem core = getItem(position);
			if (core != null)
			{
				final TextView title    = (TextView) convertView.findViewById(android.R.id.text1);
				final TextView subtitle = (TextView) convertView.findViewById(android.R.id.text2);
				
				if (title != null)
					title.setText(core.Title);
				
				if (subtitle != null)
					subtitle.setText(core.Subtitle);
			}

			return convertView;
		}
	}
}
