package com.retroarch.browser.coremanager.fragments;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentTransaction;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.retroarch.R;
import com.retroarch.browser.ModuleWrapper;

/**
 * Underlying {@link Fragment} that manages layout functionality
 * for the two fragments that rest inside of this one.
 */
public class InstalledCoresManagerFragment extends Fragment implements InstalledCoresFragment.OnCoreItemClickedListener
{
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		View v = inflater.inflate(R.layout.coremanager_installed_cores_base, container, false);

		final Fragment installedCores = new InstalledCoresFragment();
		final FragmentTransaction ft = getChildFragmentManager().beginTransaction();
		ft.replace(R.id.installed_cores_fragment_container1, installedCores);
		ft.commit();

		return v;
	}

	@Override
	public void onCoreItemClicked(int position, ModuleWrapper core)
	{
		// If this view does not exist, it means the screen
		// is not considered 'large' and thus, we use the single fragment layout.
		if (getView().findViewById(R.id.installed_cores_fragment_container2) == null)
		{
			InstalledCoreInfoFragment cif = InstalledCoreInfoFragment.newInstance(core);
			FragmentTransaction ft = getChildFragmentManager().beginTransaction();
			ft.replace(R.id.installed_cores_fragment_container1, cif);
			ft.addToBackStack(null);
			ft.commit();
		}
		else // Large screen
		{
			InstalledCoreInfoFragment cif = InstalledCoreInfoFragment.newInstance(core);
			FragmentTransaction ft = getChildFragmentManager().beginTransaction();
			ft.replace(R.id.installed_cores_fragment_container2, cif);
			ft.commit();
		}
	}
}
