#!/usr/bin/env python3
"""Static regression contract for shared cinematic and authored-post ownership."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TEST_PATH = "tools/tests/renderer_classic_cinematic_post_domain.py"


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def require_before(haystack: str, first: str, second: str, context: str) -> None:
    first_at = haystack.find(first)
    second_at = haystack.find(second)
    if first_at < 0 or second_at < 0 or first_at >= second_at:
        raise AssertionError(f"Expected {first!r} before {second!r} in {context}")


def function_body(source: str, signature: str, context: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Missing {signature!r} in {context}")
    opening = source.find("{", start + len(signature))
    if opening < 0:
        raise AssertionError(f"Missing opening brace for {signature!r} in {context}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    raise AssertionError(f"Unterminated {signature!r} in {context}")


def validate_sealed_domain() -> None:
    header = read("src/renderer/ClassicCinematicPostDomain.h")
    source = read("src/renderer/ClassicCinematicPostDomain.cpp")
    packets = read("src/renderer/ScenePackets.cpp")

    for token in (
        "CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC",
        "CLASSIC_CINEMATIC_POST_SCOPE_AUTHORED_POST",
        "CLASSIC_CINEMATIC_POST_FAILURE_SOURCE_PACKET_MISMATCH",
        "CLASSIC_CINEMATIC_POST_FAILURE_CINEMATIC_CLOCK",
        "CLASSIC_CINEMATIC_POST_FAILURE_BACKEND_COVERAGE_MISMATCH",
        "classicCinematicPostDomainView_t",
        "cinematicTimeMilliseconds",
        "currentRenderStageCount",
        "currentDepthStageCount",
        "R_ClassicCinematicPostDomain_PrepareFrame",
        "R_ClassicCinematicPostDomain_RecordOwned",
        "R_ClassicCinematicPostDomain_RecordBackendFallback",
        "RendererClassicCinematicPostDomain_RunSelfTest",
    ):
        require(header, token, "cinematic/post domain interface")

    for token in (
        "R_ClassicCinematicPostDomain_IsRootGUIPacketEligible",
        "stage->texture.cinematic",
        "MF_NEED_CURRENT_RENDER",
        "RENDER_PASS_GUI",
        "RENDER_PASS_AUTHORED_POST",
        "SCENE_PACKET_CATEGORY_POST_PROCESS",
        "packet.legacyDrawSurf != drawSurf",
        "viewDef->floatTime",
        "cinematicClock",
        "RecordBackendFallback",
    ):
        require(source, token, "sealed source, timing, and fallback handling")
    require_before(
        source,
        "int packetCursor = pass.firstDrawPacket;",
        "R_ClassicCinematicPostDomain_CommitView( view );",
        "root packet reconciliation before commit",
    )
    require(
        packets,
        "R_ScenePackets_AddFilteredDrawSurfPass( packetFrame, viewDef, RENDER_PASS_AUTHORED_POST",
        "authored post packet production",
    )


def validate_backend_handoffs() -> None:
    gl = read("src/renderer/draw_common.cpp")
    gl_backend = read("src/renderer/tr_backend.cpp")
    vk = read("src/renderer/Vulkan/vk_GuiExecutor.cpp")
    vk_backend = read("src/renderer/Vulkan/vk_Backend.cpp")

    for token in (
        "RB_DrawSharedCinematicRootView",
        "RB_DrawSharedAuthoredPostView",
        "RB_STD_DrawShaderPasses",
        "R_ClassicCinematicPostDomain_RecordOwned",
        "R_ClassicCinematicPostDomain_RecordBackendFallback",
    ):
        require(gl, token, "OpenGL dynamic-stage adapter")
    root = function_body(gl, "bool RB_DrawSharedCinematicRootView(", "OpenGL root adapter")
    require(root, "RB_BeginDrawingView();", "OpenGL root view setup")
    require(root, "RB_STD_DrawShaderPasses", "OpenGL root dynamic-stage execution")
    post = function_body(gl, "bool RB_DrawSharedAuthoredPostView(", "OpenGL post adapter")
    require(post, "drawSurfs + firstSourceSurface", "OpenGL exact authored post tail")

    for source, backend in ((gl_backend, "OpenGL"), (vk_backend, "Vulkan")):
        require(source, "R_ClassicCinematicPostDomain_ResetFrame();", f"{backend} reset")
        require(source, "R_ClassicCinematicPostDomain_PrepareFrame( *scenePackets );", f"{backend} packet preparation")
    require(gl_backend, "RB_DrawSharedCinematicRootView( drawView )", "OpenGL root dispatch")

    for token in (
        "sharedCinematicRoot",
        "sharedAuthoredPostCandidate",
        "VK_Exec_DrawAmbientStages",
        "R_ClassicCinematicPostDomain_RecordOwned",
        "R_ClassicCinematicPostDomain_RecordBackendFallback",
    ):
        require(vk, token, "Vulkan dynamic-stage adapter")
    vk_world = function_body(vk, "void VK_GuiExecutor_Draw3DView(", "Vulkan world adapter")
    require_before(
        vk_world,
        "sharedAuthoredPostCandidate",
        "R_ClassicCinematicPostDomain_RecordOwned( viewDef,",
        "Vulkan post ownership after the complete dynamic walk",
    )


def validate_controls_and_evidence() -> None:
    init = read("src/renderer/RenderSystem_init.cpp")
    local = read("src/renderer/tr_local.h")
    bootstrap = read("src/renderer/RendererBootstrap.cpp")
    matrix = read("tools/tests/renderer_validation_matrix.py")
    benchmark = read("tools/tests/renderer_gameplay_benchmark.py")
    baseline = read("tools/validation/stock_asset_baseline.py")
    registry = read("tools/validation/openq4_validate.py")
    roadmap = read("docs/dev/idtech5-modernization-roadmap.md")
    domain_doc = read("docs/dev/classic-cinematic-post-domain-modernization.md")

    require(init, 'r_rendererSharedCinematicPost( "r_rendererSharedCinematicPost", "0"', "default-off cvar")
    require(init, 'cmdSystem->AddCommand( "rendererClassicCinematicPostDomainSelfTest"', "native self-test")
    require(init, "Renderer shared cinematic/post:", "gfxInfo diagnostics")
    require(local, "extern idCVar r_rendererSharedCinematicPost;", "renderer cvar API")
    require(bootstrap, '{ "r_rendererSharedCinematicPost", &r_rendererSharedCinematicPost, 0 }', "default safety")
    for source, context in (
        (matrix, "safe renderer matrix"),
        (benchmark, "benchmark isolation"),
        (baseline, "stock-baseline isolation"),
    ):
        require(source, "r_rendererSharedCinematicPost", context)
    require(matrix, "+rendererClassicCinematicPostDomainSelfTest", "safe native self-test")
    require(registry, '"renderer_classic_cinematic_post_domain.py"', "validation registry")
    require(roadmap, "Cinematic/post special-view nesting ownership", "advanced roadmap target")
    for token in (
        "videoMap",
        "soundMap",
        "_currentRender",
        "r_rendererSharedCinematicPost 1",
        "classic fallback",
        "engine-written",
    ):
        require(domain_doc, token, "domain documentation")


def main() -> int:
    validate_sealed_domain()
    validate_backend_handoffs()
    validate_controls_and_evidence()
    print("renderer_classic_cinematic_post_domain: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
