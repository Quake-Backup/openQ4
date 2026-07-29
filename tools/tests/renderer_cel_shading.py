#!/usr/bin/env python3
"""Contract checks for openQ4 cel shading.

Two things are pinned here. First, the band ladder: the CPU helper, the three
interaction shaders and this file all have to agree on where a band boundary
falls, because a mismatch would show up as models and world geometry stepping at
different intensities. Second, the wiring: which surfaces participate, where the
passes sit in the frame, and what the feature degrades to when a backend cannot
carry part of it.
"""

import math
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
RENDERER = ROOT / "src" / "renderer"
GLPROGS = ROOT / "content" / "baseoq4" / "pak0" / "glprogs"

CEL_MIN_BANDS = 2
CEL_MAX_BANDS = 8

INTERACTION_SHADERS = (
    "material_interaction.fs",
    "shadow_interaction.fs",
    "shadow_point_interaction.fs",
)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def assert_true(condition, message):
    if not condition:
        raise AssertionError(message)


def cxx_function_body(source, signature):
    start = source.find(signature)
    assert_true(start >= 0, f"{signature} should exist")

    open_brace = source.find("{", start)
    assert_true(open_brace >= 0, f"{signature} should have a function body")

    depth = 0
    for index in range(open_brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace + 1:index]

    raise AssertionError(f"{signature} should have a closed function body")


def clamp(value, low, high):
    return max(low, min(high, value))


def cel_band_count(requested):
    return int(clamp(requested, CEL_MIN_BANDS, CEL_MAX_BANDS))


def cel_quantize_unit_value(value, bands):
    """Mirror of R_CelQuantizeUnitValue and the shaders' CelQuantizeLight."""
    if value <= 0.0:
        return 0.0
    if value >= 1.0:
        return value

    steps = float(cel_band_count(bands)) - 1.0
    if steps <= 0.0:
        return value

    band = clamp(math.floor(value * steps + 0.5), 0.0, steps)
    return band / steps


def test_band_ladder_is_evenly_spaced_and_hits_both_ends():
    for bands in range(CEL_MIN_BANDS, CEL_MAX_BANDS + 1):
        produced = sorted({cel_quantize_unit_value(i / 512.0, bands) for i in range(513)})
        assert_true(produced[0] == 0.0, f"{bands} bands should be able to produce full black")
        assert_true(produced[-1] == 1.0, f"{bands} bands should be able to produce full white")
        assert_true(len(produced) == bands, f"{bands} bands should produce exactly {bands} levels")

        spacing = {round(b - a, 6) for a, b in zip(produced, produced[1:])}
        assert_true(len(spacing) == 1, f"{bands} bands should be evenly spaced, got {sorted(spacing)}")


def test_band_ladder_is_monotonic_and_bounded():
    previous = -1.0
    for step in range(0, 1025):
        value = step / 1024.0
        quantized = cel_quantize_unit_value(value, 4)
        assert_true(quantized >= previous - 1.0e-6, "banding must never invert the lighting ramp")
        assert_true(0.0 <= quantized <= 1.0, "banded lighting must stay in range")
        previous = quantized


def test_overbright_and_unlit_pass_through_untouched():
    for bands in range(CEL_MIN_BANDS, CEL_MAX_BANDS + 1):
        assert_true(cel_quantize_unit_value(0.0, bands) == 0.0, "unlit surfaces must stay unlit")
        for overbright in (1.0, 1.5, 4.0, 16.0):
            assert_true(
                cel_quantize_unit_value(overbright, bands) == overbright,
                "overbright light must keep its headroom instead of being clamped to a band",
            )


def test_requested_band_count_is_clamped():
    assert_true(cel_band_count(0) == CEL_MIN_BANDS, "band count must clamp up to the minimum")
    assert_true(cel_band_count(999) == CEL_MAX_BANDS, "band count must clamp down to the maximum")


def test_cpu_helper_matches_the_pinned_ladder():
    source = read(RENDERER / "CelShading.cpp")
    body = cxx_function_body(source, "float R_CelQuantizeUnitValue( float value )")

    assert_true("if ( value <= 0.0f ) {" in body, "unlit input must short-circuit to black")
    assert_true("if ( value >= 1.0f ) {" in body, "overbright input must pass through untouched")
    assert_true(
        "idMath::Floor( value * steps + 0.5f )" in body,
        "the CPU ladder must round to the nearest band, matching the shaders",
    )
    assert_true("band / steps" in body, "the CPU ladder must normalize by the step count")

    header = read(RENDERER / "CelShading.h")
    assert_true(
        f"CEL_MIN_BANDS = {CEL_MIN_BANDS}" in header and f"CEL_MAX_BANDS = {CEL_MAX_BANDS}" in header,
        "the band range pinned here must match CelShading.h",
    )


def test_every_interaction_shader_carries_the_same_ladder():
    for name in INTERACTION_SHADERS:
        source = read(GLPROGS / name)

        assert_true("uniform vec4 uCelParams;" in source, f"{name} must accept the cel parameters")
        assert_true(
            "return max( uCelParams.y - 1.0, 1.0 );" in source,
            f"{name} must derive its step count from the band count",
        )
        assert_true(
            "if ( peak <= 0.0 || peak >= 1.0 ) {" in source,
            f"{name} must pass black and overbright through untouched, like the CPU helper",
        )
        assert_true(
            "return light * ( ( floor( peak * steps + 0.5 ) / steps ) / peak );" in source,
            f"{name} must band by the brightest channel so quantizing cannot shift hue",
        )
        assert_true(
            "return floor( clamp( term, 0.0, 1.0 ) * steps + 0.5 ) / steps;" in source,
            f"{name} must place the specular plateau on the same ladder",
        )
        assert_true(
            "CelQuantizeLight( light )" in source,
            f"{name} must apply banding to the full light term, shadows included",
        )
        assert_true("CelSpecularTerm(" in source, f"{name} must route its specular term through the cel gate")


def test_banding_reaches_the_shipping_lighting_path():
    source = read(RENDERER / "draw_arb2.cpp")

    # r_enhancedMaterials defaults off, so without this the GLSL interaction
    # shaders would never be bound and cel banding would silently do nothing.
    body = cxx_function_body(source, "static void RB_DrawMaterialInteractions( const drawSurf_t *surf )")
    assert_true(
        "RB_DrawSurfChainNeedsCelBanding( surf )" in body,
        "a cel-shaded light chain must be able to select the GLSL interaction path on its own",
    )
    assert_true(
        "RB_GLSLMaterial_CreateDrawInteractions( surf, !enhanced )" in body,
        "borrowing the GLSL path for cel banding must keep the stock lighting model",
    )

    for program in ("g_materialInteractionProgram", "g_shadowMapProgram", "g_pointShadowMapProgram"):
        assert_true(
            f'{program}.celParams = glGetUniformLocationARB( programObject, "uCelParams" );' in source,
            f"{program} must resolve the cel uniform",
        )
        assert_true(
            f"RB_SetCelInteractionUniform( {program}.celParams, surf );" in source,
            f"{program} must refresh the cel uniform per surface",
        )

    setter = cxx_function_body(
        source,
        "static void RB_SetCelInteractionUniform( const GLint location, const drawSurf_t *surf )",
    )
    assert_true(
        "if ( location < 0 ) {" in setter,
        "a program built before the uniform existed must be tolerated, not crashed on",
    )
    assert_true(
        "R_CelShadingSurfaceActive( surf )" in setter,
        "the cel uniform must follow the per-surface gate, not a global toggle",
    )


def test_surface_classification_separates_world_model_and_view_weapon():
    source = read(RENDERER / "CelShading.cpp")

    world = cxx_function_body(source, "bool R_CelSurfaceIsWorld( const drawSurf_t *surf )")
    assert_true(
        "IsStaticWorldModel()" in world,
        "world classification must use the engine's own static-world-model test",
    )
    assert_true(
        "entityDef == NULL || entityDef->parms.hModel == NULL" in world,
        "surfaces drawn through the synthetic world space must count as world",
    )

    weapon = cxx_function_body(source, "bool R_CelSurfaceIsViewWeapon( const drawSurf_t *surf )")
    assert_true("surf->space->weaponDepthHack" in weapon, "the depth-hacked view weapon must be recognized")
    assert_true(
        "renderEntity.allowSurfaceInViewID != 0" in weapon,
        "first-person-only surfaces must be recognized as view models",
    )

    participates = cxx_function_body(source, "static bool R_CelSurfaceParticipates( const drawSurf_t *surf )")
    assert_true(
        "R_CelShadingWorldEnabled()" in participates and "r_celViewWeapon.GetBool()" in participates,
        "world and view-weapon surfaces must answer to their own toggles",
    )

    outline = cxx_function_body(source, "bool R_CelOutlineSurfaceActive( const drawSurf_t *surf )")
    assert_true(
        "if ( R_CelSurfaceIsWorld( surf ) ) {" in outline,
        "world geometry must be inked in screen space, never by an outline shell",
    )
    assert_true(
        "material->Coverage() == MC_TRANSLUCENT" in outline,
        "translucent surfaces must not grow an outline shell",
    )


def test_world_outline_only_snapshots_world_geometry():
    source = read(RENDERER / "draw_common.cpp")

    body = cxx_function_body(source, "static bool RB_CelWorldDepthSurfFilter( const drawSurf_t *surf )")
    assert_true(
        "!R_CelSurfaceIsWorld( surf )" in body,
        "the world depth snapshot must exclude model entities so they are not inked twice",
    )
    assert_true(
        "RB_MaterialIsSkyForSSAODepth( material )" in body,
        "sky must stay out of the snapshot so the horizon reads as a silhouette",
    )
    assert_true(
        "material->Coverage() == MC_TRANSLUCENT" in body,
        "translucent surfaces must not contribute world depth",
    )

    capture = cxx_function_body(
        source,
        "static void RB_CaptureCelWorldDepthImage( drawSurf_t **drawSurfs, int numDrawSurfs )",
    )
    assert_true(
        "RB_CelWorldOutlineRequestedForCurrentView()" in capture,
        "the extra depth prepass must be paid for only while the world outline is on",
    )
    assert_true(
        "RB_BeginDrawingView();" in capture,
        "the snapshot must hand a clean depth/stencil buffer back to the real frame",
    )


def test_disabled_cel_shading_costs_nothing():
    source = read(RENDERER / "draw_common.cpp")

    outlines = cxx_function_body(
        source,
        "static void RB_STD_DrawCelOutlines( drawSurf_t **drawSurfs, int numDrawSurfs )",
    )
    assert_true(
        "!R_CelOutlineEnabled()" in outlines,
        "the outline pass must return before touching GL state when the feature is off",
    )
    assert_true(
        "if ( !RB_CelHasOutlineWork( drawSurfs, numDrawSurfs ) ) {" in outlines,
        "a view with nothing to outline must not touch GL state either",
    )

    world = cxx_function_body(source, "static void RB_STD_CelWorldOutline( void )")
    assert_true(
        "!RB_CelWorldOutlineRequestedForCurrentView()" in world,
        "the world outline pass must return early when the feature is off",
    )
    assert_true(
        "rbCelWorldDepthFrame != backEnd.frameCount" in world,
        "the world outline must not run against a stale depth snapshot",
    )


def test_graceful_degradation_is_explicit():
    source = read(RENDERER / "draw_common.cpp")

    outlines = cxx_function_body(
        source,
        "static void RB_STD_DrawCelOutlines( drawSurf_t **drawSurfs, int numDrawSurfs )",
    )
    assert_true(
        "const bool maskSilhouette = glConfig.stencilBits > 0;" in outlines,
        "the outline shell must still draw without a stencil buffer, just without the mask",
    )
    assert_true(
        "R_ValidateGLSLProgram( &rbPlayerOutlineStage )" in outlines,
        "the outline shell must fall back to the fixed-function hull when GLSL is unavailable",
    )
    assert_true(
        "glStencilMask( RB_PLAYER_OUTLINE_STENCIL_BIT );" in outlines,
        "the mask must claim a single stencil bit so shadow counts survive",
    )
    assert_true(
        "glClearStencil( RB_PlayerVisibilitySafeStencilClearValue() );" in outlines,
        "the pass must restore the stencil clear value the rest of the frame expects",
    )

    requested = cxx_function_body(source, "static bool RB_CelWorldOutlineRequestedForCurrentView( void )")
    assert_true(
        "!glConfig.GLSLProgramAvailable" in requested,
        "the screen-space world outline has no fixed-function fallback and must say so",
    )
    assert_true(
        "RB_IsMainScenePostProcessView()" in requested,
        "the world outline must not run inside mirrors, subviews or sidecar views",
    )


def test_frame_order_places_cel_passes_where_they_survive():
    source = read(RENDERER / "draw_common.cpp")
    body = cxx_function_body(source, "void\tRB_STD_DrawView( void )")

    order = [
        "RB_CaptureCelWorldDepthImage( drawSurfs, numDrawSurfs );",
        "RB_STD_ForceAmbient();",
        "RB_STD_DrawCelOutlines( drawSurfs, processed );",
        "RB_STD_Bloom();",
        "RB_STD_CelWorldOutline();",
    ]
    positions = []
    for token in order:
        index = body.find(token)
        assert_true(index >= 0, f"RB_STD_DrawView should call {token}")
        positions.append(index)

    assert_true(
        positions[0] < positions[1],
        "the world depth snapshot must be taken before the scene overwrites the depth buffer",
    )
    assert_true(
        positions[1] < positions[2],
        "outlines must be drawn after the ambient floor, or the ink would be lifted back out",
    )
    assert_true(
        positions[3] < positions[4],
        "world edges must be composited after bloom so the ink is not smeared by the blur",
    )


def test_cvars_are_archived_and_ranged():
    source = read(RENDERER / "RenderSystem_init.cpp")
    header = read(RENDERER / "tr_local.h")

    expected = {
        "r_celShading": "0",
        "r_celShadingWorld": "0",
        "r_celShadingBands": "1",
        "r_celShadingSteps": "4",
        "r_celShadingSpecular": "1",
        "r_celViewWeapon": "1",
        "r_celOutline": "1",
        "r_celOutlineWidth": "2.0",
        "r_celOutlineAlpha": "1.0",
        "r_celOutlineColor": "0 0 0 255",
        "r_celViewWeaponOutlineWidth": "1.0",
        "r_celViewWeaponOutlineAlpha": "1.0",
        "r_celShadingWorldWidth": "2.0",
        "r_celShadingWorldAlpha": "1.0",
        "r_celShadingWorldDepthThreshold": "0.0015",
        "r_celShadingWorldNormalThreshold": "0.4",
    }

    for name, default in expected.items():
        pattern = re.compile(
            r"idCVar\s+" + re.escape(name) + r"\(\s*\"" + re.escape(name) + r"\"\s*,\s*\"" + re.escape(default) + r"\"\s*,([^,]+),"
        )
        match = pattern.search(source)
        assert_true(match is not None, f"{name} should be declared with default {default!r}")
        flags = match.group(1)
        assert_true("CVAR_RENDERER" in flags, f"{name} should be a renderer cvar")
        assert_true("CVAR_ARCHIVE" in flags, f"{name} should persist across sessions")
        assert_true(f"extern idCVar {name};" in header, f"{name} should be declared in tr_local.h")

    # The debug view is deliberately not archived: it is a diagnostic, not a look.
    assert_true(
        'idCVar r_celShadingWorldDebug( "r_celShadingWorldDebug", "0", CVAR_RENDERER | CVAR_BOOL,' in source,
        "r_celShadingWorldDebug should stay an unarchived diagnostic",
    )

    # Cel shading must ship off, so stock Quake 4 still looks like Quake 4.
    assert_true(expected["r_celShading"] == "0", "cel shading must default off")
    assert_true(expected["r_celShadingWorld"] == "0", "world cel shading must default off")


def test_world_edge_shader_detects_both_silhouettes_and_creases():
    source = read(GLPROGS / "celoutline.fs")

    assert_true(
        "uniform sampler2D WorldDepthBuffer;" in source,
        "the world edge pass must read the world-only depth snapshot",
    )
    assert_true(
        "float ViewSpaceZFromDepth( float depth ) {" in source,
        "edge detection must run on linearized depth, not raw depth",
    )

    ssao = read(GLPROGS / "ssao.fs")
    for shared in ("float ViewSpaceZFromDepth( float depth ) {", "vec3 ReconstructViewPosition( vec2 uv, float depth ) {"):
        body_a = source[source.find(shared):]
        body_b = ssao[ssao.find(shared):]
        assert_true(
            body_a[: body_a.find("}\n")] == body_b[: body_b.find("}\n")],
            "cel and SSAO depth reconstruction must stay identical",
        )

    assert_true(
        "float depthTolerance = max( celEdgeParams.y, 0.00001 ) * viewDistance;" in source,
        "silhouette sensitivity must scale with distance so a line keeps its weight at range",
    )
    assert_true(
        "smoothstep( depthTolerance, depthTolerance * 2.0, depthEdge )" in source,
        "edges must be smoothstepped, not hard-stepped, or the ink crawls",
    )
    assert_true("skyNeighbour" in source, "geometry meeting sky must score as a silhouette")
    assert_true(
        "crease *= 1.0 - silhouette;" in source,
        "a crease measured across a depth step is the silhouette, and must not double up",
    )
    assert_true(
        "if ( celEdgeParams.z > 0.0 ) {" in source,
        "crease detection must be skippable via r_celShadingWorldNormalThreshold 0",
    )
    assert_true(
        "vec3 planeNormal = cross( upperRight - lowerLeft, upperLeft - lowerRight );" in source,
        "the crease plane must be fitted from the diagonal taps, disjoint from the axial ones it tests",
    )


def test_world_outline_is_masked_by_the_finished_scene():
    """The world snapshot predates every model, so it cannot say which of its
    edges survived into the frame. Without a test against the finished depth the
    pass inks world edges straight over the first-person weapon."""

    source = read(GLPROGS / "celoutline.fs")

    assert_true(
        "uniform sampler2D SceneDepthBuffer;" in source,
        "the world edge pass must also read the finished scene depth",
    )
    assert_true(
        "bool PixelIsForeground( vec2 uv, float worldDepth ) {" in source,
        "the pass must have an explicit test for the world being hidden at a pixel",
    )
    assert_true(
        "return sceneViewDepth + 1.0 < worldViewDepth;" in source,
        "occlusion must be measured in linear view space with a one-world-unit tolerance",
    )

    ssao = read(GLPROGS / "ssao.fs")
    assert_true(
        "return finalViewDepth + 1.0 < sourceViewDepth;" in ssao,
        "cel and SSAO must agree on how much nearer a foreground pixel has to be",
    )

    rejection = source.find("if ( PixelIsForeground( uv, centerDepth ) ) {")
    assert_true(rejection >= 0, "main() must reject foreground pixels")
    assert_true(
        rejection < source.find("vec2 texel = invTexSize"),
        "foreground pixels must be dropped before any edge work is done for them",
    )

    # Neighbour taps must keep reading the world snapshot. Sampling the finished
    # depth for them would let a model standing in front of a wall bend the line
    # the wall casts beside it.
    assert_true(
        "return texture2D( WorldDepthBuffer, uv ).x;" in source,
        "edge shape must still be measured from world geometry alone",
    )

    backend = read(RENDERER / "draw_common.cpp")
    stage = cxx_function_body(backend, "static void RB_InitCelOutlineStage( void )")
    assert_true(
        "rbCelOutlineStage.numShaderTextures = 3;" in stage,
        "the pass must bind the scene depth alongside the scene colour and world depth",
    )
    assert_true(
        '"SceneDepthBuffer", sizeof( rbCelOutlineStage.shaderTextureNames[2] ) );' in stage,
        "the third texture must be the finished scene depth",
    )

    world = cxx_function_body(backend, "static void RB_STD_CelWorldOutline( void )")
    assert_true(
        'RB_EnsureSSAODepthScratchImage( rbCelSceneDepthImage, "_celSceneDepth"' in world,
        "the finished depth must be captured into its own scratch image",
    )
    assert_true(
        "sceneDepthImage->CopyDepthbuffer(" in world,
        "the finished depth must be copied out before the fullscreen pass overwrites nothing",
    )
    assert_true(
        "sceneDepthImage = rbCelWorldDepthImage;" in world,
        "a failed scratch allocation must degrade to the unmasked look, not to a black frame",
    )


def main():
    tests = [
        test_band_ladder_is_evenly_spaced_and_hits_both_ends,
        test_band_ladder_is_monotonic_and_bounded,
        test_overbright_and_unlit_pass_through_untouched,
        test_requested_band_count_is_clamped,
        test_cpu_helper_matches_the_pinned_ladder,
        test_every_interaction_shader_carries_the_same_ladder,
        test_banding_reaches_the_shipping_lighting_path,
        test_surface_classification_separates_world_model_and_view_weapon,
        test_world_outline_only_snapshots_world_geometry,
        test_disabled_cel_shading_costs_nothing,
        test_graceful_degradation_is_explicit,
        test_frame_order_places_cel_passes_where_they_survive,
        test_cvars_are_archived_and_ranged,
        test_world_edge_shader_detects_both_silhouettes_and_creases,
        test_world_outline_is_masked_by_the_finished_scene,
    ]

    for test in tests:
        test()

    print("renderer_cel_shading: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
