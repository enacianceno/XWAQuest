#ifndef M8_INPUT_XR_H
#define M8_INPUT_XR_H
#include "vr_openxr.h"
int M8_InputInit(XrInstance instance, XrSession session);
int M8_InputPoll(void); /* enqueue SDL events before Aeron_BeginFrame; return XR focus */
void M8_InputApplyPointer(int focused); /* after platform mouse sampling */
void M8_InputShutdown(void);
#endif
