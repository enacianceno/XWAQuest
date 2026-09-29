/* M7 input adapter. Emits real SDL events; never writes pilot state or files. */
#include "internal.h"
#include <jni.h>
#include <pthread.h>
#include <string.h>
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static char pending_name[13];
static int text_index, erase_count, pending_key, status;
static SDL_Scancode release_scan;
static void key_event(SDL_Scancode scan) {
    SDL_Event e = {0};
    e.type=SDL_EVENT_KEY_DOWN; e.key.windowID=SDL_GetWindowID(g_aeron.window);
    e.key.scancode=scan; e.key.key=SDL_GetKeyFromScancode(scan,0,false); e.key.down=true;
    SDL_PushEvent(&e);
    release_scan=scan; /* Keep key_down visible for one real input snapshot. */
}
JNIEXPORT jboolean JNICALL Java_org_openxwa_xwaquest_m7_RuntimeActivity_nativeName(JNIEnv* env,jclass cls,jstring value) {
    (void)cls;
    const char* name=(*env)->GetStringUTFChars(env,value,NULL);
    if(!name) return JNI_FALSE;
    size_t len=strlen(name); int valid=len>0 && len<=12;
    for(size_t i=0;i<len;i++) if(!((name[i]>='A'&&name[i]<='Z')||(name[i]>='a'&&name[i]<='z')||(name[i]>='0'&&name[i]<='9'))) valid=0;
    pthread_mutex_lock(&lock);
    if(status!=1) valid=0;
    if(valid) { SDL_strlcpy(pending_name,name,sizeof pending_name); text_index=0; erase_count=12; pending_key=0; SDL_Log("M7 name queued length=%u",(unsigned)len); }
    pthread_mutex_unlock(&lock);
    (*env)->ReleaseStringUTFChars(env,value,name);
    return valid ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL Java_org_openxwa_xwaquest_m7_RuntimeActivity_nativeKey(JNIEnv* env,jclass cls,jint scan) {
    (void)env;(void)cls;
    if(scan!=SDL_SCANCODE_RETURN && scan!=SDL_SCANCODE_ESCAPE && scan!=SDL_SCANCODE_UP && scan!=SDL_SCANCODE_DOWN && scan!=SDL_SCANCODE_LEFT && scan!=SDL_SCANCODE_RIGHT && scan!=SDL_SCANCODE_SPACE) return;
    pthread_mutex_lock(&lock); pending_key=scan; pthread_mutex_unlock(&lock);
}
#include "xwa_runtime/runtime/flight_task.h"
JNIEXPORT jint JNICALL Java_org_openxwa_xwaquest_m7_RuntimeActivity_nativeStatus(JNIEnv* env,jclass cls) {
    (void)env;(void)cls;
    pthread_mutex_lock(&lock); int result=status;
    if(status==1 && (erase_count || pending_name[0])) result=4;
    pthread_mutex_unlock(&lock); return result;
}
JNIEXPORT jboolean JNICALL Java_org_openxwa_xwaquest_m7_RuntimeActivity_nativeInFlight(JNIEnv* env,jclass cls) {
    (void)env;(void)cls;
    return XwaFlightTask_IsActive() ? JNI_TRUE : JNI_FALSE;
}
/* Native SDL gamepads need not generate Activity key events on Quest. */
static int controller_key(void) {
    static unsigned previous[AERON_CONTROLLER_MAX];
    static SDL_JoystickID identities[AERON_CONTROLLER_MAX];
    int result=0;
    const SDL_Scancode scans[]={SDL_SCANCODE_RETURN,SDL_SCANCODE_ESCAPE,
        SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN};
    for(int i=0;i<AERON_CONTROLLER_MAX;i++) {
        AeronControllerDevice* d=&g_aeron.controllers[i];
        if(!d->gamepad){previous[i]=0;identities[i]=0;continue;}
        if(identities[i]!=d->instance_id) {
            identities[i]=d->instance_id;previous[i]=0;
            SDL_Log("M7 PAD attached slot=%d id=%d name=%s",i,(int)d->instance_id,SDL_GetGamepadName(d->gamepad));
        }
        int x=SDL_GetGamepadAxis(d->gamepad,SDL_GAMEPAD_AXIS_LEFTX);
        int y=SDL_GetGamepadAxis(d->gamepad,SDL_GAMEPAD_AXIS_LEFTY);
        unsigned mask=(SDL_GetGamepadButton(d->gamepad,SDL_GAMEPAD_BUTTON_SOUTH)?1u:0u)
            |(SDL_GetGamepadButton(d->gamepad,SDL_GAMEPAD_BUTTON_EAST)?2u:0u)
            |((x < -18000 || SDL_GetGamepadButton(d->gamepad,SDL_GAMEPAD_BUTTON_DPAD_LEFT))?4u:0u)
            |((x > 18000 || SDL_GetGamepadButton(d->gamepad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT))?8u:0u)
            |((y < -18000 || SDL_GetGamepadButton(d->gamepad,SDL_GAMEPAD_BUTTON_DPAD_UP))?16u:0u)
            |((y > 18000 || SDL_GetGamepadButton(d->gamepad,SDL_GAMEPAD_BUTTON_DPAD_DOWN))?32u:0u);
        if(mask!=previous[i])SDL_Log("M7 PAD slot=%d mask=%u x=%d y=%d",i,mask,x,y);
        /* Debug: log South button state every time controller_key is called */
        {
            int south=SDL_GetGamepadButton(d->gamepad,SDL_GAMEPAD_BUTTON_SOUTH);
            int east=SDL_GetGamepadButton(d->gamepad,SDL_GAMEPAD_BUTTON_EAST);
            static int prev_south=-1;
            if(south!=prev_south) { SDL_Log("M7 PAD South=%d East=%d mask=%u",south,east,mask); prev_south=south; }
        }
        unsigned pressed=mask & ~previous[i];previous[i]=mask;
        for(int b=0;b<6;b++)if((pressed&(1u<<b)) && !result)result=scans[b];
    }
    return result;
}
int32_t __real_Aeron_BeginFrame(void);
int32_t __wrap_Aeron_BeginFrame(void) {
    if(release_scan) {
        SDL_Event e={0}; e.type=SDL_EVENT_KEY_UP;
        e.key.windowID=SDL_GetWindowID(g_aeron.window);
        e.key.scancode=release_scan; e.key.key=SDL_GetKeyFromScancode(release_scan,0,false);
        SDL_PushEvent(&e); release_scan=0;
    }
    /* Wait for actual focus after closing IME/dialog. Do not fabricate focus. */
    if(g_aeron.window && (SDL_GetWindowFlags(g_aeron.window)&SDL_WINDOW_INPUT_FOCUS)) {
        int pad_key=controller_key();
        pthread_mutex_lock(&lock);
        if(!pending_key && pad_key)pending_key=pad_key;
        if(erase_count) { key_event(SDL_SCANCODE_BACKSPACE); --erase_count; }
        else if(pending_name[0]) {
            static char event_text[2];
            event_text[0]=pending_name[text_index++]; event_text[1]=0;
            SDL_Event e={0}; e.type=SDL_EVENT_TEXT_INPUT; e.text.windowID=SDL_GetWindowID(g_aeron.window); e.text.text=event_text;
            SDL_PushEvent(&e);
            if(!pending_name[text_index]) { pending_name[0]=0; SDL_Log("M7 text dispatched through SDL_TEXT_INPUT"); }
        } else if(pending_key) { key_event((SDL_Scancode)pending_key); SDL_Log("M7 SDL key scancode=%d",pending_key); pending_key=0; }
        pthread_mutex_unlock(&lock);
    }
    return __real_Aeron_BeginFrame();
}
int __real_FrontendDialog_PromptForPilotName(char* out);
int __wrap_FrontendDialog_PromptForPilotName(char* out) {
    int result=__real_FrontendDialog_PromptForPilotName(out);
    pthread_mutex_lock(&lock);
    if(status==0) { status=1; SDL_Log("M7 pilot name prompt ready"); }
    pthread_mutex_unlock(&lock);
    if(result) SDL_Log("M7 real pilot prompt accepted name=%s",out?out:"");
    return result;
}
int __real_Pilot_CreateNew(const char* name);
int __wrap_Pilot_CreateNew(const char* name) {
    SDL_Log("M7 Pilot_CreateNew BEGIN name=%s",name);
    int result=__real_Pilot_CreateNew(name);
    SDL_Log("M7 Pilot_CreateNew END result=%d name=%s",result,name);
    pthread_mutex_lock(&lock);status=result?2:3;pthread_mutex_unlock(&lock);
    return result;
}
#include "xwa/frontend/frontend_text.h"
int __real_FrontendText_DrawEditableField(FrontendRect*,char*,int,int,unsigned int,const char*);
int __wrap_FrontendText_DrawEditableField(FrontendRect* rect,char* text,int maxChars,int fieldId,unsigned int font,const char* ignored) {
    static char previous[32];
    int result=__real_FrontendText_DrawEditableField(rect,text,maxChars,fieldId,font,ignored);
    if(fieldId==0 && strcmp(previous,text)) {
        SDL_strlcpy(previous,text,sizeof previous);
        SDL_Log("M7 REAL_EDIT text=%s accepted=%d",text,result);
    }
    return result;
}

