package com.retroarch.browser;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import com.retroarch.R;
import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

/**
 * {@link ListFragment} subclass that displays the list
 * of selectable cores for emulating games.
 */
public final class CoreSelection extends DialogFragment
{
	private IconAdapter<ModuleWrapper> adapter;

	/**
	 * Creates a statically instantiated instance of CoreSelection.
	 * 
	 * @return a statically instantiated instance of CoreSelection.
	 */
	public static CoreSelection newInstance()
	{
		return new CoreSelection();
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		// Inflate the ListView we're using.
		final ListView coreList = (ListView) inflater.inflate(R.layout.line_list, container, false);
		coreList.setOnItemClickListener(onClickListener);

		// Set the title of the dialog
		getDialog().setTitle(R.string.select_libretro_core);

		final String cpuInfo = UserPreferences.readCPUInfo();
		final boolean cpuIsNeon = cpuInfo.contains("neon");

		// Populate the list
		final List<ModuleWrapper> cores = new ArrayList<ModuleWrapper>();
		final File[] libs = new File(getActivity().getApplicationInfo().dataDir, "/cores").listFiles();
		for (final File lib : libs) {
			String libName = lib.getName();

			// Never append a NEON lib if we don't have NEON.
			if (libName.contains("neon") && !cpuIsNeon)
				continue;

			// If we have a NEON version with NEON capable CPU,
			// never append a non-NEON version.
			if (cpuIsNeon && !libName.contains("neon"))
			{
				boolean hasNeonVersion = false;
				for (final File lib_ : libs)
				{
					String otherName = lib_.getName();
					String baseName = libName.replace(".so", "");
					if (otherName.contains("neon") && otherName.startsWith(baseName))
					{
						hasNeonVersion = true;
						break;
					}
				}

				if (hasNeonVersion)
					continue;
			}

			cores.add(new ModuleWrapper(getActivity(), lib));
		}

		// Sort the list of cores alphabetically
		Collections.sort(cores);

		// Initialize the IconAdapter with the list of cores.
		adapter = new IconAdapter<ModuleWrapper>(getActivity(), R.layout.line_list_item, cores);
		coreList.setAdapter(adapter);

		return coreList;
	}

	private final OnItemClickListener onClickListener = new OnItemClickListener()
	{
		@Override
		public void onItemClick(AdapterView<?> listView, View view, int position, long id)
		{
			final ModuleWrapper item = adapter.getItem(position);
			((MainMenuActivity)getActivity()).setModule(item.getUnderlyingFile().getAbsolutePath(), item.getText());
			UserPreferences.updateConfigFile(getActivity());
			dismiss();
		}
	};
}
