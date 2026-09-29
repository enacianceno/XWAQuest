package org.openxwa.xwaquest.vrprobe;

import org.libsdl.app.SDLActivity;
import java.io.File;

/** VR-PROBE minimal activity. No input adaptation: the probe is a passive
 *  stereo viewer driven by OpenXR head poses. */
public final class VrProbeActivity extends SDLActivity {
    @Override protected String[] getLibraries() {
        return new String[]{"openxr_loader", "SDL3", "OpenXWAVRP"};
    }
    @Override protected String[] getArguments() {
        File data = new File(getFilesDir(), "GameData");
        if (!data.isDirectory()) throw new IllegalStateException("GameData VRPROBE missing");
        return new String[]{"--game-data=" + data.getAbsolutePath(), "skipintro"};
    }
}
