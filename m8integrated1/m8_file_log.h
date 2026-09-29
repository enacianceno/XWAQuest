/* M8 file telemetry: duplicates M8* SDL log lines to an app-private ring
 * file so a flight telemetry survives logcat rotation. Logcat routing is
 * unchanged (the previous SDL output function is chained). No XR/sim/GPU. */
#ifndef M8INTEGRATED1_M8_FILE_LOG_H
#define M8INTEGRATED1_M8_FILE_LOG_H

/* Install once after SDL is initialized. Repeat calls are no-ops. */
void M8_FileLog_Install(void);

/* Active telemetry path, or NULL before install. */
const char *M8_FileLog_Path(void);

#endif
