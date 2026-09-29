package org.openxwa.xwaquest.m3;

import org.libsdl.app.SDLActivity;
import java.io.File;

/** SDL's real Android lifecycle and SDL_main trampoline, confined to M3. */
public final class RuntimeActivity extends SDLActivity {
    @Override protected String[] getLibraries() {
        return new String[] { "openxr_loader", "SDL3", "OpenXWAM3" };
    }
    @Override protected String[] getArguments() {
        // Explicit empty diagnostic candidate, not an importer or discovery policy.
        // The real setup validator must report the missing original data.
        File candidate = new File(getFilesDir(), "m3-empty-data");
        if (!candidate.isDirectory() && !candidate.mkdirs()) {
            throw new IllegalStateException("Cannot create M3 diagnostic directory");
        }
        return new String[] { "--game-data=" + candidate.getAbsolutePath() };
    }
}
