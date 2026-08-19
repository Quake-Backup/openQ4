#!/usr/bin/env python3
"""Compatibility contracts for opt-in PBR material metadata and resources."""

from __future__ import annotations

import os
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GAME_ROOT = Path(
    os.environ.get("OPENQ4_GAMELIBS_REPO", ROOT.parent / "openQ4-game")
).resolve()


def read(path: Path) -> str:
    if not path.is_file():
        raise AssertionError(f"required source is missing: {path}")
    return path.read_text(encoding="utf-8")


def require(source: str, token: str, context: str) -> None:
    if token not in source:
        raise AssertionError(f"missing {token!r} in {context}")


def reject(source: str, token: str, context: str) -> None:
    if token in source:
        raise AssertionError(f"unexpected {token!r} in {context}")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing function {signature!r}")
    opening = source.find("{", start)
    if opening < 0:
        raise AssertionError(f"missing body for {signature!r}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    raise AssertionError(f"unterminated body for {signature!r}")


def section(source: str, start_marker: str, end_marker: str) -> str:
    start = source.find(start_marker)
    end = source.find(end_marker, start + len(start_marker))
    if start < 0 or end < 0:
        raise AssertionError(f"missing source section {start_marker!r}..{end_marker!r}")
    return source[start:end]


def test_shared_material_abi_and_opt_in_defaults() -> None:
    engine_header = read(ROOT / "src/renderer/Material.h")
    game_header = read(GAME_ROOT / "src/renderer/Material.h")
    if engine_header != game_header:
        raise AssertionError("engine and companion Material.h must remain byte-identical")

    require(engine_header, "legacyFallbackMissing", "explicit missing-fallback state")

    init = read(ROOT / "src/renderer/RenderSystem_init.cpp")
    for name, default in (
        ("r_pbrMaterials", "0"),
        ("r_pbrGeneratedLegacyFallback", "1"),
        ("r_pbrDebug", "0"),
        ("r_pbrInferFromLegacyMaterials", "0"),
    ):
        require(init, f'idCVar {name}( "{name}", "{default}"', "PBR cvar defaults")
    require(init, "PBR materials: parser=1 modernLighting=0", "honest gfxInfo capability")


def test_parser_is_namespaced_and_fail_closed() -> None:
    material = read(ROOT / "src/renderer/Material.cpp")
    parse_material = section(material, "void idMaterial::ParseMaterial( idLexer &src )", "void idMaterial::SetGui(")
    require(parse_material, '!token.Icmp( "pbr" )', "top-level PBR namespace")
    require(parse_material, '!token.Icmp( "physicallyBased" )', "PBR namespace alias")
    reject(parse_material, "r_pbrInferFromLegacyMaterials", "classic parser path")
    reject(parse_material, "pbrInfo.metallicRegister = GetExpressionConstant", "stock register allocation")

    parse_pbr = section(material, "bool idMaterial::ParsePBRBlock(", "void idMaterial::AddPBRLegacyFallbackStages(")
    for token in (
        "workflow",
        "albedoMap",
        "normalMap",
        "normalFormat",
        "ormMap",
        "metallicMap",
        "roughnessMap",
        "aoMap",
        "emissiveMap",
        "legacyBumpMap",
        "legacyDiffuseMap",
        "legacySpecularMap",
        "legacyEmissiveMap",
        "autoLegacyFallback",
    ):
        require(parse_pbr, f'!token.Icmp( "{token}" )', "PBR block parser")
    require(parse_pbr, "unknown PBR parameter", "unknown-token rejection")
    require(parse_pbr, "requires normalFormat", "normal encoding contract")
    require(parse_pbr, "cannot combine ormMap", "packed/separate exclusivity")
    require(parse_pbr, "pbrInfo.metallicRegister = GetExpressionConstant", "opt-in PBR register defaults")
    require(parse_pbr, '( value != "0" && value != "1" )', "literal auto-fallback boolean grammar")
    reject(parse_pbr, "value.GetIntValue()", "fractional auto-fallback coercion")
    require(material, "autoLegacyFallback 0.5", "fractional auto-fallback negative self-test")
    require(material, "albedoMap reflectionRenderMap", "dynamic image negative self-test")
    require(material, "albedoMap glslProgram", "shader-token image negative self-test")
    require(material, "albedoMap add( _white, reflectionRenderMap )", "nested dynamic image negative self-test")
    require(material, "albedoMap add( _white, glslProgram )", "nested shader-token image negative self-test")
    require(material, "albedoMap add( _white, _currentRender )", "nested scene-capture negative self-test")
    require(material, "albedoMap add( _white, _reflectionRender )", "nested mutable-target negative self-test")
    require(material, "albedoMap add( _white, alphaTest )", "nested stage-state negative self-test")

    unsupported_tokens = function_body(material, "static bool R_IsUnsupportedPBRImageProgramToken(")
    parse_image = section(material, "bool idMaterial::ParsePBRImage(", "bool idMaterial::ParsePBRBlock(")
    require(unsupported_tokens, "R_IsMutableRenderImageName( token.c_str() )", "central mutable-render-image rejection")
    image_manager = read(ROOT / "src/renderer/ImageManager.cpp")
    for mutable_name in (
        "_reflectionRender",
        "_refractionRender",
        "_currentRender",
        "_forwardRenderResolved",
        "_postProcessAlbedo",
        "_shadowMap",
        "_pointShadowMap",
        "_hdrScene",
        "_ssao",
        "_bloom",
    ):
        require(image_manager, f'"{mutable_name}"', "mutable render-image classifier")
    require(image_manager, "image->scratchImage = true;", "dynamic scratch-image classification")
    for rejected in (
        "videoMap",
        "soundMap",
        "mirrorRenderMap",
        "remoteRenderMap",
        "reflectionRenderMap",
        "refractionRenderMap",
        "xrayRenderMap",
        "cameraCubeMap",
        "cubeMap",
        "program",
        "glslProgram",
        "blend",
        "map",
        "screen",
        "screen2",
        "glassWarp",
        "texGen",
        "alphaTest",
        "alphaFunc",
        "translate",
        "centerScale",
        "shear",
        "rotate",
        "vertexColor",
        "color",
        "maskColor",
        "privatePolygonOffset",
    ):
        require(unsupported_tokens, f'!token.Icmp( "{rejected}" )', "dynamic PBR image rejection")
    require(parse_image, "R_IsUnsupportedPBRImageProgramToken( token )", "direct PBR image-token rejection")
    require(parse_image, "R_IsUnsupportedPBRImageProgramToken( imageProgramToken )", "nested PBR image-token rejection")
    require(parse_image, "R_IsMutableRenderImage( target.image )", "loaded mutable-image rejection")
    require(parse_image, "while ( imageProgram.ReadToken", "complete PBR image-program validation")
    for token in (
        "target.filter",
        "target.repeat",
        "target.allowPicmip",
        "target.noMips",
        "target.highQuality",
        "target.forceHighQuality",
    ):
        require(parse_image, token, "retained PBR image options")

    fallback = section(
        material,
        "void idMaterial::AddPBRLegacyFallbackStages( const textureRepeat_t trpDefault )",
        "void idMaterial::ParseDeform( idLexer &src )",
    )
    require(fallback, "const bool hasUsableClassicInteraction = hasBump && hasDiffuse;", "complete classic fallback authority")
    require(fallback, "!hasUsableClassicInteraction", "approximation requires incomplete classic interaction")
    require(fallback, "const bool allowApproximate", "approximation gate")
    require(fallback, "pbrInfo.normalFormat == PBR_NORMAL_QUAKE4_AGB", "classic normal encoding gate")
    for token in (
        'buffer.Append( "nearest\\n" )',
        'buffer.Append( "noclamp\\n" )',
        'buffer.Append( "nopicmip\\n" )',
        'buffer.Append( "nomips\\n" )',
        'buffer.Append( "forceHighQuality\\n" )',
    ):
        require(fallback, token, "classic fallback image-option replay")
    require(material, "diffuseStage->texture.image == expectedDiffuse", "runtime fallback image identity assertion")
    require(material, "pbrInfo.legacyFallbackMissing = !( hasBump && hasDiffuse );", "missing fallback classification")
    require(material, "has no usable classic bump+diffuse fallback", "missing fallback diagnostic")
    require(material, "_pbr_selftest_missing_fallback", "missing fallback runtime self-test")
    require(material, "_pbr_selftest_tangent_normal_fallback", "tangent normal fallback runtime self-test")
    require(material, "_pbr_selftest_quake4_normal_fallback", "Quake 4 normal fallback runtime self-test")
    require(material, "bumpStage->texture.image == globalImages->flatNormalMap", "tangent normal neutral fallback assertion")
    require(material, "bumpStage->texture.image == info.normal.image", "Quake 4 normal reuse assertion")
    require(material, "const bool oldGeneratedFallback = r_pbrGeneratedLegacyFallback.GetBool();", "fallback self-test cvar save")
    require(material, "r_pbrGeneratedLegacyFallback.SetBool( oldGeneratedFallback );", "fallback self-test cvar restore")


def test_texture_usage_and_lifecycle_contracts() -> None:
    image_header = read(ROOT / "src/renderer/Image.h")
    order = [image_header.find(name) for name in ("TD_HIGH_QUALITY", "TD_PBR_COLOR", "TD_MATERIAL_DATA")]
    if any(position < 0 for position in order) or order != sorted(order):
        raise AssertionError("PBR image usages must append after historical cache identities")

    image_load = read(ROOT / "src/renderer/Image_load.cpp")
    require(image_load, "case TD_PBR_COLOR:", "PBR color derivation")
    require(image_load, "opts.gammaMips = true;", "gamma-correct PBR color mips")
    require(image_load, "case TD_MATERIAL_DATA:", "linear material-data derivation")
    gamma_downsize = function_body(image_load, "static bool R_ImageUsageUsesGammaMips(")
    require(gamma_downsize, "TD_PBR_COLOR", "gamma-correct PBR color prefiltering")
    declared_usage = function_body(image_load, "static void R_LoadImageProgramForDeclaredUsage(")
    require(declared_usage, "declaredUsage != TD_PBR_COLOR", "PBR color image-program usage preservation")
    require(declared_usage, "declaredUsage != TD_MATERIAL_DATA", "PBR data image-program usage preservation")
    load_image = function_body(image_load, "void idImage::ActuallyLoadImage(")
    if load_image.count("R_LoadImageProgramForDeclaredUsage(") != 4:
        raise AssertionError("every image-program decode in ActuallyLoadImage must preserve declared PBR usage")

    require(image_header, "textureUsage_t GetUsage() const", "PBR image usage runtime introspection")
    material = read(ROOT / "src/renderer/Material.cpp")
    require(material, "albedoMap heightmap( _white, 1 )", "PBR color usage mutation regression self-test")
    require(material, "ormMap smoothnormals( _flat )", "PBR data usage mutation regression self-test")
    require(material, "sameAlbedo == info.albedo.image", "PBR color cache identity assertion")
    require(material, "sameORM == info.orm.image", "PBR data cache identity assertion")
    require(material, "registerMatches( info.metallicRegister, 0.25f )", "PBR metallic register value assertion")
    require(material, "registerMatches( info.emissiveColorRegisters[2], 0.3f )", "PBR emissive register value assertion")

    image_manager = read(ROOT / "src/renderer/ImageManager.cpp")
    namespace_usage = function_body(image_manager, "static textureUsage_t R_ImageUsageForName(")
    require(namespace_usage, "requestedUsage != TD_DEFAULT", "explicit image-usage namespace preservation")
    require(namespace_usage, "return requestedUsage;", "explicit PBR image usage return")
    if image_manager.count("usage = R_ImageUsageForName( _name, usage );") != 3:
        raise AssertionError("all immediate, lookup, and deferred image paths must share namespace usage policy")

    for signature in (
        "void idMaterial::AddReference()",
        "void idMaterial::ResolveUse()",
        "void idMaterial::ReloadImages( bool force ) const",
    ):
        body = function_body(material, signature)
        require(body, "pbrInfo.albedo", f"PBR lifecycle in {signature}")
        require(body, "pbrInfo.legacyEmissive", f"complete PBR lifecycle in {signature}")
    free_data = function_body(material, "void idMaterial::FreeData()")
    require(free_data, "memset( &pbrInfo, 0, sizeof( pbrInfo ) );", "purged PBR metadata reset")
    require(free_data, "pbrInfo.normalFormat = PBR_NORMAL_UNSPECIFIED;", "purged normal-format reset")


def test_scene_packet_and_resource_table_are_explicitly_non_visible() -> None:
    packets_h = read(ROOT / "src/renderer/ScenePackets.h")
    packets_cpp = read(ROOT / "src/renderer/ScenePackets.cpp")
    table_h = read(ROOT / "src/renderer/MaterialResourceTable.h")
    table_cpp = read(ROOT / "src/renderer/MaterialResourceTable.cpp")

    for token in (
        "hasPBR",
        "pbrAlbedoImage",
        "pbrNormalImage",
        "pbrORMImage",
        "pbrWorkflow",
        "pbrNormalFormat",
        "pbrUsesApproximateLegacyFallback",
        "pbrLegacyFallbackMissing",
    ):
        require(packets_h, token, "scene-packet PBR record")
        require(packets_cpp, token, "scene-packet PBR capture")

    for semantic in (
        "ALBEDO",
        "NORMAL",
        "ORM",
        "METALLIC",
        "ROUGHNESS",
        "AO",
        "EMISSIVE_PBR",
    ):
        require(table_h, f"MATERIAL_RESOURCE_TEXTURE_{semantic}", "PBR resource semantics")
    require(table_h, "pbrResourceReady", "resource readiness split")
    require(table_h, "pbrModernReady", "visible readiness split")
    for token in (
        "classicRecords",
        "pbrExplicitGeneratedFallbackRecords",
        "pbrMissingLegacyFallbackRecords",
        "pbrMissingAlbedoMapRecords",
        "pbrMissingNormalMapRecords",
        "pbrMissingORMMapRecords",
    ):
        require(table_h, token, "PBR fallback/map observability")
        require(table_cpp, token, "PBR fallback/map metrics")

    texture_table = function_body(
        table_cpp,
        "static void R_MaterialResourceTable_BuildTextureArrayTable(",
    )
    require(texture_table, "if ( record.hasPBR )", "PBR isolation from classic texture table")
    require(texture_table, "binding.textureArrayLayer = -1;", "PBR texture-table layer reset")
    finalize = section(
        table_cpp,
        "static void R_MaterialResourceTable_FinalizePBRContract(",
        "static void R_MaterialResourceTable_FinalizeShadowContract(",
    )
    require(finalize, "MATERIAL_RESOURCE_PBR_FALLBACK_SHADER_PATH_UNAVAILABLE", "Phase 3 visible fail-closed gate")
    require(finalize, "record.pbrModernReady = false;", "no premature visible-PBR claim")
    reject(finalize, "record.pbrModernReady = true", "premature visible-PBR ownership")
    pbr_binding_ready = function_body(
        table_cpp,
        "static bool R_MaterialResourceTable_PBRBindingReady(",
    )
    require(pbr_binding_ready, "!R_IsMutableRenderImage( binding.image )", "mutable PBR resource rejection")

    classic_gate = function_body(
        table_cpp,
        "bool R_MaterialResourceTable_ClassicModernPathEligible(",
    )
    require(classic_gate, "return !record.hasPBR;", "classic-modern PBR exclusion")
    require(table_cpp, "!R_MaterialResourceTable_ClassicModernPathEligible( *record )", "native PBR ownership self-test")
    require(table_cpp, "pbrBindingsExcludedFromClassicTable", "native PBR texture-table isolation self-test")
    require(table_cpp, "stats.textureArrayTableDescriptors == 0", "PBR classic-table descriptor isolation")
    require(table_cpp, "stats.classicRecords == 0", "PBR records excluded from classic record count")
    require(table_cpp, "stats.classicRecords != expectedRecords", "classic packet record count assertion")
    require(table_cpp, 'RendererMaterialResourceTable PBR contract self-test passed', "native PBR table pass marker")
    require(table_cpp, "savedMaxClassicTextureUnits", "uninitialized-backend PBR table unit budget scope")
    require(table_cpp, "rg_materialResourceTable.maxClassicTextureUnits = savedMaxClassicTextureUnits;", "PBR table unit budget restoration")
    for token in (
        "separateSource",
        "scalarSource",
        "explicitSource",
        "unsupportedSource",
        "missingFallbackSource",
        "missingAlbedoSource",
        "mutableImageSource",
        "redundantExplicitSource",
    ):
        require(table_cpp, token, "PBR material-table layout self-tests")
    require(table_cpp, "unsupportedRecord->pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_WORKFLOW", "unsupported workflow table assertion")
    require(table_cpp, "missingAlbedoRecord->pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_ALBEDO", "missing albedo table assertion")
    require(table_cpp, "mutableImageRecord->pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_IMAGE", "mutable image table assertion")
    require(table_cpp, "!redundantExplicitRecord->pbrUsesGeneratedLegacyFallback", "redundant explicit-map metric assertion")
    require(table_cpp, "rg_materialResourceTable.records[i].material = NULL;", "self-test declaration lifetime cleanup")

    draw_plan = read(ROOT / "src/renderer/ModernGLDrawPlan.cpp")
    submit_plan = read(ROOT / "src/renderer/ModernGLSubmitPlan.cpp")
    executor = read(ROOT / "src/renderer/ModernGLExecutor.cpp")
    require(draw_plan, "!R_MaterialResourceTable_ClassicModernPathEligible( *materialRecord )", "draw-plan PBR exclusion")
    require(submit_plan, "!R_MaterialResourceTable_ClassicModernPathEligible( *materialRecord )", "submit-plan PBR exclusion")
    if executor.count("!R_MaterialResourceTable_ClassicModernPathEligible( *materialRecord )") < 4:
        raise AssertionError("modern executor must reject PBR ownership and all classic visible pipelines")

    scene_packet_selftest = function_body(packets_cpp, "bool RendererScenePacket_RunSelfTest(")
    require(scene_packet_selftest, "pbrPacketFrame.AddDrawPacket", "native PBR scene-packet capture")
    require(scene_packet_selftest, "pbrDrawPacket->materialRecord == pbrRecord", "public draw-packet material linkage")
    require(scene_packet_selftest, "pbrRecord->pbrLegacyFallbackMissing", "PBR missing-fallback packet propagation assertion")
    require(scene_packet_selftest, "validRegister( pbrRecord->pbrMetallicRegister )", "PBR packet register range assertion")


def test_runtime_selftest_is_registered_and_required() -> None:
    init = read(ROOT / "src/renderer/RenderSystem_init.cpp")
    material = read(ROOT / "src/renderer/Material.cpp")
    table = read(ROOT / "src/renderer/MaterialResourceTable.cpp")
    matrix = read(ROOT / "tools/tests/renderer_validation_matrix.py")
    require(init, '"rendererPBRMaterialSelfTest"', "renderer command registration")
    require(init, '"RendererPBRMaterial self-test passed\\n"', "runtime success marker")
    require(matrix, '"RendererPBRMaterial self-test passed"', "matrix result contract")
    require(matrix, '"+rendererPBRMaterialSelfTest"', "matrix command contract")
    for source, context in ((material, "parser self-test"), (table, "resource-table self-test")):
        require(source, "declManager->AllocateDecl( DECL_MATERIAL )", context)
        require(source, "DeclManager_FreeAllocatedDecl", context)
        reject(source, "idMaterial dual;", context)


def main() -> int:
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    for test in tests:
        test()
    print("renderer_pbr_materials: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
