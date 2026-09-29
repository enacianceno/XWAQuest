#include "t4_timing.h"
#include <string.h>
/* Host-thread only. Durations are CPU wall-clock spans, not GPU timestamps.
 * Nested spans overlap; never sum them to claim exclusive CPU cost. */
static const char *names[T4_STAGE_COUNT]={"xr_wait","xr_begin","locate","acquire","wait_image",
 "release","xr_end","eye0","eye1","blit","cb","submit","wsi_acquire",
 "sim","remaster","memory_sample","cockpit_record","target_record","stars_record"};
static Uint64 sum[T4_STAGE_COUNT],peak[T4_STAGE_COUNT],count[T4_STAGE_COUNT];
static Uint64 frame_start,period_start,frames,total,maximum;
void T4_Add(enum T4Stage stage,Uint64 start) {
 Uint64 ns=SDL_GetTicksNS()-start;
 sum[stage]+=ns; count[stage]++; if(ns>peak[stage]) peak[stage]=ns;
}
void T4_FrameBegin(void) { frame_start=SDL_GetTicksNS(); if(!period_start) period_start=frame_start; }
void T4_FrameEnd(int immersive) {
 Uint64 now=SDL_GetTicksNS(),elapsed=now-frame_start;
 ++frames; total+=elapsed; if(elapsed>maximum)maximum=elapsed;
 if(now-period_start<1000000000ULL)return;
 char detail[1800]; size_t used=0;
 for(int i=0;i<T4_STAGE_COUNT;++i) {
  int n=SDL_snprintf(detail+used,sizeof(detail)-used," %s_ms=%.3f/%llu/max%.3f",names[i],
   count[i]?(double)sum[i]/count[i]/1e6:0.,(unsigned long long)count[i],(double)peak[i]/1e6);
  if(n<0 || (size_t)n>=sizeof(detail)-used)break; used+=(size_t)n;
 }
 SDL_Log("M8T4_FRAME_TIMING fps=%.2f cpu_wall_ms=%.3f max_ms=%.3f immersive=%d gpu_ms=unavailable early=external_VrApi stale=external_VrApi tear=external_VrApi memory=separate_M8T4_MEMORY%s",
  (double)frames*1e9/(now-period_start),(double)total/frames/1e6,(double)maximum/1e6,immersive,detail);
 memset(sum,0,sizeof sum);memset(peak,0,sizeof peak);memset(count,0,sizeof count);
 frames=total=maximum=0;period_start=SDL_GetTicksNS();
}
