#!/usr/bin/env python3
"""Static regression contract for capture-backed shared subview ownership."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TEST_PATH = "tools/tests/renderer_classic_subview_domain.py"


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def reject(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        raise AssertionError(f"Unexpected {needle!r} in {context}")


def braced_body(source: str, marker: str, context: str) -> str:
    start = source.find(marker)
    if start == -1:
        raise AssertionError(f"Missing {marker!r} in {context}")
    opening = source.find("{", start + len(marker))
    if opening == -1:
        raise AssertionError(f"Missing opening brace after {marker!r} in {context}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"Could not find closing brace for {marker!r} in {context}")


def validate_packet_capture_edge() -> None:
    header = read("src/renderer/ScenePackets.h")
    source = read("src/renderer/ScenePackets.cpp")
    for token in (
        "SCENE_PACKET_MAX_SUBVIEW_CAPTURES",
        "SCENE_PACKET_OVERFLOW_SUBVIEW_CAPTURES",
        "sceneSubviewCapture_t",
        "viewScenePacketIndex",
        "AddSubviewCapture",
        "NumSubviewCaptures",
        "SubviewCapture",
        "R_ScenePackets_AddCopyRender( idImage *image, int x, int y",
    ):
        require(header, token, "packet-level capture edge")
    for token in (
        "rg_frontEndLastDrawView",
        "R_ScenePackets_FrontEndCaptureRequired",
        "r_rendererSharedSubview.GetBool()",
        "R_ScenePackets_AddCopyRender( idImage *image, int x, int y",
        "rg_frontEndScenePacketFrame.AddSubviewCapture",
        "rg_frontEndLastDrawView = NULL;",
        "R_ScenePackets_BuildLegacyCommandStream",
        "packetFrame.AddSubviewCapture( lastDrawView",
        "subviewCaptures=%d",
    ):
        require(source, token, "front-end and legacy capture association")


def validate_domain_contract() -> None:
    header = read("src/renderer/ClassicSubviewDomain.h")
    source = read("src/renderer/ClassicSubviewDomain.cpp")
    combined = header + source
    for token in (
        "CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA",
        "CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION",
        "CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_CAPTURE",
        "CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_VIEWPORT_MISMATCH",
        "CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_CAPTURE_MISMATCH",
        "CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL",
        "CLASSIC_SUBVIEW_DOMAIN_BACKEND_VULKAN",
        "R_ClassicSubviewDomain_ResetFrame",
        "R_ClassicSubviewDomain_PrepareFrame",
        "R_ClassicSubviewDomain_CaptureMatches",
        "R_ClassicSubviewDomain_RecordOwned",
        "R_ClassicSubviewDomain_RecordBackendFallback",
        "RendererClassicSubviewDomain_RunSelfTest",
    ):
        require(combined, token, "shared subview domain API")
    for token in (
        "FindScenePacket( packetFrame, view.parentViewDef )",
        "ParentContainsSurface",
        "FindCaptureKind",
        "DI_REMOTE_RENDER",
        "DI_REFRACTION_RENDER",
        "viewDef->numClipPlanes != 0",
        "viewDef->isMirror",
        "viewDef->isXraySubview",
        "capture.copyDepth",
        "capture.x != viewDef->viewport.x1",
        "ready = true",
        "fallbackViews",
    ):
        require(source, token, "bounded subview admission and fallback")
    reject(source, "DI_MIRROR_RENDER", "capture-backed subview scope")


def validate_backend_capture_ownership() -> None:
    gl = read("src/renderer/tr_backend.cpp")
    vk = read("src/renderer/Vulkan/vk_Backend.cpp")
    for source, backend in ((gl, "GL"), (vk, "Vulkan")):
        for token in (
            "R_ClassicSubviewDomain_PrepareFrame( *scenePackets );",
            "R_ClassicSubviewDomain_CaptureMatches",
            "R_ClassicSubviewDomain_RecordOwned",
            "R_ClassicSubviewDomain_RecordBackendFallback",
            "pendingSharedSubview",
        ):
            require(source, token, f"{backend} capture ownership")
        copy_case = braced_body(source, "case RC_COPY_RENDER:", f"{backend} copy transaction")
        require(copy_case, "sharedCaptureMatches", f"{backend} sealed copy decision")
        require(copy_case, "pendingSharedSubview->captureImage", f"{backend} sealed capture image")
        require(copy_case, "RecordOwned", f"{backend} post-copy ownership")
    require(gl, "RB_ClassicSubview_CopyOwned", "GL sealed capture consumer")
    require(vk, "VK_Exec_CopyRender(", "Vulkan capture consumer")


def validate_controls_diagnostics_and_registration() -> None:
    init = read("src/renderer/RenderSystem_init.cpp")
    local = read("src/renderer/tr_local.h")
    bootstrap = read("src/renderer/RendererBootstrap.cpp")
    matrix = read("tools/tests/renderer_validation_matrix.py")
    baseline = read("tools/validation/stock_asset_baseline.py")
    benchmark = read("tools/tests/renderer_gameplay_benchmark.py")
    registry = read("tools/validation/openq4_validate.py")
    docs = read("docs/dev/classic-subview-domain-modernization.md")
    for token in (
        'r_rendererSharedSubview( "r_rendererSharedSubview", "0"',
        '"rendererClassicSubviewDomainSelfTest"',
        "RendererClassicSubviewDomain self-test passed",
        "classicSubviewDomain requested=",
        "classicSubviewDomain backend=",
    ):
        require(init, token, "default-off subview control and diagnostics")
    require(local, "extern idCVar r_rendererSharedSubview;", "renderer cvar API")
    require(bootstrap, '"r_rendererSharedSubview", &r_rendererSharedSubview, 0', "bootstrap default")
    for source, context in (
        (matrix, "renderer validation matrix"),
        (baseline, "stock-baseline isolation"),
        (benchmark, "benchmark isolation"),
    ):
        require(source, "r_rendererSharedSubview", context)
    require(registry, '"renderer_classic_subview_domain.py"', "validation registry")
    for token in (
        "capture-backed",
        "remote-camera",
        "refraction",
        "mirror",
        "r_rendererSharedSubview 1",
        "classic fallback",
    ):
        require(docs, token, "subview contract documentation")
    for workflow_path in (
        ".github/workflows/commit-validation.yml",
        ".github/workflows/push-verification.yml",
    ):
        workflow = read(workflow_path)
        if workflow.count(TEST_PATH) != 2:
            raise AssertionError(
                f"Expected compile and execution registration in {workflow_path}"
            )


def main() -> int:
    validate_packet_capture_edge()
    validate_domain_contract()
    validate_backend_capture_ownership()
    validate_controls_diagnostics_and_registration()
    print("renderer_classic_subview_domain: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
