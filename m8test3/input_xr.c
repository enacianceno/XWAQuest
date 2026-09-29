#include "input_xr.h"
#include "frame_bridge.h"
#include "internal.h"
#include <math.h>
#include <string.h>
static XrInstance instance;
static XrSession session;
static XrActionSet actions;
enum { CONFIRM, BACK, NAME, EXIT, CLICK, NAV, CURSOR, ACTION_COUNT };
static XrAction action[ACTION_COUNT];
static PFN_xrDestroyActionSet destroy_set;
static PFN_xrSyncActions sync_actions;
static PFN_xrGetActionStateBoolean get_bool;
static PFN_xrGetActionStateFloat get_float;
static PFN_xrGetActionStateVector2f get_vector;
static PFN_xrRequestExitSession request_exit;
static int previous[5], click, old_click, prompt, erase, name_index = -1;
static float cursor_x = .5f, cursor_y = .5f;
static Uint64 last_time, exit_since, nav_next;
static SDL_Scancode release_scan;
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
#undef LOAD
    XrActionSetCreateInfo sci = { .type = XR_TYPE_ACTION_SET_CREATE_INFO };
    SDL_strlcpy(sci.actionSetName, "m8_navigation", sizeof sci.actionSetName);
    SDL_strlcpy(sci.localizedActionSetName, "M8 navigation", sizeof sci.localizedActionSetName);
    if (!check(create_set(instance, &sci, &actions), "xrCreateActionSet")) return 0;
    const char *names[ACTION_COUNT] = { "confirm", "back", "pilot_name", "exit", "click", "navigate", "cursor" };
    const char *paths[ACTION_COUNT] = {
        "/user/hand/right/input/a/click", "/user/hand/right/input/b/click",
        "/user/hand/left/input/x/click", "/user/hand/left/input/y/click",
        "/user/hand/right/input/trigger/value", "/user/hand/left/input/thumbstick",
        "/user/hand/right/input/thumbstick" };
    XrActionSuggestedBinding bindings[ACTION_COUNT] = {0};
    for (int i = 0; i < ACTION_COUNT; ++i) {
        XrActionCreateInfo ci = { .type = XR_TYPE_ACTION_CREATE_INFO };
        ci.actionType = i < CLICK ? XR_ACTION_TYPE_BOOLEAN_INPUT : i == CLICK ? XR_ACTION_TYPE_FLOAT_INPUT : XR_ACTION_TYPE_VECTOR2F_INPUT;
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
    SDL_Log("M8_INPUT_READY A=RETURN B=ESC left_stick=arrows right_stick=cursor trigger=click X=fill_m8test Y_hold_2s=exit");
    return 1;
}
static void key(SDL_Scancode scan, int down) {
    SDL_Event e = {0};
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.windowID = SDL_GetWindowID(g_aeron.window);
    e.key.scancode = scan; e.key.key = SDL_GetKeyFromScancode(scan, 0, false); e.key.down = down != 0;
    if (!SDL_PushEvent(&e)) M8_Fail("SDL input event enqueue");
}
int M8_InputPoll(void) {
    if (release_scan) { key(release_scan, 0); release_scan = 0; }
    old_click = click; click = 0;
    XrActiveActionSet set = { .actionSet = actions };
    XrActionsSyncInfo si = { .type = XR_TYPE_ACTIONS_SYNC_INFO, .countActiveActionSets = 1, .activeActionSets = &set };
    XrResult r = sync_actions(session, &si);
    if (r == XR_SESSION_NOT_FOCUSED) {
        memset(previous, 0, sizeof previous); exit_since = 0; last_time = 0;
        return 0;
    }
    if (!check(r, "xrSyncActions")) { M8_Fail("XR input sync"); return 0; }
    int buttons[4] = {0};
    XrVector2f vectors[2] = {{0}};
    for (int i = 0; i < ACTION_COUNT; ++i) {
        XrActionStateGetInfo gi = { .type = XR_TYPE_ACTION_STATE_GET_INFO, .action = action[i] };
        int ok;
        if (i < CLICK) {
            XrActionStateBoolean state = { .type = XR_TYPE_ACTION_STATE_BOOLEAN };
            ok = check(get_bool(session, &gi, &state), "xrGetActionStateBoolean");
            buttons[i] = ok && state.isActive && state.currentState;
        } else if (i == CLICK) {
            XrActionStateFloat state = { .type = XR_TYPE_ACTION_STATE_FLOAT };
            ok = check(get_float(session, &gi, &state), "xrGetActionStateFloat");
            click = ok && state.isActive && state.currentState > .55f;
        } else {
            XrActionStateVector2f state = { .type = XR_TYPE_ACTION_STATE_VECTOR2F };
            ok = check(get_vector(session, &gi, &state), "xrGetActionStateVector2f");
            if (ok && state.isActive) vectors[i - NAV] = state.currentState;
        }
        if (!ok) { M8_Fail("XR action state"); return 0; }
    }
    Uint64 now = SDL_GetTicks();
    float dt = last_time ? (float)(now - last_time) / 1000.f : 0.f;
    last_time = now;
    if (dt > .1f) dt = .1f;
    if (fabsf(vectors[1].x) > .15f) cursor_x += vectors[1].x * dt * .55f;
    if (fabsf(vectors[1].y) > .15f) cursor_y -= vectors[1].y * dt * .55f;
    cursor_x = fmaxf(0.f, fminf(.999f, cursor_x)); cursor_y = fmaxf(0.f, fminf(.999f, cursor_y));
    if (buttons[EXIT]) {
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
    memcpy(previous, buttons, sizeof buttons);
    return 1;
}
void M8_InputApplyPointer(int focused) {
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
void M8_InputShutdown(void) {
    if (actions && destroy_set) check(destroy_set(actions), "xrDestroyActionSet");
    actions = XR_NULL_HANDLE; session = XR_NULL_HANDLE; instance = XR_NULL_HANDLE;
}
