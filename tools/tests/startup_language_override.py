#!/usr/bin/env python3
"""Regression checks for startup language override ordering."""

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


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start == -1:
        raise AssertionError(f"Missing function signature {signature!r}")

    depth = 0
    for index in range(start, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]

    raise AssertionError(f"Could not find end of function {signature!r}")


def require_order(haystack: str, first: str, second: str, context: str) -> None:
    first_index = haystack.find(first)
    second_index = haystack.find(second)
    if first_index == -1 or second_index == -1:
        raise AssertionError(f"Missing ordered symbols {first!r} and/or {second!r} in {context}")
    if first_index >= second_index:
        raise AssertionError(f"Expected {first!r} before {second!r} in {context}")


def validate_language_reload_contract() -> None:
    source = read("src/framework/Common.cpp")
    init_language = function_body(source, "void idCommonLocal::InitLanguageDict( bool applyStartupSysLang ) {")
    reload_command = function_body(source, "void Com_ReloadLanguage_f( const idCmdArgs &args ) {")
    init_game = function_body(source, "void idCommonLocal::InitGame( void ) {")

    require(source, "void\t\t\t\t\t\tInitLanguageDict( bool applyStartupSysLang );", "Common language helper declaration")
    require(init_language, "if ( applyStartupSysLang ) {", "conditional startup sys_lang replay")
    require(init_language, 'StartupVariable( "sys_lang", false );', "early startup sys_lang replay")
    require(reload_command, "const bool wasFileLoadingAllowed = fileSystem->GetIsFileLoadingAllowed();", "interactive reloadLanguage preserves file-loading flag")
    require(reload_command, "fileSystem->SetIsFileLoadingAllowed( true );", "interactive reloadLanguage enables file loading")
    require(reload_command, "commonLocal.InitLanguageDict( false );", "interactive reloadLanguage preserves current sys_lang")
    require(reload_command, "fileSystem->SetIsFileLoadingAllowed( wasFileLoadingAllowed );", "interactive reloadLanguage restores file-loading flag")
    reject(reload_command, "StartupVariable", "interactive reloadLanguage should not replay launch sys_lang")

    require(init_game, "InitLanguageDict( true );", "early startup language load")
    require(init_game, "exec autoexec.cfg", "startup autoexec execution")
    require(init_game, "cmdSystem->ExecuteCommandBuffer();", "startup cfg command execution")
    require(init_game, "StartupVariable( NULL, false );", "startup command-line cvar reapply")
    require(init_game, "const bool wasFileLoadingAllowed = fileSystem->GetIsFileLoadingAllowed();", "startup final language reload preserves file-loading flag")
    require(init_game, "fileSystem->SetIsFileLoadingAllowed( true );", "startup final language reload enables file loading")
    require(init_game, "InitLanguageDict( false );", "settled startup language reload")
    require(init_game, "fileSystem->SetIsFileLoadingAllowed( wasFileLoadingAllowed );", "startup final language reload restores file-loading flag")
    reject(
        init_game,
        'cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "reloadLanguage\\n" );',
        "startup language reload should happen after command-line reapply",
    )
    require_order(init_game, "InitLanguageDict( true );", "exec autoexec.cfg", "early language load before cfg files")
    require_order(init_game, "exec autoexec.cfg", "cmdSystem->ExecuteCommandBuffer();", "cfg files before execution")
    require_order(init_game, "cmdSystem->ExecuteCommandBuffer();", "StartupVariable( NULL, false );", "cfg execution before command-line reapply")
    require_order(init_game, "StartupVariable( NULL, false );", "InitLanguageDict( false );", "command-line reapply before final language reload")
    require_order(init_game, "fileSystem->SetIsFileLoadingAllowed( true );", "InitLanguageDict( false );", "file loading enabled before final language reload")
    require_order(init_game, "InitLanguageDict( false );", "fileSystem->SetIsFileLoadingAllowed( wasFileLoadingAllowed );", "file-loading flag restored after final language reload")


def validate_ci_smoke() -> None:
    push = read(".github/workflows/push-verification.yml")
    commit = read(".github/workflows/commit-validation.yml")
    runner = read("tools/validation/openq4_validate.py")

    for source, context in (
        (push, "push verification workflow"),
        (commit, "commit validation workflow"),
        (runner, "validation runner"),
    ):
        require(source, "startup_language_override.py", context)


def main() -> None:
    validate_language_reload_contract()
    validate_ci_smoke()
    print("startup_language_override: ok")


if __name__ == "__main__":
    main()
