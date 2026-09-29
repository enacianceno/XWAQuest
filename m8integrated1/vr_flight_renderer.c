#include "vr_flight_renderer.h"
#include "vr_convert.h"
#include "aeron/scene/scene3d.h"
#include "aeron/scene/present.h"
#include "xwa_remaster/ship.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "aeron/scene/billboard.h"
#include "xwa_remaster/assets.h"
#include "vr_backdrop.h"
#include "vr_hud_draw.h"
#include "input_xr.h"
#include <math.h>
#include "vr_hud_math.h"
#include <SDL3/SDL.h>
#include <string.h>
#include "t3_features.h"
#include "xwa_remaster/sky_stars.h"

static AeronScene3D *scene[2];
static AeronRenderTarget *present[2];
static AeronScenePresentChain *chain;
static AeronSampler *sampler;
static int widths[2],heights[2],immersive,draw_mask,logged_mask,stereo_logged,eye_logged;
/* Borrowed only between BeginFrame and EndFrame: no simulation/asset sync in
   that interval. Never retained across frames or used after owner shutdown. */
static const AeronSceneMesh *frame_mesh;
static const AeronSceneMesh *frame_mesh_fb;
static int fb_logged; static VrObjectId fb_id;
static uint64_t frame_tick;
static const AeronSceneMesh *cockpit_mesh;
static XwaRemasterSkyStars *stars[2];
static int cockpit_logged,stars_logged,cockpit_missing_logged,target_missing_logged,bolt_logged,bd_logged,ships_logged,stick_logged,hand_logged;
/* P1/P3 transition telemetry: submitted-ship set, nosubmit latch, target
   view latch, cockpit enter/state latch. Enter/exit/nosubmit/cockpit-state
   log on eye 0 only; TARGET_VIEW is per-eye. */
static VrObjectId prev_ships[64]; static unsigned prev_ship_n;
static VrObjectId nosubmit_latch[8]; static unsigned nosubmit_n;
static int tv_logged[2], tv_have[2], tv_behind[2], tv_infov[2]; static VrObjectId tv_id[2];
static int ck_mesh_valid, ck_had_mesh, ck_state_valid;
static uint8_t ck_svariant; static char ck_opt[64]; static int ck_hull, ck_hmax, ck_sf, ck_hf;
static const char *begin_reason;
static const char *eye_reason[2];
static int begin_failure(const VrFlightSnapshot *s,const char *reason) {
    if(!begin_reason || strcmp(begin_reason,reason))
        SDL_Log("M8_VR_BEGIN_FRAME_BLOCKED reason=%s state=%d flags=%u tick=%llu",reason,
            s ? (int)s->state : -1,s ? s->flags : 0,(unsigned long long)(s ? s->tick_index : 0));
    begin_reason=reason;
    if(immersive) SDL_Log("M8_VR_IMMERSIVE_EXIT_REASON reason=%s state=%d flags=%u objects=%u player=%d target=%d cockpit=%d tick=%llu",reason,
        s ? (int)s->state : -1,s ? s->flags : 0,s ? s->object_count : 0,
        !s ? -2 : (s->player_index==VR_OBJECT_INDEX_NONE ? -1 : (int)s->player_index),
        !s ? -2 : (s->target_index==VR_OBJECT_INDEX_NONE ? -1 : (int)s->target_index),
        s ? (int)s->cockpit_valid : 0,(unsigned long long)(s ? s->tick_index : 0));
    if(immersive) VrFlightRenderer_Reset();
    frame_mesh=NULL; frame_mesh_fb=NULL;
    cockpit_mesh=NULL;
    return 0;
}
static AeronTexture *eye_failure(int i,const char *reason) {
    if(i<0 || i>1 || !eye_reason[i] || strcmp(eye_reason[i],reason))
        SDL_Log("M8_VR_RENDER_EYE_FAILED eye=%d reason=%s tick=%llu SDL=%s",i,reason,
            (unsigned long long)frame_tick,SDL_GetError());
    if(i>=0 && i<2) eye_reason[i]=reason;
    return NULL;
}
void VrFlightRenderer_Reset(void) {
    frame_mesh=NULL; frame_mesh_fb=NULL;
    cockpit_mesh=NULL;
    for(int i=0;i<2;++i) {
        if(scene[i]) AeronScene_Destroy(scene[i]); scene[i]=NULL;
        if(present[i]) Aeron_DestroyRenderTarget(present[i]); present[i]=NULL;
        widths[i]=heights[i]=0;
        if(stars[i]) XwaRemasterSkyStars_Destroy(stars[i]);
        stars[i]=NULL;
    }
    if(chain) AeronScenePresentChain_Destroy(chain); chain=NULL;
    if(sampler) Aeron_DestroySampler(sampler); sampler=NULL;
    VrHud_Reset();
    if(immersive) SDL_Log("M8_VR_IMMERSIVE_EXIT");
    immersive=draw_mask=logged_mask=stereo_logged=eye_logged=0;
    cockpit_logged=stars_logged=cockpit_missing_logged=target_missing_logged=bolt_logged=bd_logged=ships_logged=stick_logged=0;
    prev_ship_n=nosubmit_n=0; hand_logged=0; fb_logged=0;
    tv_logged[0]=tv_logged[1]=tv_have[0]=tv_have[1]=0;
    ck_mesh_valid=ck_had_mesh=ck_state_valid=0; ck_opt[0]=0;
}
int VrFlightRenderer_BeginFrame(const VrFlightSnapshot *s) {
    frame_mesh=NULL; frame_mesh_fb=NULL; draw_mask=0;
    if(!s || s->state!=VR_FLIGHT_ACTIVE) return begin_failure(s,"state_invalid");
    if(!(s->flags&VR_FLIGHT_VALID))
        return begin_failure(s,"flags_not_ready");
    if(s->object_count==0 || s->object_count>VR_FLIGHT_MAX_OBJECTS || s->player_index>=s->object_count)
        return begin_failure(s,"object_index_invalid");
    /* Phase A: the enemy target is opportunistic, never gating. Target
       destruction, retargeting, or asset changes must not exit VR. */
    if(s->target_index<s->object_count) {
        int runtime_opt=0;
        frame_mesh=XwaRemasterShip_MeshForNameWithSource(s->objects[s->target_index].asset.opt_basename,&runtime_opt);
        /* CS1 specifically requires original OPT, never a replacement GLB. */
        if(!runtime_opt) frame_mesh=NULL;
        if(frame_mesh && (!frame_mesh->vbo || !frame_mesh->ibo || !frame_mesh->index_count)) frame_mesh=NULL;
        if(!frame_mesh && !target_missing_logged) {
            SDL_Log("M8_VR_TARGET_MESH_UNAVAILABLE opt=%s",s->objects[s->target_index].asset.opt_basename);
            target_missing_logged=1;
        }
        if(!frame_mesh) {
            /* Playability fallback: submit the carried target through the
             * general path even when it is not original-OPT. Never CS1. */
            const AeronSceneMesh *fb=XwaRemasterShip_MeshForName(s->objects[s->target_index].asset.opt_basename);
            if(fb && fb->vbo && fb->ibo && fb->index_count) {
                VrObjectId fid=s->objects[s->target_index].id;
                frame_mesh_fb=fb;
                if(!fb_logged || fid.slot!=fb_id.slot || fid.signature!=fb_id.signature) {
                    SDL_Log("M8_VR_TARGET_MESH_FALLBACK id=%u:%u opt=%s indices=%u",fid.slot,fid.signature,
                        s->objects[s->target_index].asset.opt_basename,fb->index_count);
                    fb_logged=1; fb_id=fid;
                }
            }
        } else fb_logged=0;
    }
    begin_reason=NULL;
    cockpit_mesh=NULL;
    if(M8T3_COCKPIT && s->cockpit_valid) {
        int original=0;
        cockpit_mesh=XwaRemasterShip_MeshForNameWithSource(s->cockpit_opt,&original);
        if(!original || !cockpit_mesh || !cockpit_mesh->vbo || !cockpit_mesh->ibo || !cockpit_mesh->index_count) cockpit_mesh=NULL;
    }
    if(M8T3_COCKPIT && !cockpit_mesh && !cockpit_missing_logged) {
        SDL_Log("M8T3_COCKPIT_UNAVAILABLE valid=%d opt=%s",s->cockpit_valid,s->cockpit_opt);
        cockpit_missing_logged=1;
    }
    if(M8T3_COCKPIT) {
        int have=cockpit_mesh?1:0;
        if(!ck_mesh_valid || have!=ck_had_mesh) {
            if(have) SDL_Log("M8T3_COCKPIT_ENTER opt=%s variant=%u indices=%u emissive=1.0 lights=none cull=back",
                s->cockpit_opt,s->cockpit_variant,cockpit_mesh->index_count);
            else SDL_Log("M8T3_COCKPIT_EXIT reason=%s",s->cockpit_valid?"mesh_unavailable":"cockpit_invalid");
            ck_mesh_valid=1; ck_had_mesh=have;
        }
    }
    if(!immersive) SDL_Log("M8_VR_IMMERSIVE_ENTER opt=%s indices=%u source=runtime_OPT",
        frame_mesh ? s->objects[s->target_index].asset.opt_basename : "none",frame_mesh ? frame_mesh->index_count : 0);
    immersive=1; frame_tick=s->tick_index;
    return 1;
}
void VrFlightRenderer_ReadEye(const XrView *v,int w,int h,VrEyeState *o) {
    memset(o,0,sizeof *o);
    o->position_m[0]=v->pose.position.x; o->position_m[1]=v->pose.position.y; o->position_m[2]=v->pose.position.z;
    o->orientation[0]=v->pose.orientation.x; o->orientation[1]=v->pose.orientation.y;
    o->orientation[2]=v->pose.orientation.z; o->orientation[3]=v->pose.orientation.w;
    o->fov_left=v->fov.angleLeft; o->fov_right=v->fov.angleRight;
    o->fov_up=v->fov.angleUp; o->fov_down=v->fov.angleDown;
    o->width=w; o->height=h; o->near_z=.05f; o->valid=w>0 && h>0;
}
static int ensure_eye(int i,int w,int h) {
    if(scene[i] && widths[i]==w && heights[i]==h) return 1;
    if(scene[i]) AeronScene_Destroy(scene[i]); scene[i]=NULL;
    if(present[i]) Aeron_DestroyRenderTarget(present[i]); present[i]=NULL;
    scene[i]=AeronScene_Create(&(AeronScene3DDesc){.rt_width=w,.rt_height=h,
        .color_format=AERON_TEXTURE_FORMAT_RGBA16_FLOAT,.view_space_to_meters=1.0f});
    present[i]=Aeron_CreateRenderTarget(&(AeronRenderTargetDesc){.width=w,.height=h,
        .format=AERON_TEXTURE_FORMAT_RGBA8_SRGB,.debug_name="m8.cs1.present"});
    if(!chain) chain=AeronScenePresentChain_Create(AERON_TEXTURE_FORMAT_RGBA8_SRGB);
    if(!sampler) sampler=Aeron_CreateSampler(&(AeronSamplerDesc){.min_filter=AERON_FILTER_LINEAR,
        .mag_filter=AERON_FILTER_LINEAR,.address_u=AERON_ADDRESS_CLAMP_TO_EDGE,.address_v=AERON_ADDRESS_CLAMP_TO_EDGE});
    if(!scene[i] || !present[i] || !chain || !sampler) return 0;
    widths[i]=w; heights[i]=h;
    AeronScene_SetClearColor(scene[i],(float[4]){0,0,0,1});
    return 1;
}
/* Phase C: borrowed desktop flight-texture cache (captured via the
 * SyncFlightTextures wrap; same process/GPU device, zero duplication). */
static XwaRemasterAssets *bd_assets;
void VrFlightRenderer_NoteAssets(XwaRemasterAssets *a) { if (a) bd_assets = a; }
static int bd_resolve(int model_type, int frame, VrBdRef *out, void *user) {
    XwaAssetRef ref;
    (void)user;
    if (!out || !bd_assets) return 0;
    if (!XwaRemasterAssets_FlightModelFrame(bd_assets, model_type, frame, &ref)) return 0;
    if (!ref.texture) return 0;
    out->texture = ref.texture;
    out->u0 = ref.u0; out->v0 = ref.v0; out->u1 = ref.u1; out->v1 = ref.v1;
    out->classic_w = ref.classic_w; out->classic_h = ref.classic_h;
    return 1;
}
static float bd_corners[256][4][3];
static float bd_uvs[256][4][2];
static VrBdRef bd_refs[256];
static void nosubmit_note(const VrFlightObject *b) {
    unsigned k;
    for(k=0;k<nosubmit_n;++k)
        if(nosubmit_latch[k].slot==b->id.slot && nosubmit_latch[k].signature==b->id.signature) return;
    if(nosubmit_n>=8) { for(k=1;k<8;++k) nosubmit_latch[k-1]=nosubmit_latch[k]; nosubmit_n=7; }
    nosubmit_latch[nosubmit_n++]=b->id;
    SDL_Log("M8_VR_SHIP_NOSUBMIT id=%u:%u genus=%u opt=%s res=%u",b->id.slot,b->id.signature,b->genus,b->asset.opt_basename,b->asset.resolution);
}
static void nosubmit_clear(VrObjectId id) {
    for(unsigned k=0;k<nosubmit_n;++k)
        if(nosubmit_latch[k].slot==id.slot && nosubmit_latch[k].signature==id.signature) {
            for(unsigned j=k+1;j<nosubmit_n;++j) nosubmit_latch[j-1]=nosubmit_latch[j];
            --nosubmit_n; return;
        }
}
AeronTexture *VrFlightRenderer_RenderEye(int i,const VrEyeState *eye,const VrFlightSnapshot *s,AeronCommandBuffer *cmd) {
    if(i<0 || i>1 || !s || !eye || !cmd) return eye_failure(i,"arguments_invalid");
    if(s->tick_index!=frame_tick) return eye_failure(i,"tick_mismatch");
    if(!ensure_eye(i,eye->width,eye->height)) return eye_failure(i,"ensure_eye_failure");
    float p[3],q[4];
    if(!VrEye_Compose(s->objects[s->player_index].model_to_local_m,eye,p,q)) return eye_failure(i,"Compose_failure");
    XrPosef pose={{q[0],q[1],q[2],q[3]},{p[0],p[1],p[2]}};
    AeronSceneCamera camera;
    /* Preserve validated symmetric projection; raw four FOV angles remain in
       eye. Future asymmetric integration belongs here, not in the bridge. */
    VrConvert_Camera(&pose,atanf((tanf(eye->fov_right)-tanf(eye->fov_left))*.5f),
        atanf((tanf(eye->fov_up)-tanf(eye->fov_down))*.5f),eye->near_z,eye->width,eye->height,&camera);
    if(!eye_logged) { SDL_Log("M8_VR_EYE_STATE_READY space=LOCAL pose=vehicle_times_eye fov=symmetric_approx"); eye_logged=1; }
    if(!AeronScene_Begin(scene[i],&camera)) return eye_failure(i,"Scene_Begin_failure");
    if(M8T3_STARS && !s->death_star_mode) {
        if(!stars[i]) stars[i]=XwaRemasterSkyStars_Create();
        static const float world_to_cube[9]={1,0,0,0,0,1,0,-1,0};
        float upscale=(float)eye->height/480.0f;
        /* Run-4: denser/brighter field per physical feedback (reversible). */
        XwaRemasterSkyStarsParams params={.exposure=1,.brightness=1,.density=.9f,.grid_n=32,
            .core_radius_px=.8f*upscale,.feather_px=.8f*upscale,.pixel_pitch_px=upscale,
            .flare_strength=0,.game_time_ms=(uint32_t)s->game_time_ms};
        if(!stars[i] || !XwaRemasterSkyStars_Prepare(stars[i],scene[i],world_to_cube,&params))
            return eye_failure(i,"T3_stars_prepare_failure");
        AeronScene_SetPassHook(scene[i],AERON_SCENE_HOOK_BEFORE_OPAQUE,XwaRemasterSkyStars_Draw,stars[i]);
        if(!stars_logged) { SDL_Log("M8T3_STARS source=OpenXWA_procedural game_time_ms=%d",s->game_time_ms); stars_logged=1; }
    } else AeronScene_SetPassHook(scene[i],AERON_SCENE_HOOK_BEFORE_OPAQUE,NULL,NULL);
    /* Phase C: mission backdrops as SKY billboards (desktop derive law). */
    unsigned bd_total = 0;
    if (bd_assets && s->backdrop_count) {
        for (unsigned bi = 0; bi < s->backdrop_count; ++bi) {
            unsigned n = VrBd_Derive(&s->backdrops[bi], bd_resolve, NULL, p, VR_BD_SKY_DIST_M,
                                     bd_corners, bd_uvs, bd_refs, 256);
            for (unsigned q = 0; q < n; ++q) {
                AeronSceneBillboardDesc d;
                memset(&d, 0, sizeof d);
                d.texture = (AeronTexture *)bd_refs[q].texture;
                d.blend = AERON_SCENE_BILLBOARD_BLEND_PMA;
                d.stage = AERON_SCENE_BILLBOARD_STAGE_SKY;
                memcpy(d.corners, bd_corners[q], sizeof d.corners);
                memcpy(d.uv, bd_uvs[q], sizeof d.uv);
                for (int v = 0; v < 4; v++)
                    d.colors[v][0] = d.colors[v][1] = d.colors[v][2] = d.colors[v][3] = 1.0f;
                AeronScene_AddBillboard(scene[i], &d);
                bd_total++;
            }
        }
    }
    if (bd_total && !(bd_logged&(1<<i))) {
        SDL_Log("M8_VR_BACKDROPS_DRAWN eye=%d quads=%u backdrops=%u", i, bd_total, s->backdrop_count);
        bd_logged |= 1<<i;
    }
    if(frame_mesh || frame_mesh_fb) {
        AeronSceneMeshInstance inst={0};
        inst.mesh=frame_mesh?frame_mesh:frame_mesh_fb;
        memcpy(inst.transform,s->objects[s->target_index].model_to_local_m,sizeof inst.transform);
        memcpy(inst.prev_transform,inst.transform,sizeof inst.transform);
        inst.zero_velocity=1; inst.no_local_lights=1;
        /* Original sampled textures, unlit for CS1; no replacement geometry/shader. */
        inst.base_color_emissive_strength=1.0f;
        inst.cull_mode=AERON_CULL_BACK;
        AeronScene_AddMeshInstance(scene[i],&inst);
    }
    /* Phase B: REAL sim bolts. Desktop instance law: unlit HDR-emissive
       OPT ribbon, no shadow, no cull, submitted after ships. */
    unsigned bolts_drawn=0,bolt_indices=0,ships_drawn=0;
    VrObjectId cur_ships[64]; unsigned cur_n=0;
    for(unsigned bi=0;bi<s->object_count;++bi) {
        if(bi==s->player_index || bi==s->target_index) continue;
        const VrFlightObject *b=&s->objects[bi];
        int is_bolt = (b->genus==XWA_SNAP_GENUS_PLAYER_PROJECTILE || b->genus==XWA_SNAP_GENUS_NPC_PROJECTILE);
        if(!is_bolt && !VrHud_ShipGenus(b->genus)) continue;
        int bolt_opt=0;
        const AeronSceneMesh *bolt_mesh=XwaRemasterShip_MeshForNameWithSource(b->asset.opt_basename,&bolt_opt);
        if(!bolt_mesh || !bolt_mesh->vbo || !bolt_mesh->ibo || !bolt_mesh->index_count) { if(!i && !is_bolt) nosubmit_note(b); continue; }
        AeronSceneMeshInstance bolt={0};
        bolt.mesh=bolt_mesh;
        memcpy(bolt.transform,b->model_to_local_m,sizeof bolt.transform);
        memcpy(bolt.prev_transform,bolt.transform,sizeof bolt.transform);
        bolt.variant=b->node_switch;
        bolt.zero_velocity=1; bolt.no_local_lights=1;
        if(is_bolt) {
            bolt.shadow_flags=AERON_SCENE_INSTANCE_NO_CAST_SHADOW | AERON_SCENE_INSTANCE_NO_RECEIVE_SHADOW;
            bolt.velocity_stamp=1;
            if(bolt_opt) bolt.base_color_emissive_strength=XwaRemasterShip_OptProjectileEmissiveStrength();
            bolt.cull_mode=AERON_CULL_NONE;
        } else {
            /* Unlit emissive like the CS1 target (VR has no scene lights). */
            bolt.base_color_emissive_strength=1.0f;
            bolt.cull_mode=AERON_CULL_BACK;
        }
        AeronScene_AddMeshInstance(scene[i],&bolt);
        if(is_bolt) {
            if(!bolts_drawn) bolt_indices=bolt_mesh->index_count;
            bolts_drawn++;
        } else { ships_drawn++; nosubmit_clear(b->id); if(!i && cur_n<64) cur_ships[cur_n++]=b->id; }
    }
    if(bolts_drawn && !(bolt_logged&(1<<i))) {
        SDL_Log("M8_VR_BOLT_DRAW_RECORDED eye=%d count=%u indices=%u",i,bolts_drawn,bolt_indices);
        bolt_logged|=1<<i;
    }
    if(ships_drawn && !(ships_logged&(1<<i))) {
        SDL_Log("M8_VR_SHIPS_DRAWN eye=%d count=%u",i,ships_drawn);
        ships_logged|=1<<i;
    }
    if(!i) {
        /* Missing-row semantics: every carried ship is ENTERed on first
         * submit, NOSUBMIT-logged while skipped, EXITed when gone. */
        unsigned k,j,x;
        for(k=0;k<cur_n;++k) {
            int known=0;
            for(j=0;j<prev_ship_n;++j)
                if(prev_ships[j].slot==cur_ships[k].slot && prev_ships[j].signature==cur_ships[k].signature) { known=1; break; }
            if(!known) {
                const VrFlightObject *e=NULL;
                for(x=0;x<s->object_count;++x)
                    if(s->objects[x].id.slot==cur_ships[k].slot && s->objects[x].id.signature==cur_ships[k].signature) { e=&s->objects[x]; break; }
                SDL_Log("M8_VR_SHIP_ENTER id=%u:%u genus=%u opt=%s res=%u",cur_ships[k].slot,cur_ships[k].signature,
                    e?e->genus:0,e?e->asset.opt_basename:"?",e?e->asset.resolution:0u);
            }
        }
        for(j=0;j<prev_ship_n;++j) {
            int still=0;
            for(k=0;k<cur_n;++k)
                if(cur_ships[k].slot==prev_ships[j].slot && cur_ships[k].signature==prev_ships[j].signature) { still=1; break; }
            if(!still) {
                int carried=0, is_tgt=0;
                for(x=0;x<s->object_count;++x)
                    if(s->objects[x].id.slot==prev_ships[j].slot && s->objects[x].id.signature==prev_ships[j].signature) { carried=1; if(x==s->target_index) is_tgt=1; break; }
                SDL_Log("M8_VR_SHIP_EXIT id=%u:%u reason=%s",prev_ships[j].slot,prev_ships[j].signature,
                    is_tgt?"promoted_target":carried?"not_submitted":"not_carried");
            }
        }
        prev_ship_n=cur_n<64?cur_n:64;
        for(k=0;k<prev_ship_n;++k) prev_ships[k]=cur_ships[k];
    }
    if((frame_mesh || frame_mesh_fb) && s->target_index<s->object_count) {
        /* P1: submitted target vs eye gaze. Bearing/range from the Compose
         * eye pose; no projection-convention risk. */
        const float *tm=s->objects[s->target_index].model_to_local_m;
        float efwd[3],tgtp[3],bearing=0.f,maxhalf,t;
        float dx, dy, dz, range;
        VrHud_LocalPos(tm,tgtp);
        dx=tgtp[0]-p[0]; dy=tgtp[1]-p[1]; dz=tgtp[2]-p[2];
        range=sqrtf(dx*dx+dy*dy+dz*dz);
        VrObjectId tid=s->objects[s->target_index].id;
        int behind,infov;
        VrHud_EyeForward(q,efwd);
        if(range>1e-6f) bearing=VrHud_BearingDeg(efwd,p,tgtp);
        behind=((efwd[0]*dx+efwd[1]*dy+efwd[2]*dz)<0.f)?1:0;
        maxhalf=fabsf(eye->fov_left); t=fabsf(eye->fov_right); if(t>maxhalf) maxhalf=t;
        t=fabsf(eye->fov_up); if(t>maxhalf) maxhalf=t; t=fabsf(eye->fov_down); if(t>maxhalf) maxhalf=t;
        maxhalf=maxhalf*57.29578f+5.f;
        infov=(!behind && bearing<=maxhalf)?1:0;
        if(!tv_logged[i] || behind!=tv_behind[i] || infov!=tv_infov[i] ||
           tid.slot!=tv_id[i].slot || tid.signature!=tv_id[i].signature) {
            SDL_Log("M8_VR_TARGET_VIEW eye=%d id=%u:%u range_m=%.0f bearing_deg=%.1f behind=%d infov=%d",
                i,tid.slot,tid.signature,range,bearing,behind,infov);
            tv_logged[i]=1; tv_behind[i]=behind; tv_infov[i]=infov; tv_id[i]=tid;
        }
        tv_have[i]=1;
    } else if(tv_have[i]) {
        SDL_Log("M8_VR_TARGET_EXIT eye=%d tick=%llu",i,(unsigned long long)frame_tick);
        tv_have[i]=0; tv_logged[i]=0;
    }
    /* Physical stick V1: crossed quads in the seat frame (OVERLAY stage). */
    if(s->cockpit_valid) {
        AeronTexture *AeronSceneInternal_WhiteTexture(void);
        AeronTexture *white = AeronSceneInternal_WhiteTexture();
        const VrStickState *stick = M8_StickState();
        if(white && stick) {
            float sr[3],sf[3],su[3],sp[3],base[3],top[3],sq[4][4][3];
            /* TEST colors (final art later): bright yellow free, green hover, orange grab. */
            uint32_t hargb = !stick->grabbed && !stick->hover ? 0xffffd400u : (stick->grabbed ? 0xffff8c00u : 0xff00c800u);
            float htint[4],btint[4];
            int qq,vv;
            VrStick_SeatFrame(s->cockpit_to_local_m,sr,sf,su,sp);
            VrStick_RestPose(sp,sr,sf,su,base,top);
            VrStick_HandleTop(base,sr,sf,su,stick->dx,stick->dy,top);
            VrStick_Quads(base,top,sr,sf,su,sq);
            htint[0]=powf((float)((hargb>>16)&255u)/255.f,2.2f);
            htint[1]=powf((float)((hargb>>8)&255u)/255.f,2.2f);
            htint[2]=powf((float)(hargb&255u)/255.f,2.2f); htint[3]=1.f;
            btint[0]=btint[1]=btint[2]=powf(0.55f,2.2f); btint[3]=1.f; /* TEST: visible mount */
            for(qq=0;qq<4;qq++) {
                AeronSceneBillboardDesc d;
                memset(&d,0,sizeof d);
                d.texture=white; d.blend=AERON_SCENE_BILLBOARD_BLEND_PMA;
                d.stage=AERON_SCENE_BILLBOARD_STAGE_OVERLAY;
                d.depth_bias_view=0.02f; /* test stick wins local cockpit fights */
                memcpy(d.corners,sq[qq],sizeof d.corners);
                d.uv[0][0]=0.f; d.uv[0][1]=0.f; d.uv[1][0]=1.f; d.uv[1][1]=0.f;
                d.uv[2][0]=1.f; d.uv[2][1]=1.f; d.uv[3][0]=0.f; d.uv[3][1]=1.f;
                for(vv=0;vv<4;vv++) memcpy(d.colors[vv],qq<2?btint:htint,sizeof d.colors[vv]);
                AeronScene_AddBillboard(scene[i],&d);
            }
            { float gq[2][4][3]; int gk,gv2;
              VrStick_GripQuads(top,sr,sf,su,gq);
              for(gk=0;gk<2;gk++) {
                  AeronSceneBillboardDesc gd;
                  memset(&gd,0,sizeof gd);
                  gd.texture=white; gd.blend=AERON_SCENE_BILLBOARD_BLEND_PMA;
                  gd.stage=AERON_SCENE_BILLBOARD_STAGE_OVERLAY;
                  gd.depth_bias_view=0.02f;
                  memcpy(gd.corners,gq[gk],sizeof gd.corners);
                  gd.uv[0][0]=0.f; gd.uv[0][1]=0.f; gd.uv[1][0]=1.f; gd.uv[1][1]=0.f;
                  gd.uv[2][0]=1.f; gd.uv[2][1]=1.f; gd.uv[3][0]=0.f; gd.uv[3][1]=1.f;
                  for(gv2=0;gv2<4;gv2++) memcpy(gd.colors[gv2],htint,sizeof gd.colors[gv2]);
                  AeronScene_AddBillboard(scene[i],&gd);
              } }
            if(stick->grip_valid) {
                /* P4: hand marker + reach line wire the tested HandQuads/
                 * LineQuads into the frame; cyan marker tracks the grip. */
                float hand[3][4][3], line[2][4][3];
                float ht[4];
                int mk,vv2;
                VrStick_HandQuads(stick->grip_pos,sr,sf,su,hand);
                VrStick_LineQuads(stick->grip_pos,top,line);
                ht[0]=powf(0.1f,2.2f); ht[1]=powf(0.75f,2.2f); ht[2]=powf(1.f,2.2f); ht[3]=1.f;
                for(mk=0;mk<3;mk++) {
                    AeronSceneBillboardDesc hd;
                    memset(&hd,0,sizeof hd);
                    hd.texture=white; hd.blend=AERON_SCENE_BILLBOARD_BLEND_PMA;
                    hd.stage=AERON_SCENE_BILLBOARD_STAGE_OVERLAY;
                    hd.depth_bias_view=0.02f;
                    memcpy(hd.corners,hand[mk],sizeof hd.corners);
                    hd.uv[0][0]=0.f; hd.uv[0][1]=0.f; hd.uv[1][0]=1.f; hd.uv[1][1]=0.f;
                    hd.uv[2][0]=1.f; hd.uv[2][1]=1.f; hd.uv[3][0]=0.f; hd.uv[3][1]=1.f;
                    for(vv2=0;vv2<4;vv2++) memcpy(hd.colors[vv2],ht,sizeof hd.colors[vv2]);
                    AeronScene_AddBillboard(scene[i],&hd);
                }
                for(mk=0;mk<2;mk++) {
                    AeronSceneBillboardDesc ld;
                    memset(&ld,0,sizeof ld);
                    ld.texture=white; ld.blend=AERON_SCENE_BILLBOARD_BLEND_PMA;
                    ld.stage=AERON_SCENE_BILLBOARD_STAGE_OVERLAY;
                    ld.depth_bias_view=0.02f;
                    memcpy(ld.corners,line[mk],sizeof ld.corners);
                    ld.uv[0][0]=0.f; ld.uv[0][1]=0.f; ld.uv[1][0]=1.f; ld.uv[1][1]=0.f;
                    ld.uv[2][0]=1.f; ld.uv[2][1]=1.f; ld.uv[3][0]=0.f; ld.uv[3][1]=1.f;
                    for(vv2=0;vv2<4;vv2++) memcpy(ld.colors[vv2],ht,sizeof ld.colors[vv2]);
                    AeronScene_AddBillboard(scene[i],&ld);
                }
                if(!(hand_logged&(1<<i))) { SDL_Log("M8_VR_HAND_DRAWN eye=%d",i); hand_logged|=1<<i; }
            }
            if(!(stick_logged&(1<<i))) { SDL_Log("M8_VR_STICK_DRAWN eye=%d",i); stick_logged|=1<<i; }
        }
    }
    if(cockpit_mesh) {
        AeronSceneMeshInstance cockpit={0};
        cockpit.mesh=cockpit_mesh; cockpit.variant=s->cockpit_variant;
        memcpy(cockpit.transform,s->cockpit_to_local_m,sizeof cockpit.transform);
        memcpy(cockpit.prev_transform,cockpit.transform,sizeof cockpit.transform);
        cockpit.zero_velocity=1; cockpit.no_local_lights=1;
        cockpit.base_color_emissive_strength=1;
        cockpit.cull_mode=AERON_CULL_BACK;
        AeronScene_AddMeshInstance(scene[i],&cockpit);
    }
    if(!i && cockpit_mesh) {
        /* P3: correlate cockpit color reports with damage-variant switch,
         * damage-flash state, and hull damage. Transition-gated. */
        int hull=s->hud.valid?s->hud.instruments.hull_damage:0;
        int hmax=s->hud.valid?s->hud.instruments.hull_max:0;
        int sfl=s->hud.valid?s->hud.instruments.shield_damage_flash:0;
        int hfl=s->hud.valid?s->hud.instruments.hull_damage_flash:0;
        if(!ck_state_valid || s->cockpit_variant!=ck_svariant || hull!=ck_hull ||
           hmax!=ck_hmax || sfl!=ck_sf || hfl!=ck_hf || strcmp(s->cockpit_opt,ck_opt)) {
            SDL_Log("M8T3_COCKPIT_STATE variant=%u opt=%s hull=%d/%d sflash=%d hflash=%d tick=%llu",
                s->cockpit_variant,s->cockpit_opt,hull,hmax,sfl,hfl,(unsigned long long)frame_tick);
            ck_state_valid=1; ck_svariant=s->cockpit_variant; ck_hull=hull; ck_hmax=hmax; ck_sf=sfl; ck_hf=hfl;
            SDL_strlcpy(ck_opt,s->cockpit_opt,sizeof ck_opt);
        }
    }
    if(!AeronScene_Render(scene[i],cmd)) return eye_failure(i,"Scene_Render_failure");
    draw_mask|=1<<i;
    { float hud_vp[16];
      AeronScene_ComputeViewProj(&camera,hud_vp);
      VrHud_BuildEye(i,s,hud_vp,eye->width,eye->height);
      VrHud_PrepareEye(i,cmd); }
    if(cockpit_mesh && !(cockpit_logged&(1<<i))) {
        SDL_Log("M8T3_COCKPIT_DRAW_RECORDED eye=%d opt=%s indices=%u head_independent=1 visual=unconfirmed",i,s->cockpit_opt,cockpit_mesh->index_count);
        cockpit_logged|=1<<i;
    }
    if((frame_mesh || frame_mesh_fb) && !(logged_mask&(1<<i))) {
        const AeronSceneMesh *rm=frame_mesh?frame_mesh:frame_mesh_fb;
        SDL_Log("M8_VR_TARGET_DRAW_RECORDED eye=%d tick=%llu indices=%u src=%s visibility=unconfirmed",i,(unsigned long long)frame_tick,rm->index_count,frame_mesh?"opt":"fallback");
        logged_mask|=1<<i;
    }
    AeronTexture *source=Aeron_RenderTargetGetTexture(AeronScene_SceneRt(scene[i]));
    if(!source) return eye_failure(i,"scene_texture_missing");
    AeronRenderPass *pass=Aeron_BeginRenderPass(&(AeronRenderPassDesc){.command_buffer=cmd,
        .color_target=present[i],.clear_color=1,.clear_color_rgba={0,0,0,1},.debug_label="M8 CS1 SDR present"});
    if(!pass) return eye_failure(i,"present_pass_failure");
    AeronScenePresentChain_Draw(chain,pass,source,sampler,NULL,0,eye->width,eye->height,1,(float[4]){1,1,1,1},0);
    VrHud_CompositeEye(i,cmd,pass,present[i]);
    Aeron_EndRenderPass(pass);
    AeronTexture *result=Aeron_RenderTargetGetTexture(present[i]);
    if(!result) return eye_failure(i,"present_texture_missing");
    eye_reason[i]=NULL;
    return result;
}
void VrFlightRenderer_EndFrame(int submitted) {
    if(submitted && draw_mask==3 && !stereo_logged) {
        SDL_Log("M8_VR_FIRST_STEREO_FRAME tick=%llu eyes=2 gpu_submitted=1 presentation=unconfirmed",(unsigned long long)frame_tick);
        stereo_logged=1;
    }
    frame_mesh=NULL;
    cockpit_mesh=NULL;
}
