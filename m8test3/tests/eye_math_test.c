#include "../vr_eye_math.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void near(float a,float b) { assert(fabsf(a-b)<.0002f); }
int main(void) {
    /* OPT basis reflection -> identity physical vehicle frame. */
    float m[16]={1,0,0,0, 0,0,1,0, 0,1,0,0, 0,0,0,1},before[16];
    memcpy(before,m,sizeof m);
    VrEyeState e={.orientation={0,0,0,1},.valid=1};
    float p[3],q[4];
    assert(VrEye_Compose(m,&e,p,q)); near(q[3],1); near(p[0],0);
    e.position_m[0]=-.032f; assert(VrEye_Compose(m,&e,p,q)); near(p[0],-.032f);
    e.position_m[0]=.032f; assert(VrEye_Compose(m,&e,p,q)); near(p[0],.032f);
    e.orientation[1]=sinf(.3f); e.orientation[3]=cosf(.3f);
    assert(VrEye_Compose(m,&e,p,q)); near(q[1],sinf(.3f));
    assert(!memcmp(before,m,sizeof m)); /* head never writes vehicle */
    /* Vehicle +90 yaw, eye +0.1 right -> world -0.1 Z. */
    float yaw[16]={0,1,0,0, 0,0,1,0, -1,0,0,0, 0,0,0,1};
    e=(VrEyeState){.orientation={0,0,0,1},.position_m={.1f,0,0},.valid=1};
    assert(VrEye_Compose(yaw,&e,p,q)); near(p[0],0); near(p[2],-.1f);
    near(q[1],sqrtf(.5f)); near(q[3],sqrtf(.5f));
    e.orientation[1]=sinf(.2f); e.orientation[3]=cosf(.2f);
    assert(VrEye_Compose(yaw,&e,p,q)); near(q[1],sinf(.7853981634f+.2f));
    e.valid=0; assert(!VrEye_Compose(yaw,&e,p,q));
    e.valid=1; m[0]=0; assert(!VrEye_Compose(m,&e,p,q));
    puts("CS1 CPU PASS: identity, IPD, head yaw, vehicle yaw, composition order, no sim writes, invalid rejection");
    return 0;
}
