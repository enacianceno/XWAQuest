#ifndef M8T4_TIMING_H
#define M8T4_TIMING_H
#include <SDL3/SDL.h>
enum T4Stage { T4_XR_WAIT,T4_XR_BEGIN,T4_LOCATE,T4_ACQUIRE,T4_WAIT_IMAGE,
 T4_RELEASE,T4_XR_END,T4_EYE0,T4_EYE1,T4_BLIT,T4_CB,T4_SUBMIT,T4_WSI,
 T4_SIM,T4_REMASTER,T4_MEMORY,T4_COCKPIT,T4_TARGET,T4_STARS,T4_STAGE_COUNT };
void T4_Add(enum T4Stage stage,Uint64 start);
void T4_FrameBegin(void);
void T4_FrameEnd(int immersive);
void T4_InstallXrTiming(void);
#endif
