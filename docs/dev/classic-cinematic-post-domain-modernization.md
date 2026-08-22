# Shared Classic Cinematic and Authored-Post Transaction

## Status

This Milestone D corridor is **Experimental (implemented, default-off;
native/static validation passed; runtime and release qualification pending)**.
`r_rendererSharedCinematicPost 1` lets OpenGL and Vulkan use the same sealed
transaction boundary for two dynamic classic-frame islands:

- an eligible root 2D view containing `videoMap` or `soundMap` playback; and
- the complete ordered `SS_POST_PROCESS` tail of an eligible ordinary root 3D
  view.

The implementation is original openQ4 work. It incorporates no external source
code and changes no stock Quake 4 asset.

## Transaction boundary

The shared domain does not attempt to turn a cinematic decoder, current-frame
capture, or custom authored shader into a static material record. Those inputs
are dynamic by contract. Instead, front-end scene packets seal the complete
source range, exact admitted draw-packet identity/order, view identity, and the
cinematic render-view clock before either backend can claim it.

For root cinematics, all root sources remain in one transaction; a matching GUI
packet sequence proves the packet-admitted surface order. The stored time is
derived from the same `floatTime` and render-view parameter consumed by the
classic video path. For authored post, the transaction starts exactly at the
first `SS_POST_PROCESS` surface and must consume the entire remaining tail with
matching `RENDER_PASS_AUTHORED_POST` packet identities. It records cinematic,
`_currentRender`, and `_currentDepth` stage use for diagnostics.

After preflight, each backend dispatches the established dynamic-stage adapter:
OpenGL retains `RB_STD_DrawShaderPasses`; Vulkan retains
`VK_Exec_DrawAmbientStages`. Therefore video decode, scratch-image upload,
sound/video timing, feedback capture, custom program stages, and ordered
post-material execution stay on their known classic implementations. This is a
sealed ownership and fallback boundary, not a second decoder or post shader
implementation.

## Eligibility and fallback

The root cinematic path accepts only a non-subview, non-editor, non-world 2D
view with no post-process sources and at least one cinematic stage. The authored
post path accepts only a non-subview ordinary root 3D view with a complete,
packet-matching post tail. Both reject invalid scene/pass ranges, changed source
identity, overflow, non-finite cinematic time, render targets, debug/skip
modes, feedback-capture failure, and backend rejection.

A rejection uses the complete classic fallback path. Once an adapter starts, it
retains the whole sealed range and reports
ownership only if its exact source-surface count is reconciled; it never mixes a
partially shared cinematic or post chain with the classic walker.

## Control and diagnostics

`r_rendererSharedCinematicPost` is independent of GUI, in-world GUI, world
ambient, interaction, fog/blend, subview, and deform ownership:

- `0` (default): cinematics and authored post surfaces use the normal backend
  path;
- `1`: an eligible complete root cinematic view or authored post tail may use
  the sealed transaction; all other work remains classic-owned.

`gfxInfo` reports request/preparation state, source scenes, root/post counts,
ready/fallback views, cinematic and current-render/depth stage counts, semantic
hash, and OpenGL/Vulkan ownership/fallback totals. Focused dependency-light
validation is:

```text
rendererScenePacketSelfTest
rendererClassicCinematicPostDomainSelfTest
gfxInfo
```

`tools/tests/renderer_classic_cinematic_post_domain.py` guards the bounded
domain interface, packet/timing admission, exact OpenGL/Vulkan dynamic-stage
handoff, conservative defaults, release-harness isolation, diagnostics, and CI
registration.

## Remaining qualification

Before promotion, retain engine-written `screenshot` captures—not OS
captures—for the same stock cinematic and authored-post scene with the setting
off and on, separately on OpenGL and Vulkan. Require nonzero reconciled
ownership where the content is eligible, exact same-settings output, a named
zero-commit malformed packet/capture fallback, clean backend diagnostics, and
fresh staged-package and target-platform/driver evidence.

Render-demo and Raven special-frame ownership now have their own sealed
[transaction](classic-special-frame-domain-modernization.md). Direct
special-subview ownership is now documented in the [Shared Special-Subview
Transaction](classic-subview-domain-modernization.md); unsupported special-view
nesting ownership remains next. Temporal presentation and PBR/advanced
lighting remain downstream.
