#!/usr/bin/env python3
"""Regression checks for language-table encoding and the engine-side transcode.

openQ4 authors its string tables in UTF-8, but the stock Quake 4 fonts are a
fixed 256-glyph atlas indexed by a raw byte, so the engine transcodes UTF-8
tables to an 8-bit codepage at load time (idLangDict::Load).  Which codepage
depends on sys_lang: Polish is Windows-1250, everything else Windows-1252.
Three things therefore have to stay true:

  * every repo-authored ``.lang`` file must be valid UTF-8 whose code points are
    all representable in *its own* codepage, otherwise the transcode silently
    declines and the accents render as two wrong glyphs (GitHub issue #89);
  * the codepage tables in the engine must match the real Windows codepages -
    a wrong entry is not a missing glyph but a confidently wrong one, e.g. 0xB9
    is a-ogonek in CP1250 and superscript one in CP1252;
  * the transcode, and the font side that has to agree with it, must stay in
    place in both idlib copies.
"""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
STRINGS_DIR = ROOT / "content" / "baseoq4" / "pak0" / "strings"

# sys_lang values whose tables are Central European rather than Western.  Keep
# in step with LangDict_CodePageForLanguage.
CENTRAL_EUROPEAN_LANGUAGES = ("polish", "czech")

# Codes that land on a .notdef cell in every stock Quake 4 font.  The engine
# folds these to ASCII rather than drawing nothing; 0xA0 in particular has a
# zero advance, so leaving it alone would delete the word gap entirely.
# Windows-1250 puts the same punctuation at the same byte values, so one fold
# table serves both codepages.
FOLDED_BYTES = {0x82, 0x84, 0x85, 0x91, 0x92, 0x93, 0x94, 0x96, 0x97, 0xA0}


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def codepage_for_lang_file(path: Path) -> str:
    language = path.stem.split("_", 1)[0].lower()
    if language in CENTRAL_EUROPEAN_LANGUAGES:
        return "cp1250"
    return "cp1252"


def parse_hex_table(source: str, name: str, expected_length: int) -> list:
    """Pull a `static const unsigned short NAME[N] = { ... };` table out of C++."""
    match = re.search(
        r"\b" + re.escape(name) + r"\s*\[\s*\d+\s*\]\s*=\s*\{(.*?)\}\s*;",
        source,
        re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"Could not find the {name} table")
    values = [int(token, 16) for token in re.findall(r"0x([0-9A-Fa-f]{4})", match.group(1))]
    if len(values) != expected_length:
        raise AssertionError(
            f"{name} has {len(values)} entries, expected {expected_length}"
        )
    return values


def validate_codepage_table(source: str, name: str, codec: str, first_byte: int, count: int) -> None:
    """The engine's table must agree with the real Windows codepage, entry for entry."""
    values = parse_hex_table(source, name, count)
    for index, value in enumerate(values):
        byte = first_byte + index
        try:
            expected = ord(bytes([byte]).decode(codec))
        except UnicodeDecodeError:
            expected = 0  # unassigned slot; the engine marks these 0
        if value != expected:
            raise AssertionError(
                f"{name}[0x{byte:02X}] is 0x{value:04X}, but {codec} maps that byte to "
                f"0x{expected:04X}. A wrong entry draws a confidently wrong glyph."
            )


def validate_lang_files() -> None:
    lang_files = sorted(STRINGS_DIR.glob("*.lang"))
    if not lang_files:
        raise AssertionError(f"No .lang files found under {STRINGS_DIR}")

    for path in lang_files:
        data = path.read_bytes()
        rel = path.relative_to(ROOT).as_posix()
        codec = codepage_for_lang_file(path)

        if data.startswith(b"\xef\xbb\xbf"):
            raise AssertionError(
                f"{rel} starts with a UTF-8 BOM; idLangDict::Load tolerates it but "
                "the retail parser does not - save without a BOM"
            )

        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise AssertionError(
                f"{rel} is not valid UTF-8 at byte {exc.start} ({exc.reason}). "
                "Repo-authored string tables are UTF-8; the engine transcodes them "
                "to the 8-bit font codepage at load time."
            ) from exc

        for index, char in enumerate(text):
            try:
                char.encode(codec)
            except UnicodeEncodeError:
                raise AssertionError(
                    f"{rel}: U+{ord(char):04X} ({char!r}) at character {index} cannot be "
                    f"represented in {codec}, which is the codepage this language loads "
                    "under, so the 256-glyph fonts cannot draw it."
                ) from None


def validate_transcode_contract() -> None:
    for relative_path in (
        "src/idlib/LangDict.cpp",
        # the game repo builds its own idlib; keep the two copies in lockstep
    ):
        source = read(relative_path)
        context = f"{relative_path} language-table transcode"
        require(source, "LangDict_ConvertUtf8ToCodePage", context)
        require(source, "LANGDICT_CP1252_HIGH", context)
        require(source, "LANGDICT_CP1250_HIGH", context)
        require(source, "LANGDICT_GLYPH_FOLD", context)
        require(source, "LangDict_DecodeUtf8", context)
        require(source, "LangDict_ByteForCodePoint", context)
        require(source, "LangDict_CodePageForLanguage", context)
        require(source, "LangDict_UnicodeForByte", context)
        require(
            source,
            "idStr transcoded;",
            f"{context} (buffer must outlive the lexer)",
        )
        require(
            source,
            "src.LoadMemory( parseText, parseLength, fileName );",
            f"{context} (lexer must parse the transcoded text)",
        )

        # CP1252 only tabulates 0x80-0x9F because it agrees with Latin-1 above
        # that, which is what the engine relies on for the pass-through branch.
        validate_codepage_table(source, "LANGDICT_CP1252_HIGH", "cp1252", 0x80, 32)
        for byte in range(0xA0, 0x100):
            if ord(bytes([byte]).decode("cp1252")) != byte:
                raise AssertionError(
                    f"cp1252 does not map 0x{byte:02X} to itself, so LangDict's Latin-1 "
                    "pass-through above 0x9F is wrong"
                )
        # CP1250 diverges from Latin-1 across the whole upper half.
        validate_codepage_table(source, "LANGDICT_CP1250_HIGH", "cp1250", 0x80, 128)

        for language in CENTRAL_EUROPEAN_LANGUAGES:
            require(source, f'"{language}"', f"{context} (Central European language list)")

        declaration = source.index("idStr transcoded;")
        lexer = source.index("idLexer src(")
        if declaration > lexer:
            raise AssertionError(
                f"{relative_path}: 'idStr transcoded' must be declared before 'idLexer src' - "
                "idLexer::LoadMemory stores the pointer without copying, so the buffer has to "
                "be destroyed after the lexer"
            )

        # every folded code must be one the engine can actually produce
        for match in re.finditer(r"\{\s*0x([0-9A-Fa-f]{2}),\s*\"", source):
            code = int(match.group(1), 16)
            if code not in FOLDED_BYTES:
                raise AssertionError(
                    f"{relative_path}: unexpected glyph fold entry 0x{code:02X}; update "
                    "FOLDED_BYTES in this test if the fold table intentionally changed"
                )


def validate_codepage_selection() -> None:
    """The codepage has to be chosen before any table is read, and the fonts have
    to be rasterised through the same mapping - otherwise the tables and the
    atlas disagree about what a byte means."""
    common = read("src/framework/Common.cpp")
    require(
        common,
        "LangDict_SetActiveCodePage( LangDict_CodePageForLanguage( langName.c_str() ) )",
        "idCommonLocal::InitLanguageDict (codepage must follow sys_lang)",
    )
    select = common.index("LangDict_SetActiveCodePage")
    first_load = common.index("languageDict.Load(")
    if select > first_load:
        raise AssertionError(
            "src/framework/Common.cpp: the active codepage must be selected before the "
            "first languageDict.Load() - one dictionary cannot hold two codepages"
        )

    ttf = read("src/renderer/tr_fontTTF.cpp")
    require(
        ttf,
        "LangDict_UnicodeForByte",
        "R_TTFCodepointForByte (glyph lookup must use the active codepage)",
    )
    if "Q4_CP1252_HIGH" in ttf:
        raise AssertionError(
            "src/renderer/tr_fontTTF.cpp still carries its own CP1252 table; the font "
            "side must go through LangDict_UnicodeForByte or Polish text rasterises as "
            "Western punctuation"
        )

    # The renderer is a module with its own copy of idlib, so the codepage the
    # engine selects does not reach the glyph rasteriser on its own. Without the
    # sync the tables are CP1250 while the atlas is baked through CP1252, which
    # draws a confidently wrong glyph rather than a missing one.
    font = read("src/renderer/tr_font.cpp")
    require(font, "void R_SyncFontCodePage( void )", "renderer-module codepage sync")
    require(
        font,
        'LangDict_CodePageForLanguage( cvarSystem->GetCVarString( "sys_lang" ) )',
        "renderer-module codepage sync (must derive from the shared cvar)",
    )
    register = font[font.index("bool idRenderSystemLocal::RegisterFont") :]
    register = register[: register.index("\n}\n")]
    if "R_SyncFontCodePage();" not in register:
        raise AssertionError(
            "src/renderer/tr_font.cpp: RegisterFont must sync the module's codepage "
            "before any atlas is rasterised"
        )
    if register.index("R_SyncFontCodePage();") > register.index("LangDict_GetCodePageGeneration()"):
        raise AssertionError(
            "src/renderer/tr_font.cpp: the codepage sync must run before the generation "
            "is sampled, or the console atlas rebuild misses the language change"
        )


def validate_language_menu() -> None:
    """Every sys_lang the language chooser offers needs string tables to load."""
    game_gui = read("content/baseoq4/pak0/guis/menu/settings/game.gui")
    match = re.search(r'values\s+"([a-z;]+)"', game_gui)
    if match is None:
        raise AssertionError(
            "content/baseoq4/pak0/guis/menu/settings/game.gui: could not find the "
            "language chooser 'values' list"
        )
    offered = match.group(1).split(";")

    for language in offered:
        if not sorted(STRINGS_DIR.glob(f"{language}_*.lang")):
            raise AssertionError(
                f"The language menu offers '{language}' but no {language}_*.lang tables "
                f"ship in {STRINGS_DIR.relative_to(ROOT).as_posix()}; selecting it would "
                "fall back to English"
            )

    english_choices = re.search(
        r'"#str_229908"\s+"([^"]+)"', read("content/baseoq4/pak0/strings/english_openq4.lang")
    )
    if english_choices is None:
        raise AssertionError("english_openq4.lang: #str_229908 (language names) is missing")
    if len(english_choices.group(1).split(";")) != len(offered):
        raise AssertionError(
            f"#str_229908 lists {len(english_choices.group(1).split(';'))} language names "
            f"but the chooser offers {len(offered)} values; a choiceDef silently "
            "mismatches when the two disagree"
        )


def validate_ci_smoke() -> None:
    push = read(".github/workflows/push-verification.yml")
    commit = read(".github/workflows/commit-validation.yml")
    runner = read("tools/validation/openq4_validate.py")

    for source, context in (
        (push, "push verification workflow"),
        (commit, "commit validation workflow"),
        (runner, "validation runner"),
    ):
        require(source, "lang_table_encoding.py", context)


def main() -> None:
    validate_lang_files()
    validate_transcode_contract()
    validate_codepage_selection()
    validate_language_menu()
    validate_ci_smoke()
    print("lang_table_encoding: ok")


if __name__ == "__main__":
    main()
