#ifndef M8INPUT1_TOUCH_JOYSTICK_H
#define M8INPUT1_TOUCH_JOYSTICK_H
#include "touch_state.h"
int TouchJoystick_Init(void);
int TouchJoystick_Update(const TouchSample *sample, float dt);
void TouchJoystick_AfterPump(void);
void TouchJoystick_Shutdown(void);
int TouchJoystick_Flight(void);
int TouchJoystick_CaptureScreen(void);
#endif
