#!/usr/bin/env python3
"""Regression contract for engine-side render-geometry allocator lifetime."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TEST_PATH = "tools/tests/dmap_render_geometry_lifecycle.py"


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def require_order(haystack: str, needles: tuple[str, ...], context: str) -> None:
    previous = -1
    for needle in needles:
        position = haystack.find(needle, previous + 1)
        if position == -1:
            raise AssertionError(f"Missing {needle!r} in {context}")
        if position <= previous:
            raise AssertionError(f"Expected ordered snippets in {context}: {needles!r}")
        previous = position


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


def validate_allocator_lifecycle() -> None:
    source = read("src/render_geo/RenderGeometryTriSurf.cpp")
    require(source, "static bool\t\t\ttriSurfDataInitialized;", "triangle allocator state")

    init = braced_body(source, "void R_InitTriSurfData( void )", "triangle allocator initialization")
    require_order(
        init,
        (
            "if ( triSurfDataInitialized )",
            "return;",
            "triVertexAllocator.Init();",
            "triIndexAllocator.Init();",
            "triSurfDataInitialized = true;",
        ),
        "triangle allocator initialization",
    )

    shutdown = braced_body(source, "void R_ShutdownTriSurfData( void )", "triangle allocator shutdown")
    require_order(
        shutdown,
        (
            "if ( !triSurfDataInitialized )",
            "return;",
            "triSurfDataInitialized = false;",
            "R_StaticFree( silEdges );",
            "silEdges = NULL;",
            "triVertexAllocator.Shutdown();",
            "triIndexAllocator.Shutdown();",
        ),
        "triangle allocator shutdown",
    )


def validate_engine_tool_lifetime() -> None:
    common = read("src/framework/Common.cpp")
    require(common, '#include "../render_geo/RenderGeometry.h"', "Common render-geometry API")

    init = braced_body(common, "void idCommonLocal::Init(", "Common initialization")
    require_order(
        init,
        ("InitSIMD();", "R_InitTriSurfData();", "InitCommands();", "InitGame();"),
        "engine-side render-geometry initialization",
    )

    shutdown = braced_body(common, "void idCommonLocal::Shutdown( void )", "Common shutdown")
    require_order(
        shutdown,
        ("ShutdownGame( false );", "R_ShutdownTriSurfData();", "idLib::ShutDown();"),
        "engine-side render-geometry shutdown",
    )

    renderer = read("src/renderer/RenderSystem_init.cpp")
    require_order(
        renderer,
        ("R_InitTriSurfData();", "R_ShutdownTriSurfData();"),
        "renderer-owned render-geometry lifetime",
    )


def validate_build_and_ci_wiring() -> None:
    meson = read("meson.build")
    require(meson, "client_link_with = [bse_library, imagetools_library, render_geo_library]", "client render-geometry link")
    require(meson, "link_with: [renderer_idlib_library, imagetools_library, render_geo_library]", "renderer render-geometry link")

    validator = read("tools/validation/openq4_validate.py")
    if validator.count("dmap_render_geometry_lifecycle.py") != 1:
        raise AssertionError("Local validation must register the dmap render-geometry test exactly once")

    for workflow_path in (
        ".github/workflows/commit-validation.yml",
        ".github/workflows/push-verification.yml",
    ):
        workflow = read(workflow_path)
        if workflow.count(TEST_PATH) != 2:
            raise AssertionError(f"{workflow_path} must compile and run {TEST_PATH}")
        require(workflow, f"python {TEST_PATH}", workflow_path)


def main() -> None:
    validate_allocator_lifecycle()
    validate_engine_tool_lifetime()
    validate_build_and_ci_wiring()
    print("dmap_render_geometry_lifecycle: ok")


if __name__ == "__main__":
    main()
