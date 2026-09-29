#include "vr_flight_renderer.h"
#include "vr_convert.h"
#include "aeron/scene/scene3d.h"
#include "aeron/scene/present.h"
#include "xwa_remaster/ship.h"
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
static uint64_t frame_tick;
static const AeronSceneMesh *cockpit_mesh;
static XwaRemasterSkyStars *stars[2];
static int cockpit_logged,stars_logged,cockpit_missing_logged;
static const char *begin_reason;
static const char *eye_reason[2];
static int begin_failure(const VrFlightSnapshot *s,const char *reason) {
    if(!begin_reason || strcmp(begin_reason,reason))
        SDL_Log("M8_VR_BEGIN_FRAME_BLOCKED reason=%s state=%d flags=%u tick=%llu",reason,
            s ? (int)s->state : -1,s ? s->flags : 0,(unsigned long long)(s ? s->tick_index : 0));
    begin_reason=reason;
    if(immersive) VrFlightRenderer_Reset();
    frame_mesh=NULL;
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
    frame_mesh=NULL;
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
    if(immersive) SDL_Log("M8_VR_IMMERSIVE_EXIT");
    immersive=draw_mask=logged_mask=stereo_logged=eye_logged=0;
    cockpit_logged=stars_logged=cockpit_missing_logged=0;
}
int VrFlightRenderer_BeginFrame(const VrFlightSnapshot *s) {
    frame_mesh=NULL; draw_mask=0;
    if(!s || s->state!=VR_FLIGHT_ACTIVE) return begin_failure(s,"state_invalid");
    if((s->flags&(VR_FLIGHT_VALID|VR_FLIGHT_CS1_READY))!=(VR_FLIGHT_VALID|VR_FLIGHT_CS1_READY))
        return begin_failure(s,"flags_not_ready");
    if(s->object_count>VR_FLIGHT_MAX_OBJECTS || s->target_index>=s->object_count || s->player_index>=s->object_count)
        return begin_failure(s,"object_index_invalid");
    int runtime_opt=0;
    frame_mesh=XwaRemasterShip_MeshForNameWithSource(s->objects[s->target_index].asset.opt_basename,&runtime_opt);
    /* CS1 specifically requires original OPT, never a replacement GLB. */
    if(!frame_mesh) return begin_failure(s,"mesh_missing");
    if(!runtime_opt) return begin_failure(s,"not_runtime_opt");
    if(!frame_mesh->vbo || !frame_mesh->ibo || !frame_mesh->index_count)
        return begin_failure(s,"invalid_mesh/index_count");
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
    if(!immersive) SDL_Log("M8_VR_IMMERSIVE_ENTER opt=%s indices=%u source=runtime_OPT",
        s->objects[s->target_index].asset.opt_basename,frame_mesh->index_count);
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
AeronTexture *VrFlightRenderer_RenderEye(int i,const VrEyeState *eye,const VrFlightSnapshot *s,AeronCommandBuffer *cmd) {
    if(i<0 || i>1 || !s || !eye || !cmd || !frame_mesh) return eye_failure(i,"arguments_invalid");
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
        XwaRemasterSkyStarsParams params={.exposure=1,.brightness=1,.density=.4f,.grid_n=32,
            .core_radius_px=.5f*upscale,.feather_px=.5f*upscale,.pixel_pitch_px=upscale,
            .flare_strength=0,.game_time_ms=(uint32_t)s->game_time_ms};
        if(!stars[i] || !XwaRemasterSkyStars_Prepare(stars[i],scene[i],world_to_cube,&params))
            return eye_failure(i,"T3_stars_prepare_failure");
        AeronScene_SetPassHook(scene[i],AERON_SCENE_HOOK_BEFORE_OPAQUE,XwaRemasterSkyStars_Draw,stars[i]);
        if(!stars_logged) { SDL_Log("M8T3_STARS source=OpenXWA_procedural game_time_ms=%d",s->game_time_ms); stars_logged=1; }
    } else AeronScene_SetPassHook(scene[i],AERON_SCENE_HOOK_BEFORE_OPAQUE,NULL,NULL);
    AeronSceneMeshInstance inst={0};
    inst.mesh=frame_mesh;
    memcpy(inst.transform,s->objects[s->target_index].model_to_local_m,sizeof inst.transform);
    memcpy(inst.prev_transform,inst.transform,sizeof inst.transform);
    inst.zero_velocity=1; inst.no_local_lights=1;
    /* Original sampled textures, unlit for CS1; no replacement geometry/shader. */
    inst.base_color_emissive_strength=1.0f;
    inst.cull_mode=AERON_CULL_BACK;
    AeronScene_AddMeshInstance(scene[i],&inst);
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
    if(!AeronScene_Render(scene[i],cmd)) return eye_failure(i,"Scene_Render_failure");
    draw_mask|=1<<i;
    if(cockpit_mesh && !(cockpit_logged&(1<<i))) {
        SDL_Log("M8T3_COCKPIT_DRAW_RECORDED eye=%d opt=%s indices=%u head_independent=1 visual=unconfirmed",i,s->cockpit_opt,cockpit_mesh->index_count);
        cockpit_logged|=1<<i;
    }
    if(!(logged_mask&(1<<i))) {
        SDL_Log("M8_VR_TARGET_DRAW_RECORDED eye=%d tick=%llu indices=%u visibility=unconfirmed",i,(unsigned long long)frame_tick,frame_mesh->index_count);
        logged_mask|=1<<i;
    }
    AeronTexture *source=Aeron_RenderTargetGetTexture(AeronScene_SceneRt(scene[i]));
    if(!source) return eye_failure(i,"scene_texture_missing");
    AeronRenderPass *pass=Aeron_BeginRenderPass(&(AeronRenderPassDesc){.command_buffer=cmd,
        .color_target=present[i],.clear_color=1,.clear_color_rgba={0,0,0,1},.debug_label="M8 CS1 SDR present"});
    if(!pass) return eye_failure(i,"present_pass_failure");
    AeronScenePresentChain_Draw(chain,pass,source,sampler,NULL,0,eye->width,eye->height,1,(float[4]){1,1,1,1},0);
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
