#include "../t3_seat_math.h"
#include "../vr_eye_math.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    float ship[16]={1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1};
    float hp[3]={0,40.96f,81.92f},pan[3]={16,0,0},seat[16],saved[16];
    assert(T3_SeatMatrix(ship,hp,pan,1600.f/65536.f,seat));
    assert(fabsf(seat[3]+1600.f/65536.f)<1e-5f);
    assert(fabsf(seat[7]+1)<1e-5f && fabsf(seat[11]+2)<1e-5f);
    memcpy(saved,seat,sizeof seat);
    VrEyeState eye={.orientation={0,sinf(.4f),0,cosf(.4f)},.position_m={.2f,.1f,0},.valid=1};
    float p[3],q[4]; assert(VrEye_Compose(ship,&eye,p,q));
    assert(!memcmp(seat,saved,sizeof seat)); assert(ship[3]==0 && ship[7]==0 && ship[11]==0);
    hp[0]=NAN; assert(!T3_SeatMatrix(ship,hp,pan,1600.f/65536.f,seat));
    puts("M8T3 CPU PASS: seat units/sign, pan/16, no head-to-ship or head-to-cockpit write, invalid rejection");
    return 0;
}
