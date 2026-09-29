/* Pure physical-stick state + geometry (Phase: stick V1).
 * No XR/SDL/GPU calls. CPU-testable on host. Conventions follow
 * VrEye_Compose: model->local columns are right/up/back with OPT's
 * -Y forward (forward = negated column 1). */
#ifndef M8INTEGRATED1_VR_STICK_LOGIC_H
#define M8INTEGRATED1_VR_STICK_LOGIC_H

/* Placement guesses (seat frame); the physical test calibrates them. */
#define VR_STICK_BASE_RIGHT_M 0.02f
#define VR_STICK_BASE_FWD_M 0.38f
#define VR_STICK_BASE_UP_M -0.12f
#define VR_STICK_HANDLE_LEN_M 0.28f
#define VR_STICK_GRAB_R_M 0.09f
/* Test knob (Run-3): widened 0.14 -> 0.18 for grab discovery. */
#define VR_STICK_HOVER_R_M 0.18f
#define VR_STICK_RANGE_M 0.15f
#define VR_STICK_GRAB_SQUEEZE 0.6f
#define VR_STICK_RELEASE_SQUEEZE 0.4f
#define VR_STICK_DEADZONE 0.12f
#define VR_STICK_SMOOTH 0.4f
#define VR_STICK_TILT 0.5f

typedef struct VrStickState {
    int grabbed;
    int hover;
    float dx, dy; /* smoothed deflection: +x right, +y forward push */
    float grip_pos[3]; /* last input grip pos, cockpit-local metres */
    int grip_valid; /* pose_valid && active && focused at last update */
} VrStickState;

typedef struct VrStickInput {
    float grip_pos[3]; /* controller pos, cockpit-local metres */
    int pose_valid;
    float squeeze; /* 0..1 */
    int focused;
    int active; /* immersive flight with valid cockpit */
} VrStickInput;

/* Seat frame from a model->local matrix (columns, OPT -Y forward).
 * Vectors are normalized (matrices carry a metre scale). */
void VrStick_SeatFrame(const float mat[16], float right[3], float fwd[3],
                       float up[3], float pos[3]);

void VrStick_Init(VrStickState *st);

/* Returns 1 while grabbed. out_pitch (+ = forward push, feeds left-Y)
 * and out_roll (+ = right push, feeds right-X) are smoothed and valid
 * while grabbed. changed=1 on grab/release edges. */
int VrStick_Update(VrStickState *st, const VrStickInput *in,
                   const float seat_pos[3], const float seat_right[3],
                   const float seat_fwd[3], const float seat_up[3],
                   float *out_pitch, float *out_roll, int *changed);

/* Stick base + rest handle-top from the seat frame. */
void VrStick_RestPose(const float seat_pos[3], const float seat_right[3],
                      const float seat_fwd[3], const float seat_up[3],
                      float base[3], float top[3]);

/* Handle-top for a deflection (tilts from the base). */
void VrStick_HandleTop(const float base[3], const float right[3],
                       const float fwd[3], const float up[3],
                       float dx, float dy, float top[3]);

/* Four crossed quads (base x2, handle x2) in world coords for submit. */
void VrStick_Quads(const float base[3], const float top[3],
                   const float right[3], const float fwd[3],
                   const float up[3], float corners[4][4][3]);

/* Hand marker: 3 crossed 5cm squares at the grip point (seat-frame planes). */
void VrStick_HandQuads(const float grip[3], const float right[3],
                       const float fwd[3], const float up[3],
                       float corners[3][4][3]);

/* Connector: 2 crossed quads spanning a->b (1.2cm wide). Degenerate-safe
 * (zero-area quads at a when a==b). */
void VrStick_LineQuads(const float a[3], const float b[3],
                       float corners[2][4][3]);

/* Grip bar: 2 crossed 10x2.5cm quads centered on the handle top. */
void VrStick_GripQuads(const float top[3], const float right[3],
                       const float fwd[3], const float up[3],
                       float corners[2][4][3]);

#endif
