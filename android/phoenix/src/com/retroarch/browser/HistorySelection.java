package com.retroarch.browser;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;

import com.retroarch.R;
import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;
import android.widget.ListView;
import android.widget.Toast;

public final class HistorySelection extends ListActivity {
	
	private IconAdapter<HistoryWrapper> adapter;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// Setup the layout.
		setContentView(R.layout.line_list);

		// Setup the list
		adapter = new IconAdapter<HistoryWrapper>(this, R.layout.line_list_item);
		setListAdapter(adapter);

		// Set activity title.
		setTitle(R.string.recently_played_games);

		File history = new File(getApplicationInfo().dataDir, "retroarch-history.txt");

		try {
			BufferedReader br = new BufferedReader(new InputStreamReader(
					new FileInputStream(history)));

			for (;;) {
				String game = br.readLine();
				String core = br.readLine();
				String name = br.readLine();
				if (game == null || core == null || name == null)
					break;

				adapter.add(new HistoryWrapper(game, core, name));
			}
			br.close();
		} catch (IOException ex) {
		}
	}

	@Override
	public void onListItemClick(ListView listView, View view, int position, long id) {
		final HistoryWrapper item = adapter.getItem(position);
		final String gamePath = item.getGamePath();
		final String corePath = item.getCorePath();
		
		MainMenuActivity.getInstance().setModule(corePath, item.getCoreName());

		String current_ime = Settings.Secure.getString(getContentResolver(),
				Settings.Secure.DEFAULT_INPUT_METHOD);

		UserPreferences.updateConfigFile(this);

		Toast.makeText(this, String.format(getString(R.string.loading_gamepath), gamePath), Toast.LENGTH_SHORT).show();
		Intent myIntent = new Intent(this, RetroActivity.class);
		myIntent.putExtra("ROM", gamePath);
		myIntent.putExtra("LIBRETRO", corePath);
		myIntent.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(this));
		myIntent.putExtra("IME", current_ime);
		startActivity(myIntent);
		finish();
	}
}
