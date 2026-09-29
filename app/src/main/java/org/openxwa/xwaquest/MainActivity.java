package org.openxwa.xwaquest;

import android.os.Bundle;
import android.util.Log;
import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
    @Override protected String[] getLibraries() {
        return new String[] { "openxr_loader", "SDL3", "main" };
    }
    @Override protected void onCreate(Bundle state) {
        Log.i("XWAQuest", "BOOT Activity M1 ARM64 SDL_GPU/Vulkan/OpenXR");
        super.onCreate(state);
    }
    @Override protected void onResume() {
        super.onResume();
        Log.i("XWAQuest", "XR Android onResume");
    }
    @Override protected void onPause() {
        Log.i("XWAQuest", "XR Android onPause");
        super.onPause();
    }
}
