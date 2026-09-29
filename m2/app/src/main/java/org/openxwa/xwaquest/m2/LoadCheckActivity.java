package org.openxwa.xwaquest.m2;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

/** M2 validates dynamic linking only. No SDL, engine or GameData initialization. */
public final class LoadCheckActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        System.loadLibrary("openxr_loader");
        System.loadLibrary("SDL3");
        System.loadLibrary("OpenXWA");
        Log.i("XWAQuestM2", "LOAD_OK Android arm64 OpenXWA/Aeron/FFmpeg; SDL_main NOT invoked");
        TextView status = new TextView(this);
        status.setText("M2: bibliotecas cargadas. Motor sin inicializar.");
        setContentView(status);
    }
}
