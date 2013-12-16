package com.retroarch.browser.mainmenu.gplwaiver;

import java.io.File;

import com.retroarch.R;
import com.retroarch.browser.ModuleWrapper;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.util.Log;

/**
 * {@link DialogFragment} responsible for displaying
 * the GPL waiver dialog on the first-run of this app.
 */
public final class GPLWaiverDialogFragment extends DialogFragment
{
	/**
	 * Method for statically instatiating a new 
	 * instance of a GPLWaiverDialogFragment.
	 * 
	 * @return a new instance of a GPLWaiverDialogFragment.
	 */
	public static GPLWaiverDialogFragment newInstance()
	{
		return new GPLWaiverDialogFragment();
	}

	@Override
	public Dialog onCreateDialog(Bundle savedInstanceState)
	{
		AlertDialog.Builder gplDialog = new AlertDialog.Builder(getActivity());
		gplDialog.setTitle(R.string.gpl_waiver);
		gplDialog.setMessage(R.string.gpl_waiver_desc);
		gplDialog.setPositiveButton(R.string.keep_cores, null);
		gplDialog.setNegativeButton(R.string.remove_cores, new DialogInterface.OnClickListener()
		{
			@Override
			public void onClick(DialogInterface dialog, int which)
			{
				final File[] libs = new File(getActivity().getApplicationInfo().dataDir, "/cores").listFiles();
				for (final File lib : libs)
				{
					ModuleWrapper module = new ModuleWrapper(getActivity(), lib);
					
					boolean gplv3 = module.getCoreLicense().equals("GPLv3");
					boolean gplv2 = module.getCoreLicense().equals("GPLv2");
					
					if (!gplv3 && !gplv2)
					{
						String libName = lib.getName();
						Log.i("GPL WAIVER", "Deleting non-GPL core " + libName + "...");
						lib.delete();
					}
				}
			}
		});

		return gplDialog.create();
	}
}
