# VR combat completeness and physical cockpit controls — research handoff

Date: 2026-09-27. Integrator/implementation owner: Muse. Target inspected: `C:\OpenXWA\XWAQuest\m8integrated1`.

Research only. No product source changes, builds, installation, ADB, device access or GameData changes were performed for this report. `m8input1` is a frozen architectural reference; its Touch architecture has already been physically validated after integration. This document does not authorize implementation.

Evidence labels: **SOURCE** = confirmed from inspected source; **LOG** = existing saved runtime evidence; **USER** = latest physical observations supplied for this task; **INFERENCE** = explanation consistent with evidence, not proven for that run; **DESIGN** = proposed future behavior. Source line numbers describe this inspection and can move while Muse works. The companion `RESEARCH-VR-COMBAT-SOURCE-HASHES.csv` pins inspected inputs at handoff time, not the installed APK.

## Source reference index

References below use these exact paths and name the relevant function/structure beside each claim.

| Key | Exact path |
|---|---|
| FB | `C:\OpenXWA\XWAQuest\m8integrated1\frame_bridge.c` |
| VB | `C:\OpenXWA\XWAQuest\m8integrated1\vr_flight_bridge.c` |
| VH | `C:\OpenXWA\XWAQuest\m8integrated1\vr_flight_bridge.h` |
| VR | `C:\OpenXWA\XWAQuest\m8integrated1\vr_flight_renderer.c` |
| EYE | `C:\OpenXWA\XWAQuest\m8integrated1\vr_eye_math.h` |
| SEAT | `C:\OpenXWA\XWAQuest\m8integrated1\t3_seat_math.h` |
| XRINPUT | `C:\OpenXWA\XWAQuest\m8integrated1\input_xr.c` |
| TOUCH | `C:\OpenXWA\XWAQuest\m8integrated1\touch_joystick.c` |
| TS | `C:\OpenXWA\XWAQuest\m8integrated1\touch_state.h` |
| FL | `C:\OpenXWA\src\xwa_remaster\flight.c` |
| REM | `C:\OpenXWA\src\xwa_remaster\xwa_remaster.c` |
| SHIP | `C:\OpenXWA\src\xwa_remaster\ship.c` |
| SNAP | `C:\OpenXWA\src\xwa_runtime\snapshot\snapshot.c` |
| SH | `C:\OpenXWA\src\xwa_runtime\snapshot\snapshot.h` |
| EXPORT | `C:\OpenXWA\src\xwa_runtime\snapshot\snapshot_export.c` |
| SHUD | `C:\OpenXWA\src\xwa_runtime\snapshot\snapshot_hud.c` |
| HUD | `C:\OpenXWA\src\xwa\flight\hud\hud.c` |
| RHUD | `C:\OpenXWA\src\xwa_remaster\hud.c` |
| FIXED | `C:\OpenXWA\src\xwa_remaster\hud_fixed.c` |
| GAME | `C:\OpenXWA\src\xwa\flight\flight.c` |
| LASER | `C:\OpenXWA\src\xwa\flight\object\laser.c` |
| OBJECT | `C:\OpenXWA\src\xwa\flight\object\object.c` |
| COLLISION | `C:\OpenXWA\src\xwa\flight\object\collision.c` |
| LIGHT | `C:\OpenXWA\src\xwa\flight\flight_light.c` |
| COLOR | `C:\OpenXWA\src\xwa\frontend\frontend_color.c` |
| MAP | `C:\OpenXWA\src\xwa_runtime\input\controller_mapping.c` |
| WINMM | `C:\OpenXWA\src\xwa_runtime\input\winmm_joystick_provider.c` |
| OPT | `C:\OpenXWA\aeron\tools\opt2gltf\opt.c` and `opt.h` |
| CONVERT | `C:\OpenXWA\XWAQuest\vr-probe\vr_convert.c` |

Additional specific source files are written out where needed. Initial context read: `C:\OpenXWA\XWAQuest\m8integrated1\CHECKPOINT-M8INTEGRATED1.md`; prior input design: `C:\OpenXWA\XWAQuest\research\DESIGN-VR-FRONTEND-TOUCH-HANDOFF.md`. The prior design's flat/full-eye frontend description is historical; integrated1 now has the finite quad.

## 1. Executive technical summary

**USER:** the current integrated build has comfortable frontend distance/navigation, working immersive stereo cockpit and head tracking, ship control, firing input and audio. No cockpit flicker was observed in this latest test. Projectiles and substantial combat information are absent in immersive mode but become visible on the original Flight virtual screen. A color change and subsequent return to 2D occurred during combat. Frontend text flicker remains secondary.

**SOURCE:** this is an incomplete presentation path, not evidence that a second weapon simulation is required. Normal Flight renders all eligible objects, real projectile OPTs, effects, mission backdrops, lights and a substantial native HUD. VB retains **only the player and one TIE**; VR draws that TIE, the cockpit and a limited procedural-star pass. Native gameplay continues underneath.

**SOURCE:** continued immersive eligibility incorrectly depends on the retained TIE's existence/type/region and runtime OPT readiness. The bridge's `target_index` is a prototype render selection, **not** `PlayerData.currentTargetObjectIdx`. Changing the gameplay target alone does not necessarily invalidate it. TIE removal can invalidate it even with healthy Flight/player state.

**INFERENCE:** that dependency is the best-supported architectural explanation for combat returning to 2D, but the exact latest-run trigger is not demonstrated by the available application logs. Do not claim the TIE was killed or a particular resource disappeared.

**DESIGN:** separate Flight-mode eligibility, object availability and gameplay targeting; carry a coherent presentation snapshot with real objects/effects/HUD/environment; reuse the normal renderer's data-derived submission laws with explicit per-eye contexts. Keep the existing single XWA tick and native input/actions authoritative. Physical-stick interaction should be another source of virtual joystick axes, not a ship transform writer.

## 2. Normal Flight rendering pipeline

**SOURCE:** `XwaPort_Tick` updates the real runtime; Flight capture/commit publishes `XwaSnapshot_Current()`. FB `__wrap_XwaPort_Tick` calls the real tick once and captures the bridge afterward. `__wrap_XwaRemaster_Frame` calls the real compositor. REM `XwaRemaster_Frame` selects `XwaRemasterFlight_Render` for Flight.

FL `XwaRemasterFlight_Render` (~3699 onward) performs:

1. Snapshot/view preparation and `XwaRemasterHud_PrepareFrame` (~3715).
2. Camera/history and asset-derived preparation; billboards via `fl_derive_billboards` (~3848), mission backdrops via `fl_derive_backdrops` (~3851).
3. Sky: configured procedural `XwaRemasterSkyStars_Prepare/Draw` or sky cube (~3948). Backdrop sky quads (~3992).
4. Whole eligible `flight_objects` walk (~4058), transforms, mesh resolution/articulation, lighting/shadows/material selection. Projectile submission has its own orientation/culling/order rules.
5. Bolts after ships and before cockpit (~4222); local pulse lights (~4235); cockpit with `XwaRemasterShip_BuildCockpitMeshTable` (~4256–4281).
6. Additional state-derived effects/trails and lens presentation; scene rendering and scene/bloom present chain.
7. `fl_draw_present` (~3262): target boxes BEFORE_FIXED, fixed HUD, target boxes AFTER_FIXED, CMD and text.

The real original output is still composed when the custom VR renderer is active. The quad fallback reveals that existing output. It does not enable weapon physics that were previously absent.

## 3. Current immersive VR rendering pipeline

**SOURCE:** FB `__wrap_Aeron_BeginFrame` starts one XR frame, polls Touch before Aeron's event pump; `__wrap_XwaPort_Tick` (~148) does the single real tick then `VrFlightBridge_CaptureAfterTick(logical_frames)`. The real remaster frame and real `Aeron_Present` still run. `__wrap_Aeron_Present` (~201) calls `VrFlightRenderer_BeginFrame` and chooses immersive eyes or finite UI quad.

VB `CaptureAfterTick` (~62): copy player by `flight_camera.player_obj_idx`; use player position as translation origin; resolve player and one TIE runtime OPT; compute cockpit seat-0 transform; set CS1 readiness. VH contains no HUD, mission backdrop array, light pulses, complete craft articulation, projectile history, trail or effect packet.

VR `BeginFrame` (~57) resolves one target mesh and optional cockpit mesh. `RenderEye` (~121) composes vehicle and eye pose, creates metric eye camera, draws stars when enabled/non-Death-Star, draws one unlit TIE and optional unlit cockpit, then SDR present. Both instances use `CULL_BACK`; cockpit applies `cockpit_variant`. No native HUD calls, mission backdrop calls or other object walk are present.

Successful eye outputs feed the existing blit and XR frame submission. A false `BeginFrame` selects UI; a failure within eye rendering follows the error path, not an intentional object-missing UI policy. These are different failure classes.

## 4. Side-by-side missing-feature matrix

All cells here are **SOURCE** unless explicitly marked DESIGN. S = simulation/game presentation state; R = renderer-derived geometry/layout. “Snapshot” means `XwaSnapshot`, not a GPU resource. “Absent” in VR means the bridge/VR consumer does not expose the required complete state, even if some generic object fields exist.

| # / feature | Normal source/functions and state | Snapshot / nature | VrFlightSnapshot → VR now | Safe reuse/exposure (DESIGN) |
|---|---|---|---|---|
| 1 Player/cockpit | FL cockpit block; SHIP `BuildCockpitMeshTable`; `cockpit`, player craft | Yes; S pose/variant, R mesh table | Seat0 OPT/transform/variant drawn; no full mesh table or native instruments | Preserve working seat/head composition; add native component visibility/articulation and instrument semantics |
| 2 Other ships | FL object walk, `fl_prepare_object_mesh`, SHIP `BuildMeshTable` | Yes; `flight_objects`, S | Only retained TIE; no other craft | Eligible region objects, stable slot+signature, asset handles, component state |
| 3 Player bolts | LASER `laser_createprojectile`; FL `fl_object_is_projectile`, `fl_object_pose` | Yes; player-projectile genus, S; R ribbon-facing | Filtered out | Real live projectile objects, per-eye facing, native emissive/material/culling |
| 4 AI fire | LASER `laser_weaponsfire` → same creation; NPC-projectile genus | Yes; S | Filtered out | Same primitive adapter as player fire, not another weapon implementation |
| 5 Explosions/debris | OBJECT/COLLISION creation; FL `fl_derive_billboards`, `fl_derive_object_billboard`, SHIP `BuildDebrisMeshTable` | Yes; transient/main objects, frame/type/source/spin; S+R | Absent | Native DAT frame/OPT debris resolution, ordering, type-specific fields |
| 6 Stars | FL sky block; `sky_stars.c` Prepare/Draw | Camera/game time available; parameters R/config | Existing hardcoded-parameter procedural pass; visibility not established by source | Reuse normal mode/settings/time/axis conventions, do not add unrelated fake stars |
| 7 Space background | FL sky cube/procedural alternative + backdrops | Camera and backdrops; sky renderer/config | No cube/mission background path | Shared normal environment selection with per-eye orientation |
| 8 Planets/backdrops | FL `fl_derive_backdrops`; `XwaBackdrop` | Yes; mission S, quad R | Absent | Preserve DAT type/frame, direction/angular size and native visibility flags |
| 9 Mission environment | SNAP CaptureFlight region/backdrops; FL allobject region walk | Yes; S | Only TIE + death-star boolean | Mission region, static objects, environment records; no hardcoded test planet |
| 10 Target reticle/lock | SHUD notes/direct state; HUD `Hud_DetermineLockStrengthAndPlaySound`; FIXED reticle | Yes; `hud.reticle`, lock/threat S | Absent | Native lock/ready art and flags; ship-collimated HUD |
| 11 Aiming reticle | SH `XwaHudReticle`; FIXED reticle projection | Yes; hardpoints/aim offsets/look/ready, S+R | Absent | Preserve weapon aim, project for each eye; no head-driven aim changes |
| 12 Selected-target indicator | SHUD target capture; target boxes/radar selected flags | Yes; actual target slot+signature | Prototype target is NOT this state | Separate gameplay target reference; world-space bracket |
| 13 Offscreen direction | FIXED `ProjectTargetArrow`/targetarrow block (~760–823) | Yes; target/camera, R projection | Absent | Ship-HUD boundary arrow based on actual target direction |
| 14 Target info | HUD `Hud_UpdateTargetInfoCache`, CMD text; SHUD target | Yes; name/status/distance/hull/shield/system | Absent | Cockpit display/CMD anchored surface using native text/data |
| 15 Ship info | SHUD instruments/panes; HUD `Hud_UpdateMfdPages` | Yes; model/type, panes/glyphs | Player identity only | Native MFD/pane presentation, no reconstructed gameplay stats |
| 16 Shields | HUD shield labels; FIXED `fixed_build_shield` | Yes; instruments shield values/last-hit side | Absent | Native shield instrument and transient feedback |
| 17 Hull/damage | SHUD instruments; FIXED shield/hull; SHIP component visibility | Yes; hull/subsystem/component/damage flash | No health HUD/component visibility; cockpit texture variant retained | Expose existing health/flash/articulation separately |
| 18 Speed/throttle | SHUD instruments; HUD text/power | Yes; simulated speed/throttle/engine output | Absent | Read actual sim indicators, never display merely controller lever as actual speed |
| 19 Weapons | HUD charge/weapon text, FIXED charge/reticle | Yes; selection/charges/ammo/ready/lock | Absent | Native selected-mode status; Fire weapon remains native |
| 20 Radar/sensors | HUD `Hud_DrawRadarBlips`/AddBlip; SHUD radar notes; FIXED radar | Yes; radar blips/targeted/colors/panes | Absent | Fore/aft cockpit instruments, preserve native contacts/semantics |
| 21 Purple info | COLOR indexed palette; HUD init; RHUD/FIXED frame/reticle/radar/pane widgets | Yes; `hud_colors`, visibility and HUD payload | Absent | Reuse palette/art/widgets; exact observed widget remains UNKNOWN |
| 22 Damage/color effects | LIGHT pulses; SHUD damage/SHIELD state; OPT node switches; FL pulse/glow effects | Yes; light_pulses, component/node_switch, HUD | Cockpit variant only; no pulses/HUD flash; unlit mesh | Native bounded cockpit light/material/HUD feedback; do not assume fullscreen tint |
| 23 Other combat overlays/effects | RHUD text/CMD/MFD, threats; FL glows/trails/lens/hyperspace | Rich snapshot effects/HUD; S+R | Absent except cockpit/TIE basic texture | Stage threat/countermeasure/mission messages first; preserve native gates and effect IDs |

## 5. Projectile/laser full source trace

**SOURCE — input:** XRINPUT `M8_InputPoll` samples Touch; TS converts buttons/axes; TOUCH `TouchJoystick_Update` sets SDL virtual state. Native mapping through MAP/WINMM reaches XWA's existing joystick/button interpretation. Default button0 = action **156 “Fire weapon”**, `KEY_ALT_2`. GAME `Flight_UpdateEntity` (~11034 firing block) tests real modifier/game gates and calls LASER `laser_fireplayerweapon` (~2407). No XR firing call is required.

**SOURCE — creation:** selected weapon mode, jam, cooldown, working systems, link mode and seat decide real firing. `laser_firelasersystem` (~2104; call near2224) invokes `laser_createprojectile` (~1639). Rockets/warheads use `laser_firerocketsystem` (~2332) and `laser_firemissile` (~2007). Sound alone would not prove a live projectile, but the source establishes the route and USER observed actual projectiles in normal Flight.

`laser_createprojectile` uses player projectile slots for a player (12-slot player allocation plus shared overflow rules; warhead offset) or `Object_AllocSlotForGenus(GENUS_NpcProjectile)` for AI. It sets real object type, source object/type, region, IFF, frames alive, orientation, damage, speed, previous/current world positions. Weapon hardpoint coordinates are rotated by the firer through `pai_calcrotatedpoint`, then translated into world position; convergence/seat rules remain native. Lifetime (~1773) is native ticks: `236 * seconds + Q16 fractional contribution`.

Projectile types are **real simulation FlightObjects and OPT meshes**, not Touch-created lines. `C:\OpenXWA\src\xwa\assets\object_type.h` (~287) defines Rebel laser280, Rebel turbo281, Imperial laser282, Imperial turbo283; other laser/warhead types follow. `model_type.c` and `XwaSnapshotExport_ModelName` provide model identity; loaded assets resolve the original OPT. Do not assume every projectile is a simple red bolt or give all types one mesh.

**SOURCE — AI:** LASER `laser_weaponsfire` (~584; laser call near818) invokes the same laser system with player index -1. Enemy firing must be consumed from the same captured objects. Native AI, fire cadence, aim and damage remain untouched.

**SOURCE — movement/removal:** OBJECT `Object_UpdateLifetimeAndMovement` (~1377) updates time/movement and handles lifetime expiration; projectile genus cases (~1572) distinguish warhead explosion/proximity effects from ordinary termination. COLLISION applies native hit/damage/object conversion (`collide_ConvertObjectToExplosion`, proximity damage and lifetime changes). Render visibility ends when the authoritative object disappears/changes identity; the renderer must not run a second lifetime or collision timer.

**SOURCE — snapshot:** SNAP `XwaSnapshot_CaptureFlight` (~1069) walks live object records, copies position/previous position, orientation, genus, source type, render region and type-specific fields into `XwaFlightObject` (SH ~305). Projectile genera are not excluded here. VB excludes them because it selects `has_craft && OBJ_TIEFighter` after the player.

**SOURCE — normal rendering:** FL `fl_object_is_projectile` (~1725) recognizes both projectile genera; `fl_object_pose` (~1736) rotates the ribbon about its longitudinal axis toward the view. The ordinary `ObjectModelMatrixAtOrigin` does **not** perform this roll alignment. `XwaRemasterFlight_ObjectModelMatrixForCameraDelta` (~1761) already exposes a useful camera-delta/roll-align transform helper. `fl_prepare_object_mesh` (~2967) resolves model identity and articulation. Normal projectile instances use `OptProjectileEmissiveStrength`, no shadows, `CULL_NONE` (~4190), correct material behavior and bolt ordering before canopy glass (~4222). Merely appending objects with the current VR TIE's `CULL_BACK`, unlit1 material and generic matrix would not reproduce normal bolts reliably.

**DESIGN:** copy/pin the real object records per committed tick; select all eligible visible-region objects; resolve assets once; use separate render-class handling (mesh craft, projectile, debris, billboard), preserve slot+signature/source/genus/variant/history, recompute ribbon facing per eye from that eye's camera-minus-object. If a mesh is temporarily unavailable, skip/log that object while Flight stays immersive. No trigger-generated geometry, fake projectile velocity, TTL or hit detection.

## 6. Targeting/HUD source trace and VR classification

**SOURCE:** SHUD `hud_capture_top_level` (~298), target capture (~318), instruments (~344), direct state (~411), final capture (~485) combine real player state and classic HUD notes. `XwaSnapshotHud_BeginClassicFrame/EndClassicFrame` (~43/53) delimit captured presentation notes; do not mix stale notes from another tick/mission.

Actual gameplay target = `g_players[g_localPlayer].currentTargetObjectIdx`, validated and paired with the object's signature. SH `XwaHudTarget` holds valid/slot/signature/component, distance, name/status and hull/shield/system percentages. `XwaHudReticle` contains weapon mode, hardpoint kinds/positions, `aim_offset`, lock/ready/in-range and look/seat state. `XwaHudTargetBox` holds unprojected extents; radar records include actual targeted flags. This is distinct from VB's retained TIE.

| Element | Source and semantics | Proposed VR home |
|---|---|---|
| Central weapon/lock reticle | HUD reticle readiness/in-range notes; SH reticle; FIXED reticle/threat builder (~907) | Ship-oriented collimated HUD; direction shared in world, projected separately for eyes |
| Weapon aim/convergence | Captured hardpoints and aim offsets, FIXED projection (~854), real laser convergence | Collimated aim cue; never infer aim from head or controller pose |
| Target bracket | `C:\OpenXWA\src\xwa\flight\targeting.c` `Targeting_DrawObjectBox` (~287), `Targeting_DrawSceneObjectBoxes` (~430); native box/color/layer state | World-space at actual target extent, with native eligibility/selection |
| Offscreen arrow | FIXED target-arrow projection matches actual target slot/signature, reflects behind and clamps to viewport | Edge of ship-HUD region, directional cue; not a permanently head-locked icon |
| Target identity/distance/status | HUD `Hud_UpdateTargetInfoCache` (~8982), `Hud_UpdateCMDText`/CMD text (~13211), SH target | Cockpit display, readable and fixed to ship |
| Radar/fore-aft contact | HUD `Hud_DrawRadarBlips` (~9416), `Hud_AddBlipToRadar` area (~9531), SH radar notes | Cockpit fore/aft sensor displays, native contact filtering |
| Threat/warhead/beam feedback | HUD `Hud_UpdateThreatIndicators` (~10073); SH threats; FIXED builder | Ship-HUD/instrument cue, native alert state and sounds |
| Mission/team/critical text, MFD | HUD HUDText (~12020), MFD pages (~13684); RHUD pane/glyph/text stages | Cockpit display or finite ship-relative message panel |

**Evidence limit:** no distinct, generally applicable predictive lead-indicator contract was established by this trace. `aim_offset`/convergence/lock cues exist; they must not be renamed “lead indicator” or replaced with a new ballistic solver. Use captured native aiming semantics. Specific on-screen element identification requires a captured frame.

Avoid copying mono screen-pixel target boxes into both eyes. Reproject world semantic anchors per eye. Conversely, a cockpit instrument is a finite physical surface and should have ordinary stereo parallax. Head-locking should be reserved for an explicit accessibility/system use, not native combat aim.

## 7. Purple/magenta UI identification

**SOURCE:** COLOR `g_indexedColors` (~100) contains `0xff7020c0` — purple RGB(112,32,192). HUD initialization (~2664) sets `g_hudColors[0] = FrontendColor_GetIndexed(g_gameConfig.hudColor[profile])`; SHUD captures HUD colors. This is configurable HUD tint, not one uniquely named purple combat subsystem.

RHUD widget/asset tables (~17–140) and FIXED builders associate this family of presentation with fore/aft radar frames/scopes/blips, CMD/MFD frames, energy/weapon charge, shield/hull, beam, reticle/threats and panes/text. Asset group `OBJ_HudTextureGroup12000`: radar frames27/28/49/50, scopes4/45/46, CMD11, MFD1/2, power12–14, charge23–26, shield39–43, beam29–37/44, reticle5–10/47/48, threats15–22. Visibility comes from native HUD mode, visible-elements masks and per-widget gates, not just nonzero data.

**SOURCE:** the target-direction arrow examined in FIXED uses yellow values (`0xffC8C800` / `0xffE6E600`), so “purple arrow” is not supported by that code.

**UNKNOWN:** the exact widget(s) in the user's observed purple information cannot be uniquely identified from a color description. No verified screenshot of that exact moment was available. Candidate family and palette are source-confirmed; assigning it specifically to shields, radar or target text would be guessing. Muse should retain native art/colors and obtain one screenshot with location if exact correspondence is needed. No commercial asset was modified or packaged by this research.

## 8. Space, stars, planets and mission-selected backdrop trace

**SOURCE:** SNAP `CaptureFlight` backdrop block (~1469) copies the selected render region's `WorldRectRecord` data (saved mission region handling in hangar) into `XwaBackdrop` (SH ~593). Fields include DAT/model type, frame, flags, side, hidden, world direction, angular scale, tint/intensity and strip geometry. These are mission data, not a generic sky chosen by the headset.

FL `fl_derive_backdrops` (~2053) resolves `model_type` + frame through `fl_resolve_type_frame`. Hidden records are skipped. Flags/side0–3 produce coordinate strips with original segment/frame advancement. Side4/5 produce axis quads with native forward-Z gating and angular extent. The sky pass (~3992) renders distant background geometry without normal foreground depth writes. These records are essentially angular distant visuals; don't place a planet 10 metres from the player to manufacture stereo depth. Give both eyes the same celestial direction and correct angular extent, with eye-dependent rotation; true finite environment objects remain real scene objects.

Normal stars: FL (~3948) uses configured sky mode. Procedural mode uses `C:\OpenXWA\src\xwa_remaster\sky_stars.c` `XwaRemasterSkyStars_Prepare/Draw`; alternative uses sky cube. World-to-cube basis maps XWA's XY-horizontal/+Z-up convention consistently. Normal settings include exposure/brightness/density/grid/radius; Death Star sky behavior is distinct.

**SOURCE:** current VR uses the same procedural-star implementation but hardcoded parameters (exposure1, brightness1, density.4, grid32), and no mission backdrop/cube path. Therefore “star draw called” cannot demonstrate complete space/background, and the missing mission planet is directly explained by omitted backdrop consumption. Latest USER observation confirms normal 2D has more environment information. Whether the existing star pass itself is visually black in a particular build remains a separate visibility/settings investigation; do not conflate absent planets with failed star generation.

**DESIGN:** share native environment preparation and assets, preserve selected region, deterministic time, flags, tint and angular laws; prepare per-eye orientation/depth against foreground. Include real static models/structures via object inventory. Don't substitute a generic star texture or hardcoded planet.

## 9. Damage/color-effect source trace

**SOURCE:** normal XWA has several distinct color/alert mechanisms:

- LIGHT `FlightLight_InitLocalPlayerPulses` (~332) defines local pulses; slot0 is red. HUD `Hud_UpdateThreatIndicators` (~10245–10290) enables beam pulse2, incoming-missile pulse1, and red pulse0 when total shields <100 and the hull-damage bucket reaches2. This is a critical-health alert, not proof that every red frame represents a direct shield impact.
- SNAP light-pulse capture (~1400) and FL `fl_derive_pulse_lights` (~2829, submitted near4235) feed normal scene lighting.
- SHUD instruments carry damage/last-shield-side/health; FIXED `fixed_build_shield` (~469) renders corresponding instrument feedback.
- Craft component state and `MobileObject.nodeSwitchIndex` (SNAP ~1162) affect native geometry/material variants. VB copies player's node switch to `cockpit_variant`; VR passes that to the cockpit instance. OPT NodeSwitch selects authored texture states. This does not establish that this cockpit switches textures on a hit: `C:\OpenXWA\src\xwa\flight\mission\mission.c` (~6116) initializes `nodeSwitchIndex` from flight-group markings, and this trace did not establish a player-damage writer changing that index.
- Native collision/effect objects, glows and explosion/lens flashes supply other visible feedback through normal FL paths.

**SOURCE:** current VR sets `no_local_lights=1`, emissive1 and constant white final present tint, and omits native HUD/pulse-light consumption. Therefore attributing the observed immersive color change specifically to `FlightLight` or a fullscreen damage postprocess is unsupported. Cockpit texture variants are one color-changing route structurally retained by this bridge, but no damage-driven change or run-correlated variant/material transition is established. A rendering/resource problem also cannot be excluded. **INFERENCE, not confirmed cause.**

**DESIGN:** reuse actual native damage/alert state and original cockpit material variants; add localized/bounded cockpit light and HUD feedback deliberately. Do not add arbitrary headset-wide red flashes keyed to trigger or controller haptics. Check material variants before/after hits and correlate native health/pulse state with the presentation timestamp in a future authorized test.

## 10. Immersive-to-2D fallback root-cause analysis

**SOURCE — exact gate:** VB `CaptureAfterTick` (~90–132) requires loaded mission, active Flight, ACTIVE bridge state and valid camera. Finds player by camera slot. It retains previous TIE slot+signature when still eligible, otherwise chooses the lowest-slot eligible TIE. Eligibility: nonplayer, `has_craft`, `OBJ_TIEFighter`, same `render_region`, not OTHER slot. CS1_READY additionally requires exactly two objects, player `OBJ_XWing`, both LOADED_OPT and case-insensitive basenames XWING/TIEFIGHTER.

VR `BeginFrame` (~57–88) further requires VALID|CS1_READY, valid indices and target runtime OPT mesh with VBO/IBO/indexcount. `begin_failure` returns0 and resets the immersive renderer. FB `__wrap_Aeron_Present` sets `ui_mode = !immersive_frame`, showing the normal output on the quad. **This causal mechanism is confirmed from source.**

| Event | Effect supported by source |
|---|---|
| Player changes selected target / has no selected target | No direct effect on retained-TIE selection; bridge does not read gameplay target |
| Player targets non-TIE | Not itself a failure while retained eligible TIE still exists |
| TIE destroyed / removed / becomes noncraft | May remove only eligible TIE; objectcount1, CS1_READY absent, fallback |
| TIE slot reused | Signature protects identity; replacement eligible TIE can be selected; otherwise fallback |
| TIE leaves render region | Excluded; another eligible TIE can replace it or fallback |
| Another enemy becomes gameplay target | Not necessarily rendered; only eligible TIE selection changes independently |
| Target OPT resolution becomes unavailable or mesh invalid | Bridge flag or renderer mesh gate fails, fallback |
| Cockpit mesh unavailable alone | Logged as unavailable; does not itself gate immersive |
| Flight ends / player or camera invalid | State/validity gate legitimately changes mode; requires explicit death/transition policy |

**LOG:** `C:\OpenXWA\XWAQuest\m8integrated1\evidence\quest-integrated1-20260927-001050-flight.log` contains historical IMMERSIVE_ENTER (00:13:01.789), FIRST_STEREO_FRAME tick3694 (00:13:09.143), IMMERSIVE_EXIT (00:13:20.516) and later cycles. These prove mode transitions in an earlier saved run, not the latest combat trigger.

**LOG limitation:** `...\evidence\quest-integrated1-combat-fallback.log` has no `M8_VR_` or `M8I_` application markers found. It includes multiple process lifetimes and buffers, so cannot identify the precise immersive-exit condition. It also contains a separate SIGSEGV on 09-27 00:52:21 for PID31748, with `DInput_DrainKeyboardEvents+68` → `FlightDisplay_FreeSurfaces` → `XwaFlightTask_Shutdown` → `XwaPort_Shutdown` in lib build ID `8cac0a3655ab131fae8b9f1a8a6ce0bc5e753cc1`. That is shutdown-path evidence, **not proof it caused the earlier visual fallback**. No crash fix is proposed here.

Current source has `M8_VR_IMMERSIVE_EXIT_REASON` with flags/count/player/target/tick; its presence in source does not prove the installed test binary emitted it. Muse should correlate installed build identity and reason telemetry in a later authorized run.

**DESIGN:** Flight/camera/player lifecycle determines immersive state. An empty enemy list is valid Flight. Object asset readiness controls an individual draw, not presentation mode. Preserve an explicit transition/death/spectator policy; do not silently keep stale player transforms indefinitely. Graceful per-object absence must remain diagnostic, not hide failed mandatory renderer initialization.

## 11. Recommended architecture for complete immersive combat

**DESIGN — preserve Option C/hybrid:**

`native input → one XWA tick → committed XwaSnapshot → immutable VR presentation packet → prepare shared assets once → left-eye view + right-eye view → existing XR presentation`

Separate three concepts: `flight_active`, `camera/player_valid`, and each object's `drawable`. Separate `gameplay_target` from any diagnostic retained object. Packet identity includes mission epoch, tick, game time, origin, asset generation/revision and slot+signature. Carry actual environment, HUD semantics, effect state and craft variant/articulation needed by the selected passes. No simulation pointers in an asynchronously retained packet.

Use normal renderer's classification/transform/material laws through reusable submission helpers, not a second set of approximated gameplay effects. FL currently has static singleton `s` and mutable histories/tables; calling `XwaRemasterFlight_Render` twice with temporary camera substitutions is not a safe per-eye API. Extract or adapt render-only preparation into explicit per-frame/per-eye contexts with clear ownership. Share asset resolution; keep eye-dependent ribbon orientation, billboard sorting, HUD projection and depth per eye. Never tick, poll an action edge, update damage or allocate gameplay objects twice.

VB currently borrows/reads `XwaSnapshot_Current` after real tick; VR mesh pointers are borrowed only BeginFrame→EndFrame with no intervening simulation/asset sync. Retain this same-thread phase boundary or explicitly copy/pin data for another thread. Slot rotation in SNAP `Commit` (~351) means raw snapshot pointers cannot be kept arbitrarily across ticks. Preserve asset generations and invalidate caches on mission/load changes, not on enemy loss.

**Concrete expansion hazard:** VB uses `VrFlightObject old[2]` and copies previous records. Its current two-object policy makes that safe; increasing objectcount without changing this previous-state storage is unsafe. Change storage/diff strategy together with the allobject policy. Keep bounds/drop counters explicit; capacity is not a reason to retain stale objects.

Combat completeness can be staged without changing WSI cleanup, head composition, culling globally, FOV, physics or input binding semantics. Do not merge a generic global `CULL_NONE` change: only projectile/effect classes that use it natively need it.

## 12. Physical right-hand flight-stick architecture

**SOURCE:** integrated XRINPUT currently exposes boolean/float/vector actions (buttons, triggers, squeeze, sticks). There is **no pose action or controller action-space location** in this file. “Right grip” currently means squeeze value, not grip pose. Controller pose acquisition is a required future addition; head pose is not a substitute.

**DESIGN:** add a right-hand grip-pose action/space to the existing action set before attachment; bind controller grip pose, locate against the same reference space/time used for flight rendering, and carry validity/focus flags. Derive an interaction pose in cockpit coordinates. A small pure physical-stick helper produces normalized virtual axes and ownership state. TOUCH remains the sole virtual joystick publisher; MAP/WINMM/native XWA remain authoritative.

Use orientation-relative control for V1, with positional proximity only for grab eligibility. A strict positional lever is simple mathematically but penalizes wrist rotation, controller geometry and seated reach; a purely absolute orientation jumps when grabbed. Capture a neutral relative grip orientation at engagement, then extract bounded pitch/lateral tilt. Animate around a fixed calibrated base; do not move the base with the hand. Position can provide a generous distance safety envelope, not a second competing axis command.

Proposed initial tunables (not native facts): grab radius 8–12cm around grip; pitch/lateral travel ±20 degrees; input deadband 2 degrees; normalized saturation at travel limit; hysteresis for squeeze (.55 engage/.45 release consistent with TS). Tune after physical measurement, not by changing world scale. Start without an extra heavy filter; if needed use short time-based smoothing (e.g. ~30ms) and bypass it for safety-neutralization. XWA's configurable deadzone remains in the pipeline; don't stack a large second deadzone unknowingly.

While grabbed, physical helper owns yaw/pitch only; existing right thumbstick roll and throttle remain available. No addition of physical + thumbstick pitch/yaw: choose one source. Release returns to neutral then thumbstick after a neutral handover, preventing a held fallback stick from causing a sudden jump. Tracking/focus loss releases buttons and angular axes and requires a new grab/neutral re-arm. Do not automatically yank native throttle to zero; maintain explicit existing lever policy and decide that separately from angular-axis safety.

## 13. Existing cockpit geometry findings

**SOURCE/ASSET INSPECTION:** read-only metadata inspection of local Steam asset:

`C:\Program Files (x86)\Steam\steamapps\common\Star Wars X-Wing Alliance\FLIGHTMODELS\XWINGCOCKPIT.OPT`

525,414 bytes, OPT version5, SHA-256 `DCB854992B576C534A46CDF8778001A2D0995F6300DAFBC486C499F429A822EA`.

Inspection tool/report: `C:\OpenXWA\XWAQuest\research\Inspect-XWingCockpit-Metadata.ps1` and `XWINGCOCKPIT-METADATA.json`. Tool reads only the asset and outputs metadata; it does not compile/export a replacement model. Header length/pointer-bias layout follows OPT parser, particularly `parse_mesh_vertices`224, `parse_mesh_descriptor`266 and root traversal~780.

Six mesh roots, vertex counts **64,75,78,77,70,44** (408 total). All six descriptors use mesh type23 `OPT_MT_ROTARY_COMM_SYS`; node names inspected are empty. All have rotation/scale nodes. Root0 contains six hardpoints of type20; these are not identified as grip/flight-stick anchors by the parser's named weapon-hardpoint enum. Generic mesh type23 does not mean every mesh is a usable cockpit communications control.

| Root | Descriptor bbox minimum → maximum, raw OPT units | Rotation pivot, raw OPT units |
|---|---|---|
| 0 | (-21.64051,-45.62535,5.952657) → (23.19365,44.67767,46.66545) | (29.08105,65.61002,2.500079) |
| 1 | (-203.1862,-113.044,-75.30954) → (-16.69928,188.8688,20.47297) | (-21.93462,81.08198,3.336399) |
| 2 | (-208.3278,-113.044,6.186048) → (-17.9788,244.264,67.70235) | (-21.0983,81.08198,8.354334) |
| 3 | (19.48445,-113.044,6.138882) → (209.8335,244.264,67.6552) | (25.73576,83.59095,9.608821) |
| 4 | (16.87592,-113.044,-76.05691) → (203.3629,188.8688,3.026424) | (24.06312,79.40934,4.590886) |
| 5 | (-9.563681,48.61998,36.64474) → (11.23633,67.5489,50.33281) | (.836322,58.54259,43.48877) |

**UNKNOWN:** this metadata is not a textured visual identification. No specific mesh can defensibly be declared the flight stick, throttle, buttons or lever from these names/types/bounds alone. In particular the small root5 must not be called “the joystick” just because it is small/central. The local Steam hash has not been compared with Quest-private GameData in this read-only/no-ADB task.

**SOURCE:** SHIP `XwaRemasterShip_BuildCockpitMeshTable` (~797) delegates seat0 to `BuildMeshTable`, retaining original per-component visibility/rotation laws. Aeron per-mesh tables provide a potential independent transform/visibility mechanism, but they are not yet applied in the current VR cockpit instance.

**DESIGN:** first visually inspect the exact loaded OPT and label mesh/face subset with a calibrated grip/base in raw OPT coordinates and asset hash. If a whole root is the stick, compose an isolated presentation transform with the existing table rather than replacing damage tables. If baked into a panel, never hide/rotate the whole panel: use a non-destructive runtime-selected submesh copy and matching visibility mask, or an explicitly temporary separate interactive proxy until exact face selection is proven. An overlapping copy without hiding selected faces risks z-fighting. Do not edit the original OPT. This asset identification is a real prerequisite for an authentic visible-stick claim; it does not block designing the input math.

## 14. Controller pose → cockpit-space math

**SOURCE:** OPT uses model +Z up, -Y forward and X lateral; unit conversion `AERON_OPT_METERS_PER_UNIT = 1600/65536 = 0.0244140625m`. XWA world coordinates are int32; yaw/pitch/roll uint16 Q16 turns and cached rows Q15. FL object transform helpers already account for original orientation conventions. VB scales the first12 entries of its row-major affine model matrix, including translation, to metres. Do not apply this factor twice.

EYE `VrEye_Compose`: matrices are row-major with column-vector application. The OPT model basis includes reflection; proper vehicle rotation is reconstructed from model columns0,2,1, normalized/orthogonalized with a cross product and validated against the remaining column. Quaternion order XYZW, Hamilton product `q_vehicle * q_eye`; position `t_vehicle + B * p_eye`. Thus current XR LOCAL room coordinates are attached to the simulated vehicle by this composition; XR LOCAL is not already the rebased XWA world.

SEAT `T3_SeatMatrix`: cockpit starts from model matrix, then subtracts `(hardpoint_world + camera_pan/16) * metres_per_unit` from translation. Cockpit follows ship, independent of eye. CONVERT `VrConvert_Camera` changes to camera WXYZ/conjugate and a 180-degree-X camera convention. That is a **view adapter**; do not apply its conjugation/180-degree correction to a physical controller model pose.

**DESIGN — define spaces explicitly:** L = same XR LOCAL reference used by eyes; W = current rebased rendering world in metres; C = raw cockpit model coordinates; I = proper, orthonormal metric interaction frame calibrated in cockpit; H = grip/controller.

Let `T_WL = [B,t_vehicle]` using exactly EYE's proper vehicle basis. Locate grip as `T_LH`. Then:

`T_WH = T_WL * T_LH`

For point proximity in raw OPT coordinates:

`p_C = inverse(T_WC) * p_W`, where `T_WC` is the actual cockpit affine matrix including seat offset and OPT scale/reflection.

For orientation/control use a proper calibrated metric interaction transform `T_WI`, not a quaternion extracted from the reflected/scaled `T_WC`:

`T_IH = inverse(T_WI) * T_WH`

`q_delta = inverse(q_IH_at_grab) * q_IH_now` (chosen grab-local axes; transform to calibrated stick axes before extracting tilt).

Both cockpit and controller inherit the same vehicle transform, so common ship movement cancels in the relative interaction. Head rotation is absent from the chain. Do not multiply by inverse eye/head pose: that would make looking around steer or move the grip.

Store calibrated base/grip points with coordinate-space/unit tags and asset identity. Construct I with proper orthogonal axes from the calibration and shared vehicle basis; reflected model geometry must not silently reverse yaw. Convert bounded local forward/back tilt to pitch and lateral tilt to yaw; retain fallback roll. A suggested explicit convention for I is +X right,+Y up,-Z forward, but its signs must be checked against the existing native axis inversion settings before use. Test ±90-degree ship rotations, nonzero head pose, both eyes, negative/positive hand tilts, rebase changes and LOCAL recenter. Predicted-time pose sample and the frame's vehicle snapshot must not come from different frames.

## 15. Native XWA control mapping

**SOURCE — actual integrated defaults (not all earlier proposed mappings):** TS `TouchConvert`; TOUCH `defaults` (~91) assigns joystick axes0/1/2/3, enables roll, seeds user bindings once. WINMM `XwaWinmmJoystick_Source` (~13–27) maps them to native YAW/PITCH/THROTTLE/ROLL. MAP applies user-selected device/axis/button bindings; native XWA processes them normally.

| Physical input | Current virtual/native default |
|---|---|
| Left stick X | axis0 yaw |
| Left stick Y | axis1 pitch from `-ly` |
| Right stick X | axis3 roll |
| Right stick Y | changes persistent throttle lever at .5 units/second outside deadzone; axis2=`2*lever-1` |
| Right trigger | button0 →156 **Fire weapon** |
| A | button1 →116 **Next target** |
| B | button2 →27 **Escape** |
| X | button3 →119 **Cycle weapon settings** |
| Y | button4 →121 **Previous target** |
| Left trigger, right grip, left grip, right/left stick clicks | Published virtual buttons5–9, default action0 (unassigned) |
| Meta/system button | No binding/interception |

Exact labels/IDs are in local original `C:\Program Files (x86)\Steam\steamapps\common\Star Wars X-Wing Alliance\JOYSTICK.TXT`: Next target line18, Cycle weapon settings21, Previous target23, Fire weapon61. Action27 is the existing Escape key action. There is no independent invented Primary Fire/Secondary Fire action in this selected pipeline: native **Fire weapon** uses native selected weapon settings, ammo, link and cooldown.

Native roll is a separate enabled axis; lateral physical-stick motion should initially feed **yaw**, matching current left-stick X. Do not apply aircraft bank-and-turn physics or derive roll automatically from yaw. Keep right-stick X for roll; optional wrist twist is a later explicit mapping choice. Physical stick owns yaw/pitch only while grabbed, outputting into the same SDL raw axes before native user mapping/deadzone/inversion. Do not bypass custom configuration or reset bindings every start.

TS already neutral-rearms after focus/context changes, uses trigger/grip hysteresis and has a raw binding-capture mode. Preserve binding-screen capture behavior and suppress physical grabs there. Right grip is currently unassigned by default but users may have rebound it; grab ownership needs an explicit conflict policy, not silently stealing a configured gameplay button.

**DESIGN — future left throttle, out of V1:** calibrated bounded left-hand lever position sets the existing normalized input lever and virtual axis2 (low=-1/high=+1 under current defaults), then native bucket/smoothing logic controls actual throttle. Do not set engine speed or `g_throttleSmoothed` directly. Preserve current right-stick throttle as fallback and define ownership/handover explicitly.

## 16. Proposed V1 physical-stick state machine

**DESIGN:** small helper, no broad interaction framework.

| State | Entry/behavior | Exit |
|---|---|---|
| DISABLED | Not Flight, binding capture, no valid calibration, focus lost; angular physical contribution0 | Valid focused Flight + neutral controls → AVAILABLE |
| AVAILABLE | Existing thumbstick owns axes; track valid hand/proximity, show restrained hover cue if desired | Hand in grip region → HOVER |
| HOVER | No steering effect; grip rising edge required | Grip engagement → capture neutral pose and GRABBED; leave volume → AVAILABLE |
| GRABBED | Physical owns yaw/pitch; fixed base, bounded relative tilt, same virtual joystick; trigger/buttons remain native | Grip release, invalid pose, focus/scene/reference reset, gross reach escape → RELEASING |
| RELEASING | Immediately neutral angular contribution on safety loss; visual spring may animate separately; release ownership after fallback stick neutral | Fresh neutral/release → AVAILABLE; invalid context → DISABLED |

Grip held while entering Flight must not auto-grab. Tracking regain must not restore an old deflected command. Position/orientation validity and action activity must be checked independently; a valid head pose doesn't imply a valid controller pose. Axis source arbitration occurs once before SDL publication; the two eye renders must not each update the interaction helper.

Recenter/calibration is explicit, while released; changes invalidate grab reference. Save per-user reach offsets separately from game assets/pilots and retain an accessible thumbstick fallback. Original ship/controller bindings continue to function without calibration. Future cockpit buttons may emit existing configurable virtual buttons, but no separate direct weapon/target/shield calls.

## 17. Risks and regression points

1. **Active source vs installed binary:** Muse may edit during this research; source inventory is not an APK audit. Match build ID and telemetry before runtime causal claims.
2. **Two-object assumptions:** `old[2]`, target indices and mesh singleton all require coordinated refactoring before increasing counts.
3. **Render-region topology:** dynamic effects can have stale object region; preserve `render_region`/slot_class semantics from native snapshot, not a naive `region` filter.
4. **Asset lifetime:** no cross-thread/raw snapshot or GPU mesh pointer retention across sync/reset; pin/copy with epoch and generation or stay on same phase/thread.
5. **Per-eye state:** shared static normal-renderer/HUD state, history, sorting and tables cannot be overwritten mid-eye submission. No double simulation/side effects.
6. **Units/reflection:** OPT scale/reflection is not a quaternion; physical grip requires proper interaction axes. Use existing tested vehicle/head math, not a new global coordinate convention.
7. **Projectile rendering:** camera-facing roll, transparency, emissive strength, canopy order and per-class culling matter; allobjects alone is insufficient.
8. **HUD semantics:** actual gameplay target, visibility, lock, radar and weapon cues come from game; don't derive gameplay from head aim.
9. **Cockpit identification:** local model metadata lacks control labels, and exact Quest asset equality is unverified; visual mesh/face identification needed before hide/rotate decisions.
10. **Input ownership:** grip rebind conflicts, mode edges, tracking loss, neutral handover, native deadzone and retained throttle must be tested; never stuck fire or angular axes.
11. **Missing latest causal log:** color effect and precise fallback trigger remain unproven. Existing shutdown SIGSEGV is a separate issue to hand to Muse, not silently equated with fallback.
12. **Memory/performance:** allobjects/effects may add substantial resource work; budget shared asset preparation and bounded buffers. Do not disable simulation or change memory/WSI workaround to hide presentation cost.
13. **Visual acceptance:** draw recording/submission does not establish visibility, comfort or correct depth. Latest USER observations override older flicker reports.

## 18. Recommended implementation order and validation for Muse

**DESIGN — priorities, not changes performed:**

1. Separate immersive Flight lifecycle from retained-TIE readiness. Preserve reason diagnostics. Validate no-target, last-enemy destruction, target cycling to non-TIE, region/mission transitions, player death and modal/pause behavior. Missing enemy must not select 2D by itself.
2. Generalize packet/renderer object inventory safely, including previous-state storage and asset lifetime. Add real player/AI projectile classes using normal transform/material laws. Validate slot/signature parity with one captured tick, both eyes, weapon selection/link/cooldown, expiry/hit and canopy order; never trigger-spawn a render-only bolt.
3. Add mission backdrops/space and native effect classes, preserving original data. Validate the same mission's normal-vs-VR planet direction/angular size, explosion frame and environment identity.
4. Add semantic combat HUD: native aim/lock, actual target/brackets/direction, weapon status and threat; then cockpit radar/CMD/MFD/shield/hull/speed/messages. Validate target changes independently of render inventory, behind/offscreen behavior, component targeting, ammo/health parity, head turns without aiming changes.
5. Diagnose color changes with frame/tick/variant/light/health evidence; add native localized effects deliberately. Keep frontend shimmer investigation separate.
6. Visually identify/calibrate real cockpit control geometry; then add right grip pose and pure helper tests. Use thumbstick flight as the immediate fallback throughout. Test coordinate cancellation, signs, saturation, release, focus/tracking/reference loss, rebind conflicts and no duplicate trigger edge before authorizing a physical test.
7. One supervised seated stick test: reach/grab, small forward/back/lateral motion, release/neutral, head turns while held, thumbstick fallback; compare native input diagnostics and actual XWA behavior. Stop for discomfort/axis mismatch rather than compensating by changing ship physics.

Keep changes modular and reviewable. Don't label complete immersive combat PASS until user confirms real projectiles, readable authentic combat information, mission environment and stable lifecycle under combat. Physical stick is independently gated by exact geometry/calibration evidence.

## 19. Exact files/functions likely to change in future implementation

**DESIGN only; none were changed here.**

| File(s) | Function/area | Purpose |
|---|---|---|
| VB/VH | `CaptureAfterTick`, `copy_object`, snapshot structures, old-state storage | General object/effect/HUD/environment contract; separate actual target and readiness; lifetime/generation |
| VR | `BeginFrame`, `RenderEye`, reset/history | Flight-only eligibility, per-class allobject rendering, per-eye native-law submission |
| FB | `__wrap_Aeron_BeginFrame`, `__wrap_XwaPort_Tick`, `__wrap_Aeron_Present` | Keep one tick; explicit lifecycle/modals; integrate pose sampling without disturbing WSI/XR frame ownership |
| FL / SHIP, associated headers under `C:\OpenXWA\src\xwa_remaster\` | Reusable pose/material/effect preparation; `ObjectModelMatrixForCameraDelta`, `fl_prepare_object_mesh`, backdrop/billboard helpers; mesh tables | Narrow render-only reuse if explicit-context extraction is needed; no gameplay changes |
| RHUD / FIXED and HUD companion render modules | PrepareFrame/semantic projection/render helpers | Per-eye/ship-HUD/cockpit presentation without duplicating target logic |
| XRINPUT / `C:\OpenXWA\XWAQuest\m8integrated1\input_xr.h` | `M8_InputInit`, Poll, shutdown | Grip pose action/space/location/validity/time; cleanup alongside current action set |
| TOUCH / TS | Update/Convert/source arbitration | Physical axes into the same SDL joystick; preserve configured bindings/rearm |
| New isolated physical-stick helper/test files, names to be chosen by Muse | Pure calibrated pose→axes/state machine | Test math/ownership without XR or gameplay side effects |
| VR cockpit instance / calibrated metadata under future branch | Mesh-table composition, selected faces/root only | Optional authentic visible stick animation after geometry identification |

Likely most raw gameplay state already exists in SH/SNAP. Extend snapshots only for a concrete missing field; do not add parallel reads of live simulation from rendering threads. Native gameplay files in the next section are explanatory sources, not modification targets.

## 20. Explicitly do not modify

- Frozen `m8`, `m8test2`, `m8test3`, `m8test4`, `m8input1`; legacy milestones, OPT fixes, pilots/GameData or `hello_xr`.
- XWA flight dynamics, AI, weapon spawn/damage/targeting/mission logic to make visuals appear. In particular GAME/LASER/OBJECT/COLLISION are authoritative.
- Native binding semantics or saved user bindings via repeated default seeding.
- Head/vehicle conversion, scale, FOV, clipping or global culling as an unmeasured fix for missing presentation classes.
- WSI cleanup/SDL real swapchain acquisition and XR synchronization as part of this combat/input research.
- Original commercial cockpit assets to introduce interaction; do not hide whole unknown meshes.
- No generic fake laser/starfield/planet or frontend remake to claim authenticity.
- No builds, installations, provisioning or device tests from this research handoff.

### Handoff status

Research document complete. Confirmed presentation omissions and architectural readiness coupling are actionable. Exact latest fallback trigger, specific purple widget, cause of immersive color change, exact physical-stick mesh/pivot and local-vs-Quest asset equality remain explicitly unresolved evidence items. They do not justify guesses or product changes by this research agent.
