#include "input_xr.h"
#include "frame_bridge.h"
#include "internal.h"
#include "touch_joystick.h"
#include "vr_flight_bridge.h"
#include "vr_eye_math.h"
#include <math.h>
#include <string.h>
static XrInstance instance;
static XrSession session;
static XrActionSet actions;
enum { CONFIRM, BACK, NAME, EXIT, RC, LC, CLICK, LT, RG, LG, NAV, CURSOR, GRIP_POSE, ACTION_COUNT };
static XrAction action[ACTION_COUNT];
static PFN_xrDestroyActionSet destroy_set;
static PFN_xrSyncActions sync_actions;
static PFN_xrGetActionStateBoolean get_bool;
static PFN_xrGetActionStateFloat get_float;
static PFN_xrGetActionStateVector2f get_vector;
static PFN_xrRequestExitSession request_exit;
static PFN_xrCreateActionSpace create_space;
static PFN_xrLocateSpace locate_space;
static PFN_xrDestroySpace destroy_space;
static XrSpace grip_space = XR_NULL_HANDLE;
static VrStickState stick_state;
static unsigned long long stick_polls;
static int st_prev_pose, st_prev_hover, st_prev_active, st_prev_grabbed;
static int previous[5], click, old_click, prompt, erase, name_index = -1;
static float cursor_x = .5f, cursor_y = .5f;
static Uint64 last_time, exit_since, nav_next;
static SDL_Scancode release_scan;
static int binding_raw, binding_chord;
static int check(XrResult r, const char *operation) {
    if (r == XR_SUCCESS) return 1;
    SDL_Log("M8_INPUT_ERROR operation=%s result=%d", operation, (int)r);
    return 0;
}
int M8_InputInit(XrInstance xr_instance, XrSession xr_session) {
    instance = xr_instance; session = xr_session;
    PFN_xrGetInstanceProcAddr proc = SDL_OpenXR_GetXrGetInstanceProcAddr();
    PFN_xrCreateActionSet create_set;
    PFN_xrCreateAction create_action;
    PFN_xrStringToPath to_path;
    PFN_xrSuggestInteractionProfileBindings suggest;
    PFN_xrAttachSessionActionSets attach;
    if (!proc) return 0;
#define LOAD(variable, name) do { if (!check(proc(instance, #name, (PFN_xrVoidFunction *)&variable), #name) || !variable) return 0; } while (0)
    LOAD(create_set, xrCreateActionSet); LOAD(create_action, xrCreateAction);
    LOAD(to_path, xrStringToPath); LOAD(suggest, xrSuggestInteractionProfileBindings);
    LOAD(attach, xrAttachSessionActionSets); LOAD(destroy_set, xrDestroyActionSet);
    LOAD(sync_actions, xrSyncActions); LOAD(get_bool, xrGetActionStateBoolean);
    LOAD(get_float, xrGetActionStateFloat); LOAD(get_vector, xrGetActionStateVector2f);
    LOAD(request_exit, xrRequestExitSession);
    LOAD(create_space, xrCreateActionSpace); LOAD(locate_space, xrLocateSpace);
    LOAD(destroy_space, xrDestroySpace);
#undef LOAD
    XrActionSetCreateInfo sci = { .type = XR_TYPE_ACTION_SET_CREATE_INFO };
    SDL_strlcpy(sci.actionSetName, "m8_navigation", sizeof sci.actionSetName);
    SDL_strlcpy(sci.localizedActionSetName, "M8 navigation", sizeof sci.localizedActionSetName);
    if (!check(create_set(instance, &sci, &actions), "xrCreateActionSet")) return 0;
    const char *names[ACTION_COUNT] = { "confirm", "back", "pilot_name", "exit", "right_click", "left_click", "click", "left_trigger", "right_grip", "left_grip", "navigate", "cursor", "right_grip_pose" };
    const char *paths[ACTION_COUNT] = {
        "/user/hand/right/input/a/click", "/user/hand/right/input/b/click",
        "/user/hand/left/input/x/click", "/user/hand/left/input/y/click",
        "/user/hand/right/input/thumbstick/click", "/user/hand/left/input/thumbstick/click",
        "/user/hand/right/input/trigger/value", "/user/hand/left/input/trigger/value",
        "/user/hand/right/input/squeeze/value", "/user/hand/left/input/squeeze/value",
        "/user/hand/left/input/thumbstick",
        "/user/hand/right/input/thumbstick",
        "/user/hand/right/input/grip/pose" };
    XrActionSuggestedBinding bindings[ACTION_COUNT] = {0};
    for (int i = 0; i < ACTION_COUNT; ++i) {
        XrActionCreateInfo ci = { .type = XR_TYPE_ACTION_CREATE_INFO };
        ci.actionType = i == GRIP_POSE ? XR_ACTION_TYPE_POSE_INPUT : i < CLICK ? XR_ACTION_TYPE_BOOLEAN_INPUT : i < NAV ? XR_ACTION_TYPE_FLOAT_INPUT : XR_ACTION_TYPE_VECTOR2F_INPUT;
        SDL_strlcpy(ci.actionName, names[i], sizeof ci.actionName);
        SDL_strlcpy(ci.localizedActionName, names[i], sizeof ci.localizedActionName);
        if (!check(create_action(actions, &ci, &action[i]), "xrCreateAction")) return 0;
        bindings[i].action = action[i];
        if (!check(to_path(instance, paths[i], &bindings[i].binding), "xrStringToPath binding")) return 0;
    }
    XrInteractionProfileSuggestedBinding profile = { .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
        .countSuggestedBindings = ACTION_COUNT, .suggestedBindings = bindings };
    if (!check(to_path(instance, "/interaction_profiles/oculus/touch_controller", &profile.interactionProfile), "xrStringToPath profile") ||
        !check(suggest(instance, &profile), "xrSuggestInteractionProfileBindings")) return 0;
    XrSessionActionSetsAttachInfo ai = { .type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
        .countActionSets = 1, .actionSets = &actions };
    if (!check(attach(session, &ai), "xrAttachSessionActionSets")) return 0;
    { XrActionSpaceCreateInfo asci = { .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
        .action = action[GRIP_POSE], .subactionPath = XR_NULL_PATH };
      if (!check(create_space(session, &asci, &grip_space), "xrCreateActionSpace")) return 0; }
    VrStick_Init(&stick_state);
    if(!TouchJoystick_Init()) { M8_Fail("M8I virtual joystick init"); return 0; }
    SDL_Log("M8I_INPUT_READY flight=virtual_joystick menu=legacy_SDL no_system_button_binding=1");
    return 1;
}
static void key(SDL_Scancode scan, int down) {
    SDL_Event e = {0};
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.windowID = SDL_GetWindowID(g_aeron.window);
    e.key.scancode = scan; e.key.key = SDL_GetKeyFromScancode(scan, 0, false); e.key.down = down != 0;
    if (!SDL_PushEvent(&e)) M8_Fail("SDL input event enqueue");
}
int M8_InputPoll(XrTime display_time) {
    if (release_scan) { key(release_scan, 0); release_scan = 0; }
    old_click = click; click = 0;
    XrActiveActionSet set = { .actionSet = actions };
    XrActionsSyncInfo si = { .type = XR_TYPE_ACTIONS_SYNC_INFO, .countActiveActionSets = 1, .activeActionSets = &set };
    XrResult r = sync_actions(session, &si);
    if (r == XR_SESSION_NOT_FOCUSED) {
        TouchSample inactive={0}; inactive.flight=TouchJoystick_Flight();
        if(!TouchJoystick_Update(&inactive,0)) M8_Fail("M8I neutral input");
        VrStick_Init(&stick_state);
        memset(previous, 0, sizeof previous); exit_since = 0; last_time = 0;
        return 0;
    }
    if (!check(r, "xrSyncActions")) { M8_Fail("XR input sync"); return 0; }
    int buttons[6] = {0};
    float analog[4] = {0};
    int vector_active[2]={0};
    XrVector2f vectors[2] = {{0}};
    for (int i = 0; i < ACTION_COUNT; ++i) {
        XrActionStateGetInfo gi = { .type = XR_TYPE_ACTION_STATE_GET_INFO, .action = action[i] };
        if (i == GRIP_POSE) continue; /* pose has no polled state; located below */
        int ok;
        if (i < CLICK) {
            XrActionStateBoolean state = { .type = XR_TYPE_ACTION_STATE_BOOLEAN };
            ok = check(get_bool(session, &gi, &state), "xrGetActionStateBoolean");
            buttons[i] = ok && state.isActive && state.currentState;
        } else if (i < NAV) {
            XrActionStateFloat state = { .type = XR_TYPE_ACTION_STATE_FLOAT };
            ok = check(get_float(session, &gi, &state), "xrGetActionStateFloat");
            analog[i-CLICK] = ok && state.isActive ? state.currentState : 0.f;
        } else {
            XrActionStateVector2f state = { .type = XR_TYPE_ACTION_STATE_VECTOR2F };
            ok = check(get_vector(session, &gi, &state), "xrGetActionStateVector2f");
            if (ok && state.isActive) { vectors[i-NAV]=state.currentState; vector_active[i-NAV]=1; }
        }
        if (!ok) { M8_Fail("XR action state"); return 0; }
    }
    Uint64 now = SDL_GetTicks();
    float dt = last_time ? (float)(now - last_time) / 1000.f : 0.f;
    last_time = now;
    if (dt > .1f) dt = .1f;
    int flight=TouchJoystick_Flight();
    int capture=!flight && TouchJoystick_CaptureScreen();
    int chord=capture && buttons[RC] && buttons[LC];
    if(!capture) binding_raw=0;
    if(chord && !binding_chord) {
        binding_raw=!binding_raw;
        SDL_Log("M8I_BINDING_MODE raw_capture=%d both_stick_clicks_toggle=1",binding_raw);
    }
    binding_chord=chord;
    TouchSample sample={ .lx=vectors[0].x,.ly=vectors[0].y,.rx=vectors[1].x,.ry=vectors[1].y,
        .rt=analog[0],.lt=analog[1],.rg=analog[2],.lg=analog[3],
        .available=vector_active[0] && vector_active[1] && !chord && (flight || (capture && binding_raw)),
        .flight=flight,.capture=capture && binding_raw };
    const int ids[6]={TOUCH_A,TOUCH_B,TOUCH_X,TOUCH_Y,TOUCH_RC,TOUCH_LC};
    for(int i=0;i<6;i++) if(buttons[i]) sample.buttons |= 1u<<ids[i];
    /* Physical stick V1: grip pose + squeeze override ly/rx while grabbed. */
    { VrStickInput sti={0};
      const VrFlightSnapshot *fsnap = VrFlightBridge_GetLatest();
      float sr[3]={1.f,0.f,0.f},sf[3]={0.f,-1.f,0.f},su[3]={0.f,0.f,1.f},sp[3]={0.f,0.f,0.f};
      float out_pitch=0.f,out_roll=0.f; int changed=0,grabbed=0;
      int immersive = fsnap && fsnap->state==VR_FLIGHT_ACTIVE && (fsnap->flags&VR_FLIGHT_VALID) &&
          fsnap->object_count>0 && fsnap->player_index<fsnap->object_count && fsnap->cockpit_valid;
      sti.focused=1; sti.active=immersive && flight; sti.squeeze=analog[2];
      int loc_rc=1, loc_flags=0;
      if(grip_space!=XR_NULL_HANDLE) {
          XrSpaceLocation loc={ .type=XR_TYPE_SPACE_LOCATION };
          XrSpaceLocationFlags need=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT;
          loc_rc=(locate_space(grip_space,M8_XrLocalSpace(),display_time,&loc)==XR_SUCCESS)?0:1;
          if(!loc_rc) loc_flags=((loc.locationFlags&need)==need)?1:0;
          if(!loc_rc && loc_flags && immersive) {
              /* Grip: XR LOCAL -> snapshot local via the Compose position law. */
              const float *pm=fsnap->objects[fsnap->player_index].model_to_local_m;
              float br[3],bu[3],bb[3];
              if(VrEye_VehicleBasis(pm,br,bu,bb)) {
                  float gx=loc.pose.position.x,gy=loc.pose.position.y,gz=loc.pose.position.z;
                  sti.grip_pos[0]=pm[3]+br[0]*gx+bu[0]*gy+bb[0]*gz;
                  sti.grip_pos[1]=pm[7]+br[1]*gx+bu[1]*gy+bb[1]*gz;
                  sti.grip_pos[2]=pm[11]+br[2]*gx+bu[2]*gy+bb[2]*gz;
                  sti.pose_valid=1;
              }
          }
      }
      if(immersive) VrStick_SeatFrame(fsnap->cockpit_to_local_m,sr,sf,su,sp);
      grabbed=VrStick_Update(&stick_state,&sti,sp,sr,sf,su,&out_pitch,&out_roll,&changed);
      if(changed) SDL_Log(grabbed ? "M8_STICK_GRAB active=1" : "M8_STICK_RELEASE active=0");
      /* P4 transition telemetry: arm/pose/hover/override edges. */
      if((sti.active?1:0)!=st_prev_active) {
          SDL_Log("M8_STICK_ACTIVE active=%d flight=%d immersive=%d",sti.active?1:0,flight?1:0,immersive?1:0);
          st_prev_active=sti.active?1:0;
      }
      if((sti.pose_valid?1:0)!=st_prev_pose) {
          SDL_Log("M8_STICK_POSE valid=%d reason=%s",sti.pose_valid?1:0,
              sti.pose_valid?"located":!immersive?"not_immersive":grip_space==XR_NULL_HANDLE?"no_space":loc_rc?"locate_failed":!loc_flags?"flags_invalid":"basis_failed");
          st_prev_pose=sti.pose_valid?1:0;
      }
      if((stick_state.hover?1:0)!=st_prev_hover) {
          if(stick_state.hover) {
              float rbase[3],rrest[3];
              float hdx,hdy,hdz;
              VrStick_RestPose(sp,sr,sf,su,rbase,rrest);
              hdx=sti.grip_pos[0]-rrest[0]; hdy=sti.grip_pos[1]-rrest[1]; hdz=sti.grip_pos[2]-rrest[2];
              SDL_Log("M8_STICK_HOVER_ENTER dist_m=%.3f",sqrtf(hdx*hdx+hdy*hdy+hdz*hdz));
          } else SDL_Log("M8_STICK_HOVER_LEAVE");
          st_prev_hover=stick_state.hover?1:0;
      }
      if(grabbed && !st_prev_grabbed) SDL_Log("M8_STICK_OVERRIDE pitch=%.2f roll=%.2f",out_pitch,out_roll);
      st_prev_grabbed=grabbed?1:0;
      if(grabbed) { sample.ly=out_pitch; sample.rx=out_roll;
          if(++stick_polls%120==0) SDL_Log("M8_STICK_DEFLECT pitch=%.2f roll=%.2f",out_pitch,out_roll); }
    }
    if(!TouchJoystick_Update(&sample,dt)) { M8_Fail("M8I virtual input update"); return 0; }
    if(flight) {
        /* Native bindings own flight, including B=Options; never emit menu keys. */
        erase=0; name_index=-1; exit_since=0; nav_next=0;
        memcpy(previous,buttons,4*sizeof(int)); previous[4]=0;
        return 1;
    }
    click=analog[0]>.55f;
    if(capture && binding_raw) {
        /* Binding pages see raw controls; suppress A/X/Y and stick navigation. */
        click=0;
        if(buttons[BACK] && !previous[BACK]) { key(SDL_SCANCODE_ESCAPE,1); release_scan=SDL_SCANCODE_ESCAPE; }
        memcpy(previous,buttons,4*sizeof(int)); previous[4]=0;
        erase=0; name_index=-1; exit_since=0; nav_next=0;
        return 1;
    }
    if (fabsf(vectors[1].x) > .15f) cursor_x += vectors[1].x * dt * .55f;
    if (fabsf(vectors[1].y) > .15f) cursor_y -= vectors[1].y * dt * .55f;
    cursor_x = fmaxf(0.f, fminf(.999f, cursor_x)); cursor_y = fmaxf(0.f, fminf(.999f, cursor_y));
    if (buttons[EXIT] && !capture) {
        if (!exit_since) exit_since = now;
        if (now - exit_since >= 2000) {
            check(request_exit(session), "xrRequestExitSession");
            SDL_Log("M8_EXPLICIT_EXIT Y held for 2 seconds"); Aeron_RequestQuit();
        }
    } else exit_since = 0;
    if (buttons[NAME] && !previous[NAME] && prompt) {
        erase = 12; name_index = 0; SDL_Log("M8_PILOT_NAME queued=m8test confirmation_required=1");
    }
    SDL_Scancode scan = 0;
    int nav = fabsf(vectors[0].x) > fabsf(vectors[0].y) ?
        (vectors[0].x > .55f ? SDL_SCANCODE_RIGHT : vectors[0].x < -.55f ? SDL_SCANCODE_LEFT : 0) :
        (vectors[0].y > .55f ? SDL_SCANCODE_UP : vectors[0].y < -.55f ? SDL_SCANCODE_DOWN : 0);
    if (!nav) nav_next = 0;
    if (nav && (nav != previous[4] || now >= nav_next)) {
        scan = (SDL_Scancode)nav; nav_next = now + (nav != previous[4] ? 350 : 130);
    }
    previous[4] = nav;
    if (buttons[CONFIRM] && !previous[CONFIRM]) scan = SDL_SCANCODE_RETURN;
    if (buttons[BACK] && !previous[BACK]) { scan = SDL_SCANCODE_ESCAPE; erase = 0; name_index = -1; }
    if (erase) { scan = SDL_SCANCODE_BACKSPACE; --erase; }
    else if (name_index >= 0) {
        static char text[2]; text[0] = "m8test"[name_index++]; text[1] = 0;
        SDL_Event e = {0}; e.type = SDL_EVENT_TEXT_INPUT;
        e.text.windowID = SDL_GetWindowID(g_aeron.window); e.text.text = text;
        if (!SDL_PushEvent(&e)) M8_Fail("pilot text input enqueue");
        if (name_index == 6) name_index = -1;
        scan = 0;
    }
    if (scan) { key(scan, 1); release_scan = scan; }
    memcpy(previous, buttons, 4*sizeof(int));
    return 1;
}
void M8_InputApplyPointer(int focused) {
    TouchJoystick_AfterPump();
    /* Android global mouse position is not the XR cursor. Apply after its
     * platform sample. No relative flight motion or camera change is injected. */
    AeronMouseSnapshot *m = &g_aeron.input.mouse;
    SDL_Rect rect;
    Aeron_ComputePresentationRect(g_aeron.input.window_width, g_aeron.input.window_height, &rect);
    m->x = (int)(cursor_x * g_aeron.logical_width); m->y = (int)(cursor_y * g_aeron.logical_height);
    m->raw_x = rect.x + (int)(cursor_x * rect.w); m->raw_y = rect.y + (int)(cursor_y * rect.h);
    m->inside_content = focused; m->relative_x = m->relative_y = 0;
    m->buttons = focused && click ? AERON_MOUSE_BUTTON_LEFT : 0;
    m->pressed_buttons = focused && click && !old_click ? AERON_MOUSE_BUTTON_LEFT : 0;
    m->released_buttons = !click && old_click ? AERON_MOUSE_BUTTON_LEFT : 0;
    m->double_clicked_buttons = 0;
}
#include "diagnostics.h"
int __real_FrontendDialog_PromptForPilotName(char *out);
int __wrap_FrontendDialog_PromptForPilotName(char *out) {
    static int diagnosed;
    if(!diagnosed) { M8_Memory("M8_MEM_PILOT","prompt_enter",1); diagnosed=1; }
    int result = __real_FrontendDialog_PromptForPilotName(out);
    if(result) { M8_Memory("M8_MEM_PILOT","prompt_returned_name_not_creation_confirmation",1); diagnosed=0; }
    prompt = !result;
    if (result) { erase = 0; name_index = -1; }
    return result;
}
const VrStickState *M8_StickState(void) { return &stick_state; }
void M8_InputShutdown(void) {
    TouchJoystick_Shutdown();
    VrStick_Init(&stick_state);
    st_prev_pose=st_prev_hover=st_prev_active=st_prev_grabbed=0;
    if (grip_space != XR_NULL_HANDLE && destroy_space) check(destroy_space(grip_space), "xrDestroySpace");
    grip_space = XR_NULL_HANDLE;
    if (actions && destroy_set) check(destroy_set(actions), "xrDestroyActionSet");
    actions = XR_NULL_HANDLE; session = XR_NULL_HANDLE; instance = XR_NULL_HANDLE;
}
