#include "vr_flight_bridge.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa_remaster/flight.h"
#include "xwa/assets/opt_model.h"
#include "xwa/assets/object_type.h"
#include "aeron/asset/opt_model.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <math.h>

static VrFlightSnapshot latest;
static uint64_t epoch, revision;
static int mission_loaded, pose_logged;
static int same_id(VrObjectId a, VrObjectId b) {
    return a.slot == b.slot && a.signature == b.signature;
}
void VrFlightBridge_Reset(void) {
    if (latest.flags & VR_FLIGHT_VALID) SDL_Log("M8_VR_FLIGHT_EXIT epoch=%llu objects=0", (unsigned long long)epoch);
    if (latest.target_index != VR_OBJECT_INDEX_NONE && latest.object_count)
        SDL_Log("M8_VR_TARGET_LOST reason=reset");
    memset(&latest, 0, sizeof latest);
    latest.abi_version = VR_FLIGHT_ABI_VERSION;
    latest.player_index = latest.target_index = VR_OBJECT_INDEX_NONE;
    latest.mission_epoch = epoch;
    latest.content_revision = ++revision;
    pose_logged = 0;
}
void VrFlightBridge_BeginMissionLoad(void) { mission_loaded = 0; VrFlightBridge_Reset(); }
void VrFlightBridge_EndMissionLoad(int success) {
    mission_loaded = success != 0;
    if (success) ++epoch;
    VrFlightBridge_Reset();
}
const VrFlightSnapshot *VrFlightBridge_GetLatest(void) { return &latest; }
static void asset_identity(const XwaSnapshot *s, uint16_t type, VrAssetIdentity *out) {
    const char *name = NULL;
    if (type < XWA_LOADED_MODEL_COUNT) {
        uint16_t h = g_loadedModels.byObjectType[type];
        for (uint32_t i=0; h && i<s->opt_asset_count; ++i)
            if (s->opt_assets[i].public_handle == h) {
                name = s->opt_assets[i].name; out->resolution = VR_ASSET_LOADED_OPT; break;
            }
    }
    if (!name) { name = XwaSnapshotExport_ModelName(type); out->resolution = VR_ASSET_MODEL_DEF_FALLBACK; }
    if (!name || !*name || strlen(name) >= sizeof out->opt_basename || strpbrk(name, "/\\.")) {
        out->resolution = VR_ASSET_UNRESOLVED; return;
    }
    SDL_strlcpy(out->opt_basename, name, sizeof out->opt_basename);
}
static int copy_object(const XwaSnapshot *s, const XwaFlightObject *in, VrFlightObject *out) {
    memset(out, 0, sizeof *out);
    out->id = (VrObjectId){in->slot, in->signature};
    out->object_type=in->object_type; out->genus=in->genus; out->render_region=in->render_region;
    memcpy(out->world_xwa, in->world_pos, sizeof out->world_xwa);
    if (!XwaRemasterFlight_ObjectModelMatrixAtOrigin(in, latest.origin_xwa, out->model_to_local_m)) return 0;
    for (int j=0;j<12;++j) {
        out->model_to_local_m[j] *= AERON_OPT_METERS_PER_UNIT;
        if (!isfinite(out->model_to_local_m[j])) return 0;
    }
    asset_identity(s, in->object_type, &out->asset);
    return 1;
}
void VrFlightBridge_CaptureAfterTick(uint64_t frame) {
    const XwaSnapshot *s=XwaSnapshot_Current();
    /* CS1 policy copies at most two objects; contract itself supports N. */
    VrFlightObject old[2] = {0};
    uint32_t old_count=latest.object_count, old_flags=latest.flags, old_state=latest.state;
    int32_t old_origin[3]; memcpy(old_origin, latest.origin_xwa, sizeof old_origin);
    if (old_count) memcpy(old, latest.objects, old_count * sizeof old[0]);
    latest.host_frame_id=frame; latest.flags=0; latest.object_count=0;
    latest.player_index=latest.target_index=VR_OBJECT_INDEX_NONE;
    latest.state=VR_FLIGHT_UNAVAILABLE; latest.mission_epoch=epoch;
    latest.abi_version=VR_FLIGHT_ABI_VERSION;
    if (s) {
        latest.flags=VR_FLIGHT_SOURCE_PRESENT;
        latest.tick_index=s->tick_index; latest.game_time_ms=s->game_time_ms;
        latest.opt_asset_generation=s->opt_asset_generation;
        latest.source_dropped_records=s->dropped_records;
        if (s->dropped_records) latest.flags |= VR_FLIGHT_SOURCE_DROPPED;
        switch(s->scene_kind) {
        case XWA_SCENE_FRONTEND: latest.state=VR_FLIGHT_FRONTEND; break;
        case XWA_SCENE_LOADING: latest.state=VR_FLIGHT_LOADING; break;
        case XWA_SCENE_FRONTEND_MODAL: latest.state=VR_FLIGHT_MODAL; break;
        case XWA_SCENE_CUTSCENE: latest.state=VR_FLIGHT_CUTSCENE; break;
        case XWA_SCENE_FLIGHT: latest.state=VR_FLIGHT_ACTIVE; break;
        default: break;
        }
    }
    if (s && mission_loaded && XwaFlightTask_IsActive() && latest.state==VR_FLIGHT_ACTIVE && s->flight_camera_valid) {
        const XwaFlightObject *player=NULL, *target=NULL;
        latest.region=s->flight_camera.region;
        if (s->flight_camera.in_hangar) latest.flags |= VR_FLIGHT_IN_HANGAR;
        for (uint32_t i=0;i<s->flight_object_count;++i)
            if ((int32_t)s->flight_objects[i].slot==s->flight_camera.player_obj_idx) player=&s->flight_objects[i];
        if (player) {
            memcpy(latest.origin_xwa, player->world_pos, sizeof latest.origin_xwa);
            if (copy_object(s, player, &latest.objects[0])) {
                latest.player_index=0; latest.object_count=1; latest.flags |= VR_FLIGHT_VALID;
                for (uint32_t i=0;i<s->flight_object_count;++i) {
                    const XwaFlightObject *o=&s->flight_objects[i];
                    if (o==player || !o->has_craft || o->object_type!=OBJ_TIEFighter ||
                        o->render_region!=latest.region || o->slot_class==XWA_SNAP_SLOT_OTHER) continue;
                    if (old_count==2 && same_id(old[1].id,(VrObjectId){o->slot,o->signature})) { target=o; break; }
                    if (!target || o->slot<target->slot) target=o;
                }
                if (target && copy_object(s,target,&latest.objects[1])) { latest.target_index=1; latest.object_count=2; }
                if (latest.object_count==2 && player->object_type==OBJ_XWing &&
                    latest.objects[0].asset.resolution==VR_ASSET_LOADED_OPT &&
                    latest.objects[1].asset.resolution==VR_ASSET_LOADED_OPT &&
                    !SDL_strcasecmp(latest.objects[0].asset.opt_basename,"XWING") &&
                    !SDL_strcasecmp(latest.objects[1].asset.opt_basename,"TIEFIGHTER")) latest.flags |= VR_FLIGHT_CS1_READY;
            }
        }
    }
    if (!(latest.flags & VR_FLIGHT_VALID)) memset(latest.origin_xwa,0,sizeof latest.origin_xwa);
    int changed=old_count!=latest.object_count || old_flags!=latest.flags || old_state!=latest.state;
    if (memcmp(old_origin,latest.origin_xwa,sizeof old_origin)) changed=1;
    for (uint32_t i=0;i<latest.object_count && i<old_count;++i) {
        const VrFlightObject *a=&old[i], *b=&latest.objects[i];
        if (!same_id(a->id,b->id) || a->object_type!=b->object_type || a->genus!=b->genus ||
            a->render_region!=b->render_region || memcmp(a->world_xwa,b->world_xwa,sizeof a->world_xwa) ||
            memcmp(a->model_to_local_m,b->model_to_local_m,sizeof a->model_to_local_m) ||
            a->asset.resolution!=b->asset.resolution || strcmp(a->asset.opt_basename,b->asset.opt_basename)) changed=1;
    }
    if (changed) latest.content_revision=++revision;
    if ((old_flags & VR_FLIGHT_VALID) && !(latest.flags & VR_FLIGHT_VALID)) SDL_Log("M8_VR_FLIGHT_EXIT objects=0");
    if (!(old_flags & VR_FLIGHT_VALID) && (latest.flags & VR_FLIGHT_VALID)) {
        SDL_Log("M8_VR_FLIGHT_ENTER epoch=%llu tick=%llu",(unsigned long long)epoch,(unsigned long long)latest.tick_index);
        SDL_Log("M8_VR_PLAYER_READY slot=%u signature=%u opt=%s",latest.objects[0].id.slot,latest.objects[0].id.signature,latest.objects[0].asset.opt_basename);
    }
    if (old_count==2 && (latest.object_count<2 || !same_id(old[1].id,latest.objects[1].id))) SDL_Log("M8_VR_TARGET_LOST");
    if (latest.object_count==2 && (old_count<2 || !same_id(old[1].id,latest.objects[1].id)))
        SDL_Log("M8_VR_TARGET_READY slot=%u signature=%u opt=%s",latest.objects[1].id.slot,latest.objects[1].id.signature,latest.objects[1].asset.opt_basename);
    if (!(old_flags & VR_FLIGHT_CS1_READY) && (latest.flags & VR_FLIGHT_CS1_READY))
        SDL_Log("M8_VR_SNAPSHOT_READY cs1=1 objects=2 units=metres");
    if (!pose_logged && old_count==2 && latest.object_count==2 && changed &&
        same_id(old[1].id,latest.objects[1].id)) {
        SDL_Log("M8_VR_POSE_UPDATED tick=%llu game_ms=%d revision=%llu",(unsigned long long)latest.tick_index,latest.game_time_ms,(unsigned long long)revision);
        pose_logged=1;
    }
}
