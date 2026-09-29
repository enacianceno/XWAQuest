#include "vr_flight_bridge.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa_remaster/flight.h"
#include "xwa/assets/opt_model.h"
#include "xwa/assets/object_type.h"
#include "aeron/asset/opt_model.h"
#include "aeron/scene/world.h"
#include "vr_hud_math.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <math.h>
#include "t3_features.h"
#include "t3_seat_math.h"

static VrFlightSnapshot latest;
static uint64_t epoch, revision;
static int mission_loaded, pose_logged, ships_trunc_logged, bolts_trunc_logged, hud_ready_logged;
static uint8_t old_sflash, old_hflash;
static int32_t old_hulldmg;
static uint16_t old_tslot, old_tsig;
static int old_gen_valid, old_ncls, old_dn;
static uint32_t old_gc, old_gso, old_gp, old_go, old_rc, old_rso, old_rp;
static int old_ts_valid, old_ts_simv, old_ts_brv, old_ts_bidx;
static uint32_t old_ts_sslot, old_ts_ssig, old_ts_bslot, old_ts_bsig;
static int same_id(VrObjectId a, VrObjectId b) {
    return a.slot == b.slot && a.signature == b.signature;
}
static const char *tsrc_name(int s) { return s==1?"sim":s==2?"sticky":s==3?"nearest":"none"; }
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
    ships_trunc_logged = bolts_trunc_logged = hud_ready_logged = 0;
    old_sflash = old_hflash = 0; old_hulldmg = 0; old_tslot = old_tsig = 0;
    old_gen_valid=0; old_gc=old_gso=old_gp=old_go=old_rc=old_rso=old_rp=0; old_ncls=old_dn=0;
    old_ts_valid=0; old_ts_simv=old_ts_brv=0; old_ts_bidx=-1; old_ts_sslot=old_ts_ssig=old_ts_bslot=old_ts_bsig=0;
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
static int copy_object(const XwaSnapshot *s, const XwaFlightObject *in, VrFlightObject *out, int roll_align, const float *cam_minus_obj) {
    memset(out, 0, sizeof *out);
    out->id = (VrObjectId){in->slot, in->signature};
    out->object_type=in->object_type; out->genus=in->genus; out->render_region=in->render_region;
    out->node_switch=in->node_switch;
    memcpy(out->world_xwa, in->world_pos, sizeof out->world_xwa);
    if (!XwaRemasterFlight_ObjectModelMatrixAtOrigin(in, latest.origin_xwa, out->model_to_local_m)) return 0;
    if (roll_align && cam_minus_obj) {
        float aligned[16];
        if (!XwaRemasterFlight_ObjectModelMatrixForCameraDelta(in, cam_minus_obj, 1, aligned)) return 0;
        out->model_to_local_m[0]=aligned[0]; out->model_to_local_m[1]=aligned[1]; out->model_to_local_m[2]=aligned[2];
        out->model_to_local_m[4]=aligned[4]; out->model_to_local_m[5]=aligned[5]; out->model_to_local_m[6]=aligned[6];
        out->model_to_local_m[8]=aligned[8]; out->model_to_local_m[9]=aligned[9]; out->model_to_local_m[10]=aligned[10];
    }
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
    static VrFlightObject old[VR_FLIGHT_MAX_OBJECTS];
    uint32_t old_count=latest.object_count, old_flags=latest.flags, old_state=latest.state, old_bd=latest.backdrop_count, old_tidx=latest.target_index;
    int target_src=0;
    int32_t old_origin[3]; memcpy(old_origin, latest.origin_xwa, sizeof old_origin);
    if (old_count) memcpy(old, latest.objects, old_count * sizeof old[0]);
    latest.host_frame_id=frame; latest.flags=0; latest.object_count=0; latest.backdrop_count=0; memset(&latest.hud,0,sizeof latest.hud);
    latest.cockpit_valid=0; latest.cockpit_opt[0]=0;
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
        latest.backdrop_count=s->backdrop_count < XWA_SNAP_MAX_BACKDROPS ? s->backdrop_count : XWA_SNAP_MAX_BACKDROPS;
        memcpy(latest.backdrops,s->backdrops,latest.backdrop_count*sizeof latest.backdrops[0]);
        latest.hud=s->hud;
        for (uint32_t i=0;i<s->flight_object_count;++i)
            if ((int32_t)s->flight_objects[i].slot==s->flight_camera.player_obj_idx) player=&s->flight_objects[i];
        if (player) {
            memcpy(latest.origin_xwa, player->world_pos, sizeof latest.origin_xwa);
            if (copy_object(s, player, &latest.objects[0], 0, NULL)) {
                latest.player_index=0; latest.object_count=1; latest.flags |= VR_FLIGHT_VALID;
                latest.death_star_mode=s->flight_camera.death_star_mode;
                /* Seat 0 specialization of flight.c fl_cockpit_model_matrix:
                   camera_rows^T * camera_rows cancels for the offset. The
                   classic seat offset is negated hardpoint_world + pan/16.
                   Use the ship origin instead of the moving physical eye so
                   leaning/turning the head NEVER moves the cockpit with it. */
                if(M8T3_COCKPIT && s->cockpit_valid && s->cockpit.seat==0 &&
                   s->cockpit.model_name[0] && s->flight_camera.cockpit_visible) {
                    SDL_strlcpy(latest.cockpit_opt,s->cockpit.model_name,sizeof latest.cockpit_opt);
                    latest.cockpit_valid=T3_SeatMatrix(latest.objects[0].model_to_local_m,
                        s->cockpit.hardpoint_world,s->cockpit.camera_pan,AERON_OPT_METERS_PER_UNIT,latest.cockpit_to_local_m);
                    latest.cockpit_variant=player->node_switch;
                }
                /* Generic render-target policy (no ship-type exceptions):
                 * sim-selected target first (authoritative), then sticky
                 * incumbent, then nearest targetable combat craft. */
                if (latest.hud.valid && latest.hud.target.valid) {
                    for (uint32_t i=0;i<s->flight_object_count;++i) {
                        const XwaFlightObject *o=&s->flight_objects[i];
                        if (o==player) continue;
                        if (o->slot==latest.hud.target.slot && o->signature==latest.hud.target.signature &&
                            o->render_region==latest.region && o->slot_class!=XWA_SNAP_SLOT_OTHER) { target=o; target_src=1; break; }
                    }
                }
                if (!target && old_tidx<old_count) {
                    VrObjectId otid = old[old_tidx].id;
                    for (uint32_t i=0;i<s->flight_object_count;++i) {
                        const XwaFlightObject *o=&s->flight_objects[i];
                        if (o==player) continue;
                        if (same_id(otid,(VrObjectId){o->slot,o->signature}) &&
                            o->render_region==latest.region && o->slot_class!=XWA_SNAP_SLOT_OTHER) { target=o; target_src=2; break; }
                    }
                }
                if (!target) {
                    int64_t best = INT64_MAX;
                    for (uint32_t i=0;i<s->flight_object_count;++i) {
                        const XwaFlightObject *o=&s->flight_objects[i];
                        int64_t dx, dy, dz, d2;
                        if (o==player) continue;
                        if (!VrHud_TargetGenus(o->genus)) continue;
                        if (o->render_region!=latest.region || o->slot_class==XWA_SNAP_SLOT_OTHER) continue;
                        dx=(int64_t)o->world_pos[0]-latest.origin_xwa[0];
                        dy=(int64_t)o->world_pos[1]-latest.origin_xwa[1];
                        dz=(int64_t)o->world_pos[2]-latest.origin_xwa[2];
                        d2=dx*dx+dy*dy+dz*dz;
                        if (!target || d2<best) { target=o; best=d2; target_src=3; }
                    }
                }
                if (target && copy_object(s,target,&latest.objects[1],0,NULL)) { latest.target_index=1; latest.object_count=2; }
                /* Phase B: REAL sim projectiles. Desktop pose law (roll-aligned
                 * bolts), appended after player+target. */
                { unsigned bolts = 0;
                for (uint32_t i=0;i<s->flight_object_count && latest.object_count<VR_FLIGHT_MAX_OBJECTS;++i) {
                    const XwaFlightObject *o=&s->flight_objects[i];
                    if (o==player || o==target) continue;
                    if (o->genus!=XWA_SNAP_GENUS_PLAYER_PROJECTILE && o->genus!=XWA_SNAP_GENUS_NPC_PROJECTILE) continue;
                    if (o->render_region!=latest.region || o->slot_class==XWA_SNAP_SLOT_OTHER) continue;
                    if (bolts >= 128) { if (!bolts_trunc_logged) { SDL_Log("M8_VR_BOLTS_TRUNCATED"); bolts_trunc_logged = 1; } break; }
                    float cmd[3];
                    AeronWorld_DeltaI32(s->flight_camera.world_pos,o->world_pos,cmd);
                    if (copy_object(s,o,&latest.objects[latest.object_count],1,cmd)) { latest.object_count++; bolts++; }
                } }
                /* Phase D: in-region ships for multi-ship VR + target boxes.
                 * Nearest-48 (not snapshot order): the closest ships are the
                 * visible ones, so they must win the cap. */
                { unsigned ships = 0, ncand = 0, nw = 0, wi;
                  static unsigned cand[XWA_SNAP_MAX_FLIGHT_OBJECTS];
                  static unsigned win[48];
                  for (uint32_t i=0;i<s->flight_object_count && ncand<XWA_SNAP_MAX_FLIGHT_OBJECTS;++i) {
                      const XwaFlightObject *o=&s->flight_objects[i];
                      if (o==player || o==target) continue;
                      if (!VrHud_ShipGenus(o->genus)) continue;
                      if (o->render_region!=latest.region || o->slot_class==XWA_SNAP_SLOT_OTHER) continue;
                      cand[ncand++]=i;
                  }
                  if (ncand>48 && !ships_trunc_logged) { SDL_Log("M8_VR_SHIPS_TRUNCATED candidates=%u",ncand); ships_trunc_logged=1; }
                  nw=VrHud_SelectNearest(s->flight_objects,s->flight_object_count,latest.origin_xwa,cand,ncand,48,win);
                  for (wi=0;wi<nw && latest.object_count<VR_FLIGHT_MAX_OBJECTS;++wi) {
                      if (copy_object(s,&s->flight_objects[win[wi]],&latest.objects[latest.object_count],0,NULL)) { latest.object_count++; ships++; }
                  } }
                /* Decisive general-object telemetry (P1): native genus mix vs
                 * promoted mix. Transition-gated. dropped_nearest=1 proves the
                 * nearest targetable combat craft failed to carry. */
                { uint32_t g_c=0, g_so=0, g_p=0, g_o=0, r_c=0, r_so=0, r_p=0;
                  int64_t g_best=INT64_MAX, c_best=INT64_MAX;
                  uint32_t bi=VR_OBJECT_INDEX_NONE; int ncls=0, dn=0;
                  for (uint32_t i=0;i<s->flight_object_count;++i) {
                      const XwaFlightObject *o=&s->flight_objects[i];
                      int64_t dx, dy, dz, d2;
                      if (o==player) continue;
                      if (o->render_region!=latest.region || o->slot_class==XWA_SNAP_SLOT_OTHER) continue;
                      if (o->genus==XWA_SNAP_GENUS_PLAYER_PROJECTILE||o->genus==XWA_SNAP_GENUS_NPC_PROJECTILE) { ++g_p; continue; }
                      if (VrHud_TargetGenus(o->genus)) {
                          ++g_c;
                          dx=(int64_t)o->world_pos[0]-latest.origin_xwa[0];
                          dy=(int64_t)o->world_pos[1]-latest.origin_xwa[1];
                          dz=(int64_t)o->world_pos[2]-latest.origin_xwa[2];
                          d2=dx*dx+dy*dy+dz*dz;
                          if (d2<g_best) g_best=d2;
                      } else if (VrHud_ShipGenus(o->genus)) ++g_so;
                      else ++g_o;
                  }
                  for (uint32_t i=0;i<latest.object_count;++i) {
                      const VrFlightObject *o=&latest.objects[i];
                      int64_t dx, dy, dz, d2;
                      if (i==latest.player_index) continue;
                      if (o->genus==XWA_SNAP_GENUS_PLAYER_PROJECTILE||o->genus==XWA_SNAP_GENUS_NPC_PROJECTILE) { ++r_p; continue; }
                      if (VrHud_TargetGenus(o->genus)) {
                          ++r_c;
                          dx=(int64_t)o->world_xwa[0]-latest.origin_xwa[0];
                          dy=(int64_t)o->world_xwa[1]-latest.origin_xwa[1];
                          dz=(int64_t)o->world_xwa[2]-latest.origin_xwa[2];
                          d2=dx*dx+dy*dy+dz*dz;
                          if (d2<c_best) { c_best=d2; bi=i; }
                      } else if (VrHud_ShipGenus(o->genus)) ++r_so;
                  }
                  if (bi!=VR_OBJECT_INDEX_NONE) ncls=(bi==latest.target_index)?1:2;
                  dn=(g_c>0 && (bi==VR_OBJECT_INDEX_NONE || c_best!=g_best))?1:0;
                  if (!old_gen_valid || g_c!=old_gc || g_so!=old_gso || g_p!=old_gp || g_o!=old_go ||
                      r_c!=old_rc || r_so!=old_rso || r_p!=old_rp || ncls!=old_ncls || dn!=old_dn)
                      SDL_Log("M8_VR_SHIPS_GEN snap=%u combat=%u shipother=%u proj=%u other=%u carried=%u rcombat=%u rshipother=%u rproj=%u nearest=%s dropped_nearest=%u",
                          s->flight_object_count,g_c,g_so,g_p,g_o,latest.object_count,r_c,r_so,r_p,
                          ncls==0?"none":ncls==1?"target":"carried",dn);
                  old_gen_valid=1; old_gc=g_c; old_gso=g_so; old_gp=g_p; old_go=g_o;
                  old_rc=r_c; old_rso=r_so; old_rp=r_p; old_ncls=ncls; old_dn=dn; }
                /* Player + render target both original-OPT ready (generic). */
                if (latest.object_count>=2 && latest.target_index==1 &&
                    latest.objects[0].asset.resolution==VR_ASSET_LOADED_OPT &&
                    latest.objects[1].asset.resolution==VR_ASSET_LOADED_OPT) latest.flags |= VR_FLIGHT_CS1_READY;
            }
        }
    }
    if (!(latest.flags & VR_FLIGHT_VALID)) memset(latest.origin_xwa,0,sizeof latest.origin_xwa);
    int changed=old_count!=latest.object_count || old_flags!=latest.flags || old_state!=latest.state;
    if (memcmp(old_origin,latest.origin_xwa,sizeof old_origin)) changed=1;
    for (uint32_t i=0;i<latest.object_count && i<old_count;++i) {
        const VrFlightObject *a=&old[i], *b=&latest.objects[i];
        if (!same_id(a->id,b->id) || a->object_type!=b->object_type || a->genus!=b->genus || a->node_switch!=b->node_switch ||
            a->render_region!=b->render_region || memcmp(a->world_xwa,b->world_xwa,sizeof a->world_xwa) ||
            memcmp(a->model_to_local_m,b->model_to_local_m,sizeof a->model_to_local_m) ||
            a->asset.resolution!=b->asset.resolution || strcmp(a->asset.opt_basename,b->asset.opt_basename)) changed=1;
    }
    if (changed) latest.content_revision=++revision;
    if ((old_flags & VR_FLIGHT_VALID) && !(latest.flags & VR_FLIGHT_VALID)) SDL_Log("M8_VR_FLIGHT_EXIT objects=0");
    if (!(old_flags & VR_FLIGHT_VALID) && (latest.flags & VR_FLIGHT_VALID)) {
        SDL_Log("M8_VR_FLIGHT_ENTER epoch=%llu tick=%llu",(unsigned long long)epoch,(unsigned long long)latest.tick_index);
        SDL_Log("M8_VR_PLAYER_READY slot=%u signature=%u opt=%s",latest.objects[0].id.slot,latest.objects[0].id.signature,latest.objects[0].asset.opt_basename);
        SDL_Log("M8T3_PLAYER player_obj_idx=%u object_type=%u craft=%s confirmed_xwing=%d model_index=%d model_index_source=committed_hud",
            latest.objects[0].id.slot,latest.objects[0].object_type,latest.objects[0].asset.opt_basename,latest.objects[0].object_type==OBJ_XWing,
            s && s->hud.valid && s->hud.player_slot==latest.objects[0].id.slot ? s->hud.instruments.player_model_index : -1);
    }
    if (old_tidx<old_count && (latest.target_index>=latest.object_count || !same_id(old[old_tidx].id,latest.objects[latest.target_index].id))) SDL_Log("M8_VR_TARGET_LOST");
    if (latest.target_index==1 && latest.object_count>=2 && (old_tidx>=old_count || !same_id(old[old_tidx].id,latest.objects[1].id)))
        SDL_Log("M8_VR_TARGET_READY slot=%u signature=%u opt=%s source=%s",latest.objects[1].id.slot,latest.objects[1].id.signature,latest.objects[1].asset.opt_basename,tsrc_name(target_src));
    { uint32_t ob=0,nb=0;
      for (uint32_t i=0;i<old_count;++i) if (old[i].genus==XWA_SNAP_GENUS_PLAYER_PROJECTILE||old[i].genus==XWA_SNAP_GENUS_NPC_PROJECTILE) ob++;
      for (uint32_t i=0;i<latest.object_count;++i) if (latest.objects[i].genus==XWA_SNAP_GENUS_PLAYER_PROJECTILE||latest.objects[i].genus==XWA_SNAP_GENUS_NPC_PROJECTILE) nb++;
      if (!ob && nb) SDL_Log("M8_VR_BOLTS_CAPTURED count=%u tick=%llu",nb,(unsigned long long)latest.tick_index);
      if (ob && !nb) SDL_Log("M8_VR_BOLTS_CLEARED tick=%llu",(unsigned long long)latest.tick_index); }
    if (!old_bd && latest.backdrop_count) SDL_Log("M8_VR_BACKDROPS_CAPTURED count=%u tick=%llu",latest.backdrop_count,(unsigned long long)latest.tick_index);
    if (latest.hud.valid && !hud_ready_logged) { SDL_Log("M8_VR_HUD_READY glyphs=%u panes=%u boxes=%u blips=%u",latest.hud.glyph_count,latest.hud.pane_count,latest.hud.target_box_count,latest.hud.radar_blip_count); hud_ready_logged = 1; }
    if (latest.hud.valid) {
        const XwaHudInstruments *ni = &latest.hud.instruments;
        if (!old_sflash && ni->shield_damage_flash) SDL_Log("M8_VR_DAMAGE_FLASH kind=shield side=%u tick=%llu",ni->last_shield_damage_side,(unsigned long long)latest.tick_index);
        if (!old_hflash && ni->hull_damage_flash) SDL_Log("M8_VR_DAMAGE_FLASH kind=hull tick=%llu",(unsigned long long)latest.tick_index);
        if (ni->hull_damage != old_hulldmg) SDL_Log("M8_VR_HULL_STEP hull=%d/%d shields=%d/%d tick=%llu",ni->hull_damage,ni->hull_max,ni->shield_front,ni->shield_rear,(unsigned long long)latest.tick_index);
        old_sflash = ni->shield_damage_flash; old_hflash = ni->hull_damage_flash; old_hulldmg = ni->hull_damage;
        if (latest.hud.target.valid && (latest.hud.target.slot != old_tslot || latest.hud.target.signature != old_tsig))
            SDL_Log("M8_VR_TARGET_ID slot=%u sig=%u name=%.29s dist=%u.%02u",latest.hud.target.slot,latest.hud.target.signature,latest.hud.target.name,latest.hud.target.distance_whole,latest.hud.target.distance_frac);
        old_tslot = latest.hud.target.slot; old_tsig = latest.hud.target.signature;
        { /* P2: sim-claimed target vs bridge render target, one line. */
          int sim_v=(latest.hud.valid && latest.hud.target.valid)?1:0;
          int br_v=(latest.target_index<latest.object_count)?1:0;
          uint32_t sslot=sim_v?(uint32_t)latest.hud.target.slot:0u, ssig=sim_v?(uint32_t)latest.hud.target.signature:0u;
          uint32_t bslot=br_v?latest.objects[latest.target_index].id.slot:0u;
          uint32_t bsig=br_v?latest.objects[latest.target_index].id.signature:0u;
          int bidx=br_v?(int)latest.target_index:-1;
          int agree=(sim_v&&br_v&&sslot==bslot&&ssig==bsig)?1:0;
          if (!old_ts_valid || sim_v!=old_ts_simv || sslot!=old_ts_sslot || ssig!=old_ts_ssig ||
              br_v!=old_ts_brv || bidx!=old_ts_bidx || bslot!=old_ts_bslot || bsig!=old_ts_bsig)
              SDL_Log("M8_VR_TARGET_STATE sim_valid=%d sim=%u:%u bridge_idx=%d bridge=%u:%u src=%s agree=%d",
                  sim_v,sslot,ssig,bidx,bslot,bsig,tsrc_name(target_src),agree);
          old_ts_valid=1; old_ts_simv=sim_v; old_ts_sslot=sslot; old_ts_ssig=ssig;
          old_ts_brv=br_v; old_ts_bidx=bidx; old_ts_bslot=bslot; old_ts_bsig=bsig; }
    }
    if (!(old_flags & VR_FLIGHT_CS1_READY) && (latest.flags & VR_FLIGHT_CS1_READY))
        SDL_Log("M8_VR_SNAPSHOT_READY cs1=1 objects=%u units=metres",latest.object_count);
    if (!pose_logged && old_tidx<old_count && latest.target_index<latest.object_count && changed &&
        same_id(old[old_tidx].id,latest.objects[latest.target_index].id)) {
        SDL_Log("M8_VR_POSE_UPDATED tick=%llu game_ms=%d revision=%llu",(unsigned long long)latest.tick_index,latest.game_time_ms,(unsigned long long)revision);
        pose_logged=1;
    }
}
