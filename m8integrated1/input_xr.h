#ifndef M8_INPUT_XR_H
#define M8_INPUT_XR_H
#include "vr_openxr.h"
#include "vr_stick_logic.h"
int M8_InputInit(XrInstance instance, XrSession session);
int M8_InputPoll(XrTime display_time); /* enqueue SDL events before Aeron_BeginFrame; return XR focus */
void M8_InputApplyPointer(int focused); /* after platform mouse sampling */
void M8_InputShutdown(void);
const VrStickState *M8_StickState(void);
XrSpace M8_XrLocalSpace(void);
#endif
