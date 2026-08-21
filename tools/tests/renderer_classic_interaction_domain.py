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
        "classicInteractionDomainTexture_t",
        "R_ClassicInteractionDomain_ResetFrame",
        "R_ClassicInteractionDomain_PrepareFrame",
        "R_ClassicInteractionDomain_FindView",
        "R_ClassicInteractionDomain_ViewLight",
        "R_ClassicInteractionDomain_ViewSurface",
        "R_ClassicInteractionDomain_ViewPrimitive",
        "R_ClassicInteractionDomain_ResolveTexture",
        "R_ClassicInteractionDomain_RecordOwned",
        "R_ClassicInteractionDomain_RecordBackendFallback",
        "RendererClassicInteractionDomain_RunSelfTest",
    ):
        require(header, symbol, "shared interaction contract")

    for blocker in (
        "CLASSIC_INTERACTION_FAILURE_SHADOWS",
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

    prepare = function_body(source, "static bool R_ClassicInteractionDomain_PrepareView(")
    for token in (
        "firstLight",
        "firstSurface",
        "firstPrimitive",
        "ready = true",
        "interactionPassPacketIndex",
        "packetDrawCount",
    ):
        require(prepare, token, "transactional view preparation")
    require_before(prepare, "firstLight", "ready = true", "light arena before publication")
    require_before(prepare, "firstSurface", "ready = true", "surface arena before publication")
    require_before(prepare, "firstPrimitive", "ready = true", "primitive arena before publication")

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
    reject_raw_authored_reads(gl_preflight, "OpenGL shared preflight")
    reject_raw_authored_reads(gl_draw, "OpenGL shared draw")
    if "RB_GLSLPrepareInteractionVertexCache(" in gl_preflight:
        raise AssertionError(
            "OpenGL shared preflight must not re-enter the legacy material/cache helper"
        )
    require(gl_preflight, "R_ClassicInteractionDomain_ResolveTexture", "OpenGL sealed texture preflight")
    require(gl_preflight, "RB_SharedWorldInteractionGLCacheValid", "OpenGL sealed cache validation")
    require(gl_preflight, "R_TouchVertexCache", "OpenGL retained cache lifetime")
    require(gl, "R_ClassicInteractionDomain_ViewPrimitive", "OpenGL sealed primitive consumption")
    require(gl_draw, "prepared.primitives", "OpenGL retained sealed draw plan")
    require(gl_preflight, "preparedPrimitive.vertexBuffer = tri->ambientCache->vbo", "OpenGL retained validated vertex buffer")
    require(gl_preflight, "preparedPrimitive.indexPointer", "OpenGL retained validated index pointer")
    require(gl_draw, "idVertexCache::BindArrayBuffer", "OpenGL retained vertex binding")
    require(gl_draw, "idVertexCache::BindIndexBuffer", "OpenGL retained index binding")
    require(gl_draw, "glDrawElements", "OpenGL retained indexed draw")
    require(gl_draw, "R_ClassicInteractionDomain_RecordOwned", "OpenGL ownership reconciliation")
    for forbidden in (
        "legacyDrawSurf",
        "legacyViewLight",
        "vertexCache.Position(",
        "RB_DrawElementsWithCounters(",
    ):
        if forbidden in gl_draw:
            raise AssertionError(
                f"OpenGL committed shared draw re-enters unsealed geometry via {forbidden}"
            )
    for attrib in (1, 2, 5, 6, 7):
        require(
            gl_draw,
            f"glDisableVertexAttribArrayARB( {attrib} )",
            f"OpenGL canonical interaction attribute {attrib} reset",
        )
    gl_entry = function_body(gl, "void RB_ARB2_DrawInteractions( void )")
    require_before(gl_entry, "RB_ARB2_SharedInteractionPreflight", "RB_ARB2_DrawSharedInteractionView", "OpenGL preflight before shared draw")
    require(gl_entry, "R_ClassicInteractionDomain_RecordBackendFallback", "OpenGL whole-view fallback accounting")

    vk_preflight = function_body(vk, "bool VK_ClassicInteraction_Preflight(")
    vk_draw = function_body(vk, "void VK_ClassicInteraction_DrawOwnedView(")
    reject_raw_authored_reads(vk_preflight, "Vulkan shared preflight")
    reject_raw_authored_reads(vk_draw, "Vulkan shared draw")
    require(vk, "R_ClassicInteractionDomain_ResolveTexture", "Vulkan sealed texture preflight")
    require(vk_preflight, "R_ClassicInteractionDomain_ViewPrimitive", "Vulkan sealed primitive consumption")
    require(vk_draw, "prepared.draws", "Vulkan retained sealed draw plan")
    require(vk_draw, "R_ClassicInteractionDomain_RecordOwned", "Vulkan ownership reconciliation")
    vk_view = function_body(vk_executor, "void VK_GuiExecutor_Draw3DView(")
    require_before(vk_view, "VK_ClassicInteraction_Preflight", "VK_ClassicInteraction_DrawOwnedView", "Vulkan preflight before shared draw")
    require(vk_view, "VK_Interactions_DrawLights( viewDef )", "untouched Vulkan classic fallback")

    require(benchmark, 'append_set(args, "r_rendererSharedWorldInteraction", "0")', "benchmark launch isolation")
    require(benchmark, '"r_rendererSharedWorldInteraction 0"', "benchmark script isolation")
    require(benchmark, '"interaction": {', "controlled interaction runtime profile")
    require(benchmark, '"setviewpos 0 -192 96 20 90 0"', "controlled interaction camera")
    require(
        benchmark,
        '"testModel models/mapobjects/strogg/crates/crate1_small.lwo"',
        "controlled stock interaction receiver",
    )
    require(benchmark, '"sharedInteraction": extract_last_line', "runtime ownership diagnostics")
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

    test_path = "tools/tests/renderer_classic_interaction_domain.py"
    require(validation, '"renderer_classic_interaction_domain.py"', "local validation registration")
    require(commit_ci, test_path, "commit CI registration")
    require(push_ci, test_path, "push CI registration")
    require(roadmap, "three complete domains", "roadmap delivery status")
    require(roadmap, "shadow-coupled interaction ownership", "roadmap next target")
    require(domain_doc, "whole-view fallback", "domain documentation")
    require(domain_doc, "original openQ4 work", "source provenance statement")

    print("renderer_classic_interaction_domain: ok")


if __name__ == "__main__":
    main()
