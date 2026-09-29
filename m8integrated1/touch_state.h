#ifndef M8INTEGRATED1_TOUCH_STATE_H
#define M8INTEGRATED1_TOUCH_STATE_H
#include <stdint.h>
#include <math.h>
#include <string.h>
enum { TOUCH_RT, TOUCH_A, TOUCH_B, TOUCH_X, TOUCH_Y, TOUCH_LT,
       TOUCH_RG, TOUCH_LG, TOUCH_RC, TOUCH_LC, TOUCH_BUTTONS };
typedef struct TouchSample {
    float lx, ly, rx, ry, rt, lt, rg, lg;
    unsigned buttons; /* digital A/B/X/Y and stick clicks */
    int available, flight, capture;
} TouchSample;
typedef struct TouchState {
    int16_t axes[4]; /* yaw, pitch, throttle lever, roll */
    unsigned buttons, pressed, released;
    float throttle; /* input lever only: 0 idle .. 1 full; never ship state */
    int was_available, was_flight, armed;
} TouchState;
static inline float TouchClamp(float x) {
    return isfinite(x) ? fmaxf(-1.f, fminf(1.f, x)) : 0.f;
}
static inline int16_t TouchAxis(float x) {
    x = TouchClamp(x);
    return (int16_t)lroundf(x * (x < 0.f ? 32768.f : 32767.f));
}
static inline void TouchConvert(TouchState *s, const TouchSample *q, float dt) {
    unsigned old = s->buttons, buttons = q->buttons;
    /* Analog buttons use hysteresis, independent of action bindings. */
    const float values[4] = {q->rt,q->lt,q->rg,q->lg};
    const int ids[4] = {TOUCH_RT,TOUCH_LT,TOUCH_RG,TOUCH_LG};
    for (int i=0;i<4;i++) {
        unsigned bit=1u<<ids[i];
        if (isfinite(values[i]) && values[i] > ((old & bit) ? .45f : .55f)) buttons |= bit;
    }
    int neutral = !buttons && fabsf(TouchClamp(q->lx)) < .15f &&
        fabsf(TouchClamp(q->ly)) < .15f && fabsf(TouchClamp(q->rx)) < .15f &&
        fabsf(TouchClamp(q->ry)) < .15f;
    if (!q->available || !s->was_available || q->flight != s->was_flight) s->armed=0;
    if (q->available && neutral) s->armed=1;
    s->was_available=q->available; s->was_flight=q->flight;
    int active=q->available && s->armed;
    s->axes[0]=active ? TouchAxis(q->lx) : 0;
    s->axes[1]=active ? TouchAxis(-q->ly) : 0;
    s->axes[3]=active ? TouchAxis(q->rx) : 0;
    if (active && q->flight && fabsf(TouchClamp(q->ry)) > .15f) {
        dt=isfinite(dt) ? fmaxf(0.f,fminf(.1f,dt)) : 0.f;
        s->throttle=fmaxf(0.f,fminf(1.f,s->throttle+TouchClamp(q->ry)*dt*.5f));
    }
    /* Native bucket table: high Z -> bucket 16 -> index 0 -> Full throttle. */
    s->axes[2]=q->capture ? (active ? TouchAxis(q->ry) : 0) : TouchAxis(2.f*s->throttle-1.f);
    s->buttons=active ? buttons : 0;
    s->pressed=s->buttons & ~old; s->released=old & ~s->buttons;
}
#endif
