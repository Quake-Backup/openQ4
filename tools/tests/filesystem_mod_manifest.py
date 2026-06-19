#!/usr/bin/env python3
"""Regression checks for mod.json-only mod discovery contracts."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def reject(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        raise AssertionError(f"Unexpected {needle!r} in {context}")


def cpp_function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start == -1:
        raise AssertionError(f"Missing function signature {signature!r}")

    brace_start = source.find("{", start)
    if brace_start == -1:
        raise AssertionError(f"Missing function body for {signature!r}")

    depth = 0
    in_string = False
    in_char = False
    escaped = False
    for index in range(brace_start, len(source)):
        char = source[index]

        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue

        if in_char:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == "'":
                in_char = False
            continue

        if char == '"':
            in_string = True
        elif char == "'":
            in_char = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]

    raise AssertionError(f"Could not find end of function {signature!r}")


def validate_manifest_status_contract() -> None:
    source = read("src/framework/FileSystem.cpp")
    manifest_file = cpp_function_body(
        source,
        "modManifestStatus_t idFileSystemLocal::ReadModManifestFile(",
    )
    manifest_path = cpp_function_body(
        source,
        "modManifestStatus_t idFileSystemLocal::ReadModManifestFromSearchPath(",
    )

    for token in (
        "MOD_MANIFEST_MISSING",
        "MOD_MANIFEST_VALID",
        "MOD_MANIFEST_INVALID",
    ):
        require(source, token, "manifest status enum")

    require(manifest_file, "return MOD_MANIFEST_MISSING;", "missing manifest state")
    require(manifest_file, "return MOD_MANIFEST_INVALID;", "invalid manifest state")
    require(manifest_file, "return MOD_MANIFEST_VALID;", "valid manifest state")
    require(manifest_path, "status != MOD_MANIFEST_VALID", "manifest search-path state propagation")


def validate_json_parser_contract() -> None:
    source = read("src/framework/FileSystem.cpp")
    manifest_parser = cpp_function_body(source, "static bool FS_ParseModManifest(")
    json_string = cpp_function_body(source, "static bool FS_ParseJsonString(")
    json_skip_value = cpp_function_body(source, "static bool FS_SkipJsonValue( const char *&cursor, idStr &errorOut ) {")

    require(json_string, "case 'u':", "JSON unicode string escapes")
    require(json_string, "unescaped control character in JSON string", "JSON string control-char rejection")
    require(json_skip_value, "FS_SkipJsonObject", "unknown JSON object skipping")
    require(json_skip_value, "FS_SkipJsonArray", "unknown JSON array skipping")
    require(json_skip_value, "FS_SkipJsonNumber", "unknown JSON number skipping")

    require(manifest_parser, "FS_ModManifestKeyIsKnown( key )", "known mod.json field gate")
    require(manifest_parser, "field '%s' must be a JSON string", "known mod.json field type checks")
    require(manifest_parser, "FS_SkipJsonValue( cursor, errorOut )", "unknown mod.json field skipping")
    require(manifest_parser, "trailing comma before closing '}'", "mod.json trailing comma rejection")
    require(manifest_parser, "unexpected data after manifest object", "mod.json trailing data rejection")


def validate_manifest_only_contract() -> None:
    source = read("src/framework/FileSystem.cpp")
    list_mods = cpp_function_body(source, "idModList *idFileSystemLocal::ListMods(")
    get_mod_info = cpp_function_body(source, "bool idFileSystemLocal::GetModInfo(")

    for token in (
        "OPENQ4_LEGACY_MOD_DESCRIPTION_FILENAME",
        "ReadLegacyModInfoFromSearchPath",
        "FS_BuildLegacyModInfo",
        "allowLegacyFallback",
        "legacyFailureReason",
        "legacy .pk4",
        "description.txt",
    ):
        reject(source, token, "mod.json-only discovery")

    require(list_mods, "manifestStatus == MOD_MANIFEST_VALID", "ListMods valid manifest admission")
    require(list_mods, "manifestStatus == MOD_MANIFEST_INVALID", "ListMods invalid manifest handling")
    require(
        list_mods,
        'common->Warning( "Skipping mod',
        "ListMods visible invalid-manifest warning",
    )
    reject(list_mods, 'common->DWarning( "Skipping mod', "ListMods should not hide invalid manifests behind debug warnings")

    require(get_mod_info, "status == MOD_MANIFEST_VALID", "GetModInfo valid manifest admission")
    require(get_mod_info, "status == MOD_MANIFEST_INVALID", "GetModInfo invalid manifest diagnostics")
    require(get_mod_info, 'va( "missing %s"', "GetModInfo missing manifest diagnostic")


def validate_validation_coverage() -> None:
    validator = read("tools/validation/openq4_validate.py")
    require(validator, "filesystem_mod_manifest.py", "validation runner")


def main() -> None:
    validate_manifest_status_contract()
    validate_json_parser_contract()
    validate_manifest_only_contract()
    validate_validation_coverage()
    print("filesystem_mod_manifest: ok")


if __name__ == "__main__":
    main()
