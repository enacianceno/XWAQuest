#ifndef M8_VR_FLIGHT_BRIDGE_H
#define M8_VR_FLIGHT_BRIDGE_H
#include <stdint.h>
#include "xwa_runtime/snapshot/snapshot.h"
#define VR_FLIGHT_ABI_VERSION 5u /* + full HUD state; private to this APK. */
#define VR_FLIGHT_MAX_OBJECTS 1664u
#define VR_OBJECT_INDEX_NONE UINT32_MAX
typedef struct VrObjectId { uint16_t slot, signature; } VrObjectId;
enum { VR_ASSET_UNRESOLVED, VR_ASSET_LOADED_OPT, VR_ASSET_MODEL_DEF_FALLBACK };
typedef struct VrAssetIdentity { uint32_t resolution; char opt_basename[64]; } VrAssetIdentity;
enum { VR_FLIGHT_UNAVAILABLE, VR_FLIGHT_FRONTEND, VR_FLIGHT_LOADING,
       VR_FLIGHT_ACTIVE, VR_FLIGHT_MODAL, VR_FLIGHT_CUTSCENE };
enum { VR_FLIGHT_VALID=1u, VR_FLIGHT_SOURCE_PRESENT=2u, VR_FLIGHT_SOURCE_DROPPED=4u,
       VR_FLIGHT_IN_HANGAR=8u, VR_FLIGHT_CS1_READY=16u };
typedef struct VrFlightObject {
    VrObjectId id;
    uint16_t object_type;
    uint8_t genus, render_region;
    uint8_t node_switch, obj_pad[3];
    int32_t world_xwa[3];
    /* Row-major, column-vector: OPT-axis model metres -> local XWA-axis metres.
       Translation: [3,7,11]. Origin translates with player, never rotates. */
    float model_to_local_m[16];
    VrAssetIdentity asset;
} VrFlightObject;
typedef struct VrFlightSnapshot {
    uint32_t abi_version, state, flags;
    uint64_t host_frame_id, mission_epoch, tick_index;
    int32_t game_time_ms;
    uint64_t opt_asset_generation, content_revision;
    uint32_t source_dropped_records, object_count, player_index, target_index;
    int32_t origin_xwa[3];
    uint8_t region;
    /* T3: copied seat-0 cockpit pose in the same local metre frame as objects. */
    uint8_t cockpit_valid, cockpit_variant, death_star_mode;
    char cockpit_opt[40];
    float cockpit_to_local_m[16];
    /* Mission backdrops (suns/planets/nebulae): mission-static STATE. */
    uint32_t backdrop_count;
    XwaBackdrop backdrops[XWA_SNAP_MAX_BACKDROPS];
    /* Full HUD state (reticle/target/instruments/threats/boxes). No pointers. */
    XwaHudState hud;
    VrFlightObject objects[VR_FLIGHT_MAX_OBJECTS];
} VrFlightSnapshot;
/* Single host thread only. Borrowed adapter storage expires on any mutation. */
void VrFlightBridge_Reset(void);
void VrFlightBridge_BeginMissionLoad(void);
void VrFlightBridge_EndMissionLoad(int success);
void VrFlightBridge_CaptureAfterTick(uint64_t host_frame_id);
const VrFlightSnapshot *VrFlightBridge_GetLatest(void);
#endif
