#!/usr/bin/env python3
"""Static ownership guards for the shared classic interaction domain."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle}")


def require_before(text: str, first: str, second: str, label: str) -> None:
    first_at = text.find(first)
    second_at = text.find(second)
    if first_at < 0 or second_at < 0 or first_at >= second_at:
        raise AssertionError(f"ordering guard failed for {label}: {first!r} before {second!r}")


def reject(text: str, needle: str, label: str) -> None:
    if needle in text:
        raise AssertionError(f"forbidden {label}: {needle}")


def function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise AssertionError(f"missing function: {signature}")
    brace = text.find("{", start + len(signature))
    if brace < 0:
        raise AssertionError(f"missing function body: {signature}")
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1 : index]
    raise AssertionError(f"unterminated function body: {signature}")


def reject_raw_authored_reads(body: str, label: str) -> None:
    forbidden = (
        "GetStage(",
        "shaderRegisters",
        "conditionRegister",
        "color.registers",
        "texture.matrix",
    )
    found = [token for token in forbidden if token in body]
    if found:
        raise AssertionError(f"{label} rereads authored material state: {', '.join(found)}")


def main() -> None:
    header = read("src/renderer/ClassicInteractionDomain.h")
    source = read("src/renderer/ClassicInteractionDomain.cpp")
    packets_h = read("src/renderer/ScenePackets.h")
    packets_cpp = read("src/renderer/ScenePackets.cpp")
    init = read("src/renderer/RenderSystem_init.cpp")
    bootstrap = read("src/renderer/RendererBootstrap.cpp")
    backend = read("src/renderer/tr_backend.cpp")
    vk_backend = read("src/renderer/Vulkan/vk_Backend.cpp")
    modern = read("src/renderer/ModernGLExecutor.cpp")
    gl = read("src/renderer/draw_arb2.cpp")
    vk = read("src/renderer/Vulkan/vk_Interactions.cpp")
    vk_shadow = read("src/renderer/Vulkan/vk_ShadowMap.cpp")
    vk_shadow_h = read("src/renderer/Vulkan/vk_ShadowMap.h")
    vk_executor = read("src/renderer/Vulkan/vk_GuiExecutor.cpp")
    benchmark = read("tools/tests/renderer_gameplay_benchmark.py")
    baseline = read("tools/validation/stock_asset_baseline.py")
    matrix = read("tools/tests/renderer_validation_matrix.py")
    validation = read("tools/validation/openq4_validate.py")
    commit_ci = read(".github/workflows/commit-validation.yml")
    push_ci = read(".github/workflows/push-verification.yml")
    roadmap = read("docs/dev/idtech5-modernization-roadmap.md")
    domain_doc = read("docs/dev/classic-interaction-domain-modernization.md")

    for symbol in (
        "classicInteractionDomainView_t",
        "classicInteractionDomainLight_t",
        "classicInteractionDomainSurface_t",
        "classicInteractionDomainPrimitive_t",
        "classicInteractionDomainShadowCaster_t",
        "classicInteractionDomainShadowAlphaStage_t",
        "classicInteractionDomainShadowMapPass_t",
        "classicInteractionDomainShadowProjectedState_t",
        "classicInteractionDomainShadowPointState_t",
        "classicInteractionDomainTexture_t",
        "R_ClassicInteractionDomain_ResetFrame",
        "R_ClassicInteractionDomain_PrepareFrame",
        "R_ClassicInteractionDomain_FindView",
        "R_ClassicInteractionDomain_ViewLight",
        "R_ClassicInteractionDomain_ViewSurface",
        "R_ClassicInteractionDomain_ViewPrimitive",
        "R_ClassicInteractionDomain_ViewShadowCaster",
        "R_ClassicInteractionDomain_LightShadowCaster",
        "R_ClassicInteractionDomain_ShadowAlphaStage",
        "R_ClassicInteractionDomain_LightShadowMapPass",
        "R_ClassicInteractionDomain_ResolveTexture",
        "R_ClassicInteractionDomain_RecordOwned",
        "R_ClassicInteractionDomain_RecordBackendFallback",
        "RendererClassicInteractionDomain_RunSelfTest",
    ):
        require(header, symbol, "shared interaction contract")

    for shadow_contract in (
        "CLASSIC_INTERACTION_SHADOW_STENCIL",
        "CLASSIC_INTERACTION_SHADOW_PROJECTED",
        "CLASSIC_INTERACTION_SHADOW_POINT",
        "CLASSIC_INTERACTION_SHADOW_HYBRID",
        "CLASSIC_INTERACTION_SHADOW_MAP_PASS_MAPPED",
        "CLASSIC_INTERACTION_SHADOW_MAP_PASS_HYBRID",
        "shadowMapPassIndex[ 2 ]",
        "shadowMapPassCount",
        "hybridShadowPassCount",
        "projectedShadowMapPassCount",
        "csmShadowMapPassCount",
        "pointShadowMapPassCount",
        "projectedShadowLightCount",
        "pointShadowLightCount",
        "backendShadowMapPasses",
        "backendHybridPasses",
    ):
        require(header, shadow_contract, "sealed interaction-shadow contract")

    for state_field in (
        "resourceOwner",
        "receiverMask",
        "mappedCasterCount",
        "supplementCasterCount",
        "drawableMappedCasters",
        "noopMappedCasters",
        "drawableSupplementCasters",
        "noopSupplementCasters",
        "casterSignature",
        "incompleteMapMask",
        "incompleteStencilMask",
        "hybridIncompleteMask",
        "prelightMapMissingMask",
        "prelightStencilRequiredMask",
        "prelightStencilReadyMask",
        "resourcePlanId",
        "resourceGeneration",
        "resourceAlias",
        "mapRequired",
        "mapComplete",
        "stencilComplete",
        "hybridComplete",
        "hasStaticCasters",
        "hasDynamicCasters",
        "hasAlphaCasters",
        "hasTranslucentCasters",
        "allowCacheReuse",
        "allowCacheUpdate",
        "allowScratch",
        "hashedAlpha",
        "stableAlphaHash",
        "casterCullMode",
        "polygonFactor",
        "polygonOffset",
        "projected",
        "point",
    ):
        require(header, state_field, "sealed mapped-shadow state")

    for blocker in (
        "CLASSIC_INTERACTION_FAILURE_SHADOWS",
        "CLASSIC_INTERACTION_FAILURE_SHADOW_MAP",
        "CLASSIC_INTERACTION_FAILURE_SHADOW_PACKET_MISMATCH",
        "CLASSIC_INTERACTION_FAILURE_SHADOW_GEOMETRY",
        "CLASSIC_INTERACTION_FAILURE_CUSTOM_LIGHTING",
        "CLASSIC_INTERACTION_FAILURE_DEFORM",
        "CLASSIC_INTERACTION_FAILURE_SKINNING",
        "CLASSIC_INTERACTION_FAILURE_DEPTH_HACK",
        "CLASSIC_INTERACTION_FAILURE_DYNAMIC_RESOURCE",
        "CLASSIC_INTERACTION_FAILURE_BACKEND_COVERAGE_MISMATCH",
    ):
        require(header, blocker, "named whole-view blocker")
        require(source, blocker, "implemented whole-view blocker")

    for identity in (
        "interactionLight",
        "interactionLightOrdinal",
        "interactionReceiverClass",
        "interactionReceiverOrdinal",
        "interactionSourceOrdinal",
        "SCENE_INTERACTION_RECEIVER_LOCAL",
        "SCENE_INTERACTION_RECEIVER_GLOBAL",
        "SCENE_INTERACTION_RECEIVER_TRANSLUCENT",
    ):
        require(packets_h, identity, "explicit packet interaction identity")
        require(packets_cpp, identity, "populated packet interaction identity")
        require(source, identity, "validated packet interaction identity")

    for identity in (
        "shadowLight",
        "shadowLightOrdinal",
        "shadowCasterClass",
        "shadowChainOrdinal",
        "shadowSourceOrdinal",
        "SCENE_SHADOW_CASTER_STENCIL_GLOBAL",
        "SCENE_SHADOW_CASTER_STENCIL_LOCAL",
        "SCENE_SHADOW_CASTER_MAP_GLOBAL_STATIC",
        "SCENE_SHADOW_CASTER_MAP_LOCAL_STATIC",
        "SCENE_SHADOW_CASTER_MAP_GLOBAL_DYNAMIC",
        "SCENE_SHADOW_CASTER_MAP_LOCAL_DYNAMIC",
        "SCENE_SHADOW_CASTER_MAP_GLOBAL_TRANSLUCENT",
        "SCENE_SHADOW_CASTER_MAP_LOCAL_TRANSLUCENT",
        "SCENE_SHADOW_CASTER_SUPPLEMENT_GLOBAL",
        "SCENE_SHADOW_CASTER_SUPPLEMENT_LOCAL",
    ):
        require(packets_h, identity, "explicit packet shadow identity")
        require(packets_cpp, identity, "populated packet shadow identity")
        require(source, identity, "validated packet shadow identity")

    require(
        packets_cpp,
        "static void R_ScenePackets_AddShadowMapPass(",
        "shadow-map packet pass builder",
    )
    require(
        packets_cpp,
        "RENDER_PASS_SHADOW_MAP",
        "explicit shadow-map packet category",
    )

    stencil_eligibility = function_body(
        packets_cpp,
        "static bool R_ScenePackets_ViewLightCanCastShadows(",
    )
    reject(
        stencil_eligibility,
        "noDynamicShadows",
        "noDynamicShadows static/prelight stencil exclusion",
    )
    map_eligibility = function_body(
        packets_cpp,
        "static bool R_ScenePackets_ViewLightCanUseShadowMaps(",
    )
    require(
        map_eligibility,
        "noDynamicShadows",
        "dynamic/map-only noDynamicShadows exclusion",
    )
    shadow_map_pass = function_body(
        packets_cpp,
        "static void R_ScenePackets_AddShadowMapPass(",
    )
    require(
        shadow_map_pass,
        "R_ScenePackets_ViewLightCanUseShadowMaps",
        "shadow-map-specific light eligibility",
    )
    stencil_pass = function_body(
        packets_cpp,
        "static void R_ScenePackets_AddStencilShadowPass(",
    )
    require(
        stencil_pass,
        "R_ScenePackets_ViewLightCanCastShadows",
        "static/prelight stencil light eligibility",
    )

    for receiver in (
        "shadowMapPassIndex[ CLASSIC_INTERACTION_RECEIVER_LOCAL ]",
        "shadowMapPassIndex[ CLASSIC_INTERACTION_RECEIVER_GLOBAL ]",
    ):
        require(source, receiver, "explicit mapped receiver pass index")
    require(
        source,
        "R_ClassicInteractionDomain_LightShadowMapPass(",
        "mapped pass accessor implementation",
    )

    for planned_replay in (
        "RecomputePlannedStencilWork( domain.lights[ 0 ] )",
        "const bool mixedStencilPlan = domain.lights[ 0 ].logicalVolumeDraws == 5",
        "domain.lights[ 0 ].preloadVolumeDraws == 3",
        "const bool reusedHybridPlan = domain.lights[ 0 ].logicalVolumeDraws == 2",
        "domain.lights[ 0 ].preloadVolumeDraws == 1",
    ):
        require(
            source,
            planned_replay,
            "receiver-order physical stencil replay self-test",
        )

    map_hash = function_body(source, "static std::uint64_t HashShadowMapPass(")
    for hashed_state in (
        "pass.receiver",
        "pass.resourceOwner",
        "pass.disposition",
        "pass.mode",
        "pass.lightClass",
        "pass.receiverMask",
        "pass.mappedCasterCount",
        "pass.supplementCasterCount",
        "pass.casterSignature",
        "pass.incompleteMapMask",
        "pass.incompleteStencilMask",
        "pass.hybridIncompleteMask",
        "pass.prelightMapMissingMask",
        "pass.prelightStencilRequiredMask",
        "pass.prelightStencilReadyMask",
        "pass.resourceAlias",
        "pass.mapRequired",
        "pass.mapComplete",
        "pass.stencilComplete",
        "pass.hybridComplete",
        "pass.hasStaticCasters",
        "pass.hasDynamicCasters",
        "pass.hasAlphaCasters",
        "pass.hasTranslucentCasters",
        "pass.allowCacheReuse",
        "pass.allowCacheUpdate",
        "pass.allowScratch",
        "pass.hashedAlpha",
        "pass.stableAlphaHash",
        "pass.casterCullMode",
        "pass.polygonFactor",
        "pass.polygonOffset",
        "pass.projected",
        "pass.point",
    ):
        require(map_hash, hashed_state, "mapped-pass semantic hash")
    for backend_identity in (
        "textureHandle",
        "legacyViewLight",
        "resourcePlanId",
        "resourceGeneration",
    ):
        reject(map_hash, backend_identity, "backend/frame identity in semantic hash")

    prepare = function_body(source, "static bool R_ClassicInteractionDomain_PrepareView(")
    for token in (
        "firstLight",
        "firstSurface",
        "firstPrimitive",
        "firstShadowCaster",
        "shadowMapPassPacketIndex",
        "shadowMapPassCount",
        "ready = true",
        "interactionPassPacketIndex",
        "packetDrawCount",
    ):
        require(prepare, token, "transactional view preparation")
    require_before(prepare, "firstLight", "ready = true", "light arena before publication")
    require_before(prepare, "firstSurface", "ready = true", "surface arena before publication")
    require_before(prepare, "firstPrimitive", "ready = true", "primitive arena before publication")
    require_before(prepare, "firstShadowCaster", "ready = true", "shadow arena before publication")
    require_before(prepare, "shadowMapPassCount", "ready = true", "mapped-shadow arena before publication")
    reject(
        prepare,
        "bool mappedShadowState",
        "blanket mapped-shadow whole-view rejection",
    )

    for mapped_chain in (
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_GLOBAL_STATIC",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_LOCAL_STATIC",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_GLOBAL_DYNAMIC",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_LOCAL_DYNAMIC",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_GLOBAL_TRANSLUCENT",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_LOCAL_TRANSLUCENT",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_SUPPLEMENT_GLOBAL",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_SUPPLEMENT_LOCAL",
    ):
        require(prepare, mapped_chain, "exact mapped/supplement chain preparation")

    self_test = function_body(source, "bool RendererClassicInteractionDomain_RunSelfTest(")
    for native_case in (
        "CLASSIC_INTERACTION_SHADOW_PROJECTED",
        "CLASSIC_INTERACTION_SHADOW_POINT",
        "CLASSIC_INTERACTION_SHADOW_HYBRID",
        "CLASSIC_INTERACTION_SHADOW_MAP_PASS_MAPPED",
        "CLASSIC_INTERACTION_SHADOW_MAP_PASS_HYBRID",
        "shadowMapPassIndex",
        "HashShadowMapPass",
        "shadowMapPassCount",
        "hybridShadowPassCount",
        "CLASSIC_INTERACTION_FAILURE_BACKEND_COVERAGE_MISMATCH",
    ):
        require(self_test, native_case, "native mapped/hybrid transaction coverage")

    for semantic in (
        "SL_BUMP",
        "SL_DIFFUSE",
        "SL_SPECULAR",
        "r_skipBump",
        "r_skipDiffuse",
        "r_skipSpecular",
        "globalImages->blackImage",
        "globalImages->flatNormalMap",
        "CLASSIC_INTERACTION_PRIMITIVE_NOOP_BLACK",
    ):
        require(source, semantic, "classic interaction decomposition")

    for integration in (backend, vk_backend):
        require(integration, "R_ClassicInteractionDomain_ResetFrame();", "frame reset")
        require(integration, "R_ClassicInteractionDomain_PrepareFrame( *scenePackets );", "frame preparation")

    require(init, 'idCVar r_rendererSharedWorldInteraction( "r_rendererSharedWorldInteraction", "0"', "default-off archived cvar")
    require(bootstrap, '{ "r_rendererSharedWorldInteraction", &r_rendererSharedWorldInteraction, 0 }', "bootstrap rollback safety")
    require(init, 'cmdSystem->AddCommand( "rendererClassicInteractionDomainSelfTest"', "native self-test command")
    require(init, "R_ClassicInteractionDomain_Stats()", "gfxInfo diagnostics")
    require(modern, "!r_rendererSharedWorldInteraction.GetBool()", "aggregate visible-owner suppression")
    require(modern, "category == RENDER_PASS_ARB2_INTERACTION", "interaction legacy skip guard")
    require(modern, "category == RENDER_PASS_STENCIL_SHADOW", "shadow legacy skip guard")

    gl_preflight = function_body(gl, "static bool RB_ARB2_SharedInteractionPreflight(")
    gl_draw = function_body(gl, "static bool RB_ARB2_DrawSharedInteractionView(")
    gl_primitive_draw = function_body(
        gl, "static void RB_SharedWorldInteractionGLDrawPrimitive("
    )
    gl_shadow_draw = function_body(
        gl, "static void RB_SharedWorldInteractionGLDrawShadowCaster("
    )
    gl_interaction_state = function_body(
        gl, "static void RB_SharedWorldInteractionGLBeginInteractionState("
    )
    reject_raw_authored_reads(gl_preflight, "OpenGL shared preflight")
    reject_raw_authored_reads(gl_draw, "OpenGL shared draw")
    reject_raw_authored_reads(gl_primitive_draw, "OpenGL sealed primitive draw")
    reject_raw_authored_reads(gl_shadow_draw, "OpenGL sealed shadow draw")
    if "RB_GLSLPrepareInteractionVertexCache(" in gl_preflight:
        raise AssertionError(
            "OpenGL shared preflight must not re-enter the legacy material/cache helper"
        )
    require(gl_preflight, "R_ClassicInteractionDomain_ResolveTexture", "OpenGL sealed texture preflight")
    require(gl_preflight, "RB_SharedWorldInteractionGLCacheValid", "OpenGL sealed cache validation")
    require(gl_preflight, "R_TouchVertexCache", "OpenGL retained cache lifetime")
    require(gl, "R_ClassicInteractionDomain_ViewPrimitive", "OpenGL sealed primitive consumption")
    require(gl_preflight, "R_ClassicInteractionDomain_LightShadowCaster", "OpenGL sealed shadow consumption")
    require(gl_primitive_draw, "preparedPrimitive", "OpenGL retained sealed draw plan")
    require(gl_shadow_draw, "preparedCaster", "OpenGL retained sealed shadow plan")
    require(gl_preflight, "preparedPrimitive.vertexBuffer = tri->ambientCache->vbo", "OpenGL retained validated vertex buffer")
    require(gl_preflight, "preparedPrimitive.indexPointer", "OpenGL retained validated index pointer")
    require(gl_primitive_draw, "idVertexCache::BindArrayBuffer", "OpenGL retained vertex binding")
    require(gl_primitive_draw, "idVertexCache::BindIndexBuffer", "OpenGL retained index binding")
    require(gl_primitive_draw, "glDrawElements", "OpenGL retained indexed draw")
    require(gl_shadow_draw, "RB_SharedWorldInteractionGLSubmitShadowElements", "OpenGL sealed stencil submission")
    require(gl_draw, "R_ClassicInteractionDomain_RecordOwned", "OpenGL ownership reconciliation")
    for forbidden in (
        "legacyDrawSurf",
        "vertexCache.Position(",
        "RB_DrawElementsWithCounters(",
    ):
        if forbidden in gl_draw or forbidden in gl_primitive_draw or forbidden in gl_shadow_draw:
            raise AssertionError(
                f"OpenGL committed shared draw re-enters unsealed geometry via {forbidden}"
            )
    for attrib in (1, 2, 5, 6, 7):
        require(
            gl_interaction_state,
            f"glDisableVertexAttribArrayARB( {attrib} )",
            f"OpenGL canonical interaction attribute {attrib} reset",
        )
    gl_entry = function_body(gl, "void RB_ARB2_DrawInteractions( void )")
    require_before(gl_entry, "RB_ARB2_SharedInteractionPreflight", "RB_ARB2_DrawSharedInteractionView", "OpenGL preflight before shared draw")
    require(gl_entry, "R_ClassicInteractionDomain_RecordBackendFallback", "OpenGL whole-view fallback accounting")

    gl_map_values = function_body(
        gl, "static bool RB_SharedWorldInteractionGLMapPassValuesValid("
    )
    gl_map_preflight = function_body(
        gl, "static bool RB_SharedWorldInteractionGLPreflightMapTransaction("
    )
    gl_map_prepare = function_body(
        gl, "static bool RB_SharedWorldInteractionGLPrepareMappedPass("
    )
    gl_map_abort = function_body(
        gl, "static void RB_SharedWorldInteractionGLAbortMapTransaction("
    )
    gl_map_activate = function_body(
        gl, "static void RB_SharedWorldInteractionGLActivateMapResource("
    )
    gl_map_capture = function_body(
        gl, "static bool RB_SharedWorldInteractionGLCaptureMapResource("
    )
    gl_map_schedule = function_body(
        gl, "static bool RB_SharedWorldInteractionGLScheduleSealedMapPass("
    )
    gl_map_complete = function_body(
        gl, "static void RB_SharedWorldInteractionGLCompleteSealedCacheUpdate("
    )
    gl_projected_scratch = function_body(
        gl, "static bool RB_SharedWorldInteractionGLReserveProjectedScratch("
    )
    gl_point_scratch = function_body(
        gl, "static bool RB_SharedWorldInteractionGLReservePointScratch("
    )
    gl_projected_resource_match = function_body(
        gl, "static bool RB_SharedWorldInteractionGLProjectedEntryMatches("
    )
    gl_point_resource_match = function_body(
        gl, "static bool RB_SharedWorldInteractionGLPointEntryMatches("
    )
    gl_map_alias = function_body(
        gl, "static bool RB_SharedWorldInteractionGLAliasMappedPass("
    )
    gl_projected_map = function_body(
        gl, "static bool RB_SharedWorldInteractionGLRenderProjectedMap("
    )
    gl_point_map = function_body(
        gl, "static bool RB_SharedWorldInteractionGLRenderPointMap("
    )
    gl_map_caster = function_body(
        gl, "static int RB_SharedWorldInteractionGLDrawMapCasterChain("
    )
    mapped_shadow_chain = function_body(
        source, "static bool PrepareMappedShadowChain("
    )
    gl_mapped_begin = function_body(
        gl, "static void RB_SharedWorldInteractionGLBeginMappedReceiver("
    )
    gl_mapped_draw = function_body(
        gl, "static void RB_SharedWorldInteractionGLDrawMappedPrimitive("
    )
    gl_mapped_commit = function_body(
        gl, "static int RB_SharedWorldInteractionGLCommitMappedPass("
    )
    gl_shadow_draw = function_body(
        gl, "static void RB_SharedWorldInteractionGLDrawShadowRange("
    )
    for token in (
        "pass.mapRequired",
        "pass.mapComplete",
        "pass.hybridComplete",
        "pass.mappedCasterCount",
        "pass.supplementCasterCount",
        "pass.drawableMappedCasters",
        "pass.noopMappedCasters",
        "pass.drawableSupplementCasters",
        "pass.noopSupplementCasters",
        "pass.resourcePlanId",
        "pass.resourceGeneration",
        "pass.hasStaticCasters",
        "pass.hasDynamicCasters",
        "pass.hasTranslucentCasters",
        "pass.allowCacheReuse",
        "pass.allowCacheUpdate",
        "pass.hashedAlpha",
        "pass.stableAlphaHash",
        "pass.projected.state",
        "pass.point",
    ):
        require(gl_map_values, token, "OpenGL sealed mapped-pass validation")
    require(
        gl_map_values,
        "|| pass.hasTranslucentCasters",
        "OpenGL intentional translucent-moment fallback boundary",
    )
    reject(
        gl_map_values,
        "|| pass.hasDynamicCasters",
        "OpenGL dynamic mapped-caster rejection",
    )
    for token in (
        "RB_SharedWorldInteractionGLPrepareMappedPass(",
        "prepared.shadowMapPassCount",
        "RB_SharedWorldInteractionGLPointEntryMatches(",
        "RB_SharedWorldInteractionGLProjectedEntryMatches(",
        "RB_SharedWorldInteractionGLAbortMapTransaction( viewDef )",
        "prepared.mapTransactionPrepared = true",
        "preparedPass.sampleTextureHandle",
        "preparedPass.projectedOriginX",
        "preparedPass.projectedSpanPixels",
        "physicalAlias",
        "alias.pass->resourceAlias",
        "permittedAlias",
    ):
        require(gl_map_preflight, token, "OpenGL mapped transaction preflight")
    reject(
        gl_map_preflight,
        "glDrawElements(",
        "direct mapped draw outside retained map-render adapters",
    )
    for map_body, label in (
        (gl_projected_map, "OpenGL sealed projected map render"),
        (gl_point_map, "OpenGL sealed point map render"),
    ):
        require(
            map_body,
            "RB_SharedWorldInteractionGLDrawMapCasterChain(",
            label,
        )
        for legacy_chain in (
            "globalShadowMapCasters",
            "localShadowMapCasters",
            "globalShadowMapDynamicCasters",
            "localShadowMapDynamicCasters",
            "globalTranslucentShadowMapCasters",
            "localTranslucentShadowMapCasters",
        ):
            reject(map_body, legacy_chain, f"legacy authored caster walk in {label}")
        for sealed_dynamic_chain in (
            "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_GLOBAL_DYNAMIC",
            "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_LOCAL_DYNAMIC",
        ):
            require(map_body, sealed_dynamic_chain, f"dynamic caster coverage in {label}")
    require(gl_map_caster, "prepared.mapCasters", "OpenGL sealed mapped caster draw")
    require(
        gl,
        "prepared.shadowAlphaStages[preparedCaster.firstAlphaStage + i]",
        "OpenGL sealed perforated caster stages",
    )
    for legacy_renderer in ("RB_RenderShadowMap(", "RB_RenderPointShadowMap("):
        reject(gl_map_prepare, legacy_renderer, "legacy shadow renderer re-entry")
    reject(
        gl_map_prepare,
        "pass.hasDynamicCasters",
        "OpenGL dynamic mapped-pass rejection",
    )
    reject(
        gl_map_prepare,
        "if ( !schedule.cacheable )",
        "OpenGL non-cacheable mapped-pass rejection",
    )
    for token in (
        "pass.hasTranslucentCasters",
        "pass.allowCacheUpdate",
        "RB_SharedWorldInteractionGLScheduleSealedMapPass(",
        "RB_SharedWorldInteractionGLReservePointScratch(",
        "RB_SharedWorldInteractionGLReserveProjectedScratch(",
        "RB_SharedWorldInteractionGLCaptureMapResource(",
    ):
        require(gl_map_prepare, token, "OpenGL cache/update/scratch map preparation")
    for token in (
        "pass.allowCacheReuse",
        "RB_SharedWorldInteractionGLSealedMapSignature( pass )",
        "RB_SharedWorldInteractionGLMapUpdateAdmitted()",
        "RB_ShadowMapFindPointCacheEntry(",
        "RB_ShadowMapFindProjectedCacheEntry(",
        "RB_ShadowMapAllocPointCacheEntry( false )",
        "RB_ShadowMapAllocProjectedCacheEntry(",
        "pass.projected.state.atlasDiv, false",
        "schedule.cacheable = false",
    ):
        require(gl_map_schedule, token, "OpenGL sealed cache scheduler")
    for token in (
        "preparedPass.cacheLightIndex",
        "preparedPass.cachePassKind",
        "pass.lightClass",
        "schedule.signature",
        "entry->valid",
    ):
        require(gl_map_complete, token, "OpenGL sealed cache publication")
    require(
        gl_map_prepare,
        "RB_SharedWorldInteractionGLRenderPointMap(",
        "OpenGL sealed point map adapter",
    )
    require(
        gl_map_prepare,
        "RB_SharedWorldInteractionGLRenderProjectedMap(",
        "OpenGL sealed projected map adapter",
    )
    for token in (
        "g_pointShadowMapProgram.programObject",
        "g_shadowMapProgram.programObject",
        "preparedPass.pointLight",
        "preparedPass.pointColorImage",
        "preparedPass.pointDepthImage",
        "preparedPass.projectedDepthImage",
        "preparedPass.resourceWidth",
        "preparedPass.resourceHeight",
        "pass.point",
        "pass.projected",
    ):
        require(gl_mapped_begin, token, "OpenGL sealed mapped receiver state")
    for token in (
        "preparedPass.pointEntry",
        "preparedPass.projectedEntry",
        "preparedPass.pointColorImage",
        "preparedPass.pointDepthImage",
        "preparedPass.projectedDepthImage",
        "preparedPass.renderTexture",
    ):
        require(gl_map_activate, token, "OpenGL sealed cache/scratch activation")
    for token in (
        "sampleImage->GetDeviceHandle()",
        "sampleImage->GetStorageGeneration()",
        "preparedPass.sampleTextureHandle",
        "preparedPass.sampleStorageGeneration",
        "preparedPass.resourceWidth",
        "preparedPass.resourceHeight",
    ):
        require(gl_map_capture, token, "OpenGL retained map-resource identity")
    for scratch_body, scratch_label in (
        (gl_projected_scratch, "OpenGL projected scratch resource"),
        (gl_point_scratch, "OpenGL point scratch resource"),
    ):
        for token in (
            "preparedPass.pass->allowScratch",
            "globalImages->ScratchImage(",
            "preparedPass.scratchResource = true",
            "preparedPass.renderTexture",
            "RB_SharedWorldInteractionGLCaptureMapResource(",
        ):
            require(scratch_body, token, scratch_label)
    for resource_match, resource_label in (
        (gl_projected_resource_match, "OpenGL projected cache/scratch revalidation"),
        (gl_point_resource_match, "OpenGL point cache/scratch revalidation"),
    ):
        for token in (
            "preparedPass.sampleTextureHandle",
            "preparedPass.sampleStorageGeneration",
            "preparedPass.scratchResource",
            "preparedPass.renderTexture",
        ):
            require(resource_match, token, resource_label)
    for token in (
        "owner.sampleStorageGeneration",
        "owner.sampleTextureHandle",
        "owner.renderTexture",
        "owner.scratchResource",
    ):
        require(gl_map_alias, token, "OpenGL exact mapped-resource alias")
    require(gl_mapped_draw, "preparedPrimitive", "OpenGL retained mapped receiver geometry")
    require(gl_mapped_draw, "pass.point", "OpenGL sealed point receiver state")
    require(gl_mapped_draw, "pass.projected", "OpenGL sealed projected receiver state")
    require(
        mapped_shadow_chain,
        "caster.selectedIndexCount = 0",
        "perforated zero-alpha mapped no-op normalization",
    )
    for token in (
        "caster.materialCoverage == MC_PERFORATED",
        "caster.selectedIndexCount != 0",
        "caster.vertexCount > 0",
        "caster.totalIndexCount > 0",
    ):
        require(
            function_body(
                source,
                "bool R_ClassicInteractionDomain_ShadowCasterNoopValid(",
            ),
            token,
            "backend-neutral mapped no-op contract",
        )
    require(
        function_body(
            gl, "static bool RB_SharedWorldInteractionGLMapCasterValuesValid("
        ),
        "R_ClassicInteractionDomain_ShadowCasterNoopValid( caster )",
        "OpenGL mapped no-op contract",
    )
    require(gl_mapped_commit, "RB_SharedWorldInteractionGLSelectMapPass", "OpenGL mapped resource bind")
    require(gl_mapped_commit, "RB_SharedWorldInteractionGLBeginMappedReceiver", "OpenGL mapped receiver bind")
    require(gl_mapped_commit, "RB_SharedWorldInteractionGLDrawMappedPrimitive", "OpenGL mapped receiver draw")
    require(
        gl_mapped_commit,
        "preparedPass.pointEntry->lastUsedFrame = tr.frameCount",
        "OpenGL point shadow-map cache residency refresh",
    )
    require(
        gl_mapped_commit,
        "preparedPass.projectedEntry->lastUsedFrame = tr.frameCount",
        "OpenGL projected shadow-map cache residency refresh",
    )
    require_before(
        gl_mapped_commit,
        "RB_SharedWorldInteractionGLDrawMappedPrimitive",
        "preparedPass.pointEntry->lastUsedFrame = tr.frameCount",
        "OpenGL cache residency refresh after mapped commit",
    )
    for token in (
        "g_pointShadowMapProgram.translucentShadowMap[i]",
        "g_shadowMapProgram.translucentShadowMap[i]",
        "glUniform1iARB( momentMap, 6 )",
    ):
        require(gl_mapped_begin, token, "OpenGL neutral mapped moment samplers")
    reject_raw_authored_reads(gl_mapped_begin, "OpenGL mapped receiver state")
    reject_raw_authored_reads(gl_mapped_draw, "OpenGL mapped receiver draw")
    for supplement in (
        "CLASSIC_INTERACTION_SHADOW_CHAIN_SUPPLEMENT_GLOBAL",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_SUPPLEMENT_LOCAL",
    ):
        require(gl_draw, supplement, "OpenGL hybrid supplement order")
    require(gl_draw, "prepared.shadowMapPassCount", "OpenGL map-pass coverage")
    require(gl_draw, "prepared.hybridShadowPassCount", "OpenGL hybrid-pass coverage")
    for token in (
        "preparedVolumeMode != mode",
        "includeLocal && !preparedVolumeIncludesLocal",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_STENCIL_GLOBAL",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_STENCIL_LOCAL",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_SUPPLEMENT_GLOBAL",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_SUPPLEMENT_LOCAL",
    ):
        require(gl_draw, token, "OpenGL receiver-order physical stencil replay")
    require_before(
        gl_shadow_draw,
        "RB_SharedWorldInteractionGLDrawShadowCaster(",
        "logicalVolumeDraws++",
        "OpenGL physical logical-volume count after draw",
    )
    require_before(
        gl_shadow_draw,
        "RB_SharedWorldInteractionGLDrawShadowCaster(",
        "preloadVolumeDraws += caster.caster->preload ? 1 : 0",
        "OpenGL physical preload count after draw",
    )
    for token in (
        "backEnd.renderTexture->MakeCurrent()",
        "idRenderTexture::BindNull()",
        "glViewport(",
        "glScissor(",
        "glUseProgramObjectARB( 0 )",
        "GL_ClearStateDelta()",
    ):
        require(gl_map_abort, token, "OpenGL mapped-transaction state restoration")
    require_before(
        gl_preflight,
        "RB_SharedWorldInteractionGLPreflightMapTransaction(",
        "prepared.ready = true",
        "OpenGL mapped transaction before whole-view publication",
    )

    vk_preflight = function_body(vk, "bool VK_ClassicInteraction_Preflight(")
    vk_draw = function_body(vk, "void VK_ClassicInteraction_DrawOwnedView(")
    vk_receiver_draw = function_body(
        vk, "static void VK_ClassicInteraction_DrawReceiverRange("
    )
    vk_shadow_draw = function_body(
        vk, "static void VK_ClassicInteraction_DrawShadowRange("
    )
    vk_fail = function_body(vk, "static bool VK_ClassicInteraction_Fail(")
    reject_raw_authored_reads(vk_preflight, "Vulkan shared preflight")
    reject_raw_authored_reads(vk_draw, "Vulkan shared draw")
    reject_raw_authored_reads(vk_receiver_draw, "Vulkan sealed receiver draw")
    reject_raw_authored_reads(vk_shadow_draw, "Vulkan sealed shadow draw")
    require(
        vk_preflight,
        "r_singleTriangle.GetBool()",
        "Vulkan single-triangle debug whole-view fallback",
    )
    require(vk, "R_ClassicInteractionDomain_ResolveTexture", "Vulkan sealed texture preflight")
    require(vk_preflight, "R_ClassicInteractionDomain_ViewPrimitive", "Vulkan sealed primitive consumption")
    require(vk_preflight, "R_ClassicInteractionDomain_LightShadowCaster", "Vulkan sealed shadow consumption")
    require(vk_receiver_draw, "prepared.draws", "Vulkan retained sealed draw plan")
    require(vk_shadow_draw, "prepared.shadows", "Vulkan retained sealed shadow plan")
    require(vk_fail, "VK_Exec_InteractionUniformRestore", "Vulkan uniform rollback helper")
    require(vk_fail, "VK_Exec_SharedInteractionGeometryRestore", "Vulkan geometry rollback helper")
    require(vk_draw, "R_ClassicInteractionDomain_RecordOwned", "Vulkan ownership reconciliation")

    for api in (
        "VK_ShadowMap_PreflightClassicInteractionView",
        "VK_ShadowMap_CommitClassicInteractionView",
        "VK_ShadowMap_AbortClassicInteractionView",
    ):
        require(vk_shadow_h, api, "Vulkan mapped transaction API")
        require(vk_shadow, api, "Vulkan mapped transaction implementation")
    vk_map_preflight = function_body(
        vk_shadow, "bool VK_ShadowMap_PreflightClassicInteractionView("
    )
    vk_map_abort = function_body(
        vk_shadow, "void VK_ShadowMap_AbortClassicInteractionView("
    )
    vk_map_commit = function_body(
        vk_shadow, "void VK_ShadowMap_CommitClassicInteractionView("
    )
    vk_shadow_descriptor = function_body(
        vk_shadow, "static bool VK_ClassicShadow_ResolveDescriptor("
    )
    for token in (
        "texture->filter != texture->image->GetFilter()",
        "texture->repeat != texture->image->GetRepeat()",
    ):
        require(vk_shadow_descriptor, token, "Vulkan sealed shadow sampler state")
    vk_shadow_physical_pass = function_body(
        vk_shadow, "static bool VK_ClassicShadow_ValidatePhysicalPass("
    )
    for token in (
        "sealed.casterSignature",
        "light.vLight->shadowMapCasterSignature",
        "sealed.hashedAlpha",
        "r_shadowMapHashedAlpha.GetBool()",
        "sealed.stableAlphaHash",
        "r_shadowMapStableAlphaHash.GetBool()",
        "sealed.casterCullMode",
        "r_shadowMapCasterCulling.GetInteger()",
        "sealed.polygonFactor",
        "r_shadowMapPolygonFactor.GetFloat()",
        "sealed.polygonOffset",
        "r_shadowMapPolygonOffset.GetFloat()",
    ):
        require(vk_shadow_physical_pass, token, "Vulkan sealed shadow cache identity")
    for token in (
        "R_ClassicInteractionDomain_ViewLight(",
        "R_ClassicInteractionDomain_LightShadowMapPass(",
        "CLASSIC_INTERACTION_SHADOW_MAP_PASS_MAPPED",
        "CLASSIC_INTERACTION_SHADOW_MAP_PASS_HYBRID",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_GLOBAL_STATIC",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_GLOBAL_DYNAMIC",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_LOCAL_STATIC",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_MAP_LOCAL_DYNAMIC",
        "sealed->mappedCasterCount",
        "sealed->drawableMappedCasters",
        "sealed->noopMappedCasters",
        "reconciledPasses != view->shadowMapPassCount",
        "VK_ShadowMap_PrepareViewLights( view->viewDef, true )",
        "transaction.ready = true",
    ):
        require(vk_map_preflight, token, "Vulkan sealed mapped preflight")
    for command in ("vkCmdDraw", "vkCmdClear", "VK_ShadowMap_RenderAtlas("):
        reject(vk_map_preflight, command, "Vulkan attachment write during mapped preflight")
    require(vk_map_abort, "VK_ShadowMap_ReleasePreparedLights( false )", "Vulkan mapped resource rollback")
    require(vk_map_abort, "memset( &vkClassicShadowTransaction", "Vulkan mapped transaction reset")
    require(vk_map_commit, "VK_ShadowMap_RenderAtlas( view->viewDef )", "Vulkan mapped transaction commit")
    require(vk_map_commit, "common->FatalError(", "Vulkan infallible commit assertion")
    require_before(
        vk_fail,
        "VK_ShadowMap_AbortClassicInteractionView(",
        "VK_Exec_SharedInteractionGeometryRestore",
        "Vulkan mapped abort before shared geometry rollback",
    )
    require_before(
        vk_preflight,
        "VK_Exec_SharedInteractionGeometryCheckpoint()",
        "VK_ShadowMap_PreflightClassicInteractionView( view )",
        "Vulkan geometry checkpoint before mapped reservations",
    )
    require_before(
        vk_preflight,
        "VK_Exec_PrepareTriGeometry(",
        "VK_Exec_SharedInteractionGeometryCommit();",
        "Vulkan receiver geometry preflight before transaction commit",
    )
    require_before(
        vk_preflight,
        "VK_Exec_PrepareShadowGeometry(",
        "VK_Exec_SharedInteractionGeometryCommit();",
        "Vulkan shadow geometry preflight before transaction commit",
    )
    require_before(
        vk_preflight,
        "VK_Exec_SharedInteractionGeometryCommit();",
        "prepared.ready = true",
        "Vulkan geometry transaction before whole-view publication",
    )
    for command in ("vkCmdDraw(", "vkCmdDrawIndexed(", "vkCmdClearAttachments("):
        reject(vk_preflight, command, "Vulkan attachment write during interaction preflight")
    for token in (
        "VK_Exec_ShadowUniformAlloc(",
        "plan.projectedShadowBlock",
        "plan.pointShadowBlock",
        "plan.shadowUniformOffset",
    ):
        require(vk_preflight, token, "Vulkan mapped receiver uniform preflight")
    for token in (
        "prepared.projectedInteractionPipeline",
        "prepared.pointInteractionPipeline",
        "prepared.mappedInteractionLayout",
        "setCount = plan.mappedShadowMode != 0 ? 8u : 7u",
        "plan.mappedShadowMode != 0 ? 2u : 1u",
    ):
        require(vk_receiver_draw, token, "Vulkan mapped receiver submission")
    require_before(
        vk_draw,
        "VK_ShadowMap_CommitClassicInteractionView( prepared.view )",
        "vkCmdSetViewport(",
        "Vulkan mapped commit before main-target interaction writes",
    )
    for supplement in (
        "CLASSIC_INTERACTION_SHADOW_CHAIN_SUPPLEMENT_GLOBAL",
        "CLASSIC_INTERACTION_SHADOW_CHAIN_SUPPLEMENT_LOCAL",
    ):
        require(vk_draw, supplement, "Vulkan hybrid supplement order")
    require(vk_draw, "prepared.view->shadowMapPassCount", "Vulkan map-pass coverage")
    require(vk_draw, "prepared.view->hybridShadowPassCount", "Vulkan hybrid-pass coverage")
    vk_atlas = function_body(vk_shadow, "bool VK_ShadowMap_RenderAtlas(")
    for token in (
        "classicCommit",
        "VK_ClassicShadow_FindPassPlan(",
        "VK_ClassicShadow_DrawProjectedPass(",
        "VK_ClassicShadow_DrawPointPass(",
    ):
        require(vk_atlas, token, "Vulkan sealed mapped commit path")
    vk_view = function_body(vk_executor, "void VK_GuiExecutor_Draw3DView(")
    require_before(vk_view, "VK_ClassicInteraction_Preflight", "VK_ClassicInteraction_DrawOwnedView", "Vulkan preflight before shared draw")
    require(vk_view, "VK_Interactions_DrawLights( viewDef )", "untouched Vulkan classic fallback")
    vk_bind_tri = function_body(
        vk_executor, "bool VK_Exec_BindTriGeometry("
    )
    vk_bind_shadow = function_body(
        vk_executor, "bool VK_Exec_BindShadowGeometry("
    )
    for token in (
        "VK_Exec_CPUCacheValid(",
        "requiredBytes <= static_cast<size_t>( INT_MAX )",
        "cache->tag != TAG_FREE",
        "cache->size >= static_cast<int>( requiredBytes )",
        "cache->vbo == 0",
        "cache->virtMem != NULL",
    ):
        require(vk_executor, token, "Vulkan shared geometry cache validation")
    require(
        vk_bind_tri,
        "VK_Exec_UploadTriGeometry(",
        "Vulkan receiver/map-caster validated upload path",
    )
    require(
        vk_bind_shadow,
        "VK_Exec_CPUCacheValid(",
        "Vulkan stencil-volume cache validation",
    )
    vk_prepare_tri = function_body(
        vk_executor, "bool VK_Exec_PrepareTriGeometry("
    )
    require(
        vk_prepare_tri,
        "VK_Exec_PrepareTriGeometryOffsets(",
        "Vulkan checkpointed receiver geometry preparation",
    )

    require(benchmark, 'append_set(args, "r_rendererSharedWorldInteraction", "0")', "benchmark launch isolation")
    require(benchmark, '"r_rendererSharedWorldInteraction 0"', "benchmark script isolation")
    require(benchmark, '"interaction": {', "controlled interaction runtime profile")
    require(benchmark, '"setviewpos 0 -192 96 20 90 0"', "controlled interaction camera")
    require(
        benchmark,
        '"testModel models/mapobjects/strogg/crates/crate1_small.lwo"',
        "controlled stock interaction caster",
    )
    require(
        benchmark,
        "spawn func_static model models/mapobjects/strogg/crates/crate1_medium.lwo origin \"-90 -70 -5.7\"",
        "controlled stock interaction shadow receiver",
    )
    require(
        benchmark,
        "testPointLight 300 origin \"96 -128 0\"",
        "controlled side point shadow light",
    )
    require(benchmark, '"testLight"', "controlled stock projected shadow light")
    for preset in (
        '"unshadowed": {',
        '"stencil": {',
        '"mapped": {',
        '"csm": {',
        '"mixed": {',
        '"map-budget-fallback": {',
    ):
        require(benchmark, preset, "controlled interaction shadow preset")
    for runtime_target in (
        '"interaction-shadow-stock": {',
        '"interactionShadowTarget": "dynamic"',
        '"interactionShadowTarget": "perforated"',
        '"interactionShadowTarget": "hybrid"',
        '"interactionShadowTarget": "fallback"',
        '"sharedInteractionMaps"',
        'def shared_interaction_map_records(',
        '"shared interaction per-map reconciliation="',
        '"shared interaction dynamic caster feature missing="',
        '"shared interaction perforated caster feature missing="',
        '"shared interaction same-light hybrid="',
        "atomic fallback=",
    ):
        require(benchmark, runtime_target, "complete interaction-shadow runtime gate")
    require(
        benchmark,
        "def compare_screenshot_difference_if_requested(",
        "visible shadow image-difference gate",
    )
    require(
        benchmark,
        '"--difference-reference-dir"',
        "shadow-off engine-TGA reference option",
    )
    require(benchmark, '"sharedInteraction": extract_last_line', "runtime ownership diagnostics")
    require(
        init,
        "features=%d+%d+%d+%d",
        "per-map static/dynamic/alpha/translucent diagnostics",
    )
    require(benchmark, "def interaction_expectation(", "runtime ownership expectation")
    require(benchmark, "def evaluate_shared_interaction_evidence(", "runtime ownership evidence gate")
    require(
        benchmark,
        'match = re.match(r"^\\s*([+-]?\\d+)", value)',
        "engine-compatible boolean expectation parsing",
    )
    require(
        benchmark,
        'for backend_name in ("GL", "VK"):',
        "disabled runtime zero-coverage gate",
    )
    require(
        benchmark,
        '"shared interaction primitive reconciliation="',
        "owned runtime primitive reconciliation gate",
    )
    require(
        benchmark,
        '"shared interaction rollback {name}="',
        "fallback runtime zero-draw gate",
    )
    require(
        benchmark,
        '"shared interaction complete-view fallback="',
        "fallback runtime complete-view coverage gate",
    )
    require(baseline, 'add_set(args, "r_rendererSharedWorldInteraction", 0)', "stock baseline isolation")
    require(matrix, "+rendererClassicInteractionDomainSelfTest", "safe startup self-test")
    require(matrix, '"id": "sp-mv2-interaction"', "safe/manual interaction case")

    test_path = "tools/tests/renderer_classic_interaction_domain.py"
    require(validation, '"renderer_classic_interaction_domain.py"', "local validation registration")
    require(commit_ci, test_path, "commit CI registration")
    require(push_ci, test_path, "push CI registration")
    require(roadmap, "four complete domains", "roadmap delivery status")
    require(roadmap, "unsupported special-view nesting ownership", "roadmap next target")
    require(domain_doc, "shadow-coupled interaction ownership", "completed shadow domain")
    require(domain_doc, "translucent moment-map casters", "intentional shadow fallback")
    require(domain_doc, "features=static+dynamic+alpha+translucent", "mapped feature diagnostics")
    require(domain_doc, "whole-view fallback", "domain documentation")
    require(domain_doc, "original openQ4 work", "source provenance statement")

    print("renderer_classic_interaction_domain: ok")


if __name__ == "__main__":
    main()
