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

## Status

| Phase | State | Blocked on |
| --- | --- | --- |
| 1 Domain classification and telemetry | **done** | — |
| 2 Light grid | classification done; implementation deferred | per-draw texture binding from Phase 3; near-zero value on stock content |
| 3 Interaction lighting | audited; classifier corrected | per-light image binding (bindless or atlas) — not shader math |
| 4 Fog, blend, ambient, shadow receivers | audited; blockers made precise | fog/blend/ambient models unimplemented; shadows ride on Phase 3 lights |
| 5 Core profile and promotion | sites pinned | Phases 3 and 4 |

No parity contract is proven. `MODERN_LIGHTING_PARITY_PROVEN_DOMAINS` is still 0,
ARB2 still owns every lit frame, and the default renderer is unchanged.

The through-line of phases 2–4: every readiness signal that had not been checked
against the shading model turned out to be optimistic. Blanket blockers hid
per-light detail, per-light detail hid a capability gap, and the capability gap
hid three unimplemented light models. Treat any unverified readiness number here
as an upper bound.

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
  interaction(pass=1 lights=3 consumable=0 blocked=3 unproven=0 ready=0)
  fogBlend(pass=0 lights=0 consumable=0 blocked=0 unproven=0)
  lightGrid(pass=1 draws=0 consumable=0 blocked=0 unproven=0 unprovenPass=0 ready=1)
  shadow(consumable=0 blocked=1 unproven=0 pointConstraint=0 ready=0)
  blocker='view=3 pass=depth material=4929
           resource=models/mapobjects/strogg/terminal/towers/network_towers
           reason=stageCondition'
```

What that says, none of which was observable before this track:

- All three contributing lights are **unrepresentable**, not merely unproven.
  Phase 1 read this line as `consumable=3 unproven=3` and concluded the
  interaction remainder was shading math. That was wrong: the classifier was
  checking that the light's images *existed*, while the clustered shading model
  never samples them. Phase 3 corrected it. The remainder is plumbing —
  per-light image binding — and only then shading math.
- The light-grid domain is **vacuously ready**: the pass exists but carries no
  draws, because stock maps have no baked grids. It is not what is holding this
  frame, and proving it would change nothing here.
- One shadow light is genuinely unrepresentable, so Phase 4 has real per-light
  work behind it rather than a blanket presence gate.
- The first blocker is a specific actionable defect naming a real material,
  rather than `lighting-parity-incomplete`. Note it changed once light grid
  stopped falsely claiming the blocker slot — accurate domain verdicts make the
  reported blocker more useful, not just shorter.

The lesson worth keeping: each phase found that the previous phase's readiness
signal was optimistic, and each correction moved a domain from "nearly ready" to
"needs real work". Treat any *unverified* readiness number in this track as an
upper bound.

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

**This phase is blocked on an architectural gap, not on shader math.** It was
scoped as "port the `interaction.vfp` math from the Vulkan Phase F1 shader".
That port is necessary but nowhere near sufficient, and it cannot be written
against the current clustered light model at all.

A Quake 4 light is **textured**. Its shape and colour come from a light
projection image and its distance response from a falloff image, both sampled
projectively per pixel:

```glsl
light *= textureProj(lightFalloffMap, vLightFalloffTexCoord).rgb;
light *= textureProj(lightProjectionMap, vLightProjectionTexCoord).rgb;
```

The modern clustered model has neither. `ModernClusterLightRecord` carries
`positionRadius`, `worldOriginRadius`, `colorType`, `scissorDepth`, `flags`,
`depthRange`, `falloff`, and the `projectS/T/Q` planes — **no image handles**.
The CPU-side `rendererModernLightDescriptor_t` does record
`falloffImageHandle`, `projectionImageHandle` and `cubeImageHandle`, but nothing
in the executor ever binds them; the only references anywhere are the ownership
classifier's presence checks. `ModernClusterEvaluateLight` therefore substitutes:

```glsl
float radial = clamp(1.0 - dist / radius, 0.0, 1.0); radial *= radial;
float spec   = pow(clamp(dot(normal, halfDir), 0.0, 1.0), 24.0) * specular * fresnel;
attenuation  = radial * (ndotl + spec) * projectMask;   // projectMask is binary
```

That is a generic clustered-lighting model, not Quake 4's. Beyond the missing
images it also uses `pow(N·H, 24)` where Quake 4 uses the `_specularTable` ramp
`clamp((N·H)·4−3)²` with the specular env constant doubled.

The root problem is structural: **a clustered loop samples many lights per
pixel, but each Quake 4 light needs its own two textures.** A single bound
texture pair cannot serve N lights. The viable implementations are:

1. **Bindless light images** on GL 4.5+ — store the falloff/projection handles in
   the light record. `r_rendererBindless` and the capability plumbing already
   exist. Does not cover the GL 3.3/4.1 baseline tiers.
2. **A shared falloff/projection atlas** — pack the images and store atlas rects
   per record. Works on every tier; needs residency management and bleed control
   at tile edges, much like the shadow atlas already does.

The deferred path has two further losses that the G-buffer layout makes
unrecoverable, independent of the above: vertex colour is never written (so
`r_interactionColorMode`'s two layouts cannot be reproduced), and specular is
collapsed to a scalar `dot(specular, vec3(0.333333))`, discarding specular
colour. Forward+ retains both because it draws the surface, so **forward+ is the
only credible route to interaction parity** and the deferred resolve should be
scoped out of it.

Landed in this phase:

- [x] `MODERN_CLUSTER_LIGHT_RECORD_CARRIES_IMAGES`, a reviewed capability
      constant recording that the light record cannot reference its images, and
      the blocker reason `per-light-images-unbindable`.
- [x] Corrected the Phase 1 classifier, which reported a light "consumable"
      whenever its image handles were merely *present*. The shading model never
      samples them, so that overstated readiness — the exact failure the parity
      contract exists to prevent, occurring inside the classifier itself.

Remaining, in dependency order:

- [ ] Choose bindless vs atlas for per-light images, and build it.
- [ ] Port the `interaction.vfp` math from `Vulkan/shaders/interaction.frag`:
      DXT5/RXGB bump decode without renormalization, the real `_specularTable`
      ramp with the doubled specular env constant, ambient constant tangent
      direction with CPU-side 8-bit cube quantization, and Parallaxbump height in
      the RXGB red channel.
- [ ] Handle both `r_interactionColorMode` vertex-colour layouts (forward+ only).
- [ ] A/B forward+ against ARB2 per light type.
- [ ] Flip the `interaction` contract to proven with recorded evidence.

## Phase 4: Fog, Blend, Ambient, And Shadow Receivers

The two halves of this phase are in very different states, and one extra gap
turned up that belongs to the interaction domain rather than this one.

**Fog and blend are not implemented.** `MODERN_GL_SHADER_FOG_BLEND` is literally
the same source as `MODERN_GL_SHADER_TRANSPARENT_FORWARD` — a clustered
transparent-forward shader whose light loop accumulates only
`type == 0 || type == 1`. Fog (type 2) and blend (type 4) lights contribute
nothing whatsoever. That is not an approximation to tighten: Quake 4 fog is a
distance-driven projective fog-image lookup with its own blend state, and blend
lights modulate the framebuffer through the light's blend image. Neither is
expressible as "a clustered light contribution added to a transparent surface".

**Ambient lights are also unevaluated**, and this one belongs to `interaction`,
not `fog-blend`. `ModernClusterEvaluateLight` zeroes attenuation for every type
except point and projected, so an ambient light contributes nothing. Quake 4
ambient lights are real lights evaluated against the constant tangent-space
direction the ambient normal cube decodes to — the Vulkan Phase F1 shader
handles this explicitly.

**Shadow receivers are genuinely implemented**, and this is the one place the
modern path is further along than the classifier suggested.
`ModernClusterShadowVisibility` fetches the shadow descriptor, validates policy,
freshness against `uModernShadowContractState`, atlas readiness, the
receiver-blocked and sampling-ready flags, and then samples the real point or
projected atlas. That matches the shadow-mapping track being complete through
5d/6. The shadow domain's remaining blocker is not its own sampling — it is that
shadows ride on lights, and the lights are blocked upstream.

Landed in this phase:

- [x] Precise per-type blockers instead of one collapsed reason:
      `fog-blend-model-unimplemented` for fog and blend lights,
      `ambient-light-unevaluated` for ambient lights, `per-light-images-unbindable`
      only for point/projected lights that are otherwise complete.
- [x] Confirmed the shadow receiver path samples the real atlas, so no shadow
      capability constant is warranted.

Remaining:

- [ ] Implement the Quake 4 fog model (projective fog image, own blend state) as
      its own pass rather than a clustered light contribution.
- [ ] Implement blend lights as a framebuffer-modulating pass.
- [ ] Evaluate ambient lights against the constant tangent-space direction.
- [ ] Keep stencil-shadow lights permanently on the per-light legacy fallback.
- [ ] Flip the `fog-blend` and `shadow` contracts to proven with evidence, after
      Phase 3 unblocks the lights the shadows ride on.

## Phase 5: Core Profile And Promotion

Fully gated on Phases 3 and 4: none of it can land while the modern path cannot
own a lit frame. The exact sites are identified so the change is mechanical when
the gate opens.

- [ ] `gl_ContextSDL3.cpp:202` — `keepAutoCompatibility` is hardcoded
      `preference == RENDERER_TIER_PREF_AUTO`, which is what forces a
      compatibility profile on the default path. It must become conditional on
      the modern path being able to own the frame. The shared ladder already has
      a modern-auto mode and it is covered by `rendererContextLadderSelfTest`, so
      this half is prepared.
- [ ] `RendererCaps.cpp:450` — `compatibilityOnly` follows from the same flag.
- [ ] `RenderSystem_init.cpp:1800` — `R_ErrorForMissingRequiredOpenGLFeatures`
      must become fatal only when the modern path is *also* unavailable, instead
      of whenever `glConfig.allowARB2Path` is false.
- [ ] `draw_arb2.cpp:12468` — `R_ARB2_Init` requires
      `hasFixedFunctionCompatibility`; the bridge stays optional rather than
      required.
- [ ] Satisfy the Phase 8 evidence token honestly and revisit
      `r_rendererModernAutoPromote`.

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
