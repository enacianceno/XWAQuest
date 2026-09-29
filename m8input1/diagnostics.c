#include "diagnostics.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "aeron/render.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa/util/memory.h"

static int log_fd=-1, log_attempted, setup_active, setup_ready, movie_started;
static int last_directory=-1;
void M8_Diag(const char *format,...) {
    int saved_errno=errno;
    char line[2048];
    int prefix=snprintf(line,sizeof line,"ms=%llu ",(unsigned long long)SDL_GetTicks());
    va_list ap; va_start(ap,format);
    vsnprintf(line+prefix,sizeof line-(size_t)prefix,format,ap); va_end(ap);
    SDL_Log("%s",line);
    if(!log_attempted) {
        log_attempted=1;
        const char *dir=SDL_GetAndroidInternalStoragePath();
        if(dir) {
            char path[1024];
            snprintf(path,sizeof path,"%s/m8-cs1-diagnostics.log",dir);
            log_fd=open(path,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC,0600);
        }
        if(log_fd<0) SDL_Log("M8_DIAG_FILE_UNAVAILABLE errno=%d",errno);
    }
    if(log_fd>=0) { size_t n=strlen(line); line[n++]='\n'; (void)write(log_fd,line,n); }
    errno=saved_errno;
}
void M8_Memory(const char *checkpoint,const char *stage,int full) {
    int saved_errno=errno;
    long rss=-1,virt=-1,hwm=-1,pss=-1;
    char line[256]; FILE *f=fopen("/proc/self/status","r");
    if(f) { while(fgets(line,sizeof line,f)) {
        (void)sscanf(line,"VmRSS: %ld kB",&rss);
        (void)sscanf(line,"VmSize: %ld kB",&virt);
        (void)sscanf(line,"VmHWM: %ld kB",&hwm);
    } fclose(f); }
    if(full && (f=fopen("/proc/self/smaps_rollup","r"))) {
        while(fgets(line,sizeof line,f)) (void)sscanf(line,"Pss: %ld kB",&pss);
        fclose(f);
    }
    struct mallinfo heap=mallinfo();
    AeronRenderDataStats stats={0}; Aeron_GetRenderDataStats(&stats);
    const XwaSnapshot *s=XwaSnapshot_Current();
    M8_Diag("%s stage=%s rss_kb=%ld pss_kb=%ld vmsize_kb=%ld hwm_kb=%ld heap_used_bytes=%zu heap_arena_bytes=%zu heap_mmap_bytes=%zu resources=%d opt_assets=%u flight_objects=%u upload_staged_bytes=%llu upload_reserved_bytes=%llu upload_chunks=%u texture_copies=%u stats_scope=current_frame pss_sampled=%d handles=%d handle_bytes=%d",
        checkpoint,stage,rss,pss,virt,hwm,(size_t)heap.uordblks,(size_t)heap.arena,(size_t)heap.hblkhd,
        g_resourceCount,s?s->opt_asset_count:0,s?s->flight_object_count:0,
        (unsigned long long)stats.upload_staged_bytes,(unsigned long long)stats.upload_reserved_bytes,
        stats.upload_chunk_count,stats.texture_copy_count,full,g_handleAllocatedCount,g_handleAllocatedTotalBytes);
    errno=saved_errno;
}
void M8_MemoryPoll(void) {
    static Uint64 last;
    Uint64 now=SDL_GetTicks();
    if(now-last>=5000) { last=now; M8_Memory("M8_MEM_SAMPLE","periodic",1); }
}
int __real_CombatSimMenu_Update(int frame);
int __wrap_CombatSimMenu_Update(int frame) {
    if(frame==0) M8_Memory("M8_MEM_COMBAT_SIM","before_update_0",1);
    int r=__real_CombatSimMenu_Update(frame);
    if(frame==0) M8_Memory("M8_MEM_COMBAT_SIM","after_update_0",1);
    return r;
}
int __real_CombatSimMenu_Exit(int frame);
int __wrap_CombatSimMenu_Exit(int frame) {
    M8_Memory("M8_MEM_COMBAT_SIM","before_exit",1);
    int r=__real_CombatSimMenu_Exit(frame);
    M8_Memory("M8_MEM_COMBAT_SIM","after_exit",1); return r;
}
int __real_MissionSetup_Update(int frame);
static void observe_directory(const char *stage) {
    int d=g_pilotData.missionDirectoryId;
    if(d!=last_directory) {
        M8_Diag("M8_SETUP_DIRECTORY stage=%s old=%d new=%d",stage,last_directory,d);
        if(d==MISSION_DIRECTORY_SKIRMISH) M8_Memory("M8_MEM_SKIRMISH_ENTER",stage,1);
        last_directory=d;
    }
}
int __wrap_MissionSetup_Update(int frame) {
    if(!setup_active) {
        setup_active=1; setup_ready=0; last_directory=-1;
        M8_Memory("M8_MEM_SINGLE_PLAYER_ENTER","MissionSetup_before_first_update",1);
    }
    observe_directory("before_update"); movie_started=0;
    int r=__real_MissionSetup_Update(frame);
    if(!setup_ready && !movie_started) {
        setup_ready=1;
        M8_Memory("M8_MEM_SINGLE_PLAYER_READY","update_returned_without_starting_movie_not_visual_confirmation",1);
    }
    observe_directory("after_update"); return r;
}
int __real_MissionSetup_Exit(int frame);
int __wrap_MissionSetup_Exit(int frame) {
    M8_Memory("M8_MEM_SETUP_EXIT","before",1);
    int r=__real_MissionSetup_Exit(frame);
    M8_Memory("M8_MEM_SETUP_EXIT","after",1); setup_active=setup_ready=0; return r;
}
int __real_Movie_Play(const char *name,int no_fade);
int __wrap_Movie_Play(const char *name,int no_fade) {
    M8_Diag("M8_RESOURCE op=movie_begin name=%s",name?name:"(null)");
    M8_Memory("M8_MEM_MOVIE","before_play",1);
    int r=__real_Movie_Play(name,no_fade);
    if(setup_active && r) movie_started=1;
    M8_Memory("M8_MEM_MOVIE","after_play",1);
    M8_Diag("M8_RESOURCE op=movie_return result=%d",r); return r;
}
int __real_FrontImage_RegisterResourceDefault(const char *file,const char *name);
int __wrap_FrontImage_RegisterResourceDefault(const char *file,const char *name) {
    M8_Diag("M8_RESOURCE op=load_begin file=%s name=%s",file?file:"(null)",name?name:"(null)");
    M8_Memory("M8_MEM_RESOURCE","before_load",0);
    int r=__real_FrontImage_RegisterResourceDefault(file,name);
    M8_Memory("M8_MEM_RESOURCE","after_load",0);
    M8_Diag("M8_RESOURCE op=load_end name=%s result=%d",name?name:"(null)",r); return r;
}
void __real_FrontImage_FreeResourceByName(const char *name);
void __wrap_FrontImage_FreeResourceByName(const char *name) {
    M8_Diag("M8_RESOURCE op=free_begin name=%s",name?name:"(null)");
    M8_Memory("M8_MEM_RESOURCE","before_free",0);
    __real_FrontImage_FreeResourceByName(name);
    M8_Memory("M8_MEM_RESOURCE","after_free",0);
    M8_Diag("M8_RESOURCE op=free_end");
}
int __real_FrontImage_LoadResourceList(char *file);
int __wrap_FrontImage_LoadResourceList(char *file) {
    M8_Diag("M8_RESOURCE op=list_load_begin file=%s",file?file:"(null)");
    M8_Memory("M8_MEM_RESOURCE_LIST","before",1);
    int r=__real_FrontImage_LoadResourceList(file);
    M8_Memory("M8_MEM_RESOURCE_LIST","after",1);
    M8_Diag("M8_RESOURCE op=list_load_end result=%d",r); return r;
}
void *__real_Mem_Alloc(size_t size);
void *__wrap_Mem_Alloc(size_t size) {
    if(size>=8u*1024u*1024u) {
        M8_Diag("M8_LARGE_ALLOC requested_bytes=%zu caller=%p",size,__builtin_return_address(0));
        M8_Memory("M8_MEM_LARGE_ALLOC","before",0);
    }
    void *p=__real_Mem_Alloc(size);
    if(size>=8u*1024u*1024u) {
        M8_Memory("M8_MEM_LARGE_ALLOC","after",0);
        M8_Diag("M8_LARGE_ALLOC completed=%d",p!=NULL);
    }
    return p;
}
/* Observe the real generator; never fabricate a mission or its return value. */
int __real_Skirmish_GenerateMission(const char *path);
int __wrap_Skirmish_GenerateMission(const char *path) {
    M8_Diag("M8T3_MISSION_GENERATION begin path=%s",path?path:"(null)");
    int result=__real_Skirmish_GenerateMission(path);
    M8_Diag("M8T3_MISSION_GENERATION end result=%d",result);
    return result;
}
