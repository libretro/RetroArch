package com.retroarch.browser.preferences.fragments.util;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

import com.retroarch.R;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.Preference;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.support.v4.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

public class PreferenceListFragment extends ListFragment
{
	private static final String PREFERENCES_TAG = "android:preferences";
    private PreferenceManager mPreferenceManager;
    private ListView mList;
    private boolean mHavePrefs;
    private boolean mInitDone;

    /**
     * The starting request code given out to preference framework.
     */
    private static final int FIRST_REQUEST_CODE = 100;

    private static final int MSG_BIND_PREFERENCES = 1;
    private final Handler mHandler = new Handler()
    {
        @Override
        public void handleMessage(Message msg)
        {
            switch (msg.what)
            {
                case MSG_BIND_PREFERENCES:
                    bindPreferences();
                    break;
            }
        }
    };

    private final Runnable mRequestFocus = new Runnable()
    {
        @Override
        public void run()
        {
            mList.focusableViewAvailable(mList);
        }
    };

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle b)
    {
        View view = inflater.inflate(R.layout.preference_list_content, container, false);
        view.setScrollBarStyle(View.SCROLLBARS_INSIDE_OVERLAY);

        return view;
    }

    @Override
    public void onDestroyView()
    {
        super.onDestroyView();

        // Kill the list
        mList = null;

        // Remove callbacks and messages.
        mHandler.removeCallbacks(mRequestFocus);
        mHandler.removeMessages(MSG_BIND_PREFERENCES);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState)
    {
        super.onActivityCreated(savedInstanceState);

        if (mHavePrefs)
        {
        	bindPreferences();
        }

        // Done initializing.
        mInitDone = true;

        if (savedInstanceState != null)
        {
            Bundle container = savedInstanceState.getBundle(PREFERENCES_TAG);
            if (container != null)
            {
                final PreferenceScreen preferenceScreen = getPreferenceScreen();
                if (preferenceScreen != null)
                {
                    preferenceScreen.restoreHierarchyState(container);
                }
            }
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        mPreferenceManager = onCreatePreferenceManager();

        postBindPreferences();
    }

    @Override
    public void onStop()
    {
        super.onStop();

        try
        {
            Method m = PreferenceManager.class.getDeclaredMethod("dispatchActivityStop");
            m.setAccessible(true);
            m.invoke(mPreferenceManager);
        }
        catch(Exception e)
        {
            e.printStackTrace();
        }
    }

    @Override
    public void onDestroy()
    {
        super.onDestroy();

        try
        {
            Method m = PreferenceManager.class.getDeclaredMethod("dispatchActivityDestroy");
            m.setAccessible(true);
            m.invoke(mPreferenceManager);
        }
        catch(Exception e)
        {
            e.printStackTrace();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState)
    {
        super.onSaveInstanceState(outState);

        final PreferenceScreen preferenceScreen = getPreferenceScreen();
        if (preferenceScreen != null)
        {
            Bundle container = new Bundle();
            preferenceScreen.saveHierarchyState(container);
            outState.putBundle(PREFERENCES_TAG, container);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);

        try
        {
            Method m = PreferenceManager.class.getDeclaredMethod("dispatchActivityResult", int.class, int.class, Intent.class);
            m.setAccessible(true);
            m.invoke(mPreferenceManager, requestCode, resultCode, data);
        }
        catch(Exception e)
        {
            e.printStackTrace();
        }
    }

    /**
     * Posts a message to bind the preferences to the list view.
     * <p>
     * Binding late is preferred as any custom preference types created in
     * {@link #onCreate(Bundle)} are able to have their views recycled.
     */
    private void postBindPreferences()
    {
        if (mHandler.hasMessages(MSG_BIND_PREFERENCES))
        {
            return;
        }

        mHandler.obtainMessage(MSG_BIND_PREFERENCES).sendToTarget();
    }

    private void bindPreferences()
    {
        final PreferenceScreen preferenceScreen = getPreferenceScreen();
        if (preferenceScreen != null)
        {
            preferenceScreen.bind(getListView());
        }
    }

    /**
     * Creates the {@link PreferenceManager}.
     * 
     * @return The {@link PreferenceManager} used by this fragment.
     */
    private PreferenceManager onCreatePreferenceManager()
    {
        try
        {
            Constructor<PreferenceManager> c = PreferenceManager.class.getDeclaredConstructor(Activity.class, int.class);
            c.setAccessible(true);

            PreferenceManager preferenceManager = c.newInstance(getActivity(), FIRST_REQUEST_CODE);
            return preferenceManager;
        }
        catch(Exception e)
        {
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Gets the {@link PreferenceManager} used by this fragment.
     * 
     * @return The {@link PreferenceManager} used by this fragment.
     */
    public PreferenceManager getPreferenceManager()
    {
        return mPreferenceManager;
    }

    /**
     * Sets the root of the preference hierarchy that this fragment is showing.
     * 
     * @param preferenceScreen The root {@link PreferenceScreen} of the preference hierarchy.
     */
    public void setPreferenceScreen(PreferenceScreen preferenceScreen)
    {
        try
        {
            Method m = PreferenceManager.class.getDeclaredMethod("setPreferences", PreferenceScreen.class);
            m.setAccessible(true);
            boolean result = (Boolean) m.invoke(mPreferenceManager, preferenceScreen);
            if (result && preferenceScreen != null)
            {
                mHavePrefs = true;
                if (mInitDone)
                {
                    postBindPreferences();
                }
            }
        }
        catch(Exception e)
        {
            e.printStackTrace();
        }
    }

    /**
     * Gets the root of the preference hierarchy that this fragment is showing.
     * 
     * @return The {@link PreferenceScreen} that is the root of the preference
     *         hierarchy.
     */
    public PreferenceScreen getPreferenceScreen()
    {
        try
        {
            Method m = PreferenceManager.class.getDeclaredMethod("getPreferenceScreen");
            m.setAccessible(true);
            return (PreferenceScreen) m.invoke(mPreferenceManager);
        }
        catch(Exception e)
        {
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Adds preferences from activities that match the given {@link Intent}.
     * 
     * @param intent The {@link Intent} to query activities.
     */
    public void addPreferencesFromIntent(Intent intent)
    {
        throw new UnsupportedOperationException("addPreferencesFromIntent not implemented yet.");
    }

    /**
     * Inflates the given XML resource and adds the preference hierarchy to the current
     * preference hierarchy.
     * 
     * @param preferencesResId The XML resource ID to inflate.
     */
    public void addPreferencesFromResource(int preferencesResId)
    {
        try
        {
            Method m = PreferenceManager.class.getDeclaredMethod("inflateFromResource", Context.class, int.class, PreferenceScreen.class);
            m.setAccessible(true);
            PreferenceScreen prefScreen = (PreferenceScreen) m.invoke(mPreferenceManager, getActivity(), preferencesResId, getPreferenceScreen());
            setPreferenceScreen(prefScreen);
        }
        catch(Exception e)
        {
            e.printStackTrace();
        }
    }

    /**
     * Finds a {@link Preference} based on its key.
     *
     * @param key The key of the preference to retrieve.
     *
     * @return The {@link Preference} with the key, or null.
     *
     * @see PreferenceGroup#findPreference(CharSequence)
     */
    public Preference findPreference(CharSequence key)
    {
        if (mPreferenceManager == null)
        {
            return null;
        }

        return mPreferenceManager.findPreference(key);
    }

    public ListView getListView()
    {
        ensureList();
        return mList;
    }

    private void ensureList()
    {
        if (mList != null)
        {
            return;
        }

        final View root = getView();
        if (root == null)
        {
            throw new IllegalStateException("Content view not yet created");
        }

        final View rawListView = root.findViewById(android.R.id.list);
        if (!(rawListView instanceof ListView))
        {
            throw new RuntimeException("Content has view with id attribute 'android.R.id.list' that is not a ListView class");
        }

        mList = (ListView)rawListView;
        if (mList == null)
        {
            throw new RuntimeException("Your content must have a ListView whose id attribute is 'android.R.id.list'");
        }

        mHandler.post(mRequestFocus);
    }
}