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

## Current implementation state (2026-08-19)

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
| GPU measurement and dynamic resolution | **Partial** | OpenGL has a delayed four-frame non-blocking timer-query ring and the engine has manual render scaling. A backend-neutral full-frame result, Vulkan timestamp ring, automatic controller, discontinuity resets, and promotion evidence remain Milestones A/E. |
| General job system | **Planned** | Local workers do not provide a bounded dependency/job-list substrate. Milestone A begins with portable sleepable synchronization, cancellation, deterministic synchronous execution, and dedicated-server-safe ownership. |
| Generated caches, streaming, and learned preload manifests | **Partial** | Binary images and generated-animation patterns exist, but model/world/collision caches and a cancellable read -> decompress -> decode -> upload pipeline do not. Retail PK4 resolution remains authoritative. |
| Shared renderer contracts and GPU skinning | **Partial** | Scene packets, resource tables, upload infrastructure, and CPU skinning provide inputs, but there is no backend-neutral material/pass IR or supported joint-buffer/GPU deformation path. CPU deformation remains authoritative. |
| Modern classic-frame ownership | **Experimental** | Render-graph, modern OpenGL submission, clustered/MDI infrastructure, shadow maps, light grids, and Vulkan coverage exist, but no complete stock visible-lighting domain is promoted. ARB2 remains the supported/default owner. |
| Temporal presentation | **Planned** | Complete motion vectors, history ownership, TAA/TAAU, reactive/disocclusion handling, and dynamic-resolution integration are absent. SMAA remains the compatibility path. |
| Modern PBR lighting and idTech 6-like follow-ons | **Planned** | GGX/IBL, reflection probes, clustered decals/probes, froxel volumetrics, SSR/SSGI, GPU-driven visible ownership, and optional sparse residency all remain after the shared-contract and temporal gates. |

The practical next target is **Milestone A**. It unlocks safe parallel loading,
cache generation, renderer-front-end work, and trustworthy GPU-budget feedback
without changing stock content interpretation. The PBR Phase 0-3 foundation is
intentionally not a reason to skip ahead to visible PBR lighting.

## Best official Doom 3 BFG candidates

The paths below are relative to official `DOOM-3-BFG/neo/` at the pinned
snapshot.

| Candidate | Readily available BFG code | openQ4 use | Reuse level | Priority |
|---|---|---|---|---|
| Parallel job substrate | `idlib/ParallelJobList.*`, `idlib/Thread.*`, renderer consumers in `tr_frontend_addmodels.cpp` and `tr_frontend_addlights.cpp` | A bounded, dependency-aware worker pool for renderer front-end work, archive/decode jobs, animation work, and cache generation | Adapt architecture; replace platform primitives and spin waits with SDL3/portable C++, and retain deterministic single-thread fallback | **P1** |
| GPU skeletal skinning | `renderer/BufferObject.*`, `VertexCache.*`, `Model_md5.cpp`, `tr_frontend_addmodels.cpp`, `tr_backend_draw.cpp`, `RenderProgs*` | Joint-buffer uploads and optional four-weight GPU deformation for rendered MD5/MD5R draw surfaces and shadow-map casters while preserving CPU consumers | Port the algorithm into dedicated backend-neutral skin attributes and buffers; do not import BFG's GL backend or blindly reuse its vertex-color packing | **P1** |
| GPU timing and automatic resolution scaling | `renderer/ResolutionScale.*`, the timer query in `RenderSystem.cpp`, CPU profiling blocks in `RenderLog.*` | Feed a backend-neutral full-frame result from openQ4's existing non-blocking GL query ring and a new Vulkan timestamp path into a bounded controller | Reuse the controller logic, not BFG's single-query blocking readback | **P1** |
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

The API should be adapted, not copied blindly:

- back it with SDL3 threads/condition variables or a small portable C++ core;
- replace BFG's spinning `Wait()` behavior and fixed platform processing-unit
  assumptions with blocking waits and explicit worker limits;
- make cancellation and shutdown explicit;
- make job payload ownership and lifetime visible in the type/API contract;
- provide a synchronous implementation used by dedicated builds, tests, and
  deterministic debugging;
- bound queues and allocations; report saturation instead of silently growing;
- collect queue, execution, wait, and critical-path timings;
- prohibit renderer API calls from arbitrary workers unless the backend
  explicitly owns that queue.

First consumers should be work that already has a clean join point: learned
preload discovery, image decode/transcode, generated-cache writes, and then
renderer model/light preparation. PK4 archive mutation and game-state mutation
should not be the first consumers.

### 2. GPU skinning: a concrete idTech 5-class capability

BFG converts MD5 vertices to four normalized byte weights and four joint
indices, uploads joint matrices through an aligned joint buffer, and keeps a CPU
path for unsupported or special surfaces. That is a strong reference for
openQ4, where CPU deformation remains authoritative today.

The packed layout is not itself a safe compatibility contract. BFG asserts that
a model has fewer than 256 joints, stores joint indices in `color`, stores
weights in `color2`, and sorts, truncates, then renormalizes vertices with more
than four influences. Its own source notes residual weights above 25 percent in
some assets. Quake 4's packed MD5R path can also carry diffuse vertex colors, so
openQ4 must not silently repurpose those channels.

An openQ4 implementation needs additional compatibility work:

- inventory joint counts and influence counts, then validate the top-four
  reduction against stock Quake 4 MD5 and packed MD5R meshes; retain the CPU
  path whenever a joint-index limit or residual-error threshold is exceeded;
- use dedicated skin-index/weight attributes, or prove that an existing packed
  channel is semantically unused, so MD5R diffuse colors remain intact;
- preserve CPU deformation for collision, traces, deforms, software-only debug
  tools, decals/overlays that need current positions, stencil shadow-volume
  construction, and any shader/material path lacking the skinning contract;
- carry joint data through ambient surfaces, light interactions, shadow-map
  casters, subviews, and view models, while explicitly routing incompatible
  surfaces to CPU deformation;
- use one backend-neutral joint-buffer handle represented consistently in GL
  and Vulkan;
- compare bounds, positions, normals/tangents, silhouettes, and screenshots
  against the CPU path before enabling it by default.

This should land as capability and parity infrastructure first. It should not be
coupled to PBR, TAA, or a renderer-default switch.

### 3. Generated assets and learned preloading

BFG assumes generated BFG resources exist. Stock Quake 4 installations do not,
so its packaged manifests and resource containers cannot be required. The safe
adaptation is the pattern openQ4 already uses for generated animation and binary
image caches:

1. Load the original PK4 asset normally.
2. Record the resolved source path, containing PK4 checksum, parser/build
   version, platform-independent format version, and relevant quality settings.
3. Write derived data under `fs_savepath/baseoq4/generated/` using an atomic
   temporary-file replacement.
4. On the next run, validate every bound before allocation and every source key
   before use.
5. Delete or ignore an invalid cache and fall back to the original asset.

Useful next cache targets are parsed static/MD5/MD5R render models, `.proc`
render-world data, collision models, and a learned per-map preload manifest.
BFG's serializers are useful field inventories, but their timestamp checks and
trusted-data assumptions are not sufficient for a PK4-backed, fail-closed
runtime. A preload manifest should schedule work; it should not override VFS
resolution or become a second source of asset truth.

### 4. Dynamic resolution from real GPU time

BFG's resolution controller is compact and readily adaptable. It lowers
resolution quickly when GPU time exceeds a threshold and raises it more slowly
after several under-budget frames, avoiding constant oscillation. openQ4 already
has render scaling, renderer metrics, high-refresh presentation, and a
four-frame, non-blocking GL timer-query ring. What is missing is a
backend-neutral total-frame timing result, an equivalent Vulkan timestamp path,
the feedback controller, and complete promotion evidence.

The production version should improve on the 2012 implementation:

- extend the existing delayed GL query ring and add a Vulkan timestamp-query
  ring; never wait on the current frame's result;
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
Classic ambient, interaction, fog, blend, stage-condition/color, deform,
subview, GUI, and fallback semantics must become explicit shared contracts before
temporal or PBR work multiplies the parity surface.

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

- A staged read -> decompress -> decode -> upload pipeline with cancellation,
  per-stage budgets, and map-generation ownership tokens.
- Learned preload manifests and priority changes driven by portal visibility,
  not unconditional whole-level preloads.
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
| A. Foundation and measurement | **Partial** | The GL timing ring exists; the portable job substrate, backend-neutral timing, Vulkan timestamps, and recorded budgets do not. |
| B. Loading and cache modernization | **Partial** | Existing binary-image/generated-animation patterns do not yet form learned manifests, bounded pipeline stages, or model/world/collision caches. |
| C. Shared renderer contracts and GPU animation | **Partial** | Packet/resource infrastructure exists, but shared GL/Vulkan pass semantics and GPU skinning parity are missing. |
| D. Modern classic-frame ownership | **Experimental** | Individual modern paths exist; no complete classic-visible domain has satisfied the cross-backend parity exit gate. |
| E. Temporal presentation | **Planned** | Depends on Milestones A, C, and D for timing, motion/resource contracts, and complete frame ownership. |
| F. Modern materials and advanced lighting | **Foundation only** | PBR authoring/resource Phases 0-3 exist, but visible PBR/IBL and advanced-lighting ownership must wait for Milestones C-E. |

### Milestone A: foundation and measurement

1. Land a portable bounded job manager with synchronous mode, dependency tests,
   shutdown/cancellation tests, and timing counters.
2. Expose the existing delayed GL timer ring through a backend-neutral
   total-frame timing result and add the equivalent Vulkan timestamp-query ring.
3. Establish per-map CPU/GPU budgets in the existing benchmark and stock
   evidence tools.

Exit gate: identical stock screenshots/game state with jobs on/off, clean
shutdown under repeated map changes, and trustworthy non-blocking timing.

### Milestone B: loading and cache modernization

1. Add a learned preload manifest keyed to the exact stock PK4 set and renderer
   settings.
2. Move read/decompress/decode work into cancellable stages.
3. Add versioned binary render-model, render-world, and collision caches under
   `fs_savepath`.

Exit gate: cold and warm load measurements, bounded memory, cancellation during
map/restart/disconnect, corrupt-cache fallback, and zero required loose assets.

### Milestone C: shared renderer contracts and GPU animation

1. Define the minimum backend-neutral material/pass, clip-space, vertex-layout,
   and buffer-handle contracts needed by both GL and Vulkan.
2. Add backend-neutral joint-buffer rings and dedicated four-weight vertex data.
3. Prove the rigid/MD5/MD5R CPU-vs-GPU parity corridor, including diffuse vertex
   colors, residual weights, and joint-count fallbacks.
4. Extend coverage to interactions, decals, subviews, shadow-map paths, view
   models, and explicit CPU stencil-volume fallback.

Exit gate: stock SP/MP visual equivalence, CPU fallback parity, no collision or
hit-detection changes, and measured CPU-frame reduction in animation-heavy maps.

### Milestone D: modern classic-frame ownership

1. Express classic ambient, interaction, fog, blend, stage-condition/color,
   deform, subview, GUI, and fallback behavior through the shared contracts.
2. Make GL and Vulkan consume the same semantic records, with backend-specific
   execution and a per-domain classic rollback.
3. Promote scene-packet, resource-table, render-graph, and modern-visible domains
   only when each domain's exact parity evidence is complete.

Exit gate: at least one complete stock-frame domain is modern-owned on both
backends, no visible light or surface is silently dropped, and rollback produces
the classic result.

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
