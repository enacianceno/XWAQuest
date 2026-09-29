/* M7C XR Session API — called from trace.c wrappers.
 * Provides OpenXR session lifecycle and head pose access. */
#ifndef M7C_XR_SESSION_H
#define M7C_XR_SESSION_H

#include <SDL3/SDL.h>

/* Initialize XR session after Aeron GPU device is ready.
 * Returns 1 on success, 0 if XR unavailable. */
int M7C_XrInit(SDL_GPUDevice *device);

/* Poll XR events (session state changes). Call once per frame. */
void M7C_XrPollEvents(void);

/* Wait for next XR frame and begin it. Returns 1 if frame is ready. */
int M7C_XrWaitFrame(void);

/* End XR frame and submit layers. rendered=1 if game rendered this frame. */
int M7C_XrEndFrame(int rendered);

/* Query XR state. */
int M7C_XrIsRunning(void);
int M7C_XrShouldQuit(void);

/* Get current head pose (position + quaternion). */
void M7C_XrGetHeadPose(float *x, float *y, float *z,
                        float *qx, float *qy, float *qz, float *qw);

/* Clean up XR resources. */
void M7C_XrShutdown(void);

#endif /* M7C_XR_SESSION_H */
