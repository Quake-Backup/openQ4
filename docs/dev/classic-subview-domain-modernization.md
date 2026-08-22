# Shared Special-Subview Transaction

## Status

The direct special-subview Milestone D corridor is **Experimental (implemented,
default-off; native/static validation passed; runtime and release qualification
pending)**. `r_rendererSharedSubview 1` lets an eligible direct `SS_SUBVIEW`
mirror publish its complete camera, clip-plane, viewport, and scissor record,
and lets eligible remote-camera, mirror, reflection, refraction, and x-ray
surfaces publish their exact child-scene-to-image capture transaction. The
capture edge preserves 2D or cubemap destination type, the exact cubemap face,
and color or depth aspect. Eligible nested special views seal their complete
parent/child tree and publish shared ownership only when its outermost special
view has completed. OpenGL and Vulkan consume the same sealed record.

This seals a subview handoff, not its material/light implementation. Each
child scene still uses its established classic 3D walker, so it is not a claim
that child ambient, interaction, fog, cinematic, or post work is
shared-owned. The direct record selects the complete established child-view
executor; a capture record selects only the exact transfer after that executor
returns.

The implementation is original openQ4 work. It incorporates no external source
code and changes no stock Quake 4 asset.

## Transaction boundary

The front end emits a child `RC_DRAW_VIEW`, restores the parent view, and then
emits `RC_COPY_RENDER` for a capture-backed surface. A direct `SS_SUBVIEW`
mirror has no copy command: its child output is already composited into the
parent target. Both forms must therefore be recovered from immutable records,
not mutable global view state:

1. the child scene packet retains its exact `viewDef`, parent source surface,
   camera origin/axis, final culling parity, clip plane, viewport, scissor,
   time, and packet index;
2. a capture record retains the child identity, destination image, source
   rectangle, destination type/format, cube face, and color/depth mode when a
   transfer exists;
3. admission reconciles the direct source with no capture, or a capture source
   with exactly one full-viewport 2D/cubemap color or depth capture; and
4. the backend reports direct ownership only after the complete classic child
   view returns, or capture ownership only after the sealed transfer completes;
   and
5. a nested child identifies its exact `superView` domain parent, the
   depth-first child-before-parent scene-packet order, a bounded root/depth,
   and every descendant in that root transaction before any member publishes
   shared ownership.

OpenGL performs a direct child through the established `RB_DrawView` executor
or performs a capture transfer with `CopyFramebuffer`/`CopyDepthbuffer` sourced
from the sealed record. Vulkan similarly retains its established 3D executor
for a direct child and its image-copy implementation for a capture, creating
an exact-format six-layer depth target for a depth cubemap when required. A
changed camera, clip plane, viewport, scissor, image, destination type/aspect,
cube face, or copy rectangle never borrows the shared record: the normal
command executes on the classic path and the subview records a named fallback
instead.

Nested work remains executed by the mature classic child-view executor and
copy implementation. The shared corridor does not substitute a partial child
material or lighting pass. Its additional guarantee is ownership atomicity:
if a nested source, camera, resource, transfer, or completion edge is rejected,
the whole sealed special-view tree remains on the established command path.

## Eligibility and fallback

The current corridor accepts an eligible 3D child view that has all of the
following:

- a parent scene and source draw surface still present in that scene;
- either an `SS_SUBVIEW` direct mirror with no capture and exactly one sealed
  clip plane, or exactly one `RC_COPY_RENDER` record matching the full child
  viewport and a 2D/cubemap color/depth target with its legal exact face;
- for a capture, one matching parent dynamic stage: `DI_REMOTE_RENDER`,
  `DI_MIRROR_RENDER`, `DI_REFLECTION_RENDER`, `DI_REFRACTION_RENDER`, or
  `DI_XRAY_RENDER`; and
- the exact kind-specific camera/clip semantics: one portal plane for direct
  mirrors, dynamic mirrors, and reflections; x-ray state with no portal plane;
  and ordinary no-clip state for remote-camera/refraction, plus a loaded
  destination image for a capture backend; and
- for nesting, an exact parent subview record for every `superView` link, a
  strict descendant-before-parent packet order, and a fully admissible
  parent/child subtree. Child completion is deferred until the root view has
  completed every direct or capture-backed member.

Editor/render-demo/global-material views and every missing or
duplicate/mismatched parent, capture, material, semantic, resource, command
order, or capacity condition retain the established complete classic fallback.
A malformed or rejected nested member returns its complete root transaction to
that path; no admitted descendant remains reported shared-owned on its own. A
capture target with an unsupported texture type, illegal cube face, or
color/depth-aspect mismatch receives the named `unsupportedCaptureTarget`
fallback. Named nesting fallbacks distinguish a missing parent,
child-before-parent order failure, rejected parent/child tree, and incomplete
backend completion. The setting cannot split a capture, a direct child, or a
nested tree: it either consumes the sealed transaction or does not own it at
all.

## Control and diagnostics

`r_rendererSharedSubview` is independent of the GUI, world-ambient,
interaction, fog/blend, and deform controls:

- `0` (default): every direct and capture-backed subview uses the established
  backend path;
- `1`: an eligible direct mirror or remote-camera/mirror/reflection/refraction/
  x-ray 2D/cubemap color or depth capture—including a fully admissible nested
  tree—may consume the sealed transaction; all other subviews use the classic
  fallback.

The stock-baseline and ordinary gameplay-benchmark harnesses explicitly set the
control to `0`. `gfxInfo` reports packet/capture counts, ready and fallback
views, direct mirror/remote/mirror/reflection/refraction/x-ray totals,
color-cubemap/depth-2D/depth-cubemap target counts, semantic hash, individual
capture rectangles/types/faces/aspects, parent/root/depth/subtree records, and
OpenGL/Vulkan ownership/fallback counters including nested-tree completion and
rollback totals.

Focused dependency-light validation:

```text
rendererScenePacketSelfTest
rendererClassicSubviewDomainSelfTest
gfxInfo
```

`tools/tests/renderer_classic_subview_domain.py` guards the front-end and
legacy command capture association, bounded parent/source/capture admission,
direct-mirror and special capture scope, sealed camera/clip/scissor semantics,
sealed GL/Vulkan copy arguments, exact post-view/post-copy ownership reporting,
conservative defaults, diagnostics, and CI registration.

## Remaining qualification

Before promotion, retain engine-render-target screenshots—not OS captures—for
fixed direct mirror, dynamic mirror/reflection, x-ray, remote-camera, and
refraction views with the setting off and on, separately on OpenGL and Vulkan.
Require exact same-settings output, nonzero reconciled ownership, named
zero-commit fallback on a deliberately malformed semantic and capture edge,
clean backend diagnostics, retained final package evidence, and target-platform/
driver coverage. Retain a nested direct/capture chain with nonzero root-tree
ownership plus a malformed descendant that reports a whole-tree rollback on
both backends. When a qualifying capture source is available, retain a nonzero
2D/cubemap color/depth ownership case and an illegal-face/aspect zero-commit
fallback on both backends. Cinematic and post forms remain separate Milestone D
work. In-world GUI has its own provenance-tagged transaction documented in
[Shared Classic In-World GUI Domain](classic-inworld-gui-domain-modernization.md).
