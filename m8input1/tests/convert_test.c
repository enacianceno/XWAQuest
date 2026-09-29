#include "vr_convert.h"
#include <stdarg.h>
#include <stdio.h>
/* Host log sink only; all conversion/projection code is the real production C. */
void VrLog(const char *format, ...) {
    va_list args;
    va_start(args,format);
    vprintf(format,args);
    va_end(args);
    putchar('\n');
}
int main(void) { return VrConvert_SelfTest() ? 0 : 1; }
