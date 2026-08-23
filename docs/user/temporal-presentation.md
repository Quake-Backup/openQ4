# Temporal AA And Dynamic Resolution

openQ4 includes experimental, default-off temporal anti-aliasing/upscaling and
GPU-time dynamic resolution for OpenGL and Vulkan.

To try temporal AA:

```cfg
r_temporalAA 1
```

To let the 3D scene adjust its resolution to GPU load as well:

```cfg
r_rendererDynamicResolution 1
r_dynamicResolutionMinScale 50
r_dynamicResolutionMaxScale 100
```

The HUD, menus, and console remain at your native output resolution. Screenshots
and save previews bypass temporal history so they cannot feed captured pixels
back into later gameplay. `r_dynamicResolutionCaptureNative 1` asks known
capture frames to render the 3D scene at full resolution; its default of `0`
keeps the current scale and avoids disturbing the controller.

For a fixed scale instead of automatic adjustment, leave
`r_rendererDynamicResolution 0` and set `r_screenFraction` below `100`. Temporal
AA uses that lower-resolution 3D scene as TAAU input even with
`r_resolutionScaleMode 0`; UI remains native-sized.

Use `rendererTemporalPresentationStatus` in the console to inspect the current
scene size, GPU target, delayed timing sample, and history reset reason. Set both
`r_temporalAA 0` and `r_rendererDynamicResolution 0` to restore the established
SMAA/spatial presentation path immediately.

These features remain experimental and are not enabled automatically.
