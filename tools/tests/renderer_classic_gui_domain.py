#!/usr/bin/env python3
"""Static regression contract for whole-view shared classic GUI ownership."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TEST_PATH = "tools/tests/renderer_classic_gui_domain.py"


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def reject(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        raise AssertionError(f"Unexpected {needle!r} in {context}")


def require_order(haystack: str, needles: tuple[str, ...], context: str) -> None:
    previous = -1
    for needle in needles:
        position = haystack.find(needle, previous + 1)
        if position == -1:
            raise AssertionError(f"Missing {needle!r} in {context}")
        if position <= previous:
            raise AssertionError(
                f"Expected ordered contracts in {context}: {needles!r}"
            )
        previous = position


def braced_body(source: str, marker: str, context: str) -> str:
    start = source.find(marker)
    if start == -1:
        raise AssertionError(f"Missing {marker!r} in {context}")
    opening_brace = source.find("{", start + len(marker))
    if opening_brace == -1:
        raise AssertionError(f"Missing opening brace after {marker!r} in {context}")

    depth = 0
    for index in range(opening_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"Could not find closing brace for {marker!r} in {context}")


def validate_neutral_contract() -> None:
    header = read("src/renderer/RendererContracts.h")
    source = read("src/renderer/RendererContracts.cpp")
    for token in (
        "rendererEvaluatedMaterialPass_t",
        "RENDERER_MATERIAL_PASS_INACTIVE_CONDITION",
        "RENDERER_MATERIAL_PASS_NOOP_ZERO_ONE_BLEND",
        "RENDERER_MATERIAL_PASS_NOOP_BLACK_ADDITIVE",
        "RENDERER_MATERIAL_PASS_NOOP_TRANSPARENT_ALPHA",
        "RendererContracts_EvaluateMaterialPassList",
    ):
        require(header, token, "backend-neutral evaluated material contract")
    evaluator = braced_body(
        source,
        "rendererMaterialPassEvaluationStatus_t RendererContracts_EvaluateMaterialPassList(",
        "atomic evaluated material-list implementation",
    )
    require_order(
        evaluator,
        (
            "RendererContracts_ResetEvaluatedMaterialPassList( destination );",
            "RendererContracts_EvaluateMaterialPass(",
            "RendererContracts_ResetEvaluatedMaterialPassList( destination );",
            "return status;",
        ),
        "atomic evaluated material-list failure",
    )


def validate_material_table() -> None:
    header = read("src/renderer/MaterialResourceTable.h")
    source = read("src/renderer/MaterialResourceTable.cpp")
    for token in (
        "guiDomainReferenced",
        "firstGuiPass",
        "guiPassCount",
        "R_MaterialResourceTable_ResolveTextureResource",
        "R_MaterialResourceTable_CopyGuiPassList",
    ):
        require(header, token, "ordered GUI material resource contract")

    prepare = braced_body(
        source,
        "void R_MaterialResourceTable_PrepareFrame(",
        "GUI packet-derived material-table preparation",
    )
    require_order(
        prepare,
        (
            "drawPacket.passCategory == RENDER_PASS_GUI",
            "guiDomainReferenced[drawPacket.materialRecordIndex] = true;",
            "R_MaterialResourceTable_AddRecordFromSource(",
        ),
        "packet-derived GUI material admission",
    )

    add_binding = braced_body(
        source,
        "static int R_MaterialResourceTable_AddTextureBinding(",
        "ordered material texture bindings",
    )
    require(
        add_binding,
        "stage == NULL && R_MaterialResourceTable_HasSemanticBinding",
        "same-semantic authored-stage preservation",
    )
    require(
        source,
        "record.semanticBindingIndex[semantic] < 0",
        "first-binding semantic convenience index",
    )


def validate_transactional_domain() -> None:
    header = read("src/renderer/ClassicGuiDomain.h")
    source = read("src/renderer/ClassicGuiDomain.cpp")
    for token in (
        "CLASSIC_GUI_DOMAIN_SOURCE_SURFACE_DRAWABLE",
        "CLASSIC_GUI_DOMAIN_SOURCE_SURFACE_FALLBACK_DEPTH_HACK",
        "CLASSIC_GUI_DOMAIN_SOURCE_SURFACE_FALLBACK_DECAL_COLOR_STREAM",
        "classicGuiDomainBackendCoverage_t",
        "R_ClassicGuiDomain_RecordOwned",
        "R_ClassicGuiDomain_RecordBackendFallback",
    ):
        require(header, token, "whole-view GUI domain API")

    prepare = braced_body(
        source,
        "void R_ClassicGuiDomain_PrepareFrame(",
        "whole-view GUI domain preparation",
    )
    require(prepare, "R_ClassicGuiDomain_PrepareView", "per-view transaction")
    require(
        source,
        "RendererContracts_EvaluateMaterialPassList(",
        "single neutral register evaluator",
    )
    require(
        source,
        "R_MaterialResourceTable_ResolveTextureResource(",
        "opaque texture-resource resolution",
    )
    require(
        source,
        "RendererClassicGuiDomain_RunSelfTest",
        "whole-view rollback self-test",
    )


def validate_gl_consumer() -> None:
    backend = read("src/renderer/tr_backend.cpp")
    draw = read("src/renderer/draw_common.cpp")
    modern = read("src/renderer/ModernGLExecutor.cpp")
    handoff = braced_body(
        backend,
        "void RB_ExecuteBackEndCommands(",
        "OpenGL command handoff",
    )
    require_order(
        handoff,
        (
            "R_ClassicGuiDomain_ResetFrame();",
            "cmds->commandId == RC_NOP && !cmds->next",
            "R_ClassicGuiDomain_PrepareFrame(",
            "RB_DrawSharedGuiView(",
            "RB_DrawView( cmds );",
        ),
        "OpenGL shared-view then untouched classic fallback",
    )

    consumer = braced_body(
        draw,
        "bool RB_DrawSharedGuiView(",
        "OpenGL shared GUI consumer",
    )
    require_order(
        consumer,
        (
            "RB_SharedGuiGLPreflight(",
            "RB_BeginDrawingView();",
            "RB_DrawElementsWithCounters( tri );",
            "R_ClassicGuiDomain_RecordOwned(",
        ),
        "OpenGL preflight-before-commit ordering",
    )
    reject(consumer, "shaderRegisters", "OpenGL sealed material consumer")
    reject(consumer, "GetStage(", "OpenGL sealed material consumer")
    require(
        draw,
        "R_ClassicGuiDomain_DrawPassTexture(",
        "OpenGL opaque resource consumption",
    )
    require(
        draw,
        "RB_SharedGuiGLSourceNoopValid(",
        "OpenGL recognized source no-op accounting",
    )

    submit_plan = braced_body(
        modern,
        "static void R_ModernGLExecutor_SubmitPlan(",
        "older aggregate OpenGL submit plan",
    )
    require_order(
        submit_plan,
        (
            "r_rendererSharedGui.GetBool()",
            "command.pipeline == MODERN_GL_DRAW_PLAN_PIPELINE_GUI",
            "continue;",
        ),
        "shared GUI suppression in the older aggregate submit plan",
    )
    submit_gui = braced_body(
        modern,
        "static void R_ModernGLExecutor_SubmitModernGui(",
        "older aggregate OpenGL GUI replay",
    )
    require(
        submit_gui,
        "r_rendererSharedGui.GetBool()",
        "shared GUI suppression in older aggregate GUI replay",
    )
    visible_request = braced_body(
        modern,
        "static bool R_ModernGLExecutor_ModernVisibleRequested(",
        "modern visible request gate",
    )
    require(visible_request, "!r_skipRender.GetBool()", "skip-render GUI ordering gate")
    require(
        visible_request,
        "!r_skipRenderContext.GetBool()",
        "skip-render-context GUI ordering gate",
    )


def validate_vulkan_consumer() -> None:
    backend = read("src/renderer/Vulkan/vk_Backend.cpp")
    executor = read("src/renderer/Vulkan/vk_GuiExecutor.cpp")
    require_order(
        backend,
        (
            "R_MaterialResourceTable_PrepareFrame(",
            "R_ClassicGuiDomain_PrepareFrame(",
        ),
        "Vulkan neutral frame preparation",
    )

    consumer = braced_body(
        executor,
        "static bool VK_ClassicGui_DrawOwnedView(",
        "Vulkan shared GUI consumer",
    )
    require_order(
        consumer,
        (
            "VK_GuiExecutor_GetPipelineStrict(",
            "VK_ClassicGui_PrepareGeometry(",
            "// Commit:",
            "vkCmdDrawIndexed(",
            "R_ClassicGuiDomain_RecordOwned(",
        ),
        "Vulkan preflight-before-commit ordering",
    )
    reject(consumer, "shaderRegisters", "Vulkan sealed material consumer")
    reject(consumer, "GetStage(", "Vulkan sealed material consumer")

    draw_2d = braced_body(
        executor,
        "void VK_GuiExecutor_Draw2DView(",
        "Vulkan 2D view handoff",
    )
    require_order(
        draw_2d,
        (
            "VK_ClassicGui_DrawOwnedView( viewDef )",
            "VK_Exec_DrawAmbientStages( viewDef, drawSurf, tri, mvp, false );",
        ),
        "Vulkan shared-view then untouched classic fallback",
    )
    require(
        executor,
        "VK_ClassicGui_SourceNoopValid(",
        "Vulkan recognized source no-op accounting",
    )
    require(
        consumer,
        "planDrawCount",
        "Vulkan drawable-only commit plan",
    )

    gui_fragment = read("src/renderer/Vulkan/shaders/gui.frag")
    require(gui_fragment, "color.a != pc.params.z", "exact Vulkan EQ_255 alpha test")
    reject(gui_fragment, "0.5 / 255.0", "approximate Vulkan EQ_255 alpha test")


def validate_control_and_evidence() -> None:
    init = read("src/renderer/RenderSystem_init.cpp")
    bootstrap = read("src/renderer/RendererBootstrap.cpp")
    benchmark = read("tools/tests/renderer_gameplay_benchmark.py")
    baseline = read("tools/validation/stock_asset_baseline.py")
    require(
        init,
        'idCVar r_rendererSharedGui( "r_rendererSharedGui", "0"',
        "default-off shared GUI control",
    )
    require(bootstrap, '{ "r_rendererSharedGui", &r_rendererSharedGui, 0 }', "rollback report")
    require(benchmark, 'append_set(args, "r_rendererSharedGui", "0")', "benchmark isolation")
    require(baseline, 'add_set(args, "r_rendererSharedGui", 0)', "stock baseline isolation")


def validate_ci_registration() -> None:
    validator = read("tools/validation/openq4_validate.py")
    commit = read(".github/workflows/commit-validation.yml")
    push = read(".github/workflows/push-verification.yml")
    if validator.count("renderer_classic_gui_domain.py") != 1:
        raise AssertionError("Local validation must register this test exactly once")
    for workflow, context in (
        (commit, "commit validation workflow"),
        (push, "push verification workflow"),
    ):
        if workflow.count(TEST_PATH) != 2:
            raise AssertionError(f"{context} must compile and directly run {TEST_PATH}")
        require(workflow, f"python {TEST_PATH}", context)


def main() -> None:
    validate_neutral_contract()
    validate_material_table()
    validate_transactional_domain()
    validate_gl_consumer()
    validate_vulkan_consumer()
    validate_control_and_evidence()
    validate_ci_registration()
    print("renderer_classic_gui_domain: ok")


if __name__ == "__main__":
    main()
