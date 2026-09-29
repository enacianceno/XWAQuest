#include "../touch_state.h"
#include "xwa_runtime/input/controller_mapping.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    TouchState s={0}; TouchSample q={.available=1,.flight=1};
    AeronControllerSnapshot c={.connected=1,.kind=AERON_CONTROLLER_KIND_JOYSTICK,
        .axis_count=4,.button_count=TOUCH_BUTTONS};
    XwaControllerOptions o={0}; XwaControllerLogicalState result;
    o.joystick.pov_source=-1;
    for(int i=0;i<4;i++) { o.joystick.axes[i].source=i; o.joystick.axes[i].deadzone=i==2 ? 0.f : .15f; }
    for(int i=0;i<TOUCH_BUTTONS;i++) {
        o.joystick.buttons[i].kind=AERON_CONTROLLER_DIGITAL_BUTTON;
        o.joystick.buttons[i].index=(uint8_t)i;
    }
    TouchConvert(&s,&q,0);
    q.rt=1;q.lx=1;q.ly=1;q.rx=-1;
    TouchConvert(&s,&q,.02f);
    memcpy(c.raw_axes,s.axes,sizeof s.axes); c.raw_buttons=s.buttons;
    XwaControllerMapping_MapSnapshot(&o,&c,1,&result);
    assert(result.axes[0]==65535 && result.axes[1]==0 && result.axes[2]==0 && result.axes[3]==0);
    assert(result.buttons==1 && !result.has_pov);
    /* Real native remapping/inversion must remain authoritative. */
    o.joystick.axes[0].invert=1;o.joystick.buttons[0].index=TOUCH_X;
    o.joystick.buttons[3].index=TOUCH_RT;
    XwaControllerMapping_MapSnapshot(&o,&c,1,&result);
    assert(result.axes[0]==0 && result.buttons==(1u<<3));
    XwaControllerMapping_MapSnapshot(&o,&c,0,&result);
    assert(!result.buttons && result.axes[0]==32768);
    c.connected=0; XwaControllerMapping_MapSnapshot(&o,&c,1,&result); assert(!result.buttons);
    puts("NATIVE_MAPPING_TEST PASS: real XWA map + real Aeron digital sources, endpoints, focus, rebind, invert");
    return 0;
}
