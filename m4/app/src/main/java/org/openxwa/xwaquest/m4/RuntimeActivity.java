package org.openxwa.xwaquest.m4;

import org.libsdl.app.SDLActivity;
import android.util.Log;
import java.io.File;

/** Separate M4 lifecycle; GameData is external to the APK and app-scoped. */
public final class RuntimeActivity extends SDLActivity {
    @Override protected String[] getLibraries() {
        return new String[] { "openxr_loader", "SDL3", "OpenXWAM4" };
    }
    @Override protected String[] getArguments() {
        File data = new File(getFilesDir(), "GameData");
        if (!data.isDirectory() && !data.mkdirs()) {
            throw new IllegalStateException("App-scoped GameData storage unavailable");
        }
        Log.i("XWAQuestM4", "GameData root=" + data.getAbsolutePath());
        return new String[] { "--game-data=" + data.getAbsolutePath() };
    }
}

