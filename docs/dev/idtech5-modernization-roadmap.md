# idTech 5-Level Modernization Roadmap

This document turns the project's modernization goal into an implementation
order that preserves the shipped Quake 4 asset and gameplay contracts. It is
based on the official Doom 3 BFG Edition source snapshot (`1caba197`) pinned in
the [source-provenance inventory](source-provenance.md), the current openQ4
[capability matrix](engine-capability-matrix.md), and the stock-asset acceptance
harness.

Doom 3 BFG is not the full idTech 5 or idTech 6 source tree. It is a late
idTech 4 branch containing several idTech 5-era architectural ideas: a parallel
job manager, jobbed renderer front end, GPU skinning, explicit GPU buffers,
binary/generated resources, preload manifests, GPU timing, automatic resolution
scaling, a refined front-end/back-end render command stream, and a newer
session/network stack. Those subsystems are useful references, but importing the
whole engine would replace Quake 4 contracts rather than modernize them.

## Non-negotiable compatibility boundary

Modernization is acceptable only when all of these remain true:

- Retail Quake 4 PK4s remain the source of truth. Players are not required to
  convert, unpack, or patch them.
- Classic material, decl, GUI, map, MD5/MD5R, BSE, sound, save, demo, and
  protocol behavior remains available as the fallback and comparison path.
- Generated data is a disposable, versioned cache under `fs_savepath`, keyed to
  its source identity (including the containing PK4 checksum where relevant).
  Missing or invalid cache data falls back to the retail source. A cache never
  becomes downloadable content or part of the pure-package authority set.
- New material features use namespaced, opt-in syntax. A stock material is not
  silently reinterpreted as PBR content.
- Wire, save, and demo format changes are explicitly versioned. Protocol 2.41
  compatibility is not changed by an internal refactor.
- Dedicated builds do not acquire renderer, presentation, or client-only job
  dependencies.
- Every promoted default passes the four-role stock baseline and a human review
  of engine-written screenshots. MP validation explicitly uses
  `+set ui_autoJoin 1`, `+set si_pure 1`, and
  `+set net_serverAllowServerMod 0` unless the test is specifically for the join
  menu or non-pure behavior.

## What openQ4 already uses from the BFG lineage

The most directly reusable BFG work is not hypothetical. openQ4 already carries
audited BFG-lineage code in these areas:

- binary image loading/writing, image options, image programs, color-space
  conversion, and DXT encode/decode;
- renderer image management and intrinsic-image support;
- the newer sound world/emitter/voice/sample architecture, with the OpenAL
  implementation arriving through the documented RBDOOM lineage;
- small idlib utilities such as static strings, swapping, and sorting.

The authoritative inventory currently records 37 BFG-headered files. Any
additional incorporation must retain the upstream notices, update that inventory
and the applicable Additional Terms coverage, and record any intermediate
lineage. This roadmap is technical guidance, not a legal conclusion.

"Readily available" below means that the named implementation is present in the
audited official snapshot and can be studied or adapted without reverse
engineering. It does not mean drop-in: several paths assume Win32, PS3/SPU,
trusted pre-generated data, 32-bit offsets, or assert-only validation. New code
must use openQ4's portable interfaces and fail-closed input rules.

## Current implementation state (2026-08-22)

This table is the dated delivery snapshot for this roadmap. **Implemented**
means the compatibility-safe foundation is present and covered by the cited
project evidence; **Experimental** means substantial code exists but is not a
supported/default capability; **Partial** means only part of the required
contract exists; and **Planned** means the roadmap is still design guidance.
The [engine capability matrix](engine-capability-matrix.md) remains authoritative
when a status differs or a narrower qualification is needed.

| Workstream | State | Current boundary and next work |
|---|---|---|
| Stock-compatibility and security foundation | **Implemented** | Protocol 2.41 preservation, pure-MP game-module containment, bounded malformed-input handling with immediate session teardown, challenge entropy, rcon2, private-CVar redaction/remote authority, HTTP(S)-only transfer policy, source-provenance auditing, archived MP auto-join test policy, and the four-role retail-PK4 evidence harness are present. Release promotion still requires a clean source pair, final-package capture, and retained human review. |
| Audited BFG-lineage image, sound, and idlib work | **Implemented** | The existing 37-file BFG inventory is tracked with source lineage and Additional Terms. Further imports must update the same manifest and notices. |
| PBR material authoring/resource foundation | **Implemented foundation; visible capability missing** | Namespaced parsing, typed color/data image usage, classic ARB2 fallbacks, scene-packet metadata, resource-table diagnostics, and fail-closed exclusion from unsupported modern-visible paths cover Phases 0-3 of the PBR plan. PBR shaders, direct lighting, visible ownership, IBL, and specular probes do not exist yet. |
| GPU measurement and dynamic resolution | **Partial** | OpenGL and Vulkan publish the same delayed, non-blocking whole-frame microsecond result through renderer ABI v9, including frame/generation identity and availability/drop/reset counters. Map loads, context/device changes, swapchain recreation, capture discontinuities, and shutdown invalidate the timing generation. High-resolution CPU and unique GPU samples feed the versioned benchmark marker, and the gameplay/stock tools enforce and replay exact map/backend/profile CPU/GPU budgets under a fixed bordered-window 1280x720 promotion contract. Current-build storage, repeated-map, and complete required-profile captures have exercised both timing backends; all eight OpenGL and all eight Vulkan cases pass and replay-verify. The initial target rows still need release-candidate/platform qualification before they support universal performance claims. The automatic controller remains Milestone E. |
| General job system | **Implemented foundation** | The engine-owned [portable bounded job service](parallel-job-system.md) provides sleepable workers and waits, bounded list/job/dependency admission, low/normal/high priority aging, dependency ordering, cooperative cancellation, deterministic inline execution, metrics, and dedicated-safe lifecycle ownership. Threaded and synchronous native coverage passes. Its first production consumer is the learned level-load read/PK4-inflate and framing/integrity pipeline; live asset parsing, renderer/audio upload, and renderer-front-end work remain with their established owners. Current-build stock validation also produced identical jobs-on/off storage1 screenshots and game state, completed jobs-on/off OpenGL plus jobs-on Vulkan repeated-map campaigns with clean shutdown markers, and recorded five deterministic synchronous dedicated-server exits. Clean final-package recapture remains a separate release-promotion gate. |
| Generated caches, streaming, and learned preload manifests | **Implemented and locally validated; release promotion pending** | Successful loads produce exact map/mode/entity-filter/search/PK4/settings manifests. Matching loads use bounded cancellable read/PK4 inflation followed by worker-safe typed framing and integrity validation, publish an immutable generation/source-identity DTO, and let the established main owner parse, adopt, and upload. Transactional static/MD5/MD5R model, classic-proc world, collision, and animation-v3 caches are private to `fs_savepath` and fall back to authoritative VFS sources. This is learned level-load preparation, not general asynchronous asset decode/upload streaming or portal-aware live reprioritization. Local Windows runtime evidence is recorded; clean committed-package performance, broader cancellation/failure campaigns, and release-platform evidence remain open. |
| Shared renderer contracts and GPU skinning | **Implemented; opt-in and release promotion pending** | Ordered material/pass, clip-space, semantic vertex-layout, typed buffer-slice, exact four-weight, and joint-palette contracts are shared by OpenGL and Vulkan. `r_gpuSkinning` remains default-off; admitted MD5/MD5R surfaces produce the ordinary `idDrawVert` stream through backend compute while CPU positions and complete fallback remain authoritative for gameplay consumers and stencil volumes. Clean-package visual/performance and platform/driver promotion evidence remains open. |
| Modern classic-frame ownership | **Experimental; four complete domains, in-world GUI subset, capture-backed subview transaction, plus material-deform dependency implemented** | Fixed-function root 2D GUI, eligible ambient-only 3D world, fixed-classic interaction, and complete eligible fog/blend-phase ownership have separate default-off shared corridors. A separate default-off in-world GUI corridor owns only the complete provenance-tagged subset emitted by `R_RenderGuiSurf`, with a 3D depth contract and all-tagged-surface classic rollback. The capture-backed remote-camera/refraction transaction seals the immediate child-scene/copy edge without claiming child material or lighting ownership. The independent default-off material-deform contract distinguishes finalized CPU results from generated/skinned geometry, seals completed or intentional-empty output and classic receiver non-applicability, and lets the shared consumers use ordinary final geometry without rerunning the deform in either backend. Particle/particle2, skip/failure, freshness, cache, or provenance blockers retain complete-domain rollback. Clean-package and platform promotion remain open. |
| Temporal presentation | **Planned** | Complete motion vectors, history ownership, TAA/TAAU, reactive/disocclusion handling, and dynamic-resolution integration are absent. SMAA remains the compatibility path. |
| Modern PBR lighting and idTech 6-like follow-ons | **Planned** | GGX/IBL, reflection probes, clustered decals/probes, froxel volumetrics, SSR/SSGI, GPU-driven visible ownership, and optional sparse residency all remain after the shared-contract and temporal gates. |

Milestones A, B, and C have completed their implementation and local integration
gates. Milestone D now has four implemented complete domains, a bounded
in-world GUI subset transaction, a capture-backed remote-camera/refraction
subview transaction, and their shared material-deform dependency: eligible
fixed-function root 2D GUI views, eligible ambient-only 3D world views,
fixed-classic interaction, and complete eligible fog/blend phases use
backend-neutral sealed records on both backends, with independent settings and
complete classic rollback. Interaction accepts eligible unshadowed and
shadow-coupled views with explicit light, receiver, shadow, map, material,
resource, and fallback accounting. Fog/blend preserves complete light,
GLOBAL-to-LOCAL receiver, ordered stage, fog-cap, texgen/state/resource, and
backend accounting without mixing shared and classic phase draws. All four
domains have retained local runtime qualification; fog/blend's controlled
stock-declaration profile passes exact parity, effect-delta, ownership, and
rollback gates on both backends. The controlled shipped-material deform profile
also passes exact parity, visible effect-delta, completed ownership, and
zero-commit skipped rollback gates on both backends. Completed/empty finalized
material deforms and the classic source-geometry receiver roles now have an
explicit sealed contract, while particle/particle2 remain named fallback. The
capture-backed subview corridor seals the immediate child scene/copy edge for
remote-camera and refraction views; direct mirror/reflection/clip-plane, x-ray,
depth/cubemap, nested, and other special subviews retain their complete classic
owners. In-world GUI output from `R_RenderGuiSurf` now has independently
sealed, depth-aware ownership with complete tagged-subset fallback. The next
recommended implementation target is **Milestone D cinematic playback and
authored post-chain ownership**, without skipping ahead to temporal or PBR
lighting.
Release qualification remains a separate track: repeat and retain the
Milestone A-D acceptance sets from clean committed source and a freshly staged
final package, with the required platform and driver coverage. The PBR Phase
0-3 foundation is intentionally not a reason to skip ahead to visible PBR
lighting.

## Best official Doom 3 BFG candidates

The paths below are relative to official `DOOM-3-BFG/neo/` at the pinned
snapshot.

| Candidate | Readily available BFG code | openQ4 use | Reuse level | Priority |
|---|---|---|---|---|
| Parallel job substrate | `idlib/ParallelJobList.*`, `idlib/Thread.*`, renderer consumers in `tr_frontend_addmodels.cpp` and `tr_frontend_addlights.cpp` | A bounded, dependency-aware worker pool for renderer front-end work, archive/decode jobs, animation work, and cache generation | Adapt architecture; replace platform primitives and spin waits with SDL3/portable C++, and retain deterministic single-thread fallback | **P1** |
| GPU skeletal skinning | `renderer/BufferObject.*`, `VertexCache.*`, `Model_md5.cpp`, `tr_frontend_addmodels.cpp`, `tr_backend_draw.cpp`, `RenderProgs*` | Joint-buffer uploads and optional four-weight GPU deformation for rendered MD5/MD5R draw surfaces and shadow-map casters while preserving CPU consumers | Port the algorithm into dedicated backend-neutral skin attributes and buffers; do not import BFG's GL backend or blindly reuse its vertex-color packing | **P1** |
| GPU timing and automatic resolution scaling | `renderer/ResolutionScale.*`, the timer query in `RenderSystem.cpp`, CPU profiling blocks in `RenderLog.*` | Feed openQ4's implemented backend-neutral, non-blocking GL/Vulkan whole-frame timing result into a bounded controller | Reuse the controller logic, not BFG's single-query blocking readback | **P1** |
| Generated model, render-world, and collision caches | `renderer/Model.cpp`, `Model_md5.cpp`, `ModelManager.cpp`, `RenderWorld_load.cpp`, `cm/CollisionModel_files.cpp` | Cache parsed static/MD5 geometry, `.proc` world data, and collision data after first trusted-source load | Design a hardened openQ4 format; follow the generated-animation cache contract and include Quake 4 MD5R/source-PK4 identity | **P1** |
| Preload manifests | `framework/File_Manifest.*` and resource-type discovery in `FileSystem.cpp` | Record actual per-map image/model/animation/sample/collision use and replay it through a cancellable preload queue | Reuse the manifest concept, not BFG's retail manifest contents | **P1** |
| Resource containers | `framework/File_Resource.*` | Optional developer-generated, sequential cache containers for derived data | Reuse the access-order concept only; BFG uses 32-bit offsets and trusted tables, so prefer individual cache files or a new bounded 64-bit format and never require a BFG `.resources` package | **P2** |
| Parallel renderer front end | `renderer/tr_frontend_*`, `renderer/jobs/ShadowShared.*`, atomic frame allocation in `tr_frontend_main.cpp` | Parallel entity/light visibility, interaction preparation, and shadow-caster work after ownership is made immutable for a frame | Architectural port with substantial Quake 4/BSE and modern-renderer adaptation | **P1/P2** |
| Explicit transient/static GPU buffers | `renderer/BufferObject.*`, `VertexCache.*` | Complete the current upload manager with backend-neutral vertex/index/joint handles, frame rings, fences, budgets, and overflow diagnostics | Mine lifecycle and handle ideas; current openQ4 GL/Vulkan ownership must remain authoritative | **P1** |
| Render-matrix and culling utilities | `idlib/geometry/RenderMatrix.*` | Shared, tested MVP/frustum/depth-bounds math for scene packets, shadow planning, Hi-Z, and GL/Vulkan clip-space variants | Selectively adapt algorithms and tests; do not force BFG's matrix or vertex ABI onto Quake 4 data | **P2** |
| Render-program parameter model | `renderer/RenderProgs.*`, `RenderProgs_GLSL.cpp` | Common parameter names/layouts for optional skinning and shared passes | Selective reference only; build a backend-neutral material/pass IR rather than another GL-specific shader manager | **P2** |
| Stereo presentation | `renderer/RenderContext.h`, stereo portions of `GuiModel.cpp`, `RenderSystem.*`, and `OpenGL/gl_backend.cpp` | Optional side-by-side/top-bottom rendering and stereo-aware full-screen GUI depth | Adapt only after ordinary presentation is stable; use SDL/OpenXR-era platform interfaces instead of old WGL assumptions | **P3** |
| Lightweight compression | `sys/LightweightCompression.*` | Potential LZW/zero-run compression for new cache payloads where profiling proves a benefit | Reuse only behind a new bounded, fuzzed decoder and versioned container; do not insert it into protocol 2.41 | **P3** |

### 1. Parallel jobs: the highest-leverage import

BFG's `idParallelJobList` provides job lists, priorities, synchronization
points, list dependencies, bounded parallelism, timing, and a deterministic wait
boundary. Its real BFG renderer users are deliberately coarse: add visible
models, add lights, and build shadow work. That is a better starting point than
spawning ad-hoc threads throughout openQ4.

The landed openQ4 service adapts that architecture rather than copying it:

- portable C++ threads and condition variables provide bounded workers and
  blocking waits instead of BFG's spinning `Wait()` behavior and fixed
  processing-unit assumptions;
- cancellation, shutdown, payload ownership, and lifetime are explicit in the
  service contract;
- deterministic synchronous mode is available for dedicated builds, tests,
  and debugging;
- list, job, and dependency admission is bounded, and saturation is reported
  instead of growing or dropping work silently;
- queue, execution, wait, high-water, rejection, and starvation-aging metrics
  are observable; dependency critical-path aggregation remains for real
  consumer graphs;
- arbitrary worker-side renderer calls remain prohibited unless a future
  backend-owned queue defines that boundary.

The production contract, controls, saturation behavior, native coverage, and
remaining consumer/promotion boundary are documented in the
[portable job-system guide](parallel-job-system.md).

The first production consumer is now the learned level-load read/PK4-inflate and
source-framing pipeline. Further consumers should still require a clean join
point and detached output: asset-specific image/audio/model decode or transcode,
generated-cache preparation, and only then renderer model/light preparation.
PK4 archive mutation and game-state mutation remain outside the worker contract.

### 2. GPU skinning: a concrete idTech 5-class capability

BFG converts MD5 vertices to four normalized byte weights and four joint
indices, uploads joint matrices through an aligned joint buffer, and keeps a CPU
path for unsupported or special surfaces. openQ4 now adapts that architecture
through its own full-precision, backend-neutral and fail-closed contract rather
than adopting BFG's packed vertex ABI.

The packed layout is not itself a safe compatibility contract. BFG asserts that
a model has fewer than 256 joints, stores joint indices in `color`, stores
weights in `color2`, and sorts, truncates, then renormalizes vertices with more
than four influences. Its own source notes residual weights above 25 percent in
some assets. Quake 4's packed MD5R path can also carry diffuse vertex colors, so
openQ4 must not silently repurpose those channels.

The landed corridor applies the required compatibility rules:

- validate joint and influence counts and retain the CPU path rather than
  truncating any MD5 vertex with more than four meaningful influences;
- use dedicated `uint32[4]` joint indices and `float32[4]` weights so packed
  MD5R diffuse colors and signed implicit residual-weight behavior remain intact;
- preserve CPU deformation for collision, traces, deforms, software-only debug
  tools, decals/overlays that need current positions, stencil shadow-volume
  construction, and any shader/material path lacking the skinning contract;
- carry joint data through ambient surfaces, light interactions, shadow-map
  casters, subviews, and view models, while explicitly routing incompatible
  surfaces to CPU deformation;
- use one typed, generational joint-buffer/slice contract represented
  consistently in GL and Vulkan;
- keep the capability default-off while clean-package bounds, vertex,
  silhouette, screenshot, and performance promotion evidence is accumulated.

This should land as capability and parity infrastructure first. It should not be
coupled to PBR, TAA, or a renderer-default switch.

### 3. Generated assets and learned preloading

BFG assumes generated BFG resources exist. Stock Quake 4 installations do not,
so its packaged manifests and resource containers cannot be required. The landed
Milestone B adaptation extends openQ4's generated-animation and binary-image
pattern:

1. Load the original PK4 asset normally.
2. Record the resolved source path, containing PK4 checksum, parser/build
   version, platform-independent format version, and relevant quality settings.
3. Write derived data under `fs_savepath/baseoq4/generated/` using an atomic
   temporary-file replacement.
4. On the next run, validate every bound before allocation and every source key
   before use.
5. Delete or ignore an invalid cache and fall back to the original asset.

Parsed static/MD5/MD5R render models, classic `.proc` render-world data,
collision models, animation v3, and an exact learned per-map preload manifest
now follow that contract. BFG's serializers remain useful field inventories,
but their timestamp checks and trusted-data assumptions are not sufficient for
a PK4-backed, fail-closed runtime. The openQ4 manifest schedules a bounded
deterministic subset and never overrides VFS resolution or becomes a second
source of asset truth. Asset-specific asynchronous owner decode/upload and
portal-aware live reprioritization remain future work. The exact format,
ownership, limits, rollback, and promotion-evidence boundary are documented in
[Level-Load Cache Modernization](loading-cache-modernization.md).

### 4. Dynamic resolution from real GPU time

BFG's resolution controller is compact and readily adaptable. It lowers
resolution quickly when GPU time exceeds a threshold and raises it more slowly
after several under-budget frames, avoiding constant oscillation. openQ4 already
has render scaling, renderer metrics, high-refresh presentation, and a
four-slot, non-blocking GL timestamp ring. Milestone A exposes that ring
as a backend-neutral whole-frame result and provides the equivalent Vulkan
timestamp-query path. Both backends resolve only retired/available slots, reset
their generation at workload discontinuities, and feed high-resolution CPU plus
de-duplicated GPU samples into `OPENQ4_FRAME_TIMING_V1`. What remains is the
feedback controller and complete promotion evidence.

The future controller should build on this implemented timing foundation:

- preserve the non-blocking GL/Vulkan timestamp contract and never wait on a
  current-frame result;
- target a user/display frame budget and account for VRR;
- quantize dimensions to backend-friendly alignments;
- expose minimum scale, response rate, and a conservative default-off rollout;
- reset history on map load, teleport, video restart, backend switch, and other
  discontinuities;
- keep GUI/HUD composition at native output resolution;
- integrate with future TAAU, while remaining useful with SMAA/bilinear scaling.

## BFG systems to study but not transplant

| BFG subsystem | Why it is not a direct openQ4 import | Safer direction |
|---|---|---|
| `sys/PacketProcessor.*`, `Snapshot*`, lobby/session code | Different wire model, object snapshots, lobby assumptions, platform services, and game semantics; replacing it would break protocol 2.41 and existing Quake 4 networking | Keep the hardened Quake 4 path. If a new transport is justified, negotiate an explicit openQ4 protocol while retaining 2.41 as a separate path |
| Depth-fail stencil-shadow back end | The official BFG release expressly omits the code that enables Carmack's Reverse; the included shadow jobs and shared geometry helpers are not a complete replacement renderer | Keep openQ4's existing Quake 4-compatible stencil path authoritative. Study the BFG job boundaries independently of the omitted back-end operation |
| SWF UI runtime | Quake 4 ships idTech 4 GUI scripts, not BFG SWFs; replacing the UI runtime would strand stock menus and in-world GUIs | Modernize the existing GUI renderer/parser and add optional new UI surfaces without removing the stock path |
| XAudio2 backend | Windows-specific and redundant with openQ4's cross-platform OpenAL voice/HRTF/EFX work | Continue improving the current backend and SDL/platform device lifecycle |
| Doom 3 `d3xp` gameplay, aim, inventory, achievements, and save/session code | Different game rules, class layouts, scripts, maps, and save data | Port isolated engine-agnostic ideas only; implement Quake 4 gameplay changes canonically in `openQ4-game` |
| BFG render backend as a whole | Assumes BFG vertex formats, shaders, material behavior, generated resources, and GL/platform interfaces | Extract contracts and algorithms into the current backend-neutral GL/Vulkan architecture |
| BFG resource packages as shipped data | No corresponding generated packages exist in a retail Quake 4 installation | Generate disposable caches locally and retain source-PK4 fallback |
| Doom Classic integration and platform storefront code | Unrelated content and unavailable proprietary service pieces | Keep out of the runtime |

## Capabilities beyond the official BFG drop

Reaching an idTech 6-like standard requires work that the 2012 BFG source does
not provide. These should build on the BFG-derived foundations rather than be
treated as code-import tasks.

The highest-leverage renderer step is to finish coherent ownership, not add one
more isolated experimental pass. openQ4 already has experimental scene packets,
a render graph, clustered/MDI submission, shadow maps, and Vulkan coverage, but
the capability matrix records no proven modern visible-lighting domain yet.
Classic ambient/material, interaction, and fog/blend semantics now have
independent shared contracts alongside root 2D GUI, and their material-deform
dependency is explicit. The remote-camera/refraction capture-backed subview
edge and the provenance-tagged in-world GUI subset are also explicit; cinematic
and remaining post/fallback semantics must still become explicit before temporal
or PBR work multiplies the parity surface.

### Backend and submission

- A backend-neutral material/pass intermediate representation shared by GL and
  Vulkan.
- Complete modern visible-lighting ownership before promoting GPU-driven
  submission.
- Persistent resource descriptors, pipeline/shader caches, indirect draws,
  Hi-Z culling, and a safe CPU rollback.
- Explicit resource state/lifetime tracking and asynchronous upload budgets.

### Image quality

- Complete motion vectors for rigid, skinned, particle, deform, subview, GUI,
  and view-model surfaces.
- TAA/TAAU with reactive masks, disocclusion handling, camera-cut resets, and
  SMAA fallback.
- Dynamic resolution driven by backend timestamps.
- Namespaced PBR material extensions, GGX lighting, IBL/specular probes, and
  stock-material defaults that preserve the classic look.
- Froxel fog/volumetrics, SSR, and carefully bounded screen-space GI only after
  depth/history infrastructure is reliable.
- True HDR output (scRGB/HDR10 negotiation, paper-white GUI composition, and HDR
  screenshot policy), distinct from the existing internal HDR scene chain.

### Streaming and CPU scalability

- Extend the implemented bounded read/PK4-inflate -> framing/integrity DTO ->
  main-owner adoption corridor into asset-specific decode/transcode/upload only
  where detached results and per-stage budgets make ownership safe.
- Add portal-aware live priority changes to the current exact-match learned
  manifest. The implemented replay is a bounded, deterministically ordered
  subset rather than an unconditional whole-level preload, but it does not
  reprioritize dynamically from portal visibility.
- Optional virtual-texture/sparse-residency support for high-resolution community
  content, after ordinary streaming is reliable. The official BFG drop does not
  provide idTech 5's virtual-texturing implementation, and stock Quake 4 assets
  must never depend on this path.
- GPU skinning plus jobbed animation/model preparation.
- Background shader/pipeline compilation with deterministic cache keys and an
  always-available synchronous fallback.

### Networking and operations

- Keep protocol 2.41 for compatibility, but consider a separately negotiated
  openQ4 transport for larger sequence spaces, stronger session authentication,
  modern congestion/fragmentation behavior, and optional traffic protection.
- Continue bounded parser work, fuzzable decode APIs, rate limits, structured
  diagnostics, and headless dedicated-server soak tests independently of any
  future protocol.

## Recommended implementation order

| Milestone | Current state | Dependency that prevents promotion |
|---|---|---|
| A. Foundation and measurement | **Implemented and locally validated; release promotion pending** | The portable bounded job substrate, backend-neutral delayed GL/Vulkan whole-frame timing, and versioned, replay-verifiable per-map CPU/GPU budget tooling are implemented. Current-build jobs-on/off parity, repeated map-change shutdown, deterministic dedicated exits, schema-10 stock capture/replay, and complete replay-verified 8/8 OpenGL plus 8/8 Vulkan required profiles have passed. Promotion still requires the same evidence retained from clean committed source and a freshly staged final package, plus release platform/driver qualification. |
| B. Loading and cache modernization | **Implemented but default-off; performance requalification required** | Exact learned manifests, bounded cancellable read/PK4-inflate and framing/integrity stages, immutable source DTOs, and transactional model/world/collision plus animation-v3 caches are integrated with source fallback. A 2026-08-20 regression audit found that the prior default-on experiment could materially lengthen stock map loads, so `com_levelLoadModernization 0` now restores the classic baseline and gates every framework/animation cache read and write. Promotion requires a clean committed-package campaign that beats or matches classic cold and warm loads without rewrite churn, plus release-platform qualification. |
| C. Shared renderer contracts and GPU animation | **Implemented; promotion pending** | Ordered pass semantics, clip/viewport conversion, semantic layouts, typed buffer slices, exact four-weight MD5/MD5R sidecars, bounded joint palettes, and GL/Vulkan deformation paths are present with full-surface CPU rollback. Dependency-light and module self-tests cover the common contract; clean-package SP/MP image, collision/hit, animation-heavy performance, and platform/driver evidence remains the promotion gate. |
| D. Modern classic-frame ownership | **Experimental; four complete domains, in-world GUI subset, capture-backed subview transaction, and material-deform dependency implemented** | Eligible fixed-function root 2D GUI views, ambient-only 3D world views, fixed-classic interaction views, and complete eligible fog/blend phases are transactionally evaluated through backend-neutral sealed records by GL and Vulkan, with complete-domain classic rollback. The default-off in-world GUI corridor transactionally owns the complete provenance-tagged `R_RenderGuiSurf` output subset with 3D depth semantics; one rejection restores all tagged sources to the classic walker. The default-off capture-backed subview transaction seals the immediate remote-camera/refraction child scene/copy edge and consumes that copy on both backends; direct clip/camera special views remain classic-owned. A separate default-off deform contract seals the authoritative CPU result, source-preserving interaction/fog roles, frame/cache provenance, intentional empty work, and named unsupported outcomes for the shared consumers. Cinematic and authored post ownership are next. Authored-stock, clean-package, and platform qualification remain open as applicable. |
| E. Temporal presentation | **Planned** | Milestones A and C now supply the timing and shared-contract prerequisites; incomplete Milestone D still blocks complete frame ownership and the visible motion-vector corridor. |
| F. Modern materials and advanced lighting | **Foundation only** | PBR authoring/resource Phases 0-3 exist, but visible PBR/IBL and advanced-lighting ownership must wait for Milestones C-E. |

### Milestone A: foundation and measurement

1. **Implemented:** the engine-owned portable bounded job manager provides
   synchronous mode, starvation-safe priorities, dependency tests,
   shutdown/cancellation tests, and timing counters.
2. **Implemented:** the delayed GL timestamp result is exposed through a
   backend-neutral whole-frame timing contract, and the equivalent Vulkan
   timestamp path is integrated; both are generation-aware and never wait for a
   current-frame result.
3. **Implemented and locally validated; release promotion pending:** enforce versioned,
   configurable map/backend/profile CPU and GPU percentile budgets in the
   gameplay benchmark and stock baseline; bind contract/runtime/artifact
   provenance and replay measurements fail-closed. The initial repeated 20/28
   ms rows are target ceilings until complete GL/Vulkan captures calibrate and,
   where justified, tighten each explicit identity.

The 2026-08-19 current-build evidence snapshot closes the local job lifecycle
portion of this gate:

- jobs-on and jobs-off `game/storage1` runs produced identical engine TGA bytes
  and matching game-state evidence;
- jobs-on and jobs-off OpenGL campaigns, plus a jobs-on Vulkan campaign, crossed
  `game/mcc_2` -> `game/storage1` -> `game/storage2` -> `game/storage1` ->
  `game/tram1` and ended with
  `jobsShutdown PASS v1 initialized=0 queued=0 running=0`;
- five dedicated-server runs exited normally with one synchronous self-test and
  one clean shutdown marker each;
- the schema-10 four-role retail-PK4 baseline passed capture and immediate
  replay under the canonical display/budget contract, and its engine screenshots
  and save preview passed local human review;
- storage and repeated-map runs exercised nonblocking OpenGL and Vulkan timing;
  the final immutable development runtime then passed and replay-verified all
  eight OpenGL and all eight Vulkan required-profile cases. The earlier
  `game/medlabs` failure was fixed by ordering and clamping depth bounds before
  the OpenGL call; its debug-context rerun records zero GL errors.

This evidence set culminated in the immutable development runtime
`milestone-a-20260819-final3`; its complete required-profile reports are
`ma-a-gl3` and `ma-a-vk3`. It came from an uncommitted current source tree and is
not a retained release artifact. It does not replace clean-source provenance, a
freshly staged final package, platform/driver qualification, or retained release
review.

Exit gate: replay-valid exact bordered-window 1280x720 GL/Vulkan captures for
the required budget identities; identical stock screenshots and game state with
jobs on/off; repeated map changes ending in
`jobsShutdown PASS v1 initialized=0 queued=0 running=0`; deterministic
dedicated-server exit; and the general four-role retail-PK4 and human-review
promotion evidence. The current-build job, lifecycle, timing-path, required-map,
and stock-baseline checks above satisfy the local implementation gate. Only the
general clean-source, final-package, retained-review, and platform/driver gates
keep release promotion open.

### Milestone B: loading and cache modernization

1. **Implemented:** a learned manifest is keyed to the exact normalized map,
   full SHA-256 runtime-role and entity-filter identities, ordered VFS/search and
   pure-PK4 state, individual source identities, and load-affecting settings.
2. **Implemented:** bounded cancellable workers read independently opened
   sources, perform the PK4 inflation reached by those reads, validate supported
   source framing and whole-buffer integrity, and publish a sealed immutable DTO.
   The ordinary VFS lookup runs again before substitution; format-specific asset
   parsing, adoption, and renderer/audio upload remain with the main owner.
3. **Implemented:** versioned binary render-model, classic render-world, and
   collision caches under `fs_savepath`, plus the companion SP/MP animation-v3
   cache, validate detached state and publish atomically or fall back to source.
4. **Corrected after regression audit:** all Milestone B cache, preload, and
   animation-cache paths now require the default-off
   `com_levelLoadModernization` master gate. Archived individual controls from
   earlier builds cannot silently retain the slower path.

Exit gate: cold and warm load measurements, bounded memory, cancellation during
map/restart/disconnect, corrupt-cache fallback, and zero required loose assets.
Focused native/static tests, Windows integration builds, and current-build
stock compatibility checks satisfy the implementation/safety gate, but not the
performance gate. Local development cold/warm and bounded-memory measurements, focused
corruption/rollback, dedicated teardown, and engine-screenshot review are now
recorded; the staged stock report remains failed on its unchanged MP CPU budget.
Exact committed source/package identity, a clean-package budget pass, broader
cancellation/failure campaigns, and release-platform coverage remain required
for promotion. Requalification must also demonstrate that both cold and warm
opt-in loads are no slower than the classic default before any default change.
Their authoritative status is recorded in
[Level-Load Cache Modernization](loading-cache-modernization.md).

### Milestone C: shared renderer contracts and GPU animation

1. **Implemented:** define the minimum backend-neutral material/pass,
   clip-space, vertex-layout, and typed buffer-slice contracts needed by both GL
   and Vulkan.
2. **Implemented:** add bounded backend joint-buffer rings and dedicated
   full-precision four-weight vertex data.
3. **Implemented:** establish the rigid/MD5/MD5R CPU-vs-GPU parity corridor,
   including preserved diffuse vertex colors, signed MD5R residual weights,
   exact-only MD5 admission, and joint/data/capability fallbacks.
4. **Implemented:** feed an ordinary deformed `idDrawVert` stream to depth,
   ambient, interaction, subview, shadow-map, and view-model consumers while
   decals/overlays keep CPU positions and stencil volumes remain explicitly CPU.

Exit gate: stock SP/MP visual equivalence, CPU fallback parity, no collision or
hit-detection changes, and measured CPU-frame reduction in animation-heavy maps.
The implementation and deterministic contract gate is complete. Clean committed
package screenshots, pure-MP collision/hit digests, repeated animation-heavy
measurements, and target-platform/driver coverage remain required before the
default-off capability can be promoted; the exact procedure is recorded in
[Shared Renderer Contracts and GPU Animation](gpu-skinning-modernization.md).

### Milestone D: modern classic-frame ownership

1. **In progress:** fixed-function root 2D GUI, world ambient/material,
   unshadowed plus shadow-coupled fixed-classic interaction, and fog/blend
   conditions, colors, repeated order, matrices/texgen, images, samplers,
   light/receiver/shadow/cap identity, and render state are expressed through
   shared contracts. Their CPU material-deform dependency is also sealed. The
   immediate capture-backed remote-camera/refraction subview edge is sealed;
   direct special subviews, cinematic, and post behavior remain.
2. **Implemented for the first four complete domains:** GL and Vulkan consume the
   same per-draw evaluated semantic records, with backend-specific execution and
   an untouched complete-domain classic rollback.
3. **Implemented for the first four complete domains:** scene packets and the
   material resource table promote a GUI, eligible ambient-only world view, or
   eligible unshadowed/shadow-coupled fixed-classic interaction view, while the
   fog/blend transaction promotes only a complete eligible phase, after
   transactional preparation and complete backend preflight; all other domains
   retain their established owner.
4. **Implemented for world ambient/material:** opaque and perforated draws must
   match an established depth packet, translucent draws retain their classic
   depth behavior, and ordered material work is split into pre-fog and post-fog
   phases without taking ownership of depth or fog.
5. **Implemented for fixed-classic interaction:** every accepted
   light, local/global/translucent receiver, light stage, decomposed primitive,
   no-op, and resource is sealed and reconciled together.
6. **Implemented for shadow-coupled fixed-classic interaction:** classic stencil
   volumes, projected single-map and CSM/parallel shadows, point cubes, mixed
   mapped/stencil lights, complete hybrid supplements, dynamic casters, and
   perforated casters are sealed and reconciled by both backends. Translucent
   moment casters and any incomplete/custom/unsupported/backend condition reject
   the whole interaction view before visible ownership.
7. **Implemented for fog/blend:** every fog/blend light, GLOBAL-to-LOCAL receiver,
   ordered active/inactive blend stage, fog receiver/cap, evaluated texgen/state,
   and resource is sealed and reconciled together. Any source, geometry,
   resource, capacity, target, or backend blocker rejects the complete phase
   before the first shared main-target draw.
8. **Implemented for classic material deforms:** finalized root GUI, world
   ambient, and mapped-shadow draws seal source/result material and geometry,
   evaluated inputs, frame/cache lifetime, outcome, and semantic hash around the
   authoritative CPU deform. Interaction and fog receivers seal the classic
   not-applicable/source-geometry role. Completed and intentional-empty results
   are admissible only behind `r_rendererSharedDeform`; particle/particle2,
   skipped, failed, stale, or mismatched records reject the complete owning
   transaction before backend work.
9. **Implemented for capture-backed subviews:** a child scene packet, parent
   source surface, exact full-viewport color `RC_COPY_RENDER`, and destination
   image are reconciled into one bounded remote-camera/refraction record.
   OpenGL and Vulkan consume the sealed capture arguments and report ownership
   only after the transfer. Mirror/reflection/clip-plane, x-ray, cubemap/depth,
   nested, cinematic, and other special views retain their untouched classic
   owners pending dedicated semantic records.
10. **Implemented for in-world GUI:** only GUI quads emitted under
    `R_RenderGuiSurf` receive front-end provenance. Their complete tagged subset
    uses a world-category GUI packet stream and depth-aware world pass records;
    GL and Vulkan preflight and commit before the ambient walks, then suppress
    only a successfully owned subset from their matching classic walker. Any
    source, packet, material, resource, target, capacity, or backend blocker
    retains every tagged source on the untouched classic path.

Exit gate: at least one complete stock-frame domain is modern-owned on both
backends, no visible light or surface is silently dropped, and rollback produces
the classic result. The fixed-function root 2D GUI corridor satisfies this
implementation gate while remaining default-off; its exact contract, validation
procedure, and remaining boundaries are recorded in
[Shared Classic 2D GUI Domain](classic-gui-domain-modernization.md). This is not
a claim that modern visible lighting or all of Milestone D is complete. Local
Windows stock `game/storage1` engine captures passed with the corridor both off
and on for GL and Vulkan; enabled diagnostics recorded complete owned views and
explicit whole-view fallbacks on both backends. The second complete-domain
implementation is documented in
[Shared Classic World Ambient/Material Domain](classic-world-ambient-domain-modernization.md);
its bordered 1280x720 stock `maps/tools/mv2` option-off/on engine captures match
exactly on GL and Vulkan, enabled diagnostics report one owned pre-fog draw with
domain hash `dc18ed8c0539bbfc`, and the stock `shaderDemos/move` deform override
reports a named zero-draw whole-view fallback on both backends. These are local
development-worktree results, not clean-package or platform promotion. The
third complete-domain implementation and its shadow-coupled expansion are documented in
[Shared Classic Interaction-Lighting Domain](classic-interaction-domain-modernization.md);
its unshadowed controlled GL/Vulkan captures retain exact classic image parity,
and its expanded native/static gates cover the completed stencil, mapped,
mixed, dynamic/perforated, hybrid, and atomic-fallback contract. The documented
controlled and stock shadow profile is the runtime release-acceptance set.
Clean committed-package and target-platform/driver recapture remain promotion
requirements. The fourth complete-domain implementation is documented in
[Shared Classic Fog/Blend Domain](classic-fog-blend-domain-modernization.md).
Its native/static gate and controlled GL/Vulkan profile now pass exact
shared/classic engine-image parity, nonempty reconciled ownership, material
fog/blend deltas, and named
zero-commit atomic rollback. Authored-stock fog plus clean-package and
target-platform/driver promotion remain open.
The material-deform dependency is documented in
[Shared Classic Material-Deform Contract](classic-deform-domain-modernization.md).
The capture-backed subview implementation is documented in
[Shared Capture-Backed Subview Transaction](classic-subview-domain-modernization.md).
The in-world GUI implementation is documented in
[Shared Classic In-World GUI Domain](classic-inworld-gui-domain-modernization.md).
The next implementation target is cinematic playback and authored post-chain
ownership, not Milestone E temporal presentation or Milestone F PBR/advanced
lighting.

### Milestone E: temporal presentation

1. Promote the GPU-time controller to experimental dynamic resolution.
2. Add complete motion-vector ownership.
3. Implement TAA/TAAU with SMAA rollback and native-resolution UI.

Exit gate: stable motion, camera cuts, particles, weapon view, portals/subviews,
menus, screenshots, save previews, and GL/Vulkan parity.

### Milestone F: modern materials and advanced lighting

1. Extend the shared material/pass IR with namespaced PBR/IBL semantics without
   changing stock defaults.
2. Add reflection/specular-probe ownership and bounded clustered decal/probe
   records.
3. Promote clustered/GPU-driven submission only after visible-lighting parity.
4. Stage froxel volumetrics and screen-space reflection/GI work behind separate
   evidence gates rather than one all-or-nothing renderer switch.

Exit gate: the modern renderer owns complete validated domains rather than
isolated passes, with the classic path remaining a one-setting rollback.

## Promotion evidence for every milestone

Each milestone should record:

- source provenance and retained notices for incorporated code;
- focused unit/static tests and malformed-cache/input tests;
- Windows x64 builds plus Linux/macOS compile-policy coverage appropriate to the
  changed subsystem;
- stock-only SP load/save/reload/demo evidence;
- pure MP listen-server and auto-joined client gameplay evidence;
- engine-render-target screenshots and human visual review;
- before/after CPU frame, GPU frame, load time, memory, and cache-size metrics;
- rollback results with the new feature disabled;
- confirmation that no new loose content is required.

The [engine capability matrix](engine-capability-matrix.md) remains the current
truth. This roadmap describes sequence and acceptance gates; it does not mark a
capability implemented merely because BFG source exists or a prototype compiles.
