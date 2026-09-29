/* CPU tests for mission-backdrop quad derivation (Phase C). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "vr_backdrop.h"

static int failures;
#define CHECK(cond, name) do { \
    if (!(cond)) { ++failures; printf("FAIL %s (line %d)\n", name, __LINE__); } \
} while (0)
static int near(float a, float b) { return fabsf(a - b) < 1e-3f; }

static int resolve_calls;
static int resolve_frames[8];
static int fake_h2 = 100;
static int fake_resolve(int model_type, int frame, VrBdRef *out, void *user) {
    (void)model_type; (void)user;
    if (resolve_calls < 8) resolve_frames[resolve_calls] = frame;
    resolve_calls++;
    if (frame == 99) return 0;
    memset(out, 0, sizeof *out);
    out->texture = (void *)0x1234;
    out->u0 = 0.25f; out->v0 = 0.5f; out->u1 = 0.75f; out->v1 = 1.0f;
    out->classic_w = 200;
    out->classic_h = (frame == 2) ? fake_h2 : 100;
    return 1;
}
static void resolve_reset(void) {
    resolve_calls = 0;
    memset(resolve_frames, 0, sizeof resolve_frames);
}

static float dist3(const float a[3], const float b[3]) {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

int main(void) {
    XwaBackdrop b;
    float corners[4][4][3];
    float uvs[4][4][2];
    VrBdRef refs[4];
    float eye[3] = { 0.f, 0.f, 0.f };
    unsigned n;

    /* Hidden record -> nothing. */
    memset(&b, 0, sizeof b);
    b.hidden = 1;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 4);
    CHECK(n == 0 && resolve_calls == 0, "hidden-skip");

    /* Strip: 2 segments, 2 segs/frame, fixed frame 5. */
    memset(&b, 0, sizeof b);
    b.model_type = 521;
    b.flags = 1;
    b.frame = 5;
    b.side = 0;
    b.strip_half_height = 100;
    b.strip_segment_count = 2;
    b.strip_segments_per_frame = 2;
    b.strip_coords[0][0] = 1000.f;
    b.strip_coords[1][0] = 2000.f;
    b.strip_coords[2][0] = 3000.f;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 4);
    CHECK(n == 2, "strip-count");
    CHECK(resolve_calls == 1 && resolve_frames[0] == 5, "strip-frame");
    /* Quad 0 slides U 0..0.5 inside the atlas sub-rect. */
    CHECK(near(uvs[0][0][0], 0.25f) && near(uvs[0][1][0], 0.5f), "strip-u0");
    CHECK(near(uvs[0][0][1], 0.5f) && near(uvs[0][2][1], 1.0f), "strip-v0");
    /* Corners sit at sky distance along the world directions. */
    CHECK(near(dist3(corners[0][0], eye) / 100000.f, 1.0f), "strip-dist");
    CHECK(near(corners[0][0][0] / corners[0][0][2], 10.0f), "strip-dir");
    CHECK(near(corners[0][2][0] / corners[0][2][2], -20.0f), "strip-dir-neg");

    /* Strip frame advance: 3 segments, 2/frame -> frames 1,1,2. */
    memset(&b, 0, sizeof b);
    b.side = 1;
    b.strip_half_height = 100;
    b.strip_segment_count = 3;
    b.strip_segments_per_frame = 2;
    b.strip_coords[0][0] = 1000.f;
    b.strip_coords[1][0] = 2000.f;
    b.strip_coords[2][0] = 3000.f;
    b.strip_coords[3][0] = 4000.f;
    fake_h2 = 100;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 4);
    CHECK(n == 3, "advance-count");
    CHECK(resolve_calls == 2 && resolve_frames[0] == 1 && resolve_frames[1] == 2, "advance-frames");

    /* Half-height rescale on classic_h change: 100 -> 50 halves offsets. */
    fake_h2 = 50;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 4);
    CHECK(n == 3, "rescale-count");
    CHECK(near(corners[2][0][0] / corners[2][0][2], 60.0f), "rescale-ratio");

    /* Axis quad (side 4): extents from angular_scale * classic dims. */
    memset(&b, 0, sizeof b);
    b.model_type = 522;
    b.side = 4;
    b.world_dir[2] = 1048576.f; /* >>9 -> 2048 */
    b.angular_scale = 256;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 4);
    CHECK(n == 1, "axis-count");
    CHECK(resolve_calls == 1 && resolve_frames[0] == 1, "axis-frame");
    /* ext_w = (256*300)>>8 = 300, ext_h = (256*150)>>8 = 150. */
    CHECK(near(corners[0][0][0] / corners[0][0][1], 0.5f), "axis-ext");
    CHECK(near(uvs[0][0][0], 0.75f) && near(uvs[0][1][0], 0.25f), "axis-u");
    CHECK(near(uvs[0][0][1], 1.0f) && near(uvs[0][2][1], 0.5f), "axis-v");

    /* Resolver failure -> no quads. */
    memset(&b, 0, sizeof b);
    b.flags = 1;
    b.frame = 99;
    b.side = 0;
    b.strip_half_height = 100;
    b.strip_segment_count = 2;
    b.strip_segments_per_frame = 2;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 4);
    CHECK(n == 0, "strip-resolve-fail");

    /* Degenerate geometry -> eye position, no NaN. */
    memset(&b, 0, sizeof b);
    b.side = 2;
    b.strip_segment_count = 1;
    b.strip_segments_per_frame = 1;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 4);
    CHECK(n == 1, "degen-count");
    CHECK(corners[0][0][0] == 0.f && corners[0][0][1] == 0.f && corners[0][0][2] == 0.f, "degen-eye");

    /* Output cap respected. */
    memset(&b, 0, sizeof b);
    b.side = 0;
    b.strip_half_height = 100;
    b.strip_segment_count = 2;
    b.strip_segments_per_frame = 2;
    b.strip_coords[0][0] = 1000.f;
    b.strip_coords[1][0] = 2000.f;
    b.strip_coords[2][0] = 3000.f;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 1);
    CHECK(n == 1, "cap-count");

    /* Invalid side without strip flag -> nothing; strip flag forces strip. */
    memset(&b, 0, sizeof b);
    b.side = 6;
    b.strip_half_height = 100;
    b.strip_segment_count = 2;
    b.strip_segments_per_frame = 2;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 4);
    CHECK(n == 0, "side6-skip");
    b.flags = 2;
    resolve_reset();
    n = VrBd_Derive(&b, fake_resolve, NULL, eye, 100000.f, corners, uvs, refs, 4);
    CHECK(n == 2, "stripflag-force");

    if (failures) { printf("VR_BACKDROP_TEST FAILURES=%d\n", failures); return 1; }
    printf("VR_BACKDROP_TEST PASS: derive, frames, axis, edge\n");
    return 0;
}
