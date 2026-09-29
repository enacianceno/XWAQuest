package org.openxwa.xwaquest.m6;

import org.libsdl.app.SDLActivity;
import android.app.AlertDialog;
import android.os.*;
import android.util.Log;
import android.view.*;
import android.view.inputmethod.InputMethodManager;
import android.widget.*;
import java.io.*;

/** M6-only native IME + temporary frontend toolbar. No pilot data writer. */
public final class RuntimeActivity extends SDLActivity {
    private static native boolean nativeName(String name);
    private static native void nativeKey(int scancode);
    private static native int nativeStatus();
    private final Handler handler = new Handler(Looper.getMainLooper());


    private int previousStatus = -1;
    private int captures;
    private boolean nameRequested, pointerDown;
    private int lastStick;
    private long hoverCount;
    @Override protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        handler.postDelayed(new Runnable() {
            @Override public void run() {
                if(isFinishing()) return;
                int status=nativeStatus();


                if(status!=previousStatus) {
                    Log.i("XWAQuestM6","INPUT_STATUS="+status);
                    previousStatus=status;
                    if(status==1 && !nameRequested) { nameRequested=true; nameDialog(); }
                    handler.postDelayed(() -> captureFrame("state"+status),2000);
                }
                handler.postDelayed(this,500);
            }
        },1000);
    }
    // Quest panel hover may use FINGER/UNKNOWN instead of SDL's MOUSE tool type.
    // Accept real Android hover coordinates; never derive pointing from a pressed trigger.
    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) {
        int a=e.getActionMasked();
        if(a==MotionEvent.ACTION_HOVER_MOVE || a==MotionEvent.ACTION_HOVER_ENTER) {
            if(mSurface!=null) {
                int[] loc=new int[2]; mSurface.getLocationInWindow(loc);
                float x=e.getX()-loc[0], y=e.getY()-loc[1];
                SDLActivity.onNativeMouse(pointerDown?1:0,MotionEvent.ACTION_HOVER_MOVE,x,y,false);
                if(++hoverCount==1 || hoverCount%300==0)
                    Log.i("XWAQuestM6","HOVER count="+hoverCount+" source="+e.getSource()+" tool="+e.getToolType(0)+" x="+x+" y="+y);
                return true;
            }
        }
        if((e.getSource()&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK) {
            float x=e.getAxisValue(MotionEvent.AXIS_X), y=e.getAxisValue(MotionEvent.AXIS_Y);
            if(Math.abs(e.getAxisValue(MotionEvent.AXIS_HAT_X))>Math.abs(x))x=e.getAxisValue(MotionEvent.AXIS_HAT_X);
            if(Math.abs(e.getAxisValue(MotionEvent.AXIS_HAT_Y))>Math.abs(y))y=e.getAxisValue(MotionEvent.AXIS_HAT_Y);
            int scan=Math.abs(x)>Math.abs(y)?(x>.6f?79:x<-.6f?80:0):(y>.6f?81:y<-.6f?82:0);
            if(scan!=0 && scan!=lastStick){nativeKey(scan);Log.i("XWAQuestM6","STICK scan="+scan);}
            lastStick=scan;
        }
        return super.dispatchGenericMotionEvent(e);
    }
    @Override public boolean dispatchTouchEvent(MotionEvent e) {
        if(mSurface==null)return super.dispatchTouchEvent(e);
        int a=e.getActionMasked();
        if(a==MotionEvent.ACTION_DOWN || a==MotionEvent.ACTION_UP || a==MotionEvent.ACTION_MOVE || a==MotionEvent.ACTION_CANCEL) {
            int[] loc=new int[2]; mSurface.getLocationInWindow(loc);
            float x=e.getX()-loc[0], y=e.getY()-loc[1];
            if(a==MotionEvent.ACTION_DOWN)pointerDown=true;
            if(a==MotionEvent.ACTION_UP || a==MotionEvent.ACTION_CANCEL)pointerDown=false;
            SDLActivity.onNativeMouse(pointerDown?1:0,a==MotionEvent.ACTION_CANCEL?MotionEvent.ACTION_UP:a,x,y,false);
            if(a!=MotionEvent.ACTION_MOVE)Log.i("XWAQuestM6","POINTER action="+a+" down="+pointerDown+" x="+x+" y="+y);
            return true; // Prevent duplicate touch-to-mouse clicks in SDL.
        }
        return super.dispatchTouchEvent(e);
    }
    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        int key=e.getKeyCode();
        int scan=key==KeyEvent.KEYCODE_BUTTON_A?40:key==KeyEvent.KEYCODE_BUTTON_B?41:
            key==KeyEvent.KEYCODE_DPAD_LEFT?80:key==KeyEvent.KEYCODE_DPAD_RIGHT?79:
            key==KeyEvent.KEYCODE_DPAD_UP?82:key==KeyEvent.KEYCODE_DPAD_DOWN?81:0;
        if(scan!=0) {
            if(e.getAction()==KeyEvent.ACTION_DOWN && e.getRepeatCount()==0){nativeKey(scan);Log.i("XWAQuestM6","BUTTON key="+key+" scan="+scan);}
            return true;
        }
        // X provides explicit access to text input without covering the game.
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
        if(isFinishing() || mSurface==null || !mSurface.getHolder().getSurface().isValid() || mSurface.getWidth()==0)return;
        android.graphics.Bitmap bmp=android.graphics.Bitmap.createBitmap(mSurface.getWidth(),mSurface.getHeight(),android.graphics.Bitmap.Config.ARGB_8888);
        final int id=++captures;
        PixelCopy.request(mSurface,bmp,result -> {
            try {
                if(result==PixelCopy.SUCCESS) {
                    File target=new File(getFilesDir(),"m6-"+id+"-"+reason+".png");
                    try(FileOutputStream stream=new FileOutputStream(target)){bmp.compress(android.graphics.Bitmap.CompressFormat.PNG,100,stream);}
                    Log.i("XWAQuestM6","CAPTURE="+target.getName());
                }
            }catch(Exception e){Log.e("XWAQuestM6","capture failed",e);}finally{bmp.recycle();}
        },handler);
    }
    @Override protected void onDestroy(){handler.removeCallbacksAndMessages(null);super.onDestroy();}
    @Override protected String[] getLibraries(){return new String[]{"openxr_loader","SDL3","OpenXWAM6"};}
    @Override protected String[] getArguments(){
        File data=new File(getFilesDir(),"GameData");
        if(!data.isDirectory())throw new IllegalStateException("GameData M6 missing");
        // Existing application option, only to reach the interactive frontend directly.
        return new String[]{"--game-data="+data.getAbsolutePath(),"skipintro"};
    }
}

