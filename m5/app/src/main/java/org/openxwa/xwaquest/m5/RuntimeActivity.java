package org.openxwa.xwaquest.m5;

import org.libsdl.app.SDLActivity;
import android.util.Log;
import java.io.File;

/** Separate M5 lifecycle; GameData is external to the APK and app-scoped. */
public final class RuntimeActivity extends SDLActivity {
    @Override protected void onCreate(android.os.Bundle saved) {
        super.onCreate(saved);
        android.os.Handler handler = new android.os.Handler(getMainLooper());
        for (int second : new int[] {2, 5, 10, 20, 35, 60, 100}) {
            handler.postDelayed(() -> captureFrame(second), second * 1000L);
        }
    }
    private void captureFrame(int second) {
        if (isFinishing() || mSurface == null || !mSurface.getHolder().getSurface().isValid()) return;
        android.graphics.Bitmap bitmap = android.graphics.Bitmap.createBitmap(mSurface.getWidth(), mSurface.getHeight(), android.graphics.Bitmap.Config.ARGB_8888);
        android.view.PixelCopy.request(mSurface, bitmap, result -> {
            try {
                if (result == android.view.PixelCopy.SUCCESS) {
                    File output = new File(getFilesDir(), "m5-frame-" + second + ".png");
                    try (java.io.FileOutputStream stream = new java.io.FileOutputStream(output)) {
                        bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, stream);
                    }
                    Log.i("XWAQuestM5", "CAPTURE " + output.getName() + " surface=" + bitmap.getWidth() + "x" + bitmap.getHeight());
                } else Log.w("XWAQuestM5", "PixelCopy result=" + result);
            } catch (Exception e) { Log.e("XWAQuestM5", "Capture failed", e); }
            finally { bitmap.recycle(); }
        }, new android.os.Handler(getMainLooper()));
    }
    @Override protected String[] getLibraries() {
        return new String[] { "openxr_loader", "SDL3", "OpenXWAM5" };
    }
    @Override protected String[] getArguments() {
        File data = new File(getFilesDir(), "GameData");
        if (!data.isDirectory() && !data.mkdirs()) {
            throw new IllegalStateException("App-scoped GameData storage unavailable");
        }
        Log.i("XWAQuestM5", "GameData root=" + data.getAbsolutePath());
        return new String[] { "--game-data=" + data.getAbsolutePath() };
    }
}



