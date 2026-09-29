package org.openxwa.xwaquest.m7c;

import org.libsdl.app.SDLActivity;
import android.app.AlertDialog;
import android.os.*;
import android.util.Log;
import android.view.*;
import android.view.inputmethod.InputMethodManager;
import android.widget.*;
import java.io.*;

/**
 * M7B Quest input: laser pointer as flight stick, trigger as fire/confirm.
 *
 * Quest 3S in panel VR mode only forwards laser-pointer touch events; no
 * thumbstick, sensor, or gamepad data reaches the Activity.  We therefore
 * map the pointer position relative to screen centre as proportional
 * pitch/roll control during flight:
 *
 *   Pointer above  centre → pitch up    (+g_ctrlAxisX)
 *   Pointer below  centre → pitch down  (-g_ctrlAxisX)
 *   Pointer left   centre → roll left   (-g_ctrlAxisY)
 *   Pointer right  centre → roll right  (+g_ctrlAxisY)
 *   Pointer far from centre → stronger deflection (proportional)
 *
 * Trigger click → RETURN (menus / confirm) or SPACE (fire in flight).
 */
public final class RuntimeActivity extends SDLActivity {
    private static native boolean nativeName(String name);
    private static native void nativeKey(int scancode);
    private static native int nativeStatus();
    private static native boolean nativeInFlight();
    private static native void nativeSetQuestAxes(float pitch, float roll, float yaw, float throttle);
    private static native void nativeXrSetPanelState(int state);
    private final Handler handler = new Handler(Looper.getMainLooper());

    private int previousStatus = -1;
    private int captures;
    private boolean nameRequested, pointerDown;
    private long hoverCount;
    /* Last pointer position (surface-relative) for flight control */
    private float lastPtrX, lastPtrY;

    @Override protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        handler.postDelayed(new Runnable() {
            @Override public void run() {
                if(isFinishing()) return;
                int status=nativeStatus();
                if(status!=previousStatus) {
                    Log.i("XWAQuestM7C","INPUT_STATUS="+status);
                    previousStatus=status;
                    if(status==1 && !nameRequested) { nameRequested=true; nameDialog(); }
                    handler.postDelayed(() -> captureFrame("state"+status),2000);
                }
                handler.postDelayed(this,500);
            }
        },1000);
    }

    /**
     * Generic motion event handler.
     * Hover-move events carry the laser pointer position; we forward them
     * to SDL as mouse movement AND to native as flight axes when in flight.
     */
    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) {
        int a=e.getActionMasked();
        if(a==MotionEvent.ACTION_HOVER_MOVE || a==MotionEvent.ACTION_HOVER_ENTER) {
            if(mSurface!=null) {
                int[] loc=new int[2]; mSurface.getLocationInWindow(loc);
                float x=e.getX()-loc[0], y=e.getY()-loc[1];
                lastPtrX=x; lastPtrY=y;
                SDLActivity.onNativeMouse(pointerDown?1:0,MotionEvent.ACTION_HOVER_MOVE,x,y,false);
                ++hoverCount;
                /* In flight mode, use pointer offset from centre as flight axes */
                if(nativeInFlight()) {
                    float w=mSurface.getWidth(), h=mSurface.getHeight();
                    float cx=w/2f, cy=h/2f;
                    /* Normalise to [-1,+1]: above centre = positive pitch */
                    float dx=(x-cx)/(w/2f);  /* +1 = right edge */
                    float dy=(y-cy)/(h/2f);  /* +1 = bottom edge */
                    /* Dead-zone near centre to avoid drift */
                    float dz=0.08f;
                    if(Math.abs(dx)<dz) dx=0;
                    if(Math.abs(dy)<dz) dy=0;
                    /* pitch: negative dy = pointer above centre = look up = positive */
                    float pitch = -dy;
                    /* roll:  positive dx = pointer right = roll right = positive */
                    float roll  =  dx;
                    nativeSetQuestAxes(pitch, roll, 0, 0);
                    if(hoverCount%120==0)
                        Log.i("XWAQuestM7C","FLIGHT_PTR dx="+dx+" dy="+dy+" pitch="+pitch+" roll="+roll);
                }
                return true;
            }
        }
        return super.dispatchGenericMotionEvent(e);
    }

    /**
     * Touch event handler.
     * ACTION_DOWN / ACTION_UP map to mouse clicks (for menus) and also
     * inject RETURN (menus) or SPACE (fire in flight) via native key.
     */
    @Override public boolean dispatchTouchEvent(MotionEvent e) {
        if(mSurface==null)return super.dispatchTouchEvent(e);
        int a=e.getActionMasked();
        if(a==MotionEvent.ACTION_DOWN || a==MotionEvent.ACTION_UP || a==MotionEvent.ACTION_MOVE || a==MotionEvent.ACTION_CANCEL) {
            int[] loc=new int[2]; mSurface.getLocationInWindow(loc);
            float x=e.getX()-loc[0], y=e.getY()-loc[1];
            if(a==MotionEvent.ACTION_DOWN)pointerDown=true;
            if(a==MotionEvent.ACTION_UP || a==MotionEvent.ACTION_CANCEL)pointerDown=false;
            SDLActivity.onNativeMouse(pointerDown?1:0,a==MotionEvent.ACTION_CANCEL?MotionEvent.ACTION_UP:a,x,y,false);
            if(a!=MotionEvent.ACTION_MOVE)Log.i("XWAQuestM7C","POINTER action="+a+" down="+pointerDown+" x="+x+" y="+y);
            /* Trigger click: RETURN (menus) or SPACE (fire in flight) */
            if(a==MotionEvent.ACTION_DOWN) {
                if(nativeInFlight()) {
                    nativeKey(44); /* SDL_SCANCODE_SPACE → fire weapon */
                    Log.i("XWAQuestM7C","FIRE trigger (SPACE)");
                } else if(nativeStatus()!=1) {
                    nativeKey(40); /* SDL_SCANCODE_RETURN → menu confirm */
                }
            }
            return true;
        }
        return super.dispatchTouchEvent(e);
    }

    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        int key=e.getKeyCode();
        int scan=key==KeyEvent.KEYCODE_BUTTON_A?40:key==KeyEvent.KEYCODE_BUTTON_B?41:
            key==KeyEvent.KEYCODE_DPAD_LEFT?80:key==KeyEvent.KEYCODE_DPAD_RIGHT?79:
            key==KeyEvent.KEYCODE_DPAD_UP?82:key==KeyEvent.KEYCODE_DPAD_DOWN?81:0;
        if(scan!=0) {
            if(e.getAction()==KeyEvent.ACTION_DOWN && e.getRepeatCount()==0){nativeKey(scan);Log.i("XWAQuestM7C","BUTTON key="+key+" scan="+scan);}
            return true;
        }
        if(key==KeyEvent.KEYCODE_BUTTON_X && e.getAction()==KeyEvent.ACTION_DOWN && e.getRepeatCount()==0 && nativeStatus()==1){nameDialog();return true;}
        return super.dispatchKeyEvent(e);
    }

    private void nameDialog() {
        EditText edit=new EditText(this);
        edit.setSingleLine(true);
        edit.setInputType(android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        edit.setFilters(new android.text.InputFilter[]{new android.text.InputFilter.LengthFilter(12)});
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("Nombre del piloto")
            .setMessage("1–12 letras o números. El juego creará el piloto cuando confirmes en su pantalla.")
            .setView(edit).setNegativeButton("Cancelar",null).setPositiveButton("Enviar al juego",null).create();
        dialog.setOnShowListener(d -> {
            edit.requestFocus();
            dialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
            ((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).showSoftInput(edit,InputMethodManager.SHOW_IMPLICIT);
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
                String name=edit.getText().toString();
                if(!name.matches("[A-Za-z0-9]{1,12}")){edit.setError("Usa 1–12 letras o números");return;}
                if(!nativeName(name)){edit.setError("El juego aún no está esperando un nombre");return;}
                ((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(edit.getWindowToken(),0);
                dialog.dismiss();
                mSurface.requestFocus();
            });
        });
        dialog.show();
    }

    private void captureFrame(String reason) {
        if(isFinishing()||mSurface==null||!mSurface.getHolder().getSurface().isValid()||mSurface.getWidth()==0)return;
        android.graphics.Bitmap bmp=android.graphics.Bitmap.createBitmap(mSurface.getWidth(),mSurface.getHeight(),android.graphics.Bitmap.Config.ARGB_8888);
        final int id=++captures;
        PixelCopy.request(mSurface,bmp,result -> {
            try{
                if(result==PixelCopy.SUCCESS){
                    File target=new File(getFilesDir(),"m7-"+id+"-"+reason+".png");
                    try(FileOutputStream stream=new FileOutputStream(target)){bmp.compress(android.graphics.Bitmap.CompressFormat.PNG,100,stream);}
                    Log.i("XWAQuestM7","CAPTURE="+target.getName());
                }
            }catch(Exception e){Log.e("XWAQuestM7","capture failed",e);}finally{bmp.recycle();}
        },handler);
    }

    @Override protected void onDestroy(){handler.removeCallbacksAndMessages(null);super.onDestroy();}
    @Override protected String[] getLibraries(){return new String[]{"openxr_loader","SDL3","OpenXWAM7C"};}
    @Override protected String[] getArguments(){
        File data=new File(getFilesDir(),"GameData");
        if(!data.isDirectory())throw new IllegalStateException("GameData M7C missing");
        return new String[]{"--game-data="+data.getAbsolutePath(),"skipintro"};
    }
}
