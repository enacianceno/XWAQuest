/* Input-only adapter. No gameplay calls or writes to simulated entities. */
#include "touch_joystick.h"
#include "internal.h"
#include "host_config.h"
#include "aeron/paths.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/config/modern_controller_options_screen.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/hangar.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight.h"
#include <stdio.h>

static SDL_Joystick *joystick;
static SDL_JoystickID joystick_id;
static TouchState state;
static Uint64 sample_id, next_log, capture_frame;
static unsigned accumulated_pressed, accumulated_released;
static TouchSample latest;
static char guid[33];
int TouchJoystick_Flight(void) {
    return XwaFlightTask_IsActive() && !g_inHangarReady && !FlightDisplay_IsFrontendModalActive();
}
int TouchJoystick_CaptureScreen(void) {
    return capture_frame && sample_id <= capture_frame+1;
}
int TouchJoystick_Init(void) {
    /* Hidden Android window focus is not XR focus. SDL otherwise drops joystick
       updates; XR action availability/focus remains the adapter's input gate. */
    if(!SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1")) return 0;
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_FLIGHT_STICK;
    desc.naxes=4; desc.nbuttons=TOUCH_BUTTONS;
    desc.name="XWAQuest Touch Input1";
    joystick_id=SDL_AttachVirtualJoystick(&desc);
    if (!joystick_id) return 0;
    joystick=SDL_OpenJoystick(joystick_id);
    if (!joystick) { SDL_DetachVirtualJoystick(joystick_id); joystick_id=0; return 0; }
    SDL_GUIDToString(SDL_GetJoystickGUID(joystick),guid,sizeof guid);
    SDL_Log("M8I_DEVICE id=%u guid=%s kind=raw_joystick axes=4 buttons=%d",joystick_id,guid,TOUCH_BUTTONS);
    TouchSample empty={0};
    return TouchJoystick_Update(&empty,0);
}
int TouchJoystick_Update(const TouchSample *sample,float dt) {
    latest=*sample; ++sample_id;
    TouchConvert(&state,sample,dt);
    accumulated_pressed |= state.pressed; accumulated_released |= state.released;
    if (!joystick) return 0;
    for(int i=0;i<4;i++) if(!SDL_SetJoystickVirtualAxis(joystick,i,state.axes[i])) return 0;
    for(int i=0;i<TOUCH_BUTTONS;i++)
        if(!SDL_SetJoystickVirtualButton(joystick,i,(state.buttons & (1u<<i))!=0)) return 0;
    /* Aeron's immediately following SDL event pump consumes these values. */
    return 1;
}
void TouchJoystick_AfterPump(void) {
    Uint64 now=SDL_GetTicks();
    if(now<next_log) return;
    next_log=now+1000;
    const AeronControllerSnapshot *device=XwaControllerMapping_SelectedController();
    SDL_Log("M8I_SAMPLE seq=%llu active=%d flight=%d capture=%d armed=%d XR_sticks=%.3f,%.3f,%.3f,%.3f XR_triggers=%.3f,%.3f XR_grips=%.3f,%.3f XR_digital=%x virtual_axes=%d,%d,%d,%d virtual_buttons=%x pressed_window=%x released_window=%x throttle_lever=%.3f selected_id=%u expected_id=%u raw_buttons=%llx",
        (unsigned long long)sample_id,latest.available,latest.flight,latest.capture,state.armed,
        latest.lx,latest.ly,latest.rx,latest.ry,latest.rt,latest.lt,latest.rg,latest.lg,latest.buttons,
        state.axes[0],state.axes[1],state.axes[2],state.axes[3],state.buttons,
        accumulated_pressed,accumulated_released,state.throttle,
        device ? device->instance_id : 0,joystick_id,
        (unsigned long long)(device ? device->raw_buttons : 0));
    accumulated_pressed=accumulated_released=0;
}
void TouchJoystick_Shutdown(void) {
    if(joystick) { SDL_CloseJoystick(joystick); joystick=NULL; }
    if(joystick_id) SDL_DetachVirtualJoystick(joystick_id);
    joystick_id=0;
}

static void defaults(XwaModernInputOptions *o) {
    XwaControllerOptions *c=&o->controller;
    memset(&c->joystick,0,sizeof c->joystick);
    SDL_strlcpy(c->device.guid,guid,sizeof c->device.guid);
    c->device.path[0]=0; c->device.ordinal=0;
    c->roll_enabled=1; c->rumble_enabled=0;
    for(int i=0;i<4;i++) {
        c->joystick.axes[i].source=i;
        c->joystick.axes[i].deadzone=i==2 ? 0.f : .15f;
    }
    c->joystick.pov_source=-1;
    const uint16_t actions[TOUCH_BUTTONS]={156,116,27,119,121,0,0,0,0,0};
    for(int i=0;i<TOUCH_BUTTONS;i++) {
        c->joystick.buttons[i].kind=AERON_CONTROLLER_DIGITAL_BUTTON;
        c->joystick.buttons[i].index=(uint8_t)i;
        c->joystick.buttons[i].threshold=.5f;
        c->joystick.actions[i]=actions[i];
    }
    o->mouse_flight_enabled=0;
}
int __real_XwaHostConfig_Load(AeronVfs*,XwaHostConfig*,char*,size_t);
int __wrap_XwaHostConfig_Load(AeronVfs *vfs,XwaHostConfig *out,char *error,size_t size) {
    if(!__real_XwaHostConfig_Load(vfs,out,error,size)) return 0;
    if(!joystick || !guid[0]) { SDL_snprintf(error,size,"M8I virtual joystick unavailable"); return 0; }
    defaults(&out->input_defaults);
    /* One-time package-private seed, never reapply over later user bindings.
       Marker also preserves the user's later deliberate 'no device' selection. */
    char marker[2048];
    SDL_snprintf(marker,sizeof marker,"%s/m8input1-bindings-v1",Aeron_UserPath());
    FILE *f=fopen(marker,"rb");
    if(f) { fclose(f); SDL_Log("M8I_BINDINGS preserved=1"); return 1; }
    if(!out->input_options.controller.device.guid[0]) {
        defaults(&out->input_options);
        if(!XwaHostConfig_SaveInputOptions(vfs,&out->input_options,error,size)) return 0;
        SDL_Log("M8I_BINDINGS seeded=1 actions=156,116,27,119,121 axes=yaw,pitch,throttle,roll");
    } else SDL_Log("M8I_BINDINGS preserved_existing_selection=1");
    f=fopen(marker,"wb");
    if(!f) { SDL_snprintf(error,size,"M8I cannot persist bindings seed marker"); return 0; }
    int ok=fputs("v1\n",f)>=0;
    if(fclose(f)!=0) ok=0;
    if(!ok) { SDL_snprintf(error,size,"M8I cannot finish bindings seed marker"); return 0; }
    return 1;
}

/* Observe values actually consumed by the unmodified native input pipeline.
   Each wrapper calls the original once and returns its exact result. */
int __real_Joystick_PollRawAxesIfEnabled(int*,int*,int*,int*,int);
int __wrap_Joystick_PollRawAxesIfEnabled(int *x,int *y,int *z,int *r,int unused) {
    int buttons=__real_Joystick_PollRawAxesIfEnabled(x,y,z,r,unused);
    static Uint64 next;
    static unsigned seen;
    seen |= (unsigned)buttons;
    Uint64 now=SDL_GetTicks();
    if(now>=next) {
        next=now+1000;
        SDL_Log("M8I_NATIVE seq=%llu joystick_enabled=%u axes=%d,%d,%d,%d buttons=%x seen_window=%x actions=%u,%u,%u,%u,%u previous_native_keymods=%u previous_controlmask=%x previous_throttle_smoothed=%d",
            (unsigned long long)sample_id,g_joystickEnabled,*x,*y,*z,*r,buttons,seen,
            g_gameConfig.joyButtons[0],g_gameConfig.joyButtons[1],g_gameConfig.joyButtons[2],
            g_gameConfig.joyButtons[3],g_gameConfig.joyButtons[4],g_keyMods,g_controlMask,g_throttleSmoothed);
        seen=0;
    }
    return buttons;
}
int __real_XwaModernControllerAxesScreen_Update(int,int*);
int __wrap_XwaModernControllerAxesScreen_Update(int center,int *row) {
    capture_frame=sample_id;
    return __real_XwaModernControllerAxesScreen_Update(center,row);
}
int __real_XwaModernControllerButtonsScreen_Update(int,int*);
int __wrap_XwaModernControllerButtonsScreen_Update(int center,int *row) {
    capture_frame=sample_id;
    return __real_XwaModernControllerButtonsScreen_Update(center,row);
}
