# XWAQuest: VR frontend and Touch input design checkpoint

Date: 2026-09-26. Status: **DESIGN ONLY; implementation and hardware validation not performed.**

Only this report and its adjacent source-hash inventory are deliverables of this audit. No source changes, builds, installs or ADB operations. m8, m8test2, m8test3 and m8test4 remain frozen for this task. Muse owns the separate flicker/starfield/video-options investigation. Nothing below claims to fix flicker.

## Decision

Preserve the original composed frontend texture and display it on a finite, world-locked OpenXR quad. Feed a composite Quest Touch **SDL virtual joystick** into the existing Aeron/XWA controller mapping, WinMM compatibility and flight-input pipeline. Keep pointer/text/menu input as a separate context-sensitive adapter. XWA remains authoritative for simulation, weapons, targeting and throttle behavior.

These are proposed integration choices, supported by local source. They have not been implemented or tested on Quest. A dedicated independent experiment should contain them; promotion into official m8 requires subsequent approval.

## Evidence and baseline

Reviewed project status/development skill, M8test3 checkpoint/report and M8test4 checkpoint, source and local GameData action dictionary. Physical M8test3 evidence establishes cockpit, independent head tracking, TIE, combat effects and audio; it does not establish flight controls or a comfortable main frontend.

Muse's later M8test4 checkpoint records cockpit flicker disappearing after changes in **OpenXWA Video Options**, despite lower frame rate, while frontend flicker remained. Subsequent configuration writes prevent reliably reconstructing every setting in that successful session. This supersedes any simplistic claim that low FPS alone proves the cause. No changes to the cleanup workaround, synchronization, video settings or starfield are proposed here.

Read-only Git inspection: XWAQuest is on `codex/m1-sdl-gpu-openxr`, with **no commits**; milestone directories are untracked. Thus current directories are baselines, not independently committed Git branches. A cherry-pick workflow is not available yet. The companion SHA-256 inventory identifies the examined source versions without changing them.

## 1. Current frontend presentation

All paths in this report are absolute unless explicitly prefixed by a directory stated in the same table.

| Source | Relevant functions / role |
|---|---|
| `C:\OpenXWA\src\xwa_remaster\xwa_remaster.c` | `XwaRemaster_Frame`: dispatches frontend/loading/modal rendering through `XwaRemasterFrontend_Render`; can retain the last complete render during transitions |
| `C:\OpenXWA\XWAQuest\m8test3\frame_bridge.c` | `__wrap_SDL_WaitAndAcquireGPUSwapchainTexture`, `__wrap_Aeron_Present`: capture original final SDR composition and select flat or immersive output |
| `C:\OpenXWA\XWAQuest\m8test4\frame_bridge.c` | Same functional capture/presentation route, with timing instrumentation |
| `C:\OpenXWA\XWAQuest\m8test3\xr_bridge.c` and `C:\OpenXWA\XWAQuest\vr-probe\vr_openxr.c` | Bridge includes reusable XR implementation; `VrXr_EndFrame` submits projection views in LOCAL reference space |
| `C:\OpenXWA\src\xwa_runtime\runtime\presentation.c` | `XwaPresentation_AspectFit`, `XwaPresentation_ToClassic`: preserve original presentation and input coordinate mapping |

Current nonimmersive path:

`XwaRemaster_Frame → original frontend composition → __real_Aeron_Present → captured output.texture → VrBlit_Copy(same output, each eye) → VrXr_EndFrame(projection layer)`.

The capture is a sampled/color-target SDR texture, sized from the Android window. In nonimmersive mode the same image fills both eye swapchains. `VrXr_EndFrame` describes those full-eye images using the current located eye poses/FOV. There is **no finite screen plane, distance, UI anchor or independent screen transform** in this path. Therefore the image fills the visual field and follows viewing direction rather than remaining at a comfortable point in space. It is not accurate to assign an existing numerical distance such as 20 cm: the code does not define one.

Flight instead selects per-eye `VrFlightRenderer_RenderEye` output. That cockpit/world route must remain independent from the proposed menu placement.

### What is known about the comfortable pre-flight screen

The user's report of a comfortable, spatially stable pre-flight/loading screen is accepted as physical evidence. However, the examined code does **not** identify a separate quad or reusable pre-flight screen transform. Frontend/loading/modal composition shares the captured-output route until immersive rendering becomes ready.

The recorded M8test3 transition includes `configb.bmp` at 14:27:14.982, `dpmed.bmp` at 14:27:15.069, snapshot readiness at 14:27:18.378 and first immersive frame at 14:27:36.066. These events do not identify which exact visible screen the user meant. Retention/reprojection of a previous image during the long transition, or an image already inside the immersive route, are possibilities, **not confirmed causes**.

Consequently: reuse the proven original UI composition, not an unverified supposed pre-flight transform. A future annotated visual observation correlated with scene/mode/frame logs is needed to identify that screen conclusively. It is not necessary to alter loading or simulate the favorable behavior to design a proper virtual screen.

## 2. Proposed frontend screen

Use one `XrCompositionLayerQuad` with `XR_EYE_VISIBILITY_BOTH`, backed by a dedicated UI swapchain receiving the existing final SDR output. This gives a real spatial plane while retaining every original menu and its logic. The local OpenXR headers expose this layer type; integration on this application/runtime remains a future validation task.

Suggested configurable defaults:

- Distance: **2.0 m** from the user at UI entry.
- Width: **2.3 m**, approximately 60 degrees horizontal at that distance.
- Height: width divided by the displayed content aspect ratio. For 4:3, 1.725 m, approximately 46.7 degrees vertical.
- Preserve the actual game content rectangle and pixel aspect. Do not stretch a 4:3 interface to an eye texture's aspect. Account for letterboxing already produced by the compositor; do not crop meaningful UI or add a second unintentional set of bars.
- Center at captured head height, optionally a small configurable downward offset; keep the plane upright with yaw only.
- Capture the anchor once on entering the UI. In LOCAL coordinates: `screen_position = head_position + yaw_rotation * (0, 0, -distance)`; screen orientation follows that captured yaw with its visible side facing the user. Do not update the anchor from head pose every frame.
- Explicit recenter resets this UI anchor only. Handle runtime reference-space changes consistently. Neither recenter nor head movement changes the simulated ship orientation.

A world-locked default allows inspection by turning the head. An optional user-selected head-relative mode could exist later, but is not the default and must not reproduce a face-filling image. Automatic recenter on every submenu change would be uncomfortable; anchor across one UI session, reset on an explicit request or a deliberate new UI entry.

### Scope and layer policy

Use a small independent presentation-mode helper: frontend/loading → UI quad; active flight → existing stereo projection. In-flight Options must be explicitly classified as a modal screen, rather than inferred from whether the last flight mesh is still available. Choose and document whether the cockpit remains beneath that modal; preserve XWA's own pause semantics. Never add a second simulation tick to support UI.

The UI swapchain needs its own lifetime, acquired image and completion tracking; reuse the existing GPU blit facility where compatible. Keep one existing XR frame lifecycle and one `xrEndFrame`, selecting the appropriate layer list. This is a required implementation invariant, **not a diagnosis or proposed fix for current flicker**. Do not change the real window-swapchain cleanup acquire workaround as part of this feature.

### Pointer and original menu interaction

Add controller aim-pose actions for the future screen adapter. Intersect the controller ray with the anchored plane, require a forward intersection and valid content bounds, map plane UV through the displayed content rectangle to the original logical mouse coordinates, then retain `XwaPresentation_ToClassic`/the existing frontend mouse route.

Track button press and release, including release on lost focus/disconnected controller. The original frontend bridge latches mouse clicks on release. Hover must not require a held trigger. Preserve a stick-pointer fallback and the real text-input path. No menu recreation or direct calls into menu selection logic.

Menu-only long stick-click is a possible explicit recenter gesture; it must not also emit that stick-click's flight action. A menu entry can expose recenter later. Do not bind the Meta/system button: reserve it for runtime/system behavior and do not assume it is available to the application.

## 3. Real input architecture

| Source | Evidence / integration role |
|---|---|
| `C:\OpenXWA\XWAQuest\m8test3\input_xr.c` | `M8_InputInit`, `M8_InputPoll`, `M8_InputApplyPointer`; byte-identical to examined m8test4 counterpart |
| `C:\OpenXWA\aeron\include\aeron\input.h` | `AeronInputSnapshot`, `AeronControllerSnapshot`; keyboard edges/state/text, mouse, raw joystick axes/buttons/hats and gamepad fields |
| `C:\OpenXWA\aeron\src\gamepad.c` | SDL device discovery, gamepad opening with raw `SDL_OpenJoystick` fallback; `Aeron_UpdateControllers` |
| `C:\OpenXWA\src\xwa_runtime\config\modern_input_options.h` | Four logical axis bindings, 16 button sources and 20 action entries including POV |
| `C:\OpenXWA\src\xwa_runtime\input\controller_mapping.c` | `XwaControllerMapping_SelectedController`, `MapSnapshot`, `GetState`, `CopySelectedActions`; selection, inversion, deadzones, digital thresholds and focus handling |
| `C:\OpenXWA\src\xwa_runtime\input\winmm_joystick_provider.c` | `XwaWinmmJoystick_Source`, `XwaWinmmJoystick_RegisterSource`; existing mapped controller → WinMM compatibility |
| `C:\OpenXWA\src\xwa_runtime\runtime\port.c` | `XwaPort_TickBody`: selected-controller changes, joystick detection and action mapping into `g_gameConfig.joyButtons` |
| `C:\OpenXWA\src\xwa\flight\flight.c` | `FlightInput_GetNextKey`, `Joystick_PollRawAxesIfEnabled` call path, `FlightInput_ScaleAxesForFlight`, `Flight_UpdateEntity`; native axis, throttle, held-fire and command processing |
| `C:\OpenXWA\src\xwa\flight\object\laser.c` | `laser_fireplayerweapon`; existing selected weapon mode, laser/warhead, ammunition and cooldown behavior |
| `C:\OpenXWA\src\xwa_runtime\input\input_bridge.c` | `XwaInputBridge_UpdateFrontendMouse`, keyboard translation and release-click latch |
| `C:\OpenXWA\src\xwa_runtime\input\mouse_flight.c` | Existing optional mouse flight; not a new Quest flight engine |

Keyboard input is translated from Aeron key states/edges into original XWA key codes. Text has a separate SDL/Aeron text path. Mouse state includes logical/presentation coordinates, relative motion and buttons. Controllers expose signed 16-bit raw axes, button down/pressed/released bitfields and hats. The existing mapping turns axes into WinMM-style unsigned 0..65535, centered at 32768, then XWA processes them.

Exact logical axis identifiers:

`XWA_CONTROLLER_AXIS_YAW`, `XWA_CONTROLLER_AXIS_PITCH`, `XWA_CONTROLLER_AXIS_THROTTLE`, `XWA_CONTROLLER_AXIS_ROLL`.

The provider presents yaw/pitch/throttle/roll in axes 0/1/2/3. Native flight scales angular inputs, processes throttle buckets and interprets selected `joyButtons` actions. Held action 156 sets the existing fire modifier; flight subsequently reaches `laser_fireplayerweapon`. No direct ship velocity, orientation, throttle state or weapon calls are required from XR.

### Current Touch exposure: menu adapter, not flight controller

`M8_InputInit` uses `/interaction_profiles/oculus/touch_controller`:

| Physical input | Current source behavior |
|---|---|
| Right trigger | Float threshold 0.55 → left mouse press/release |
| A | SDL Return pulse |
| B | SDL Escape pulse |
| X | Pilot helper: backspaces then SDL text `m8test` |
| Y held 2 seconds | Request XR/session/application exit |
| Left stick | Arrow-key navigation/repeat |
| Right stick | Absolute mouse cursor movement |
| Grips, left trigger, stick clicks, aim poses | Not currently exposed by this adapter |

Source intention is not identical to prior physical proof: A was physically observed to activate/click, B/sticks had earlier limitations. Current flight controls have not been validated. The adapter applies zero relative mouse movement, and does not supply the native four flight axes. Head tracking already belongs to the view path and must stay independent.

## 4. Exact assignable actions

Dictionary read from `C:\Program Files (x86)\Steam\steamapps\common\Star Wars X-Wing Alliance\JOYSTICK.TXT`. `C:\OpenXWA\src\xwa\config\game_config.c::Config_LoadJoystickActionDictionary` loads `joystick.txt` through asset VFS, parsing code/name/description. These are exact local dictionary labels and codes, not newly invented actions. This audit does not verify the current on-device dictionary against this local copy.

| Code | Exact label | Key symbol where relevant |
|---:|---|---|
| 156 | Fire weapon | KEY_ALT_2 |
| 157 | Roll/Target ship in sights | KEY_ALT_3 |
| 155 | Pick target in sight | KEY_ALT_1 |
| 119 | Cycle weapon settings | KEY_W |
| 120 | Cycle firing settings | KEY_X |
| 116 | Next target | KEY_T |
| 121 | Previous target | KEY_Y |
| 114 | Target nearest fighter | KEY_R |
| 101 | Cycle through fighters targeting you | KEY_E |
| 105 | Target nearest incoming warhead | KEY_I |
| 111 | Target nearest objective craft | KEY_O |
| 117 | Target newest craft | KEY_U |
| 97 | Target attacker of target | KEY_A |
| 197 / 198 | Target next enemy craft / Target previous enemy craft | KEY_F3 / KEY_F4 |
| 44 / 60 | Cycle through target's components / Reverse cycle through target's components | KEY_COMMA / KEY_LESS_THAN |
| 99 | Fire countermeasure | KEY_C |
| 118 | Toggle S-Foil | KEY_V |
| 122 | Toggle laser convergence | KEY_Z |
| 13 | Match targeted craft's speed | KEY_ENTER |
| 27 | Options screen | KEY_ESCAPE |
| 32 | Confirm critical orders | KEY_SPACE |
| 9 | Bring up wingman command screen in MFD | KEY_TAB |
| 8 | Full throttle | KEY_BACKSPACE |
| 61 / 45 | Increase throttle / Decrease throttle | KEY_EQUAL / KEY_MINUS |
| 92 | Zero throttle | KEY_FOWARD_SLASH (source spelling) |
| 91 / 93 | 1/3 throttle / 2/3 throttle | KEY_LEFT_BRACKET / KEY_RIGHT_BRACKET |
| 143 | Pause game | KEY_ALT_P |
| 46 | Toggle cockpit on/off | KEY_PERIOD |
| 175 | Toggle mouse look mode | KEY_SCROLL_LOCK |

Important: **no separate assignable “primary fire” and “secondary fire” action was found in this dictionary/path.** `Fire weapon` uses the selected native weapon mode. For lasers: select laser mode and hold fire; for warheads: cycle to that mode and use the same fire action. A dedicated secondary-fire button would need further design if it must select a weapon automatically; do not implement it by writing `selectedWeaponMode` or calling the weapon code directly. Space is a critical-order confirmation, not a safe substitute for fire. Enter's flight meaning is not menu confirmation.

## 5. Proposed Touch → existing XWA mapping

Initial **stick-based** profile; configurable through the existing controller system. Button names below mean physical Touch buttons, not XWA key symbols.

| Physical input | Flight binding | Notes |
|---|---|---|
| Left stick X | XWA_CONTROLLER_AXIS_YAW | Deadzone and inversion through native mapping |
| Left stick Y | XWA_CONTROLLER_AXIS_PITCH | Confirm sign in controlled test |
| Right stick X | XWA_CONTROLLER_AXIS_ROLL | Enable native Roll option |
| Right stick Y | XWA_CONTROLLER_AXIS_THROTTLE | Adjust a persistent virtual throttle lever; spring-center holds its value |
| Right trigger | 156 Fire weapon | Held state, correct release; native selected laser/warhead |
| Right grip | 120 Cycle firing settings | Edge action, not continuous repeats |
| A | 116 Next target | Menu context remains confirm |
| B | 27 Options screen | Menu cancel/back through existing route |
| Right stick click | 114 Target nearest fighter | No head/ship recenter in flight |
| Left trigger | 99 Fire countermeasure | Edge action |
| Left grip | 101 Cycle through fighters targeting you | Edge action |
| X | 119 Cycle weapon settings | Replaces pilot-helper behavior only in flight |
| Y | 121 Previous target | Disable legacy hold-to-quit in flight |
| Left stick click | 155 Pick target in sight | Native ship-sight targeting, not head-gaze targeting |

Other real actions, particularly 105 incoming warhead, 111 objective, 13 match speed and 9 wingman commands, remain configurable alternatives. Do not add a complex hidden chord layer merely to expose every action in the first experiment.

A throttle lever adapter integrates stick displacement into a bounded normalized **input position**, not into craft speed or acceleration. The native axis/throttle pipeline still owns all game behavior. Define its initial value, pause/resume behavior and acquisition policy explicitly to prevent an unintended jump to half/full throttle. An alternative simpler first validation uses native Increase/Decrease throttle commands, but that is a distinct selectable input profile rather than two simultaneously active throttle sources.

The development skill records a prior *proposed*, unimplemented right-grip controller-tilt scheme. It is not existing hardware behavior. Preserve it as a possible later selectable profile: grip captures controller-neutral orientation; relative motion generates angular input; release neutralizes that input without resetting the ship. It conflicts with the default right-grip action above and must not be enabled simultaneously. Head pose must never be its input.

### Why an SDL virtual joystick

`C:\OpenXWA\third_party\SDL3\include\SDL3\SDL_joystick.h` provides `SDL_AttachVirtualJoystick`, `SDL_SetJoystickVirtualAxis/Button/Hat`. The actual Android configuration at `C:\OpenXWA\XWAQuest\m2\build-android\SDL3\include-config-relwithdebinfo\build_config\SDL_build_config.h` enables `SDL_JOYSTICK_VIRTUAL`.

Aeron already enumerates raw SDL joysticks. A single composite device named, for example, **Quest Touch**, can expose four logical axes plus physical buttons from both hands. It avoids requiring an SDL gamepad mapping database entry and fits XWA's selected-device model. Raw virtual axes use the raw signed range; do not confuse this with standardized gamepad trigger ranges.

Proposed chain:

`xrSyncActions → Touch sample → virtual SDL joystick state → AeronInputSnapshot → XwaControllerMapping → XwaWinmmJoystick_Source → native XWA joystick/actions → one XwaPort_Tick`.

Create the virtual device after SDL joystick initialization, before controller selection/mission detection. Update once before Aeron's input pump/snapshot on the existing host frame thread. SDL virtual setters take effect at joystick update/event pumping, so ordering must be tested; do not add a second simulation tick. Use stable device identity plus configured GUID/path/ordinal, not a transient instance ID alone.

Action focus loss/disconnection must release buttons and prevent stuck axes. Preserve the virtual throttle position separately from whether input is currently active; verify native focus/pause behavior before choosing any throttle reset policy. Runtime system/menu controls are not game bindings.

### Existing Controls/Options integration

`C:\OpenXWA\src\xwa_runtime\config\modern_controller_options_screen.c` already offers Active Device, Roll Enabled, Configure Axes, Configure Button Bindings and defaults. Its physical capture connects to `Config_RunJoystickActionPicker` in `game_config.c`. `C:\OpenXWA\src\xwa_app\host_config.c` persists joystick axis sources/inversion/deadzones, buttons, POV and numeric actions under `input.controller.joystick`.

Thus the virtual device should be selectable and bindable without replacing these screens. Initially it can show the device name but generic **Button N / Axis N** labels. Friendly Left Trigger/Right Grip/etc. labels require a later small labeling adapter; they are not present today and do not require replacing the action dictionary or modifying commercial assets.

Raw virtual inputs must remain available while the binding-capture screen is open. Neutralizing the entire joystick whenever a menu is visible would prevent rebinding. Instead, separate raw capture from menu navigation side effects and suppress duplicate pointer/key activation during capture. Outside binding capture, use explicit menu/flight/modal contexts so one trigger does not both click and fire, nor X both type a pilot name and change weapons.

## 6. Branch and integration strategy

Actual examined test3 → test4 source differences:

- `input_xr.c`, `vr_flight_bridge.c`, `vr_eye_math.h`: identical.
- `frame_bridge.c`: timing around memory polling, simulation, remaster, command acquisition, real WSI acquire, per-eye rendering/blit and submission; XR hook installation and diagnostic title. Same presentation decision and cleanup acquire.
- `xr_bridge.c`: wrappers timing wait/begin/end/locate/acquire/wait-image/release calls.
- `vr_flight_renderer.c`: timing wrappers for stars, target and cockpit recording; star callback telemetry. Not a new frontend placement.
- CMake/package/build/provision/test/diagnostic files differ to support the independent diagnostic application.

**Recommendation: C for development, B for eventual promotion.** Prepare independent modular UI and Touch changes against a hash-pinned copy of test3, then integrate onto a validated/promoted test4 state after Muse finishes. Do not copy over test4 files wholesale.

| Choice | Benefit | Cost |
|---|---|---|
| A: test3-derived alone | Stable physically demonstrated cockpit/head baseline | Carries known presentation/flicker limits; later integration still necessary |
| B: wait for promoted test4 | One agreed performance baseline | Blocks unrelated input/UI work while diagnosis continues |
| C: isolated modules, later selective integration | Can progress without touching Muse's sources; keeps experiments reversible | Glue overlaps in frame_bridge/xr_bridge require careful integration and revalidation |

Keep future modules conceptually separate: UI anchor/pointer mapping, XR UI presentation adapter, Touch raw-device adapter, context/default-binding policy. The critical overlaps with Muse are frame/presentation hooks and build glue; keep those patches small. Avoid changes to `vr_flight_renderer.c`, eye math, coordinate conversion, cockpit, starfield and existing memory workaround.

Because there are currently no commits, first preserve a source manifest and explicit patch/base pairing. After separate authorization for implementation, establish versioned history or a dedicated source copy; only then promise selective commits/cherry-picks. No branch or copy has been created by this audit. Future package identity must be distinct to protect test3/test4 private data and pilots.

## 7. Implementation phases after authorization

1. Pin independent baseline and its source hashes; define package and feature flags. Preserve existing app/private-data baselines.
2. Implement/test pure UI anchor and ray-to-original-content mapping, and pure Touch sample → virtual joystick conversion. No renderer or simulation changes.
3. Integrate raw virtual device, native selection/configuration and capture-aware context handling. Validate axes and real action states before expanding mappings.
4. Add UI swapchain/quad adapter to the existing XR lifecycle, preserving Flight projection and current cleanup workaround. Preserve original texture and menu logic.
5. Integrate only onto a specifically approved test4 state, retaining Muse's telemetry and fixes. Review overlapping patches rather than replacing entire bridges.
6. Build/install only with subsequent authorization, then one prepared physical test cycle with no approval-requiring commands while the user wears the headset.

## 8. Risks and validation plan

| Risk | Required validation |
|---|---|
| Wrong plane facing, ray sign, aspect or letterbox mapping | CPU tests: center/corners, yaw, forward/backface, outside bounds, asymmetric content rectangle |
| Recenter moves ship or screen follows head | Anchor remains constant between explicit recenter events; inspect simulation orientation unchanged by head-only movement |
| Texture stale or lifecycle ownership conflict | Trace mode, source readiness, acquired UI image and one end-frame per XR frame; do not change synchronization without evidence |
| UI placement mistaken for flicker fix | Report comfort and flicker independently; compare equivalent settings with Muse's approved baseline |
| Modal state incorrectly treated as immersive flight | Frontend → loading → flight → Options → flight transitions; no simulation tick duplication |
| Stuck fire/axes, trigger repeats or duplicate menu actions | Press/hold/release, focus loss, disconnect, context transition, binding capture tests |
| Virtual device not selected or identity changes | Aeron discovery, raw snapshot, selected GUID/profile, persistence across restart, native joystick detection |
| Axis signs/ranges or throttle jumps | Inspect raw and mapped values; center/deadzone/extremes; restart/pause/menu transitions; no forced ship state |
| Menu capture cannot configure Touch | Bind an axis and button through existing Options, persist, restart and verify actual native action |
| Weapon mapping misrepresented | Test laser and warhead selection through 119 then 156; retain ammo/cooldown/targeting logic; no invented secondary-fire action |
| Pre-flight explanation remains uncertain | User identifies exact screen at timestamp, correlate retained/flat/immersive mode; do not claim a reusable transform before evidence |

Physical acceptance: original full UI readable at comfortable distance, stable anchor during head turns, correct ray hover/click and text/menu behavior; flight pitch/yaw/roll/throttle/fire/target/weapon actions enter native input paths; head-only movement does not steer the ship. Existing cockpit/TIE/audio/damage remain functional. Comfort is confirmed by the user, not inferred from successful API calls.

Not validated in this task: runtime virtual-device behavior, actual Quad presentation, physical mappings, system-button availability, pre-flight screen identity, flicker root cause, starfield or performance fixes.

## Handoff

**Next concrete action, only after implementation authorization:** pin an independent test3-derived source baseline, then add pure input-conversion and UI-anchor helpers with CPU tests outside all frozen directories. Wire the virtual SDL device through the existing controller path before making any changes to simulation or flight rendering. Do not modify m8test4 during Muse's investigation.

No implementation was started. No frozen branch, GameData or pilot was changed.
