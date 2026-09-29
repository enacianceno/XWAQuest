#include "m8_file_log.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define M8_FILELOG_MAX_BYTES (4u*1024u*1024u)
#define M8_FILELOG_NAME "m8telemetry.log"
#define M8_FILELOG_PREV "m8telemetry.prev.log"

static SDL_LogOutputFunction prev_fn = NULL;
static void *prev_ud = NULL;
static char log_dir[512];
static char log_path[512];
static int installed = 0;

static int want_line(const char *msg) {
    return msg && msg[0]==(char)77 && msg[1]==(char)56;
}

static void file_append(const char *line) {
    FILE *f;
    if (!log_path[0]) return;
    f = fopen(log_path, "a");
    if (!f) return;
    fputs(line, f);
    fputc(10, f);
    fclose(f);
}

static void SDLCALL file_log_fn(void *userdata, int category, SDL_LogPriority priority, const char *message) {
    char line[2048];
    (void)userdata;
    if (prev_fn) prev_fn(prev_ud, category, priority, message);
    if (!installed || !want_line(message)) return;
    SDL_snprintf(line, sizeof line, "[%d] %s", (int)priority, message);
    file_append(line);
}

void M8_FileLog_Install(void) {
    char *pref;
    char prevp[512];
    FILE *probe;
    long sz;
    if (installed) return;
    pref = SDL_GetPrefPath("XWAQuest", "M8");
    if (!pref || !pref[0]) return;
    SDL_snprintf(log_dir, sizeof log_dir, "%s", pref);
    SDL_snprintf(log_path, sizeof log_path, "%s%s", pref, M8_FILELOG_NAME);
    SDL_snprintf(prevp, sizeof prevp, "%s%s", pref, M8_FILELOG_PREV);
    SDL_free(pref);
    probe = fopen(log_path, "rb");
    sz = -1;
    if (probe) { fseek(probe, 0, SEEK_END); sz = ftell(probe); fclose(probe); }
    if (sz > (long)M8_FILELOG_MAX_BYTES) { remove(prevp); rename(log_path, prevp); }
    SDL_GetLogOutputFunction(&prev_fn, &prev_ud);
    installed = 1;
    SDL_SetLogOutputFunction(file_log_fn, NULL);
    SDL_Log("M8_FILELOG_START path=%s", log_path);
}

const char *M8_FileLog_Path(void) {
    return (installed && log_path[0]) ? log_path : NULL;
}
