#ifndef M8T3_SEAT_MATH_H
#define M8T3_SEAT_MATH_H
#include <math.h>
#include <string.h>
/* Seat-0 specialization of the real cockpit transform. Units conversion is
   supplied by the bridge. No physical-eye pose enters this function. */
static inline int T3_SeatMatrix(const float ship[16],const float hardpoint[3],
                               const float pan[3],float metres_per_unit,float out[16]) {
    memcpy(out,ship,16*sizeof(float));
    for(int axis=0;axis<3;++axis) {
        float offset=(hardpoint[axis]+pan[axis]*.0625f)*metres_per_unit;
        out[axis*4+3]-=offset;
        if(!isfinite(out[axis*4+3])) return 0;
    }
    return 1;
}
#endif
