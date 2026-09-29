#ifndef M8_VR_EYE_MATH_H
#define M8_VR_EYE_MATH_H
#include <math.h>
/* Pure CPU contract: XYZW quaternion, physical LOCAL metres, radians. */
typedef struct VrEyeState {
    float position_m[3], orientation[4];
    float fov_left, fov_right, fov_up, fov_down, near_z;
    int width, height, valid;
} VrEyeState;
static inline void VrEye_QMul(const float a[4],const float b[4],float o[4]) {
    o[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];
    o[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];
    o[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];
    o[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];
}
/* Vehicle basis (right, up, back) from a model->local matrix: the same B-frame
   columns VrEye_Compose uses (OPT -Y forward), normalized. The Q15 noise the
   Compose Gram-Schmidt removes is irrelevant for hand tracking. */
static inline int VrEye_VehicleBasis(const float m[16],float r[3],float u[3],float b[3]) {
    float n;
    r[0]=m[0]; r[1]=m[4]; r[2]=m[8];
    u[0]=m[2]; u[1]=m[6]; u[2]=m[10];
    b[0]=m[1]; b[1]=m[5]; b[2]=m[9];
    n=r[0]*r[0]+r[1]*r[1]+r[2]*r[2];
    if(!isfinite(n) || n<1e-12f) return 0;
    n=sqrtf(n); r[0]/=n; r[1]/=n; r[2]/=n;
    n=u[0]*u[0]+u[1]*u[1]+u[2]*u[2];
    if(!isfinite(n) || n<1e-12f) return 0;
    n=sqrtf(n); u[0]/=n; u[1]/=n; u[2]/=n;
    n=b[0]*b[0]+b[1]*b[1]+b[2]*b[2];
    if(!isfinite(n) || n<1e-12f) return 0;
    n=sqrtf(n); b[0]/=n; b[1]/=n; b[2]/=n;
    return 1;
}
/* Compose B * eye. B columns = MODEL.right, MODEL.up, -MODEL.forward
   = MODEL.col0, MODEL.col2, MODEL.col1 for OPT's -Y forward.
   MODEL contains the OPT reflection; this axis swap yields a proper rotation.
   Gram-Schmidt removes Q15 quantization before quaternion conversion. */
static inline int VrEye_Compose(const float m[16],const VrEyeState *eye,float pos[3],float q[4]) {
    float r[3],u[3],b[3],n=0,d=0;
    if (!eye->valid) return 0;
    for(int i=0;i<3;++i) { r[i]=m[i*4]; u[i]=m[i*4+2]; n+=r[i]*r[i]; }
    if (!isfinite(n) || n<0.5f || n>1.5f) return 0;
    n=sqrtf(n); for(int i=0;i<3;++i) { r[i]/=n; d+=r[i]*u[i]; }
    n=0; for(int i=0;i<3;++i) { u[i]-=d*r[i]; n+=u[i]*u[i]; }
    if (!isfinite(n) || n<0.5f) return 0;
    n=sqrtf(n); for(int i=0;i<3;++i) u[i]/=n;
    b[0]=r[1]*u[2]-r[2]*u[1]; b[1]=r[2]*u[0]-r[0]*u[2]; b[2]=r[0]*u[1]-r[1]*u[0];
    d=0; for(int i=0;i<3;++i) d+=b[i]*m[i*4+1];
    if (!isfinite(d) || d<0.9f) return 0;
    float a[9]={r[0],u[0],b[0],r[1],u[1],b[1],r[2],u[2],b[2]},v[4],s;
    float t=a[0]+a[4]+a[8];
    if(t>0) { s=sqrtf(t+1)*2; v[3]=s*.25f; v[0]=(a[7]-a[5])/s; v[1]=(a[2]-a[6])/s; v[2]=(a[3]-a[1])/s; }
    else if(a[0]>a[4] && a[0]>a[8]) { s=sqrtf(1+a[0]-a[4]-a[8])*2; v[3]=(a[7]-a[5])/s; v[0]=s*.25f; v[1]=(a[1]+a[3])/s; v[2]=(a[2]+a[6])/s; }
    else if(a[4]>a[8]) { s=sqrtf(1+a[4]-a[0]-a[8])*2; v[3]=(a[2]-a[6])/s; v[0]=(a[1]+a[3])/s; v[1]=s*.25f; v[2]=(a[5]+a[7])/s; }
    else { s=sqrtf(1+a[8]-a[0]-a[4])*2; v[3]=(a[3]-a[1])/s; v[0]=(a[2]+a[6])/s; v[1]=(a[5]+a[7])/s; v[2]=s*.25f; }
    VrEye_QMul(v,eye->orientation,q);
    n=0; for(int i=0;i<4;++i) n+=q[i]*q[i];
    if(!isfinite(n) || n<0.5f || n>1.5f) return 0;
    n=sqrtf(n); for(int i=0;i<4;++i) q[i]/=n;
    for(int i=0;i<3;++i) {
        pos[i]=m[i*4+3]+r[i]*eye->position_m[0]+u[i]*eye->position_m[1]+b[i]*eye->position_m[2];
        if(!isfinite(pos[i])) return 0;
    }
    return 1;
}
#endif
