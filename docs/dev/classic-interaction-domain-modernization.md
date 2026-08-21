# Shared Classic Interaction-Lighting Domain

## Status

The third Milestone D classic-frame domain is implemented as an experimental,
default-off path for complete eligible unshadowed fixed-classic interaction
views. `r_rendererSharedWorldInteraction 1` asks OpenGL and Vulkan to consume
the same sealed light, receiver, and primitive stream. The established
interaction renderer remains the supported default and the whole-view rollback.

This is a complete interaction-phase ownership boundary for an accepted view,
not a claim that the complete frame or every Quake 4 lighting feature is shared.
Any shadow requirement, custom lighting program, unsupported surface, resource
failure, or backend-preflight failure keeps every interaction in the view on the
untouched classic path. Ambient/material, fog/blend, GUI, subview, and post work
retain their existing owners.

The implementation is original openQ4 work. It incorporates no external source
code and changes no stock Quake 4 asset.

## Why the boundary is whole-view

An idTech 4 interaction is not one material draw. Each contributing light
multiplies its active light stages by an ordered bump/diffuse/specular surface
decomposition. One surface can occur under multiple lights and in local,
global, or translucent receiver chains. A partial handoff can therefore draw a
receiver twice, omit a contribution, or apply a different depth/shadow rule.

The shared domain accepts or rejects the entire interaction contribution of one
ordinary root 3D view:

- every non-fog/non-blend light with material interactions;
- every local, global, and translucent receiver in classic order;
- every active light stage and decomposed surface interaction;
- every draw and no-op disposition; and
- exact light, surface, primitive, resource, and stable-hash accounting.

If any part cannot be represented, no shared record from that view is published
and neither backend submits a shared interaction draw.

## Packet identity and sealed records

The interaction pass no longer relies on an ambiguous flat surface list.
Interaction draw packets carry the source `viewLight_t`, light ordinal,
receiver class, receiver ordinal, and global interaction source ordinal.
Preparation verifies those identities while walking the original light and
receiver chains. This distinguishes the same surface under two lights and
proves that no receiver was reordered or silently dropped.

`ClassicInteractionDomain` prepares bounded frame-local arenas for:

- views, including packet/pass identity, expected ranges, computed light scale,
  readiness, fallback detail, and per-backend outcomes;
- lights, including source order, ambient classification, shadow disposition,
  receiver spans, primitive spans, and a stable hash;
- receiver surfaces, including packet/material/geometry/instance identity,
  local/global/translucent class, scissor/state data, and a primitive span; and
- interaction primitives, including the five authored/intrinsic images,
  projection planes, texture matrices, local light/view origins, evaluated
  diffuse/specular colors, vertex-color mode, ambient flag, and disposition.

The retained legacy draw-surface pointer is a sealed geometry bridge only.
Backend consumers do not reinterpret authored stages or reread shader
registers after the transaction is published.

## Exact classic decomposition

Preparation mirrors the fixed-classic interaction rules once for both
backends:

- light-stage order and local, global, then translucent receiver order;
- condition-register handling with checked finite register access;
- bump-stage flushes, repeated diffuse/specular flushes, and the final flush;
- surface-color clamping and light-color multiplication using the view's
  classic light scale;
- texture-translation wrapping outside the classic `+/-40` range;
- missing or skipped diffuse/specular replacement with the black image;
- missing or skipped bump replacement with the flat normal image;
- ambient-light specular suppression and ambient normal-cube selection;
- `IGNORE`, `MODULATE`, and `INVERSE_MODULATE` vertex-color semantics; and
- explicit no-op records where an authored combination produces no visible
  diffuse or specular contribution.

No-op work is counted. It is never used as a reason to publish a clipped view.

## Eligibility and whole-view fallback

The initial shared corridor admits ordinary root world views containing static
fixed-classic receiver geometry and unshadowed point, projected, parallel, or
ambient lights. Opaque local/global receivers retain depth-equal submission;
translucent receivers retain the classic less-or-equal depth rule. Scissor,
culling, and represented polygon-offset state remain part of the sealed plan.

Preparation rejects the complete view for any of these conditions:

- effective stencil-shadow or shadow-map caster/receiver work;
- fog/blend lights inside the interaction pass;
- custom/new-style GLSL lighting, parallax, enhanced-material, cel, flat-
  diffuse, simple/test, or other alternate interaction modes;
- deformed, skinned, packed-MD5R, primitive-batch, missing-cache, depth-hack,
  negative-scale, or otherwise unsupported receiver geometry;
- subview, mirror, x-ray, editor, offscreen, render-demo, global-material, or
  other non-root view ownership;
- cinematic, dynamic, defaulted, unloaded, unbound, or invalid image resources;
- out-of-range/non-finite registers or derived values;
- packet/light/receiver/material/geometry/instance identity mismatch;
- bounded arena overflow or an incomplete interaction pass; or
- any backend program, descriptor, geometry, or transient-capacity preflight
  failure.

Fallback is decided before the first shared framebuffer write. After the first
owned draw, an unexpected mismatch is recorded as a diagnostic and the classic
path is not layered over partially submitted shared work.

## Backend execution

OpenGL performs complete-view preflight inside the ARB2 interaction slot. It
checks the required programs, texture-unit contract, intrinsic and authored
images, vertex/index caches, and sealed geometry before drawing. Accepted
records use the existing ARB2 interaction math and state conventions; rejected
views continue through the unchanged classic interaction walker.

Vulkan performs the equivalent preflight before selecting interaction
ownership. It checks pipelines/layouts, all required image descriptors,
light-triangle geometry, scissors, and uniform-ring capacity for the complete
sealed stream. Accepted records use the existing Vulkan interaction pipeline.
Rejected views continue through `VK_Interactions_DrawLights`, including its
established shadow and compatibility behavior.

The option also disables aggregate modern-visible interaction/shadow skipping
for the view so an older experimental owner cannot suppress or duplicate the
transactional rollback.

## Controls and diagnostics

- `r_rendererSharedWorldInteraction 0|1` controls the domain and defaults to
  `0`.
- `rendererClassicInteractionDomainSelfTest` exercises the backend-neutral
  transaction and coverage contract.
- `gfxInfo` reports frame readiness, accepted/fallback view counts, light,
  receiver, primitive/no-op counts, stable hashes, named failure details, and
  OpenGL/Vulkan ownership outcomes.

The setting participates in renderer default-safety reporting. Unrelated stock
baseline, gameplay benchmark, and startup profiles force it off. The dedicated
`interaction` gameplay-benchmark profile supplies a bordered 1280x720,
windowed, stock-assets-only, fixed-camera, shadows-off capture corridor.

## Validation contract

Automated coverage includes:

- multi-light packet identity and all three receiver classes;
- light/surface stage ordering, inactive stages, repeated-stage flushes, and
  black/no-op classification;
- checked register math, translation wrapping, colors, vertex modes, and stable
  hashes;
- explicit shadow/custom/deform/resource blockers and atomic arena rewind;
- duplicate, mismatched, and incomplete backend coverage rejection;
- complete backend preflight before ownership, with the classic fallback kept
  intact; and
- default-off isolation in renderer bootstrap, baseline, benchmark, validation,
  and CI entry points.

Runtime qualification uses only the engine's registered `screenshot` command.
For each backend, the controlled owned case must report a ready domain with
nonzero lights, receivers, and primitives, zero mixed ownership, and an exact
option-off/on engine TGA match. A shadow-enabled blocker case must report the
named whole-view fallback, zero shared draws, and an exact match to the classic
reference. All game launches remain bordered/windowed.

## Local runtime evidence

The 2026-08-21 Windows development-worktree run used the stock
`maps/tools/mv2` room plus the stock
`models/mapobjects/strogg/crates/crate1_small.lwo` test model at the profile's
fixed view. Every capture was produced by the engine at bordered/windowed
1280x720; no operating-system capture path was used.

- OpenGL and Vulkan each reported one ready owned view with two lights, four
  global receiver surfaces, eight sealed primitives, four submitted draws,
  four explicit no-ops, and no fallback or coverage mismatch. Both backends
  reported domain hash `931c507bb531224b` and view hash
  `990026ba7a164782`.
- The OpenGL option-off/on TGAs matched exactly at RMS `0`, maximum delta `0`,
  and zero differing channels; the shared/classic image SHA-256 is
  `ddee1696c70d6eb482e9ea36fc744d9d047c7d3d0739a8ccbb7cd5b7eff02177`.
- The Vulkan option-off/on TGAs were byte-identical with SHA-256
  `39ceb4ad035c1b6576f006d86ab90fe62df554dd88e33b42392997c74c672019`.
- Enabling shadows on both backends produced failure `shadows`, domain status
  `fallback`, zero shared lights/surfaces/primitives/draws, backend coverage
  `0/1/0`, and fallback hash `f6751ea3f1fd3a7b`. Each fallback TGA matched its
  same-settings classic reference exactly.

The gameplay harness now derives an owned, disabled, or shadow-fallback
expectation from the effective interaction cvars and fails empty ownership,
mixed/fallback coverage, mismatches, or missing counts. These results qualify
the controlled local implementation boundary. Clean committed-package reruns
and target-platform/driver coverage remain required for promotion.

## Remaining Milestone D work

The shared GUI, world ambient/material, and unshadowed fixed-classic interaction
corridors are three independently guarded complete domains. They do not yet
make an ordinary shadowed stock frame fully shared.

The next ownership increment is shadow-coupled interaction parity, followed by
fog/blend. Deform, subview, in-world GUI, render-demo, cinematic, authored post,
and other special domains remain downstream. Temporal presentation and
PBR/advanced-lighting work must continue to wait for coherent classic-frame
ownership.
