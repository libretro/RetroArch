package com.retroarch.browser;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;

import com.retroarch.R;
import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivityFuture;
import com.retroarch.browser.retroactivity.RetroActivityPast;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;
import android.widget.Toast;

/**
 * Represents the {@link ListFragment} responsible
 * for displaying the list of previously played games.
 */
public final class HistorySelection extends DialogFragment
{
	private FragmentActivity ctx;
	private IconAdapter<HistoryWrapper> adapter;
	
	public Intent getRetroActivity()
	{
		if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB))
		{
			return new Intent(ctx, RetroActivityFuture.class);
		}
		return new Intent(ctx, RetroActivityPast.class);
	}

	/**
	 * Creates a statically instantiated instance of HistorySelection.
	 * 
	 * @return a statically instantiated instance of HistorySelection.
	 */
	public static HistorySelection newInstance()
	{
		return new HistorySelection();
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Cache the context
		this.ctx = getActivity();
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		ListView rootView = (ListView) inflater.inflate(R.layout.line_list, container, false);
		rootView.setOnItemClickListener(onItemClickListener);

		// Set the title for this dialog.
		getDialog().setTitle(R.string.load_game_history);

		// Setup the list adapter
		adapter = new IconAdapter<HistoryWrapper>(ctx, R.layout.line_list_item);

		// Populate the adapter
		File history = new File(ctx.getApplicationInfo().dataDir, "retroarch-history.txt");
		try
		{
			BufferedReader br = new BufferedReader(new InputStreamReader(
					new FileInputStream(history)));

			for (;;)
			{
				String game = br.readLine();
				String core = br.readLine();
				String name = br.readLine();
				if (game == null || core == null || name == null)
					break;

				adapter.add(new HistoryWrapper(game, core, name));
			}
			br.close();
		}
		catch (IOException ignored)
		{
		}

		// Set the adapter
		rootView.setAdapter(adapter);
		return rootView;
	}

	private final OnItemClickListener onItemClickListener = new OnItemClickListener()
	{
		@Override
		public void onItemClick(AdapterView<?> listView, View view, int position, long id)
		{
			final HistoryWrapper item = adapter.getItem(position);
			final String gamePath = item.getGamePath();
			final String corePath = item.getCorePath();
	
			// Set the core the selected game uses.
			((MainMenuActivity)getActivity()).setModule(corePath, item.getCoreName());
	
			// Update the config accordingly.
			UserPreferences.updateConfigFile(ctx);
	
			// Launch the game.
			String current_ime = Settings.Secure.getString(ctx.getContentResolver(),
					Settings.Secure.DEFAULT_INPUT_METHOD);
			Toast.makeText(ctx, String.format(getString(R.string.loading_gamepath), gamePath), Toast.LENGTH_SHORT).show();
			Intent retro = getRetroActivity();
			retro.putExtra("ROM", gamePath);
			retro.putExtra("LIBRETRO", corePath);
			retro.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(ctx));
			retro.putExtra("IME", current_ime);
			startActivity(retro);
			dismiss();
		}
	};
}
