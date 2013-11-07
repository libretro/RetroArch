package com.retroarch.browser.coremanager.fragments;

import java.io.File;

import com.retroarch.R;
import com.retroarch.browser.ModuleWrapper;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

/**
 * Fragment that displays information about a selected core.
 */
public final class InstalledCoreInfoFragment extends ListFragment
{
	/**
	 * Creates a new instance of a InstalledCoreInfoFragment.
	 * 
	 * @param core The wrapped core to represent.
	 * 
	 * @return a new instance of a InstalledCoreInfoFragment.
	 */
	public static InstalledCoreInfoFragment newInstance(ModuleWrapper core)
	{
		InstalledCoreInfoFragment cif = new InstalledCoreInfoFragment();

		// Set the core path as an argument.
		// This will allow us to re-retrieve information if the Fragment
		// is destroyed upon state changes
		Bundle args = new Bundle();
		args.putString("core_path", core.getUnderlyingFile().getPath());
		cif.setArguments(args);

		return cif;
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		// Inflate the view.
		ListView infoView = (ListView) inflater.inflate(R.layout.coremanager_listview, container, false);

		// Get the appropriate info providers.
		final Bundle args = getArguments();
		final ModuleWrapper core = new ModuleWrapper(getActivity(), new File(args.getString("core_path")));

		// Initialize the core info.
		CoreInfoAdapter adapter = new CoreInfoAdapter(getActivity(), android.R.layout.simple_list_item_2);
		adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_displayNameTitle),  core.getDisplayName()));
		adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_internalNameTitle), core.getInternalName()));
		adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_systemNameTitle),   core.getEmulatedSystemName()));
		adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_manufacterer),      core.getManufacturer()));
		adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_emu_author),        core.getEmulatorAuthors()));
		adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_licenseTitle),      core.getCoreLicense()));

		// Set the list adapter.
		infoView.setAdapter(adapter);

		return infoView;
	}

	/**
	 * Adapter backing this InstalledCoreInfoFragment
	 */
	private final class CoreInfoAdapter extends ArrayAdapter<InstalledCoreInfoItem>
	{
		private final Context context;
		private final int resourceId;

		/**
		 * Constructor
		 * 
		 * @param context    The current {@link Context}.
		 * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
		 */
		public CoreInfoAdapter(Context context, int resourceId)
		{
			super(context, resourceId);

			this.context = context;
			this.resourceId = resourceId;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent)
		{
			if (convertView == null)
			{
				LayoutInflater vi = LayoutInflater.from(context);
				convertView = vi.inflate(resourceId, parent, false);
			}

			final InstalledCoreInfoItem item = getItem(position);
			if (item != null)
			{
				final TextView title    = (TextView) convertView.findViewById(android.R.id.text1);
				final TextView subtitle = (TextView) convertView.findViewById(android.R.id.text2);

				if (title != null)
				{
					title.setText(item.getTitle());
				}

				if (subtitle != null)
				{
					subtitle.setText(item.getSubtitle());
				}
			}

			return convertView;
		}
	}
}
