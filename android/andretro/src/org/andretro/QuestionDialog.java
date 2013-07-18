package org.andretro;

import android.content.*;
import android.os.*;
import android.app.*;

public class QuestionDialog extends android.support.v4.app.DialogFragment
{
	public interface QuestionHandler
	{
		void onAnswer(int aID, QuestionDialog aDialog, boolean aPositive);
	}
	
	/**
	 * Build a question dialog with the specified strings.
	 * 
	 * @param aID The ID to pass to callback.
	 * @param aTitle The title of the dialog.
	 * @param aMessage The message to display in the dialog.
	 * @param aPositive The text on the positive button of the dialog.
	 * @param aNegative The text on the negative button of the dialog.
	 * @return The dialog.
	 */
    public static QuestionDialog newInstance(int aID, String aTitle, String aMessage, String aPositive, String aNegative, Bundle aUserData)
    {
       	// Create the output
        QuestionDialog result = new QuestionDialog();

        // Fill the details
        Bundle args = new Bundle();
        args.putInt("id", aID);
        args.putString("title", aTitle);
        args.putString("message", aMessage);
        args.putString("positive", aPositive);
        args.putString("negative", aNegative);
        args.putBundle("userdata", aUserData);
        result.setArguments(args);
        
        // Done
        return result;
    }
    
    Bundle getUserData()
    {
    	return getArguments().getBundle("userdata");
    }
    
	@Override public Dialog onCreateDialog(Bundle aState)
	{
		final Bundle args = getArguments();
		final int id = args.getInt("id");

		// TODO: Use string resources
		AlertDialog result = new AlertDialog.Builder(getActivity())
				.setTitle(args.getString("title"))
				.setMessage(args.getString("message"))
				.setPositiveButton(args.getString("positive"), new DialogInterface.OnClickListener()
		        {
		            @Override public void onClick(DialogInterface UNUSED, int aWhich)
		            {
		            	((QuestionHandler)getActivity()).onAnswer(id, QuestionDialog.this, true);
		            }
		        })
		        .setNegativeButton(args.getString("negative"), new DialogInterface.OnClickListener()
		        {
		        	@Override public void onClick(DialogInterface UNUSED, int aWhich)
		        	{
		        		((QuestionHandler)getActivity()).onAnswer(id, QuestionDialog.this, false);
		        	}
		        })
		        .create();
		
		return result;
	}
		
	@Override public void onCancel(DialogInterface aDialog)
	{
		((QuestionHandler)getActivity()).onAnswer(getArguments().getInt("id"), QuestionDialog.this, false);
		super.onCancel(aDialog);
	}
}
