# Shared Capture-Backed Subview Transaction

## Status

The first Milestone D subview corridor is **Experimental (implemented,
default-off; native/static validation passed; runtime and release qualification
pending)**. `r_rendererSharedSubview 1` lets an eligible remote-camera or
refraction subview publish one bounded child-scene-to-image capture transaction
that OpenGL and Vulkan both consume from the same sealed record.

This owns the capture edge only. The child scene still uses its established
classic 3D walker, so this is not a claim that all child material, lighting,
mirror, reflection, x-ray, cinematic, or post work is shared-owned. Those
forms remain on their complete classic fallback until they have dedicated
records and parity evidence.

The implementation is original openQ4 work. It incorporates no external source
code and changes no stock Quake 4 asset.

## Transaction boundary

The front end emits a child `RC_DRAW_VIEW`, restores the parent view, and then
emits `RC_COPY_RENDER`. That restoration makes the capture relationship
impossible to recover from mutable global view state, so scene packets now seal
the edge explicitly:

1. the child scene packet retains its exact `viewDef` and packet index;
2. the capture record retains the child identity, destination image, source
   rectangle, cube face, and color/depth mode;
3. admission reconciles the parent scene and source surface with exactly one
   color capture; and
4. the backend reports ownership only after the capture transfer completes
   using the sealed image and rectangle.

OpenGL performs the owned transfer with `CopyFramebuffer` sourced from the
sealed record. Vulkan submits its existing image-copy implementation with the
same sealed arguments. A mismatched command never borrows the shared record:
the normal command executes on the classic path and the subview records a named
fallback instead.

## Eligibility and fallback

The current capture-backed corridor accepts only a non-nested, ordinary 3D
child view that has all of the following:

- one parent root scene and a parent draw surface still present in that scene;
- exactly one non-depth, non-cubemap `RC_COPY_RENDER` record matching the full
  child viewport;
- one matching parent material dynamic stage: `DI_REMOTE_RENDER` for a
  remote-camera surface or `DI_REFRACTION_RENDER` for a refraction surface;
  and
- a loaded destination image that is still available to the selected backend.

Mirrors, reflections, clip-plane subviews, x-ray views, cubemaps, depth
captures, editor/render-demo/global-material views, nested subviews, and every
missing or duplicate/mismatched parent, capture, material, resource, or
capacity condition retain the established complete classic fallback. The
setting cannot split a capture: it either consumes the sealed transfer or does
not own it at all.

## Control and diagnostics

`r_rendererSharedSubview` is independent of the GUI, world-ambient,
interaction, fog/blend, and deform controls:

- `0` (default): every subview capture uses the established backend command;
- `1`: an eligible remote-camera or refraction color capture may consume the
  sealed transaction; all other subviews use the classic fallback.

The stock-baseline and ordinary gameplay-benchmark harnesses explicitly set the
control to `0`. `gfxInfo` reports packet/capture counts, ready and fallback
views, remote-camera/refraction totals, semantic hash, individual capture
rectangles, and OpenGL/Vulkan ownership/fallback counters.

Focused dependency-light validation:

```text
rendererScenePacketSelfTest
rendererClassicSubviewDomainSelfTest
gfxInfo
```

`tools/tests/renderer_classic_subview_domain.py` guards the front-end and
legacy command capture association, bounded parent/source/capture admission,
remote-camera/refraction scope, sealed GL/Vulkan copy arguments, exact
post-copy ownership reporting, conservative defaults, diagnostics, and CI
registration.

## Remaining qualification

Before promotion, retain engine-render-target screenshots—not OS captures—for
the same fixed remote-camera and refraction views with the setting off and on,
separately on OpenGL and Vulkan. Require exact same-settings output, nonzero
reconciled ownership, named zero-commit fallback on a deliberately malformed
capture edge, clean backend diagnostics, retained final package evidence, and
target-platform/driver coverage. Direct mirror/reflection/clip-plane, x-ray,
depth/cubemap, nested, in-world GUI, cinematic, and post ownership remain
separate Milestone D work.
