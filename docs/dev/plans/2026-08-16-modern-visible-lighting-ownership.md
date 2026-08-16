# Modern Visible Lighting Ownership

## Purpose

Replace the four blanket pass-presence blockers that currently prevent the modern
GL visible path from owning any lit frame with per-light, per-domain
classification backed by an explicit parity contract.

This does not change what the default renderer draws. It changes an unmeasurable
boolean ("this frame has interaction draws, therefore blocked") into a measurable
ladder ("this frame has 37 contributing lights, 34 are modern-consumable, 3 are
genuinely unrepresentable, and the lighting domain's parity contract is still
unproven").

## Current Baseline

`R_ModernGLExecutor_AnalyzeModernVisibleOwnershipReadiness` runs twice per frame
from `R_ModernGLExecutor_PrepareFrame`, at a point **before**
`R_ModernShadowPlanner_PrepareFrame` and
`R_ModernClusteredLighting_PrepareFrame` have built this frame's descriptors. It
therefore has nothing to consult but front-end packet draw counts, and blocks on
raw presence:

| Trigger | Flag cleared | Blocker |
| --- | --- | --- |
| any `RENDER_PASS_ARB2_INTERACTION` draws | `modernVisibleLightingReady` | `lighting-parity-incomplete` |
| any `RENDER_PASS_FOG_BLEND` draws | `modernVisibleLightingReady` | `fog-blend-parity-incomplete` |
| any `RENDER_PASS_LIGHT_GRID` draws | `modernVisibleLightGridReady` | `light-grid-parity-incomplete` |
| any `RENDER_PASS_SHADOW_MAP` / `RENDER_PASS_STENCIL_SHADOW` draws | `modernVisibleShadowOwnershipReady` | `shadow-ownership-parity-incomplete` |

Every lit Quake 4 surface emits interaction draws, so `modernVisibleBlockedByLegacy`
trips on every real frame and `R_ModernGLExecutor_FailClosedPassOwnership` returns
the whole frame to ARB2. Nothing ever restores those flags.

A second consequence is that the phase 5c **per-light** shadow gate,
`R_ModernGLExecutor_ModernVisibleShadowReceiversReady`, is already implemented and
already correct — walking descriptors, separating consumable from blocked lights,
handling the single-cube point-light constraint — but is dead code. Its verdict is
ANDed with the blanket `modernVisibleShadowOwnershipReady` at
`ModernGLExecutor.cpp:5990`, which is always false whenever any shadow draw
exists, so the per-light result can never influence the outcome.

## Design Goals

- [ ] Keep the default visible renderer on ARB2, with byte-identical behavior.
- [ ] Decide each lighting domain from descriptors, after the planners have run.
- [ ] Never let a domain be owned because it is merely *representable*; require an
      explicit, reviewed parity contract to have been proven for it.
- [ ] Report, per frame, how many lights are consumable, how many are blocked, and
      how many are blocked *only* by an unproven contract.
- [ ] Resurrect the phase 5c per-light shadow gate as the sole shadow verdict.
- [ ] Give bring-up a way to force a domain on so parity can actually be measured.

## Non-Goals

- [ ] Do not change lighting math in this phase. Proving parity is separate work.
- [ ] Do not flip any domain's parity contract to proven in this phase.
- [ ] Do not remove the ARB2 escape, the evidence token, or the promotion gates.
- [ ] Do not alter the front-end packet contract or descriptor layouts.

## Parity Contract Model

Each ownership domain carries a contract state:

- `unproven` — the modern path may be able to represent these lights, but the
  output has not been shown equal to ARB2. Lights are counted, then blocked.
- `proven` — parity has been demonstrated and recorded. Consumable lights count
  toward ownership; unrepresentable lights still block.

Contract state lives in a code-level table so flipping it is a reviewed source
change, not a user-facing switch. `r_rendererModernLightingParityOverride` is a
diagnostic bitmask (default `0`, not archived) that forces domains proven for
bring-up and A/B capture only.

Domains:

| Bit | Domain | Covers |
| --- | --- | --- |
| 1 | `interaction` | point / projected / ambient light interactions |
| 2 | `fog-blend` | fog and blend lights |
| 4 | `light-grid` | light-grid indirect contribution |
| 8 | `shadow` | shadow receiver sampling |

## Light Classification

A contributing light is **consumable** when its clustered-light descriptor is one
the modern deferred/forward+ path can represent:

- type is `POINT`, `PROJECTED`, or `AMBIENT` for the `interaction` domain
- type is `FOG` or `BLEND` for the `fog-blend` domain
- required image handles are live (falloff, and projection for projected lights)
- the light's shadow descriptor is either absent or consumable per the phase 5c
  shadow walk

It is **blocked** when it is genuinely unrepresentable — `SPECIAL` (including
parallel lights), missing falloff/projection resources, or a shadow descriptor
that would silently drop shadows.

It is **unproven** when it is otherwise consumable but its domain's contract has
not been proven. This is the number that measures remaining parity work.

## Phase 1: Domain Classification And Telemetry

Goal: replace presence blockers with descriptor-driven verdicts, default behavior
unchanged.

- [x] Add the parity-contract table and `r_rendererModernLightingParity`.
- [x] Move the four domain verdicts out of the early packet-count analysis into a
      late evaluation that runs after both planners.
- [x] Keep the early analysis owning per-draw material/geometry blockers only.
- [x] Classify interaction lights per descriptor into consumable/blocked/unproven.
- [x] Classify fog and blend lights per descriptor.
- [x] Classify light-grid contribution per pass with an explicit reason.
- [x] Drop the blanket shadow flag and use the phase 5c per-light verdict.
- [x] Record per-domain counts and a specific named blocker naming the light.
- [x] Report contract state and counts in `gfxInfo` and `r_rendererMetrics 2`.
- [x] Extend `rendererModernCompatibilitySelfTest` to cover the default
      all-unproven verdict and per-domain contract independence.
- [x] Guard the new cvar in `rendererDefaultSafetySelfTest`.
- [x] Acceptance: default startup and gameplay are unchanged, `gfxInfo` reports
      per-domain light counts, and forcing a domain proven changes only that
      domain's verdict.

Note on the diagnostic cvar name: shipped as `r_rendererModernLightingParity`
rather than the `...Override` working name, matching the existing
`r_renderer*` cvar naming.

## Measured Baseline

Real-scene reading on `game/storage1` with
`r_rendererModernExecutor 1 r_rendererModernVisible 1`, after Phases 1 and 2:

```
modernLightingOwnership proven=none override=0 requested=1
  interaction(pass=1 lights=3 consumable=3 blocked=0 unproven=3 ready=0)
  fogBlend(pass=0 lights=0 consumable=0 blocked=0 unproven=0)
  lightGrid(pass=1 draws=0 consumable=0 blocked=0 unproven=0 unprovenPass=0 ready=1)
  shadow(consumable=0 blocked=1 unproven=0 pointConstraint=0 ready=0)
  blocker='view=3 pass=depth material=4929
           resource=models/mapobjects/strogg/terminal/towers/network_towers
           reason=stageCondition'
```

What that says, none of which was observable before this track:

- All three contributing lights are already representable by the modern
  descriptor set. Nothing about their kind or resources blocks ownership — only
  the unproven interaction contract does. Phase 3 is shading math, not plumbing.
- The light-grid domain is **vacuously ready**: the pass exists but carries no
  draws, because stock maps have no baked grids. It is not what is holding this
  frame, and proving it would change nothing here.
- One shadow light is genuinely unrepresentable, so Phase 4 has real per-light
  work behind it rather than a blanket presence gate.
- The first blocker is a specific actionable defect naming a real material,
  rather than `lighting-parity-incomplete`. Note it changed once light grid
  stopped falsely claiming the blocker slot — accurate domain verdicts make the
  reported blocker more useful, not just shorter.

Re-measure this line on the same map after each contract flip. Take it from the
`gfxInfo` dump of a default-paced run: `r_rendererMetrics 2` prints the same
line per frame but costs roughly an order of magnitude of frame time.

## Phase 2: Light Grid Ownership

**The original premise for this phase was wrong and is corrected here.** It was
scoped as "the cheapest domain — no shadow coupling, no per-light math, just
reconcile the two implementations". There is nothing to reconcile, and it is not
the cheapest domain.

What the investigation actually found:

- The modern light-grid fragment shader is a flat debug colour
  (`out_Color = uDebugColor.rgb * lightScale`). There is no light-grid
  implementation in the modern path at all.
- The graph's `lightGrid` resource is an *imported placeholder buffer*, not the
  three atlas textures. `forwardPlusLightGridContributions` is a presence flag
  (`handle != NULL ? 1 : 0`), not a sample count.
- The deferred resolve reads `uGBufferEmissive` and calls the result
  `lightGrid`. That attachment carries material emissive, not baked probe
  irradiance — the two are unrelated quantities.
- `lightGridModern` is hardcoded `false` in `FinalizePassOwnership`, so the pass
  could never be modern-owned even with a proven contract.
- The ARB2 implementation is substantial: 16 backend helpers, 24 uniform vec4
  blocks, 6 texture bindings, octahedral probe-atlas sampling, 8-corner
  trilinear probe blending, Chebyshev/VSM visibility moments, probe relocation,
  gamma decode, contribution clamping, and depth rejection.

The decisive constraint is architectural. **Quake 4's light grid is baked per
portal area**, selected by the receiving surface, with portal blending that
resubmits a surface once per contributing area grid. A screen-space deferred
resolve cannot pick the correct per-area atlas per pixel. So the modern
implementation must remain a *forward, per-surface* pass that binds its area's
atlas set per draw — a different shape from the clustered and screen-space
resolves the rest of the modern pipeline is built around.

That makes light grid the most architecturally awkward domain, not the cheapest.
It is re-sequenced after interaction lighting: Phase 3's forward+ per-surface
work establishes the per-draw texture-binding path this needs.

A second finding makes the re-sequencing decisive. Measured on `game/storage1`
over 75 gameplay frames, every frame reports:

```
lightGrid(pass=1 draws=0 consumable=0 blocked=0 unproven=0 ready=1)
```

The pass exists but describes **zero draws**. `R_ScenePackets_AddFilteredDrawSurfPass`
declares the light-grid pass unconditionally and only then filters eligible
surfaces into it, and eligibility requires a usable baked area grid. Stock Quake
4 maps ship no irradiance volumes, so on retail content the light-grid domain
has no work at all. Proving its parity buys nothing for stock assets; it matters
only for content with openQ4-generated grids.

Landed in this phase:

- [x] Per-draw light-grid ownership classification, replacing the blanket
      pass-presence check, via `RB_LightGridSurfaceModernRepresentable`.
- [x] Blocker reasons: `no-area-light-grid`, `light-grid-atlas-missing`,
      `portal-blend-multi-grid`.
- [x] A declared-but-empty light-grid pass is correctly vacuous rather than
      blocking. An earlier revision of this work blocked it as
      `light-grid-draws-undescribed`, which would have pinned every stock-content
      frame on a pass carrying no work.
- [x] Per-draw counts reported in `modernLightingOwnership`.
- [x] Self-test coverage for the draw classification and for per-domain contract
      resolution.

Deferred to the re-sequenced phase, and explicitly **not** done:

- [ ] Port the ARB2 light-grid algorithm into the modern shader library.
- [ ] Import the three per-area atlases as real graph resources.
- [ ] Extend the modern submit path with per-draw atlas binding and the 24
      uniform blocks.
- [ ] Reproduce or explicitly drop portal blending.
- [ ] Flip the `light-grid` contract to proven with recorded evidence.

The contract stays unproven. Nothing about the visible frame changed.

## Phase 3: Interaction Lighting Parity

Goal: the real work. Reproduce Quake 4 interaction lighting in the modern path.

- [ ] Port the documented `interaction.vfp` parity math from the Vulkan Phase F1
      shader (`src/renderer/Vulkan/shaders/interaction.frag`): DXT5/RXGB bump
      decode without renormalization, the real `_specularTable` ramp including the
      doubled specular env constant, ambient constant tangent direction with
      CPU-side 8-bit cube quantization, and Parallaxbump height in the RXGB red
      channel.
- [ ] Handle both `r_interactionColorMode` vertex-color layouts.
- [ ] A/B the deferred and forward+ paths against ARB2 per light type.
- [ ] Flip the `interaction` contract to proven with recorded evidence.

## Phase 4: Fog, Blend, And Shadow Receivers

- [ ] Fog and blend light parity against the ARB2 fog/blend pass.
- [ ] Modern shadow receiver sampling parity for projected and point lights.
- [ ] Keep stencil-shadow lights permanently on the per-light legacy fallback.
- [ ] Flip the `fog-blend` and `shadow` contracts to proven with evidence.

## Phase 5: Core Profile And Promotion

- [ ] Stop preferring compatibility profiles in the context ladder once the modern
      path can own a lit frame.
- [ ] Make `R_ErrorForMissingRequiredOpenGLFeatures` fatal only when the modern
      path is also unavailable.
- [ ] Satisfy the Phase 8 evidence token honestly and revisit
      `r_rendererModernAutoPromote`.

## Validation

```powershell
python tools\tests\renderer_validation_matrix.py
```

Self-tests touched by Phase 1: `rendererModernCompatibilitySelfTest`,
`rendererModernVisibleSelfTest`, `rendererClusterGridSelfTest`,
`rendererShadowPlannerSelfTest`, `rendererDefaultSafetySelfTest`.

Ownership telemetry in a real scene:

```powershell
python tools\tests\renderer_gameplay_benchmark.py --profile smoke `
  --set-cvar r_rendererModernExecutor=1 `
  --set-cvar r_rendererModernVisible=1 `
  --set-cvar r_rendererMetrics=2
```

Note that `r_rendererMetrics 2` prints the full executor block every frame and
costs roughly an order of magnitude of frame time — the smoke case above fails
its p95 budget on that alone. It is fine for reading `modernLightingOwnership`,
but never quote a frame time from a run that has it enabled. Phase 2 and 3
perf comparisons must use the harness defaults and read the ownership line from
the `gfxInfo` dump instead, which prints the same line once.

`renderer-gpu-driven-selftest` fails on main independently of this track
(`indirect multi-draw execution mismatch`), verified against a clean tree at
f14ea1a8. It is tracked separately and is not a regression from this work.

Two failure modes here are environmental, not real, and both mimic a regression
convincingly enough to waste an hour:

- A second session building in the shared `builddir`/`.install` takes the meson
  lock and can make a concurrently launched gameplay capture die with exit -1
  mid-load and no logged error. Wait the lock out rather than killing the other
  build, and always re-run a one-off gameplay crash before believing it.
- A self-test case that fails in under ~2s never started its client; that is
  launch contention with a previous run's shutdown, not a test result.
