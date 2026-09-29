#ifndef VR_LOG_H
#define VR_LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Timestamped dual log (logcat + files/vrprobe-log.txt). Defined in vr_main.c. */
void VrLog(const char *fmt, ...);

extern FILE *g_vrLog;
extern volatile int g_vrPhase;
extern volatile unsigned g_vrPhaseTick;
extern volatile unsigned g_vrStereo;

#ifdef __cplusplus
}
#endif

#endif
