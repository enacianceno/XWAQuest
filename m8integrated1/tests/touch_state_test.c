#include "../touch_state.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    TouchState s={0}; TouchSample q={.available=1,.flight=1};
    TouchConvert(&s,&q,0); assert(s.armed && s.axes[2]==-32768);
    q.lx=1;q.ly=1;q.rx=-1; TouchConvert(&s,&q,.02f);
    assert(s.axes[0]==32767 && s.axes[1]==-32768 && s.axes[3]==-32768);
    q.rt=.56f; TouchConvert(&s,&q,.02f); assert(s.pressed==(1u<<TOUCH_RT));
    q.rt=.50f; TouchConvert(&s,&q,.02f); assert(s.buttons && !s.pressed);
    q.rt=.44f; TouchConvert(&s,&q,.02f); assert(s.released==(1u<<TOUCH_RT));
    q.lx=q.ly=q.rx=0; q.ry=1;
    for(int i=0;i<20;i++) TouchConvert(&s,&q,.1f);
    assert(s.throttle==1 && s.axes[2]==32767);
    q.ry=0; TouchConvert(&s,&q,10); assert(s.throttle==1);
    q.ry=-1; for(int i=0;i<20;i++) TouchConvert(&s,&q,.1f);
    assert(s.throttle==0 && s.axes[2]==-32768);
    q.rt=1; q.available=0; TouchConvert(&s,&q,.1f);
    assert(!s.buttons && !s.armed && !s.axes[0] && !s.axes[1] && !s.axes[3]);
    q.available=1; TouchConvert(&s,&q,.1f); assert(!s.buttons);
    q.rt=q.ry=0; TouchConvert(&s,&q,.1f); assert(s.armed);
    q.buttons=1u<<TOUCH_A; TouchConvert(&s,&q,.1f); assert(s.buttons);
    q.flight=0; TouchConvert(&s,&q,.1f); assert(!s.buttons && s.released);
    q.buttons=0; TouchConvert(&s,&q,.1f); assert(s.armed);
    q.capture=1;q.ry=1; TouchConvert(&s,&q,.1f);
    assert(s.axes[2]==32767 && s.throttle==0);
    q.capture=0;q.flight=1; TouchConvert(&s,&q,.1f); assert(!s.armed && s.throttle==0);
    q.ry=0;TouchConvert(&s,&q,.1f);q.ry=1;TouchConvert(&s,&q,100);
    assert(fabsf(s.throttle-.05f)<.00001f);
    assert(TouchAxis(NAN)==0 && TouchAxis(INFINITY)==0);
    puts("TOUCH_STATE_TEST PASS: axes, throttle, hysteresis, focus, context, capture, dt bounds");
    return 0;
}
