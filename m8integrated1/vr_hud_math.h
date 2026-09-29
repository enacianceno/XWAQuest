/* Pure per-eye HUD math (Phase D/E): projection, anchors, brackets, clamps.
 * No XR/SDL/GPU calls. CPU-testable on host. */
#ifndef M8INTEGRATED1_VR_HUD_MATH_H
#define M8INTEGRATED1_VR_HUD_MATH_H

#include <stdint.h>
#include "xwa_runtime/snapshot/snapshot.h"

/* Project a world point with a row-major view-projection matrix using the
 * same row-vector convention as AeronScene_ComputeViewProj consumers
 * (cx = vp[0]*x+vp[1]*y+vp[2]*z+vp[3], cy = vp[4..7], cw = vp[12..15]).
 * Pixels are top-left origin, +Y down. Returns 1 when in front (cw > 0);
 * *behind is set when cw <= 0 (caller mirrors for behind-camera arrows). */
int VrHud_Project(const float vp[16], const float world[3], int vp_w, int vp_h,
                  float *out_x, float *out_y, float *out_w, int *behind);

/* Ship-forward anchor: player local position (matrix translation [3,7,11])
 * plus ship forward (model -Y = negated matrix column 1) times dist_m. */
void VrHud_ShipForward(const float player_mat[16], float dist_m, float out[3]);

/* Pixel half-size of a world-radius sphere at view depth. focal_px is the
 * horizontal focal length in pixels (vp[0]*vp_w*0.5 for symmetric views).
 * Returns 0 for non-positive depth (caller skips). */
float VrHud_AngularHalfPx(float radius_m, float view_depth_m, float focal_px);

/* Clamp a point into [margin, w-margin]x[margin, h-margin]. Returns 1 when
 * the input was already inside (no clamp needed). */
int VrHud_ClampToRect(float x, float y, int w, int h, float margin,
                      float *out_x, float *out_y);

/* Mesh-drawn ship genera carried/rendered in VR V1 (fighters through
 * stations, mines, containers, asteroids). Debris/explosions/scenery/
 * tunnel/droids/rubble/salvage are deferred. */
int VrHud_ShipGenus(uint8_t genus);

/* Targetable combat genera for the bridge render-target heuristic (fighters
 * through stations plus mines). Satellites, containers and asteroids render
 * but are never auto-targeted. */
int VrHud_TargetGenus(uint8_t genus);

/* Nearest-cap selection by XWA-unit distance to origin. idx[] holds candidate
 * indices into objs[0..n); writes up to cap winners into out_idx, nearest
 * first. Returns the winner count. */
unsigned VrHud_SelectNearest(const XwaFlightObject *objs, unsigned n,
                             const int32_t origin[3], const unsigned *idx,
                             unsigned idx_count, unsigned cap, unsigned *out_idx);

/* Range in metres between two XWA world positions. */
float VrHud_RangeM(const int32_t a[3], const int32_t b[3], float metres_per_unit);

/* Bearing in degrees between a unit forward vector and from->to (0 = ahead,
 * 180 = astern). Returns -1 when degenerate. */
float VrHud_BearingDeg(const float fwd[3], const float from[3], const float to[3]);

/* Eye forward (-Z) rotated by a quaternion (need not be unit; zero
 * falls back to identity). Output is unit length. */
void VrHud_EyeForward(const float q[4], float fwd[3]);

/* Local position from a model->local matrix: translation [3,7,11]
 * (row-vector convention shared with ShipForward/SeatFrame). */
void VrHud_LocalPos(const float m[16], float p[3]);

/* Target-marker mode: 0 none, 1 sim-selected HUD target, 2 bridge render
 * target (VR presentation aid when the sim selected nothing). Pure switch. */
int VrHud_TargetMarker(int hud_usable, int sim_valid, uint32_t target_index,
                       uint32_t object_count, uint32_t player_index);

#endif
