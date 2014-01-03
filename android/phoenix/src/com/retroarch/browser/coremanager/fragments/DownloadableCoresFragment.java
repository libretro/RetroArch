package com.retroarch.browser.coremanager.fragments;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

/**
 * {@link ListFragment} that is responsible for showing
 * cores that are able to be downloaded or are not installed.
 */
public final class DownloadableCoresFragment extends Fragment
{
	// TODO: Implement complete functionality.

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		TextView tv = new TextView(getActivity());
		tv.setText("In development, just a little longer!");

		return tv;
	}
}
