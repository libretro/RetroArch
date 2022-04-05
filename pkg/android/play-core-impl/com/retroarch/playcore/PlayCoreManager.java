package com.retroarch.playcore;

import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;
import com.google.android.play.core.splitinstall.SplitInstallSessionState;
import com.google.android.play.core.splitinstall.SplitInstallStateUpdatedListener;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;
import com.google.android.play.core.tasks.OnFailureListener;
import com.google.android.play.core.tasks.OnSuccessListener;
import com.retroarch.browser.retroactivity.RetroActivityCommon;

import java.util.Collections;
import java.util.List;

public class PlayCoreManager {

    private PlayCoreManager() {}

    private RetroActivityCommon activity;
    private static PlayCoreManager instance;

    public static int INSTALL_STATUS_DOWNLOADING = 0;
    public static int INSTALL_STATUS_INSTALLING = 1;
    public static int INSTALL_STATUS_INSTALLED = 2;
    public static int INSTALL_STATUS_FAILED = 3;

    private final SplitInstallStateUpdatedListener listener = new SplitInstallStateUpdatedListener() {
        @Override
        public void onStateUpdate(SplitInstallSessionState state) {
            List<String> moduleNames = state.moduleNames();
            String[] coreNames = new String[moduleNames.size()];

            for(int i = 0; i < moduleNames.size(); i++) {
                coreNames[i] = activity.unsanitizeCoreName(moduleNames.get(i));
            }

            switch(state.status()) {
                case SplitInstallSessionStatus.DOWNLOADING:
                    activity.coreInstallStatusChanged(coreNames, INSTALL_STATUS_DOWNLOADING, state.bytesDownloaded(), state.totalBytesToDownload());
                    break;
                case SplitInstallSessionStatus.INSTALLING:
                    activity.coreInstallStatusChanged(coreNames, INSTALL_STATUS_INSTALLING, state.bytesDownloaded(), state.totalBytesToDownload());
                    break;
                case SplitInstallSessionStatus.INSTALLED:
                    activity.updateSymlinks();

                    activity.coreInstallStatusChanged(coreNames, INSTALL_STATUS_INSTALLED, state.bytesDownloaded(), state.totalBytesToDownload());
                    break;
                case SplitInstallSessionStatus.FAILED:
                    activity.coreInstallStatusChanged(coreNames, INSTALL_STATUS_FAILED, state.bytesDownloaded(), state.totalBytesToDownload());
                    break;
            }
        }
    };

    public static PlayCoreManager getInstance() {
        if (instance == null) {
            instance = new PlayCoreManager();
        }

        return instance;
    }

    public void onCreate(RetroActivityCommon newActivity) {
        activity = newActivity;

        SplitInstallManager manager = SplitInstallManagerFactory.create(activity);
        manager.registerListener(listener);
    }

    public void onDestroy() {
        SplitInstallManager manager = SplitInstallManagerFactory.create(activity);
        manager.unregisterListener(listener);

        activity = null;
    }

    public String[] getInstalledModules() {
        SplitInstallManager manager = SplitInstallManagerFactory.create(activity);
        return manager.getInstalledModules().toArray(new String[0]);
    }

    public void downloadCore(final String coreName) {
        SplitInstallManager manager = SplitInstallManagerFactory.create(activity);
        SplitInstallRequest request = SplitInstallRequest.newBuilder()
                .addModule(activity.sanitizeCoreName(coreName))
                .build();

        manager.startInstall(request)
                .addOnSuccessListener(new OnSuccessListener<Integer>() {
                    @Override
                    public void onSuccess(Integer result) {
                        activity.coreInstallInitiated(coreName, true);
                    }
                })

                .addOnFailureListener(new OnFailureListener() {
                    @Override
                    public void onFailure(Exception e) {
                        activity.coreInstallInitiated(coreName, false);
                    }
                });
    }

    public void deleteCore(String coreName) {
        SplitInstallManager manager = SplitInstallManagerFactory.create(activity);
        manager.deferredUninstall(Collections.singletonList(activity.sanitizeCoreName(coreName)));
    }
}
