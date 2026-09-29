#ifndef M8_FRAME_BRIDGE_H
#define M8_FRAME_BRIDGE_H
#include "vr_openxr.h"
int M8_XrAcquire(int eye, SDL_GPUTexture **texture);
int M8_XrRelease(int eye);
void M8_Fail(const char *operation);
#endif
