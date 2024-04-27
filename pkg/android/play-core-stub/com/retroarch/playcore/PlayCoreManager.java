package com.retroarch.playcore;

import com.retroarch.browser.retroactivity.RetroActivityCommon;

public class PlayCoreManager {

    private PlayCoreManager() {}

    private static PlayCoreManager instance;

    public static PlayCoreManager getInstance() {
        if (instance == null) {
            instance = new PlayCoreManager();
        }

        return instance;
    }

    public void onCreate(RetroActivityCommon newActivity) {}
    public void onDestroy() {}

    public String[] getInstalledModules() {
        return new String[0];
    }

    public void downloadCore(final String coreName) {}
    public void deleteCore(String coreName) {}
}
