package com.retroarch.browser.coremanager.fragments;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import org.jsoup.Connection;
import org.jsoup.Jsoup;
import org.jsoup.nodes.Element;
import org.jsoup.select.Elements;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.ListFragment;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.ListView;
import android.widget.Toast;

import com.retroarch.R;

/**
 * {@link ListFragment} that is responsible for showing
 * cores that are able to be downloaded or are not installed.
 */
public final class DownloadableCoresFragment extends ListFragment
{
	// List of TODOs.
	// - Eventually make the core downloader capable of directory-based browsing from the base URL.
	// - Allow for 'repository'-like core downloading.
	// - Clean this up a little better. It can likely be way more organized.
	// - Don't re-download the info files on orientation changes.
	// - Use a loading wheel when core retrieval is being done. User may think something went wrong otherwise.
	// - Check the info directory for an info file before downloading it. Can save bandwidth this way (and list load times would be faster).
	// - Should probably display a dialog or a toast message when the Internet connection process fails.

	/**
	 * Dictates what actions will occur when a core download completes.
	 * <p>
	 * Acts like a callback so that communication between fragments is possible.
	 */
	public interface OnCoreDownloadedListener
	{
		/** The action that will occur when a core is successfully downloaded. */
		void onCoreDownloaded();
	}

	private static final String BUILDBOT_BASE_URL = "http://buildbot.libretro.com";
	private static final String BUILDBOT_CORE_URL_ARM = BUILDBOT_BASE_URL + "/nightly/android/latest/armeabi-v7a/";
	private static final String BUILDBOT_CORE_URL_X86 = BUILDBOT_BASE_URL + "/nightly/android/latest/x86/";
	private static final String BUILDBOT_INFO_URL = BUILDBOT_BASE_URL + "/assets/info/";

	private OnCoreDownloadedListener coreDownloadedListener = null;

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		super.onCreateView(inflater, container, savedInstanceState);
		final ListView coreList = (ListView) inflater.inflate(R.layout.coremanager_listview, container, false);
		registerForContextMenu(coreList);

		final DownloadableCoresAdapter adapter = new DownloadableCoresAdapter(getActivity(), android.R.layout.simple_list_item_2);
		adapter.setNotifyOnChange(true);
		coreList.setAdapter(adapter);

		coreDownloadedListener = (OnCoreDownloadedListener) getActivity();

		new PopulateCoresListOperation(adapter).execute();

		return coreList;
	}

	@Override
	public void onListItemClick(final ListView lv, final View v, final int position, final long id)
	{
		super.onListItemClick(lv, v, position, id);
		final DownloadableCore core = (DownloadableCore) lv.getItemAtPosition(position);

		// Prompt the user for confirmation on downloading the core.
		AlertDialog.Builder notification = new AlertDialog.Builder(getActivity());
		notification.setMessage(String.format(getString(R.string.download_core_confirm_msg), core.getCoreName()));
		notification.setTitle(R.string.download_core_confirm_title);
		notification.setNegativeButton(R.string.no, null);
		notification.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialog, int which)
			{
				// Begin downloading the core.
				new DownloadCoreOperation(getActivity(), core.getCoreName()).execute(core.getCoreURL(), core.getShortURLName());
			}
		});
		notification.show();
	}

	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo)
	{
		super.onCreateContextMenu(menu, v, menuInfo);

		menu.setHeaderTitle(R.string.downloadable_cores_ctx_title);

		MenuInflater inflater = getActivity().getMenuInflater();
		inflater.inflate(R.menu.downloadable_cores_context_menu, menu);
	}

	@Override
	public boolean onContextItemSelected(MenuItem item)
	{
		final AdapterContextMenuInfo info = (AdapterContextMenuInfo)item.getMenuInfo();

		switch (item.getItemId())
		{
			case R.id.go_to_wiki_ctx_item:
			{
				String coreUrlPart = ((DownloadableCore)getListView().getItemAtPosition(info.position)).getCoreName().replace(" ", "_");
				Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://wiki.libretro.com/index.php?title=" + coreUrlPart));
				startActivity(browserIntent);
				return true;
			}

			default:
				return super.onContextItemSelected(item);
		}
	}

	// Async event responsible for populating the Downloadable Cores list.
	private static final class PopulateCoresListOperation extends AsyncTask<Void, Void, ArrayList<DownloadableCore>>
	{
		// Acts as an object reference to an adapter in a list view.
		private DownloadableCoresAdapter adapter;

		/**
		 * Constructor
		 *
		 * @param adapter The adapter to asynchronously update.
		 */
		public PopulateCoresListOperation(DownloadableCoresAdapter adapter)
		{
			this.adapter = adapter;
		}

		@Override
		protected ArrayList<DownloadableCore> doInBackground(Void... params)
		{
			try
			{
				final Connection core_connection = Build.CPU_ABI.startsWith("arm") ? Jsoup.connect(BUILDBOT_CORE_URL_ARM)
				                                                                   : Jsoup.connect(BUILDBOT_CORE_URL_X86);
				final Elements coreElements = core_connection.get().body().getElementsByClass("fb-n").select("a");

				final ArrayList<DownloadableCore> downloadableCores = new ArrayList<DownloadableCore>();

				// NOTE: Start from 1 to skip the ".." (parent directory element)
				//       Set this to zero if directory-based browsing becomes a thing.
				for (int i = 1; i < coreElements.size(); i++)
				{
					Element coreElement = coreElements.get(i);

					final String coreURL = BUILDBOT_BASE_URL + coreElement.attr("href");
					final String coreName = coreURL.substring(coreURL.lastIndexOf("/") + 1);
					final String infoURL = BUILDBOT_INFO_URL + coreName.replace(".so.zip", ".info");

					downloadableCores.add(new DownloadableCore(getCoreName(infoURL), coreURL));
				}

				return downloadableCores;
			}
			catch (IOException e)
			{
				Log.e("PopulateCoresListOperation", e.toString());

				// Make a dummy entry to notify an error.
				return new ArrayList<DownloadableCore>();
			}
		}

		@Override
		protected void onPostExecute(ArrayList<DownloadableCore> result)
		{
			super.onPostExecute(result);

			if (result.isEmpty())
			{
				Toast.makeText(adapter.getContext(), R.string.download_core_list_error, Toast.LENGTH_SHORT).show();
			}
			else
			{
				for (int i = 0; i < result.size(); i++)
				{
					adapter.add(result.get(i));
				}
			}
		}

		// Literally downloads the info file, writes it, and parses it for the corename key/value pair.
		// AKA an argument for having a manifest file on the server.
		//
		// This makes list loading take way longer than it should.
		//
		// One way this can be improved is by checking the info directory for
		// existing info files that match the core. Eliminating the download retrieval.
		private String getCoreName(String urlPath)
		{
			String name = "";

			try
			{
				final URL url = new URL(urlPath);
				final BufferedReader br = new BufferedReader(new InputStreamReader(url.openStream()));
				final StringBuilder sb = new StringBuilder();
			
				String str = "";
				while ((str = br.readLine()) != null)
					sb.append(str + "\n");
				br.close();

				// Write the info file.
				File outputPath = new File(adapter.getContext().getApplicationInfo().dataDir + "/info/", urlPath.substring(urlPath.lastIndexOf('/') + 1));
				BufferedWriter bw = new BufferedWriter(new FileWriter(outputPath));
				bw.append(sb);
				bw.close();
	
				// Now read the core name
				String[] lines = sb.toString().split("\n");
				for (int i = 0; i < lines.length; i++)
				{
					if (lines[i].contains("corename"))
					{
						// Gross
						name = lines[i].split("=")[1].trim().replace("\"", "");
						break;
					}
				}
			}
			catch (FileNotFoundException fnfe)
			{
				// Can't find the info file. Name it the same thing as the package.
				final int start = urlPath.lastIndexOf('/') + 1;
				final int end = urlPath.lastIndexOf('.');
				if (end == -1)
					name = urlPath.substring(start);
				else
					name = urlPath.substring(start, end);
			}
			catch (IOException ioe)
			{
				name = "Report this: " + ioe.getMessage();
			}

			return name;
		}
	}

	// Executed when the user confirms a core download.
	private final class DownloadCoreOperation extends AsyncTask<String, Integer, Void>
	{
		private final ProgressDialog dlg;
		private final Context ctx;
		private final String coreName;

		/**
		 * Constructor
		 *
		 * @param ctx      The current {@link Context}.
		 * @param coreName The name of the core being downloaded.
		 */
		public DownloadCoreOperation(Context ctx, String coreName)
		{
			this.dlg = new ProgressDialog(ctx);
			this.ctx = ctx;
			this.coreName = coreName;
		}

		@Override
		protected void onPreExecute()
		{
			super.onPreExecute();

			dlg.setMessage(String.format(ctx.getString(R.string.downloading_msg), coreName));
			dlg.setCancelable(false);
			dlg.setCanceledOnTouchOutside(false);
			dlg.setIndeterminate(false);
			dlg.setMax(100);
			dlg.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
			dlg.show();
		}

		@Override
		protected Void doInBackground(String... params)
		{
			InputStream input = null;
			OutputStream output = null;
			HttpURLConnection connection = null;
			try
			{
				URL url = new URL(params[0]);
				connection = (HttpURLConnection) url.openConnection();
				connection.connect();

				if (connection.getResponseCode() != HttpURLConnection.HTTP_OK)
				{
					Log.i("DownloadCoreOperation", "HTTP response code not OK. Response code: " + connection.getResponseCode());
					return null;
				}

				// Set up the streams
				final int fileLen = connection.getContentLength();
				final File zipPath = new File(ctx.getApplicationInfo().dataDir + "/cores/", params[1]);
				input = new BufferedInputStream(connection.getInputStream(), 8192);
				output = new FileOutputStream(zipPath);

				// Download and write to storage.
				long totalDownloaded = 0;
				byte[] buffer = new byte[4096];
				int countBytes = 0;
				while ((countBytes = input.read(buffer)) != -1)
				{
					totalDownloaded += countBytes;
					if (fileLen > 0)
						publishProgress((int) (totalDownloaded * 100 / fileLen));

					output.write(buffer, 0, countBytes);
				}

				unzipCore(zipPath);
			}
			catch (IOException ignored)
			{
				// Can't really do anything to recover.
			}
			finally
			{
				try
				{
					if (output != null)
						output.close();

					if (input != null)
						input.close();
				}
				catch (IOException ignored)
				{
				}

				if (connection != null)
					connection.disconnect();
			}

			return null;
		}

		@Override
		protected void onProgressUpdate(Integer... progress)
		{
			super.onProgressUpdate(progress);

			dlg.setProgress(progress[0]);
		}

		@Override
		protected void onPostExecute(Void result)
		{
			super.onPostExecute(result);

			if (dlg.isShowing())
				dlg.dismiss();

			// Invoke callback to update the installed cores list.
			coreDownloadedListener.onCoreDownloaded();
		}
	}

	// Java 6 ladies and gentlemen.
	private static void unzipCore(File zipFile)
	{
		ZipInputStream zis = null;

		try
		{
			zis = new ZipInputStream(new FileInputStream(zipFile));
			ZipEntry entry = zis.getNextEntry();

			while (entry != null)
			{
				File file = new File(zipFile.getParent(), entry.getName());

				FileOutputStream fos = new FileOutputStream(file);
				int len = 0;
				byte[] buffer = new byte[4096];
				while ((len = zis.read(buffer)) != -1)
				{
					fos.write(buffer, 0, len);
				}
				fos.close();

				entry = zis.getNextEntry();
			}
		}
		catch (IOException ignored)
		{
			// Can't do anything.
		}
		finally
		{
			try
			{
				if (zis != null)
				{
					zis.closeEntry();
					zis.close();
				}
			}
			catch (IOException ignored)
			{
				// Can't do anything
			}

			zipFile.delete();
		}
	}
}
