# Retained UI measurements and vector caching

8 September 2026. This checkpoint adds runtime CPU measurements and removes
redundant path compilation during fades and whole-pixel movement. It advances
the [replacement plan](../plans/idtech5-ui.md); full performance qualification,
composition, production artwork, editor and GUI migration gates remain open.

The later [composition checkpoint](composition.md) supersedes the per-primitive
opacity path described below: canonical fades now use isolated layers and reuse
both geometry and vertex tint. Measurements here remain the historical baseline.

## Cache behavior

Vector elements retain compiled, premultiplied path geometry independently of
opacity. A fade updates the submitted vertex colors without repeating curve
subdivision, stroke union, coverage integration or gradient subdivision.
Opacity still uses the existing per-primitive contract; isolated group fades
require the planned composition renderer.

Whole output pixels of translation are applied when submitting cached geometry.
Only the fractional translation phase participates in path compilation. The
viewport/scissor region is transformed into that coordinate space and expanded
to a 256-pixel grid for the cache key. Small movements can reuse the region;
crossing a cache-region boundary recompiles it. The render manager always clips
to the exact current viewport/scissor, including after movement or resize.

Changes to fractional position, scale, rotation, shear, layout dimensions or
the cached coverage region rebuild paths. Source reload clears both geometry
and tint caches. Density/video changes use the existing regeneration path.
This is a bounded cache per element, not an accumulating history of positions.

The native retained test compares animated cached geometry against a newly
loaded document at the same final position and opacity. It also checks live
clipping during translation, zero path compilation for opacity-only changes,
no geometry upload during whole-pixel movement, fractional-phase invalidation
and zero retained geometry allocations after shutdown.

## Tessellation precision regression

The moving 200% fixture exposed a single-precision tessellation failure at a
thin chamfered rail. A boundary returned two nominally coincident points at
`791.79998779296875` and `791.7999267578125`, creating a tiny extra loop. Its
calculated coverage at pixel `(791,0)` was `1.000010482367812`. The invariant
check rejected the path; static captures had not exercised that phase.

The pinned libtess2 subproject now uses a small
[double-precision patch](../../../subprojects/packagefiles/libtess2/double-precision.patch).
The patch changes `TESSreal` and the corresponding subnormal assertions; the
archive pin, allocator bounds and upstream license/credits are retained.
The compiler checks the expected scalar size, preventing a stale unpatched
header from silently using an incompatible ABI. No system libtess2 is used.

The boundary integrator also evaluates clamped linear trapezoids directly,
avoiding subtraction of nearly equal antiderivatives. The coverage invariant
remains intact. The failing rail is now exercised over ten fractional phases
with independent clipped-triangle area checks, and the 60-frame 200% motion
benchmark completes without path errors.

Meson applies the patch through its documented
[wrap diff mechanism](https://mesonbuild.com/Wrap-dependency-system-manual.html#diff-files).
Existing checkouts with an already-extracted unpatched dependency must refresh
that generated subproject before rebuilding:

```powershell
& tools/build/meson_setup.ps1 subprojects update --reset libtess2
```

This refresh replaces the generated libtess2 source directory. Keep any local
dependency changes as reviewed patches before refreshing it. Clean checkouts
apply the patch automatically when the pinned archive is extracted.

## Measurement interfaces

`Runtime::Statistics()` returns the most recent frame's CPU update/render time,
path compilation and geometry preparation time, compilation/cache/upload counts,
host draw counts and submitted vertex/index totals. Resident compiled-geometry
counts/bytes follow resource lifetime, including hidden elements. Vector cache
bytes describe the elements visited during that frame. These are tracked CPU
buffers, not total process memory, RmlUi's entire allocation graph or GPU memory.

The developer command `ui_retainedProfile <1..3600 frames>` records a bounded
sequence of rendered frames. Its final log line is JSON prefixed by
`Retained UI profile: `. Engine CPU timing includes retained evaluation,
submission and the surrounding GUI flushes. GPU execution, presentation and
the rest of gameplay are outside this measurement. Profile data does not change
the presentation clock or inject any input.

Without a preview document, the same command profiles the session-owned root
views (the title, pause, SYSTEM and loading screens) by submitted frame. Its
line is prefixed by `Retained UI root profile: ` and reports the views drawn per
frame, their summed retained CPU time, the wall interval from each frame's first
root draw to the next frame's, and their layer composites, backdrop passes and
backdrop fallbacks. A hidden window on Windows is capped at 60 fps, so its
frame interval measures CPU-bound frames, not GPU cost.

The capture harness accepts `--profile-frames N`, waits for the requested frames
before taking its engine screenshot and requires completed profile records.
With video restart, it checks both the original and recreated preview. The
report retains profile records alongside launch arguments and binary/log hashes.

## Native benchmark

`openq4-retained-ui-benchmark` loads the real vector fixture and uses the real
canonical evaluator, RmlUi layout and retained renderer. It exercises stationary,
opacity-only, whole-pixel and fractional translation scenarios, reporting JSON
lines with CPU percentiles, compilation/upload counts and tracked buffer peaks.
It uses synthetic font metrics and an empty draw host; it excludes the game's
font backend, engine vertex conversion, actual graphics APIs and GPU execution.
It is a diagnostic executable, not a timing threshold in the normal test suite.

```powershell
& tools/build/meson_setup.ps1 compile -C builddir openq4-retained-ui-benchmark
& builddir/openq4-retained-ui-benchmark.exe tools/ui/fixtures/vector-smoke.q4ui 1.25 60
& builddir/openq4-retained-ui-benchmark.exe tools/ui/fixtures/vector-smoke.q4ui 2 60
```

The baseline was `d096cc9f` with measurement instrumentation, before cache
changes, using the same fixture and scenarios. Its preserved benchmark binary
SHA-256 is `957e5c207fa893ec9f8bc83afc312d4f58bf5a93cdf24e67236b4385ef41b27d`.
Both comparisons use the existing Windows **debug, optimization 0** build.
They demonstrate work avoided in this fixture; they are not shipping-build
frame-rate results or cross-platform performance claims.

The 60-frame baseline compiled 780 paths during both opacity and whole-pixel
movement. The new cache compiles zero in either scenario. Opacity still rebuilds
780 tint submissions; whole-pixel movement also eliminates those uploads.
Fractional motion continues to compile the changing coverage. Additional cached
geometry increases tracked resident CPU memory and is included in the reports.

The final native benchmark binary SHA-256 is
`d5edcaa4dc38662c4d1307578b1013e027cdea4b11b034357e17b0828b94e200`.
Reports are under `.tmp/ui/ui-profile-baseline-{125,200}.jsonl` and
`.tmp/ui/ui-profile-final-{125,200}.jsonl`. Median CPU times in milliseconds:

| Scenario | 125% before | 125% after | 200% before | 200% after |
| --- | ---: | ---: | ---: | ---: |
| Stationary | 0.85 | 0.57 | 1.39 | 0.81 |
| Opacity | 15.96 | 1.13 | 20.34 | 1.52 |
| Whole-pixel movement | 14.96 | 0.62 | 16.14 | 0.88 |
| Fractional movement | 17.38 | 13.12 | 39.64 | 22.32 |

These are individual local runs and include measurement variability. The final
implementation also has more precise tessellation, which removes numerical
fragmentation as well as fixing the rejected rail. The stationary fixture's
tracked buffer peak rises from 407,696 to 807,224 bytes at 125%, and from
480,600 to 1,470,080 bytes at 200%. Cold first-frame evaluation remains about
24 ms and 36 ms respectively in this unoptimized benchmark. Fractional movement
and cold compilation therefore remain explicit optimization/qualification work.

All three native test targets pass after the precision patch. Client, dedicated
and both renderer modules were built/staged before the final gameplay profiles.

`--screen` runs a production root document as the session presents it:
`onInit`, the `open` timeline and a four-second settle, then timed 60 Hz frames
on its view-height canvas, with each declared CVar source at its type's default.
It reports frame, update, render and tessellation times, paths compiled, draw
calls, submitted vertices, and layers pushed and elided:

```powershell
& builddir/openq4-retained-ui-benchmark.exe --screen content/baseoq4/pak0/guis/menu/title.q4ui 240 1280 720
```

## Gameplay evidence

The final hidden, windowed runs used the registered engine screenshot command
after active SP/MP gameplay, with mouse/controller input disabled and no host
input injection. The unchanged vector fixture's frames, thin rails, gradients,
ring/hole, check stroke and localized labels were reviewed from render-target
TGA files. Each profile contains 180 actual rendered frames, including entry
motion. SP repeats the profile after full video restart and timeline replay.

| Profile under `.tmp/ui/performance/` | CPU p50 / p95 / max (ms) | Path compilations | Outcome |
| --- | --- | ---: | --- |
| `sp-gl-125-restart`, original preview | 1.47 / 2.10 / 28.31 | 57 | SP `airdefense1`, OpenGL, 125%; exit 0, no warnings/errors/retained diagnostics |
| Same run, recreated preview | 1.48 / 12.37 / 13.80 | 130 | Second 180-frame profile and timeline replay confirmed |
| `mp-vulkan-200` | 2.23 / 7.72 / 45.48 | 109 | MP `q4dm1`, Vulkan, 200%, auto-join; exit 0, no errors/retained diagnostics |

The high maxima and motion percentiles remain visible here because cold and
fractional-motion work are not yet fully qualified. These unoptimized CPU
profiles include GUI flushes but do not measure GPU completion or frame pacing.

Both runs used client SHA-256
`c3ce0146e2c7b1466210728450ca454d5ed1dfa039e93cb7493ea53b72ff0df3`,
fixture SHA-256
`78dc7bc86a65d6983ac23646f5534f205ec670bb85ade94cffecb4c8da921bbe`.
The final rebuilt OpenGL module SHA-256 is
`95ff9ca571526cc7668756552d91334093ecb472a1a227bba155962a5d354118`;
the Vulkan module SHA-256 is
`0c858cd74c859d9bacab961bd52cbcfbea1169bf7f5a1398a9d8936fc9707f41`.
SP screenshot SHA-256:
`68fd1b33c134e3d7d049d6bc696f7f24752a567e85fd92c2a4d5b612b12fb48a`.
MP screenshot SHA-256:
`120bd3c469282c0b23ce221f7f34f88556620633665bfa9a1de51f84180f8921`.

The MP log retains 93 existing content warnings; comparison with the preceding
MP capture found no new warning messages. The capture reports retain full
launch arguments, hashes, profile records and `replacement_acceptance: false`.
Windowed gameplay and these fixture screenshots do not qualify other platforms
or the complete 271-resource GUI corpus.

```powershell
$assets = 'C:\Program Files (x86)\Steam\steamapps\common\Quake 4'
python tools/ui/capture_legacy_baseline.py --assets $assets --mode sp --renderer gl --retained-document tools/ui/fixtures/vector-smoke.q4ui --density 1.25 --timeline enter --video-restart --profile-frames 180 --output .tmp/ui/performance/sp-gl-review
python tools/ui/capture_legacy_baseline.py --assets $assets --mode mp --renderer vulkan --retained-document tools/ui/fixtures/vector-smoke.q4ui --density 2 --timeline enter --profile-frames 180 --output .tmp/ui/performance/mp-vulkan-review
```

## Retained title screen CPU

1 October 2026. With `ui_retained 1` the title screen took 9.5 ms of retained
CPU a frame on a debugoptimized build, and 15-19 ms when the GPU fell behind
the hidden window's 60 fps cap: graphics calls inside the timed region then
block. The title is mostly static, so nearly all of that was work repeated
every frame. A temporary per-phase probe on the 1280x720 home screen split it:

| Work per frame (debugoptimized, 60 fps cap) | Before | After |
| --- | ---: | ---: |
| Path tessellation | 3.1 ms, 2 paths | 0.4 ms, 1 path |
| Host vertex conversion | 2.4 ms, 192,000 indices | none for unchanged meshes |
| Composition layers | 33 | 15 |
| Runtime update | 1.1 ms | 0.7 ms |

The changes keep the presentation:

- **Zero-opacity layers.** Any opacity below 1 is an isolated `filter`
  layer, and the hidden focus rails, hot markers and message lines sit at 0.
  A pushed layer now stays pending, recording its draws, until something
  needs its target: a nested layer, a mask snapshot, a backdrop or a visible
  composite. Composited at zero opacity, or into an empty clip, it is dropped
  without a target, a clear or a composite, which is exact: a premultiplied
  composite at zero opacity leaves its destination unchanged. The new
  `layerElisions` statistic counts them, and `layerPushes` now counts the
  layers that received a target.
- **Sub-pixel jitter.** The glint's rim turns back under its turning parent,
  so it stays put, but single-precision composition moves its sub-pixel phase
  by about 1e-4 px between frames, and the exact-match cache key compiled it
  almost every frame. A phase change below 1/1024 px now keeps the compiled
  mesh. Coverage moves no further than the phase, a quarter of one 8-bit step;
  every other key term must still match exactly.
- **Tessellation.** The coverage emitter extends rows through sorted vectors
  instead of a map per row, gradient stop planes clip only the cells they
  cross, and scratch buffers are reused. The turning wedge mask, which must be
  tessellated every frame, falls from 0.9 to 0.3 ms natively. A local harness compiled
  the committed tessellator beside this one and compared 264,984 cases, every
  path and mask of the six production screens and three fixtures across
  sizes, densities, sub-pixel phases, rotations, mirroring, bounds and both
  antialiasing modes: every mesh is byte-identical.
- **Retained meshes.** `RenderGeometry` keeps each geometry's transformed and
  clipped submission and reuses it while its translation, transform, clip,
  texture and multiply resolution are unchanged. Hosts receive a stable mesh
  identity and a revision through the optional `Host::DrawMesh` and
  `ReleaseMesh`, whose defaults draw immediately. The engine host converts
  a mesh once per revision, material and view size. Each chunk passes the
  contiguous vertex range its whole triangles use, so vertices are no longer
  duplicated per index, and the material is classified once per mesh rather
  than once per vertex.
- **Update.** `ApplyMotion` holds every authored property, nearly all static,
  and formatted all of them each frame. A value already applied under the same
  state revision and text ratio is now skipped; text and backdrop pairs are
  still reapplied. Input eligibility looked up each canonical node with its own
  search of the element tree, and now resolves all of them with one
  breadth-first walk in `GetElementById`'s order.

Each pair alternated a baseline client (origin/main `c5119c41`) with this
change on one machine (Windows 11, RTX 4060 Laptop GPU): a hidden 1280x720
window, the title at home for 4 s, then `ui_retainedProfile 120`. Medians of
`retained_cpu_p50_ms`, with the median frame interval:

| Build and renderer | Before | After |
| --- | ---: | ---: |
| debugoptimized, OpenGL, 60 fps cap | 9.59 ms | 3.59 ms |
| debugoptimized, OpenGL, uncapped | 9.43 ms | 3.51 ms |
| debugoptimized, Vulkan, 60 fps cap | 9.72 ms (19.8 ms frames) | 3.77 ms (16.7 ms frames) |
| release (`b_ndebug=true`), OpenGL, 60 fps cap | 10.21 ms | 3.89 ms |
| release, OpenGL, uncapped | 9.53 ms | 4.77 ms |
| release, Vulkan, 60 fps cap | 9.69 ms (20.1 ms frames) | 3.55 ms (16.7 ms frames) |

Baseline runs also fell into a slower, GPU-bound mode at times: Vulkan at
25-27 ms with 38 ms frames, OpenGL at 15.6 ms with 19.7 ms frames. The native
`--screen` benchmark of the title, with an empty draw host, falls from 5.3 to
1.0-1.3 ms on debugoptimized and 0.96 ms on release.

Engine captures of the title scenario in
`tools/ui/capture_retained_screens.py`, with reduced motion so that every frame
settles, are byte-identical before and after on OpenGL and Vulkan for the home,
depth, focus, exit-confirmation (over soft focus), and depart screenshots. The
hand-off to the stock settings page differs between two baseline runs as well,
because of its own animation. A motion-on capture shows the glint, rim and
montage unchanged.

What remains per frame: the engine's GUI path still copies every retained
vertex, into the GUI model and then into frame memory and the vertex cache at
each flush. That is about 2 ms for the title's 127,000 vertices, and static GPU
buffers for retained meshes would remove it. The wedge mask turns and is
tessellated every frame. Fifteen layers remain, for the 0.4-opacity rails and
markers and for the glint and its mask. A cold first frame takes 45-50 ms,
most of it stroke unions, the reticle grid alone 28 ms.

## Gameplay HUD state updates

The game writes HUD state every frame, and the multiplayer document carries
about 1,700 bindings with the Competitive preset. Each changed batch used to
copy the whole state, evaluate every binding and then reapply every bound
property. With dependency-tracked evaluation (see
[bindings](bindings.md#updates-ownership-and-lifetime)) a batch evaluates and
applies only what it reaches. The native benchmark
(`openq4-ui-retained-screens-test --hud-bench`, debugoptimized, one variable
changed per batch) on `remastered_mp.q4ui`:

| Document | State update | Frame after a change |
| --- | ---: | ---: |
| Remastered bindings, every binding evaluated | 0.71 ms | 3.1 ms |
| With Competitive, every binding evaluated | 1.53 ms | 5.3 ms |
| With Competitive, dependency-tracked | 0.01 ms | 0.38 ms |

## Gameplay instrument performance

The October 2026 gameplay investigation found that the Remastered ECG's
repeated core/glow vector strokes remained expensive after tessellation:
each moving frame still transformed and submitted their geometry. With the
user-approved bitmap exception, the ECG uses two adjacent copies of each
installed stock texture through the existing vector mask. Full texture periods,
Marine/Strogg rates, low-health tint and essential motion are preserved. No
extracted proprietary texture is added to the repository. The native Marine
HUD diagnostic drops from 64,272 vertices / 51 draws to 22,180 / 25, with warm
frame CPU time falling from 2.896 to 0.585 ms on this machine.

The radio graph's bar transforms used the full HUD canvas as their reference
box, moving the visible ink outside its mask. Correcting that revealed a
second cost: forty changing audio columns added about 1.4–1.6 ms per frame in
the native fixture; two multiplayer talker graphs added about 3.6 ms. The
approved fallback scrolls a fixed repeated waveform, keeps existing masks,
colours, speaker labels and show/hide events, and freezes under Reduced Motion.
These graphs indicate activity rather than measured loudness. Documents no
longer declare audio peak fields, so the HUD peer skips their unused history
reads and state writes. Warm preset graphs compile no vector paths and add
about 0.01 ms (Marine), 0.02 ms (Strogg), or 0.05 ms (both MP talkers) in the
same native diagnostic. These timings exclude GPU execution and use synthetic
fonts; the optional `--hud-audio-bench` mode records all phases.

The original four gameplay scenes were repeated on an i7-13700H / RTX 4060
Laptop GPU, NVIDIA 591.74, Windows, using the optimized staged client. Each
1280x720 window used MSAA 8, SMP off, VSync off and uncapped rendering, with
25 seconds warmup followed by alternating UI-off/on windows of 10 seconds.
Multiplayer used a solo listen server without bots and explicitly enabled
auto-join. Recorded camera poses match the original runs, allowing 0.125
world units for ground settling; Storage 2 excludes its moving-lift samples.

| Scene | UI on before (FPS) | UI on after (FPS) | Gain | UI off after (FPS) | UI-on P95 before / after |
| --- | ---: | ---: | ---: | ---: | ---: |
| Storage 2 / Vulkan | 86.5 | 216.2 | +150.1% | 170.8 | 19 / 6 ms |
| Air Defense 1 / Vulkan | 121.0 | 204.5 | +69.0% | 354.0 | 15 / 7 ms |
| Q4DM1 / Vulkan | 112.9 | 158.5 | +40.4% | 366.8 | 13 / 9 ms |
| Q4DM1 / OpenGL | 104.6 | 139.7 | +33.6% | 232.7 | 16 / 10 ms |

FPS values are medians of smoothed end-of-window snapshots, not averages
over each entire window. P95 uses the full sample histogram in millisecond
bins. CPU load varied substantially: the stock UI controls varied as well,
and a separate unchanged-baseline Air Defense repeat fell from 121 to 84.4
FPS. Treat gains as local observations. In particular, Storage 2's faster
UI-on result does not establish that retained UI is cheaper than stock UI.
Multiplayer retains measurable UI overhead after this instrument fix. The
comparison isolates the task's changes against the original working-tree
build; it is not a checkout comparison with the previous release.

Separate 300-frame retained CPU profiles fall from 2.7091 to 0.949 ms at p50
in Air Defense, from 4.623 to 1.602 ms in Vulkan Q4DM1, and from 3.25 to
1.4934 ms in OpenGL Q4DM1. Storage 2 measures 1.199 ms after the fix; its
original CPU profile is excluded because its setup changed after FPS sampling.

Qualification includes all three native retained UI/screen suites, generated
document consistency, SP decoder/playback-history checks, and engine render
target captures on OpenGL and Vulkan. Six moving graph pairs change pixels;
all six Reduced Motion pairs remain identical. In sixteen paired HUD captures,
all changed pixels are confined to the approved ECG area. Live microphone and
network voice were not exercised. Logs, runtime identities, per-window results,
captures and the method are retained in
`.tmp/ui-perf-fix-20261011/REPORT.md`; original UI on/off evidence is in
`.tmp/ui-ab-20261011/`.

## Initialization animation (11 October 2026)

The initialization view now advances between stage changes at cooperative
startup checkpoints: existing print/read progress offers, completed GUI loads
and menu-image preloads. A scoped main-thread owner ends before normal session
presentation, including on exception. The pump does no work on worker threads,
inside its own draw or during retained document loading/resource callbacks.
It never starts a rendering thread, sleeps, waits for animation to finish or
invents progress. The retained clock already measures monotonic elapsed time;
the defect was the lack of frames during synchronous initialization.

Redraws target at most 30 Hz, respect lower presentation caps, and reserve nine
times the previous completed frame cost for loading before another optional
frame. Slow drawing or a blocking present therefore reduces animation frequency.
The first cold compilation is paid once and does not poison the warm budget.
Stage changes still present immediately. Reduced Motion skips optional redraws
and preserves the existing still poses. Normal gameplay does not run this pump.

The dim rotating plate band, teeth and soft light sweep use the existing
`antialias: false` authoring option. Fill-only paths without analytic coverage
now retain local triangles across motion; the render manager applies their
current transform and clip. Curve precision uses a conservative output-scale
bucket, invalidated on density/size changes. Lettering, bright details, masks
and strokes retain their original output-space coverage. No raster assets are
introduced. Unchanged startup messages also avoid state publication and binding
updates on every animated frame.

Matched 120-frame native benchmarks in the existing Windows `debugoptimized`
preset, after the same 240-frame warmup:

| View | Previous CPU p50 / p95 | Updated CPU p50 / p95 | Updated paths compiled / uploads per median frame |
| --- | ---: | ---: | ---: |
| 1280 × 720 | 13.530 / 21.496 ms | 0.532 / 0.892 ms | 0 / 0 |
| 3840 × 2160 | 28.229 / 35.964 ms | 0.482 / 0.942 ms | 0 / 0 |

These measure retained CPU work with a native test host, not GPU time or total
game frame cost. An actual OpenGL screen profile measured 3.128 ms median
retained submission and a 16.661 ms median frame interval. Real startup runs
presented 14–19 frames, including 11–16 optional updates, with about 60–75 ms
total drawing time. Reduced Motion presented only the three remaining startup
stages (12.7 ms drawing in the measured Vulkan run). Work that has no safe
checkpoint still produces gaps: observed maxima were 329–412 ms. This is a
bounded improvement, not a guarantee of uninterrupted 30 fps or zero startup
cost on every device. Earlier startup, before the graphics device exists,
cannot show the initialization view.

Validation covers the native pacing and transform-cache checks, retained
screens through 4K, vector/mask tests, client and dedicated loading-pacifier
reentry tests, and generated-document consistency. Staged engine tests enter
Air Defense 1 in SP/OpenGL and Q4DM1 in MP/Vulkan with host input disabled.
Engine `screenshot` pairs change under animation on both renderers; Vulkan
Reduced Motion pairs are pixel-identical. There were no retained UI warnings
or runtime errors. The MP smoke still reports missing stock AAS variants and
late material/skin precaches; these are unrelated to initialization animation.
The native benchmark's existing JSON output also requires forward-slash input
paths on Windows because backslashes are emitted without JSON escaping.
Other platforms and visible-window GPU timings remain unqualified.

Logs, command lines, captures, runtime identities and measurements are retained
under `.tmp/initialization-animation/`. `ui_retainedTrace 1` now reports optional
`INITIALIZATION_UPDATE` frames and an `INITIALIZATION_PRESENTATION` summary of
frame count, total draw cost and largest gap without collecting host input.

## Legacy instruments and HUD submission (11 October 2026)

The radio and voice trace now samples the installed legacy waveform bitmap,
with the stock material's 21-interval vertical scale table and 0.6-width/second
horizontal scroll. It uses one stationary image quad with repeating UVs. The
ECG uses the same UV path for its installed Marine/Strogg textures. These are
runtime asset references; proprietary bitmap data is not included in Git.
Reduced Motion freezes the decorative radio/voice trace; the essential ECG
keeps its existing motion policy.

Icons already originate in editable SVG paths and become cached native meshes.
Loading those same shapes through another SVG renderer would not eliminate
tessellation, vertex submission or mask composition. Keep SVG as the authoring
format and reuse compiled geometry. Raster atlases remain a possible measured
tradeoff for especially expensive artwork, with density, memory and invalidation
costs; a blanket icon format replacement is not justified by current evidence.

A native isolation probe with full health/armor hides the two visible gauge
symbols while leaving the surrounding HUD intact. That removes three draws
and 1,672 Marine / 1,596 Strogg vertices; both versions compile zero paths per
warm frame. Three alternating repeats did not show a stable total CPU gain
beyond timing variation. This bounds these particular symbols' cost, not every
icon, GPU work, weapon selection, flashing or all density settings.

The runtime now reuses immutable mesh submissions and cached bounds. Stationary
masked vector panels can reuse their rendered pixels, with revisions, clips and
viewport changes invalidating the cache. Textured content is excluded from that
cache so time-varying materials cannot freeze. An additive image fully covering
one non-overlapping coverage mask can instead texture the mask's triangles
directly; other masks and alpha images retain the general compositor. Cached
target allowances account for every view sharing the host. Allocation pressure
can evict another view's idle cached pixels; references in the current frame
still prevent resizing their target. Native checks cover shared target pressure
and three 4K contexts sharing two cached targets while reserving scratch space.
Pending layers composite only their artwork's pixel bounds. The engine avoids redundant
target binds, preserving GUI batching and explicit backdrop flushes.

The adapter publishes changed application values while preserving atomic event
batches, case-insensitive state names, pending values and save/restore behavior.
Presentation-only documents skip unchanged layout work; controls, scrollbars,
modals and pointer interaction continue to update normally. CVar sampling visits
only CVar declarations, and animated UV changes avoid layout invalidation.
Empty focus no longer looks up the unnamed document and scans its canonical
tree. Hidden branches retain their current inline animation values without
invalidating visible layout; showing a branch resolves them before drawing.
The bundled RmlUi presentation update similarly defers `display:none`
descendants. This is an explicit opt-in limited to canonical presentation
documents; ordinary RmlUi, CSS animations and interactive widgets keep full
updates. The complete patch stack is checked against the original archive.

Qualification includes the complete native retained/screen/HUD suites and the
compiled production adapter tests, including failed publication retries and
pending state across events and saves. Native checks cover exact waveform
samples, wrapping, Reduced Motion restarts, masked holes/edges, opacity, resize,
partial-image fallback and cache invalidation. Gameplay timing and engine
render-target captures on OpenGL and Vulkan also qualify the installed bitmap:
six graph animation pairs change and six Reduced Motion graph regions remain
pixel-identical. The Air Defense 1 gauge capture preserves the centered layout
and the previous one-step 8-bit background-color tolerance against legacy.
Evidence is retained in `.tmp/ui-legacy-fidelity-perf/REPORT.md`; gameplay timing
is recorded there separately from these rendering checks.

The final Windows `debugoptimized` native benchmark reduced Marine frame work
from 0.644 to 0.147 ms, Strogg from 0.815 to 0.258 ms, and the MP overlay from
0.285 to 0.064 ms. These measurements exclude the engine and GPU; they describe
the combined working-tree candidate, including the concurrent transform-cache
improvement. A new SVG loader would not address the remaining engine cost.

The isolated, uncapped 1280×720 gameplay comparison used 8× MSAA, SMP disabled,
three ten-second windows per condition and one warmed process per scene:

| Scene / renderer | Legacy frame ms | New UI frame ms | Difference | New UI root CPU p50 ms |
| --- | ---: | ---: | ---: | ---: |
| Air Defense 1 SP / Vulkan | 2.98 | 3.58 | +0.60 | 0.380 |
| Q4DM1 MP / OpenGL | 3.80 | 5.05 | +1.25 | 0.474 |

Frame columns are medians of the three window means, not per-frame medians.
These results **do not meet negligible-overhead acceptance**. Root CPU excludes
later renderer/backend execution; the gap cannot be assigned wholly to icons
or layout. The final trace recorded zero SP layout updates per warmed frame
and 0.63 MP updates. Further profiling must separate remaining state/motion,
submission, backend and GPU work before choosing another rendering method.
The final OpenGL captures are 1280×720 and Vulkan captures are 2538×1312:
the requested 3840×2160 window was clamped, so this is not a 4K GPU qualification.
SP gameplay logged no warnings; MP retained the stock missing AAS variants,
`mp_buying_givecash` and late-precache warnings. Test-GUI documents also log
late precaches when loaded outside the level-loading path.

## Coverage atlases and presentation replay (11 October 2026)

The follow-up keeps the centered legacy gauge dimensions, saturation and installed
bitmap instruments. SVG-derived artwork still supplies the editable source. At
runtime, unchanged solid, whole-pixel coverage cells can share a nearest-filtered
RGBA8 atlas. This removes repeated vertex conversion/submission without changing
the cells' premultiplied colours or resampling their edges. Gradients, overlapping
cells, fractional coordinates, unsupported topology and oversized shapes keep
the original meshes. Overlap retains the GPU's original destination blending.
The atlas has at most 32 pages of 512×512 pixels (32 MiB each of CPU and GPU
storage). Live allocations never move; completely unused pages wait four host
frames before reuse. Rasterization also bounds visited-cell work. Named image
uploads preserve their sampler; the cinematic upload helper's linear/repeat
defaults must not replace the atlas's nearest/clamp sampling.

Canonical presentation documents record a bounded draw list after their layout
settles. Static meshes and valid cached layer composites replay directly. Bitmap
offset and scale values remain live: a scrolling ECG or radio waveform updates
the recorded sampling coordinates every frame. Geometry/style changes, document
replacement, resize, resource retirement and target eviction invalidate the
recording. Clipped/perspective image sampling, transient composition, backdrops,
live binding text and interactive documents keep the full rendering path. The
diagnostic CVars `ui_retainedAtlas` and `ui_retainedReplay` independently disable
these optimizations; neither is archived.

Hidden branches defer CSS writes while their canonical animation clocks continue.
Revealing a branch flushes the deferred values before layout. Fully transparent
canonical stacking contexts skip painting, and unchanged vector paint reuses its
last geometry and transform. Plain legacy text reuses glyph layout and batches
adjacent glyphs from the same material; live escapes still consult the host.

The GUI-root boundary now preserves batching when the target and actual viewport
are unchanged. The renderer API advances to 24 for the appended `BeginGuiRoot`
slot. Explicit `BindRenderTexture` calls retain their original flush/barrier
semantics, and cropped/editor/different-aspect paths retain required viewport
flushes. Stage matching renderer modules with the engine.

The implementation review also repairs same-frame legacy fallback when retained
rendering fails and reclaims obsolete, unleased target allocations under memory
pressure. Unuploaded atlas entries draw their original geometry until upload
succeeds. Isolated images and non-overlapping glyph runs apply opacity directly;
overlapping glyphs retain the compositor. CVar sampling allocates state changes
only for changed values, and the legacy HUD bridge caches normalization while
still publishing every application write. Its declared-key lookup now follows
the legacy dictionary's case-insensitive names. Native tests cover failed allocations, shared-view leases, resize,
hidden animation/reveal, replacement readbacks, static replay and live sampling.
The visible-window follow-up retains crosshair coverage quads through clipping
and replays unchanged captured HUD packets. Changed packets, failed preparation,
resizing and resource replacement invalidate that replay. Isolated opacity in
noninteractive presentations resolves only the affected element's filter.
Leaf text and images retain live opacity values in their recorded commands;
fades within (0,1) need neither style updates nor a document repaint. Disjoint
quads multiply their original premultiplied colours, avoiding cumulative rounding.
Overlapping text retains its completed composition target in the existing bounded
layer cache, after the same geometry has painted in an earlier host frame.
That admission delay allows a font's first GPU upload to finish; freezing its
first draw produced missing text on Vulkan. Images do not enter that text cache, so material animation stays
live. Zero/full-opacity transitions repaint, and changes to text, clipping,
geometry, resources or target leases invalidate reuse. Other isolated opacity
changes repaint without relaying out the document. Interactive/layout changes retain
the full update. Engine captures compare cached text against ordinary group
composition at six opacity levels with zero changed pixels on both OpenGL and
Vulkan. HUD geometry readbacks run after layout changes; hidden
animations retain resolved element handles. The legacy peer advances its
scripts, expressions and transitions through `RunTimeEvents`, with normal
layout restored immediately whenever Classic renders. Per-document CPU work is
now included in the root profiler.
Unchanged motion defaults avoid CSS formatting and use a hashed property index.
Forwarded HUD state also uses a transparent ASCII-case-insensitive hash lookup,
retaining the legacy dictionary's naming and event-write semantics.
Detailed runtime and per-draw stopwatches run only while `ui_retainedProfile`
requests timing. Ordinary gameplay retains work counters without querying the
clock for every draw. Native/tool hosts keep timing enabled by default, and the
runtime test verifies that disabling timing preserves the submitted geometry.
Prompt labels and glyphs refresh on binding/program writes, controller appearance
changes and resource/language reloads; older non-SDL input backends retain polling
because they do not publish keyboard-layout changes. The adapter tests cover
paired glyph updates, case-insensitive writes, deletion and controller changes.
Timing CSVs now append `frameBuildUs` and `frameSubmitUs`, splitting `cpuUs`
immediately before command submission. In serial mode submission includes backend
execution/presentation; in SMP mode it measures submission and waiting, not a
separate backend-thread interval.
Evidence and the final timing assessment belong in
`.tmp/ui-negligible-overhead/REPORT.md`. The earlier acceptance failure above is
historical evidence, not a claim that this follow-up has met the performance goal.

Final Windows captures show zero changed gauge pixels outside the live ECG
between the original meshes and optimized rendering, including MP over gray.
Decorative Strogg grain is held still for this comparison; unfrozen grain
changes between captures even with optimizations disabled. Radio/voice motion
and Reduced Motion pass on OpenGL/Vulkan for Marine, Strogg and MP.
The first atlas build removed 5,100 submitted vertices per MP frame and 3,240
per SP frame in the measured scenes. Earlier diagnostic medians were distorted
by shared 500 ms presentation stalls. A coordinated visible-window retry
removed those stalls and measured snapshot-average increases of 0.343 ms
SP/Vulkan and 0.570 ms MP/OpenGL. The logged `present` field was subsequently
confirmed to be a smoothed recent average, not a whole-window mean; earlier
whole-window labels were incorrect. These remain diagnostics, not acceptance.
`framePacingSnapshot` now also prints `Frame timing window` with sample count,
summed interval milliseconds and their arithmetic mean since `framePacingReset`.
The gameplay harness exports these separately as `pacingWindowSamples`,
`pacingWindowSpanMs` and `pacingWindowMeanMs`, without changing the live pacing
display or its existing fields. The old presentation stalls are no longer an
acceptance blocker. Final measured acceptance belongs in the evidence report.
The subsequent true-window checks still do not establish negligible overhead.
After the user paused background jobs, default-settings whole-window comparisons
add 0.049 ms in SP/Vulkan and 0.358 ms in MP/OpenGL. A further leaf-opacity
optimization avoids formatting CSS when the recorded filter already reads the
live value; the last installed CSS remains the comparison baseline for later
topology changes. Eight native suites pass. A sequential engine test revisits
an earlier alpha and crosses zero/full opacity without reloading the view:
all seven reference/optimized pairs match exactly on each graphics API (28
captures). This qualifies correctness, not a measured frame-time improvement.

The updated build's MP aggregate of +0.011 ms hides opposing ABBA block
differences (+0.364/-0.605 ms), so it is not accepted as negligible overhead.
Twelve shorter pairs add 0.683 ms, with all six blocks slower. Since switching
off destroys the retained HUD, a further check allows six seconds of settling
after each switch: MP still adds 0.218 ms (+4.93%), with block differences of
+0.035/+0.420 ms. No samples or stalls are discarded. The target remains open;
the user requested a pause after this validation pass. Test reservations are
released at the checkpoint. CPU frontend work remains the main investigation
direction; changing already-cached SVG icon sources does not remove that work.

## Remaining requirements

Fractional motion and cold compilation still need optimized-build profiling,
and long documents need resource/performance stress tests. Geometry preparation
for opacity, retained clipping and engine vertex conversion can be improved
further, and the engine GUI path still copies every retained vertex each
frame. GPU timings, frame pacing at the supported refresh rates, the complete
density/aspect matrix, all GUI families and other platforms remain unqualified.
Composition/masks, complete paints/strokes/fonts, the visual editor and all
current GUI replacements remain separate required work.
