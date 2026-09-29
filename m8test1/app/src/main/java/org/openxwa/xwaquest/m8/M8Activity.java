package org.openxwa.xwaquest.m8;
import java.io.File;
import android.util.Log;
import org.libsdl.app.SDLActivity;
/** Isolated app data; no reads/writes to the installed M7 pilot or game data. */
public class M8Activity extends SDLActivity {
    @Override protected String[] getLibraries() {
        return new String[]{"openxr_loader", "SDL3", "OpenXWAM8"};
    }
    @Override protected String[] getArguments() {
        File data = new File(getFilesDir(), "GameData");
        if (!data.isDirectory()) Log.e("XWAQuestM8", "M8_DATA_MISSING: provision private GameData before test: " + data);
        return new String[]{"--game-data=" + data.getAbsolutePath(), "skipintro"};
    }
}
