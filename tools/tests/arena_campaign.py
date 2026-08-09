#!/usr/bin/env python3
"""Static contract checks for the single-player Arena Campaign.

The Arena Campaign crosses boundaries that the compiler cannot check together:

* a small lexer-authored manifest describes the full five-tier ladder;
* four language tables supply every selector, tier, match, and result string;
* the main menu hands control to a dedicated campaign GUI;
* the framework launches a private game_mp server, populates named bots, and
  persists unlocks while preserving the player's ordinary server settings;
* game_mp reports the authoritative final score back to the framework.

This test parses the manifest independently, pins its stock-only map and roster
contract, and checks the static integration points without requiring retail
assets or launching the renderer.
"""

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
GAME_LIBS_ROOT = Path(
    os.environ.get("OPENQ4_GAMELIBS_REPO", ROOT.parent / "openQ4-game")
).resolve()

CAMPAIGN_PATH = (
    ROOT / "content" / "baseoq4" / "pak0" / "arena" / "openq4_campaign.cfg"
)
STRINGS_DIR = ROOT / "content" / "baseoq4" / "pak0" / "strings"
MAIN_MENU = ROOT / "content" / "baseoq4" / "pak0" / "guis" / "mainmenu.gui"
ARENA_MENU = ROOT / "content" / "baseoq4" / "pak0" / "guis" / "arena_menu.gui"

EXPECTED_MAPS_AND_MODES = (
    ("mp/q4dm1", "Duel"),
    ("mp/q4dm2", "DM"),
    ("mp/q4dm3", "Team DM"),
    ("mp/q4dm4", "Duel"),
    ("mp/q4dm5", "DM"),
    ("mp/q4dm6", "Red Rover"),
    ("mp/q4dm7", "Clan Arena"),
    ("mp/q4tourney1", "Duel"),
    ("mp/q4dm8", "DM"),
    ("mp/q4dm9", "Team DM"),
    ("mp/q4dm11", "Clan Arena"),
    ("mp/q4dm11v1", "Duel"),
    ("mp/q4xdm10", "DM"),
    ("mp/q4xdm13", "Red Rover"),
    ("mp/q4xdm11", "Team DM"),
    ("mp/q4xtourney2", "Duel"),
    ("mp/q4xdm14", "Team DM"),
    ("mp/q4xdm15", "Clan Arena"),
    ("mp/q4dm11", "DM"),
    ("mp/q4xtourney1", "Duel"),
)

# Latest official map declarations (pak019) advertise these base entity layouts.
# Derived openQ4 modes deliberately borrow one of these keys.
STOCK_MAP_KEYS = {
    "mp/q4dm1": {"DM", "Team DM"},
    "mp/q4dm2": {"DM", "Team DM"},
    "mp/q4dm3": {"DM", "Team DM", "Tourney"},
    "mp/q4dm4": {"DM", "Team DM"},
    "mp/q4dm5": {"DM", "Team DM"},
    "mp/q4dm6": {"DM", "Team DM", "Tourney"},
    "mp/q4dm7": {"DM", "Team DM", "Tourney"},
    "mp/q4tourney1": {"DM", "Team DM", "Tourney"},
    "mp/q4dm8": {"DM", "Team DM", "Tourney"},
    "mp/q4dm9": {"DM", "Team DM", "Tourney"},
    "mp/q4dm11": {"DM", "Team DM", "Tourney"},
    "mp/q4dm11v1": {"DM", "Team DM", "Tourney"},
    "mp/q4xdm10": {"DM", "Team DM", "DeadZone"},
    "mp/q4xdm13": {"DM", "Team DM", "DeadZone"},
    "mp/q4xdm11": {"DM", "Team DM", "DeadZone"},
    "mp/q4xtourney2": {"DM", "Team DM", "Tourney"},
    "mp/q4xtourney1": {"DM", "Team DM", "Tourney"},
    "mp/q4xdm14": {"DM", "Team DM", "DeadZone"},
    "mp/q4xdm15": {"DM", "Team DM", "DeadZone"},
}

MODE_MAP_KEY = {
    "DM": "DM",
    "Duel": "DM",
    "Team DM": "Team DM",
    "Red Rover": "Team DM",
    "Clan Arena": "Team DM",
}

EXPECTED_ROSTERS = (
    (("Bagby", 0),),
    (("Anderson", 0), ("Bagby", 1), ("Tetzlaff", 0)),
    (
        ("Anderson", 0),
        ("Bagby", 1),
        ("Tetzlaff", 1),
        ("Sorg", 1),
        ("Strauss", 1),
    ),
    (("Sorg", 1),),
    (("Sorg", 0), ("Strauss", 0), ("Hollenbeck", 0)),
    (
        ("Sorg", 0),
        ("Strauss", 0),
        ("Hollenbeck", 0),
        ("Marsh", 1),
        ("Anderson", -1),
    ),
    (
        ("Sorg", 0),
        ("Strauss", 0),
        ("Hollenbeck", 1),
        ("Marsh", 1),
        ("Tetzlaff", -1),
    ),
    (("Marsh", 1),),
    (("Bidwell", 0), ("Cortez", 0), ("Morris", 0), ("Rhodes", 0)),
    (
        ("Bidwell", 0),
        ("Cortez", 0),
        ("Morris", 0),
        ("Rhodes", 0),
        ("Voss", 1),
    ),
    (
        ("Bidwell", 0),
        ("Cortez", 0),
        ("Morris", 0),
        ("Rhodes", 0),
        ("Voss", 1),
    ),
    (("Voss", 1),),
    (("Gunner", 0), ("Rhodes", -1), ("Cortez", 0), ("Voss", 0)),
    (
        ("Gunner", 0),
        ("Rhodes", -1),
        ("Cortez", 0),
        ("Voss", 0),
        ("Kane", 1),
    ),
    (
        ("Gunner", 0),
        ("Kane", 0),
        ("Sledge", 0),
        ("Voss", 0),
        ("Rhodes", -1),
    ),
    (("Sledge", 1),),
    (
        ("Kane", 0),
        ("Sledge", 0),
        ("Gunner", 0),
        ("Morris", -1),
        ("Voss", 0),
    ),
    (
        ("Kane", 0),
        ("Sledge", 0),
        ("Gunner", 0),
        ("Morris", -1),
        ("Voss", 0),
    ),
    (
        ("Kane", 0),
        ("Sledge", 0),
        ("Gunner", 0),
        ("Morris", -1),
        ("Voss", 0),
    ),
    (("Makron", 0),),
)

REQUIRED_ROSTER = {
    "Anderson",
    "Bagby",
    "Bidwell",
    "Cortez",
    "Gunner",
    "Hollenbeck",
    "Kane",
    "Makron",
    "Marsh",
    "Morris",
    "Rhodes",
    "Sledge",
    "Sorg",
    "Strauss",
    "Tetzlaff",
    "Voss",
}


def read(path: Path) -> str:
    if not path.is_file():
        raise AssertionError(f"Missing {relative(path)}")
    return path.read_text(encoding="utf-8")


def relative(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def require_regex(haystack: str, pattern: str, context: str) -> None:
    if re.search(pattern, haystack, re.DOTALL) is None:
        raise AssertionError(f"Missing /{pattern}/ in {context}")


TOKEN_RE = re.compile(
    r"""
      (?P<comment>//[^\n]*|/\*.*?\*/)
    | (?P<string>"(?:[^"\\\n]|\\.)*")
    | (?P<int>-?\d+)
    | (?P<word>[A-Za-z_][A-Za-z0-9_]*)
    | (?P<brace>[{}])
    | (?P<space>\s+)
    """,
    re.VERBOSE | re.DOTALL,
)


@dataclass(frozen=True)
class Token:
    kind: str
    value: str
    line: int


def tokenize(source: str) -> list[Token]:
    tokens: list[Token] = []
    position = 0
    line = 1
    while position < len(source):
        match = TOKEN_RE.match(source, position)
        if match is None:
            excerpt = source[position : position + 30].splitlines()[0]
            raise AssertionError(
                f"{relative(CAMPAIGN_PATH)}:{line}: invalid token near {excerpt!r}"
            )
        text = match.group(0)
        kind = match.lastgroup or ""
        if kind not in {"space", "comment"}:
            value = text[1:-1] if kind == "string" else text
            tokens.append(Token(kind, value, line))
        line += text.count("\n")
        position = match.end()
    return tokens


class Parser:
    def __init__(self, tokens: list[Token]) -> None:
        self.tokens = tokens
        self.index = 0

    def peek(self) -> Token | None:
        return self.tokens[self.index] if self.index < len(self.tokens) else None

    def take(self, value: str | None = None, kind: str | None = None) -> Token:
        token = self.peek()
        if token is None:
            raise AssertionError(f"{relative(CAMPAIGN_PATH)}: unexpected end of file")
        if value is not None and token.value != value:
            raise AssertionError(
                f"{relative(CAMPAIGN_PATH)}:{token.line}: expected {value!r}, "
                f"found {token.value!r}"
            )
        if kind is not None and token.kind != kind:
            raise AssertionError(
                f"{relative(CAMPAIGN_PATH)}:{token.line}: expected {kind}, "
                f"found {token.kind} {token.value!r}"
            )
        self.index += 1
        return token


@dataclass(frozen=True)
class Bot:
    name: str
    offset: int


@dataclass(frozen=True)
class Match:
    name: str
    description: str
    map: str
    game_type: str
    base_skill: int
    frag_limit: int
    time_limit: int
    round_limit: int
    score_limit: int
    bots: tuple[Bot, ...]


@dataclass(frozen=True)
class Tier:
    name: str
    description: str
    matches: tuple[Match, ...]


@dataclass(frozen=True)
class Campaign:
    version: int
    tiers: tuple[Tier, ...]


def unique_field(seen: set[str], key: str, line: int) -> None:
    if key in seen:
        raise AssertionError(
            f"{relative(CAMPAIGN_PATH)}:{line}: duplicate field {key!r}"
        )
    seen.add(key)


def parse_bots(parser: Parser) -> tuple[Bot, ...]:
    parser.take("{")
    bots: list[Bot] = []
    while parser.peek() is not None and parser.peek().value != "}":
        parser.take("bot")
        name = parser.take(kind="string").value
        offset = int(parser.take(kind="int").value)
        bots.append(Bot(name, offset))
    parser.take("}")
    return tuple(bots)


def parse_match(parser: Parser) -> Match:
    parser.take("{")
    seen: set[str] = set()
    values: dict[str, object] = {}
    string_fields = {
        "name": "name",
        "description": "description",
        "map": "map",
        "gameType": "game_type",
    }
    int_fields = {
        "baseSkill": "base_skill",
        "fragLimit": "frag_limit",
        "timeLimit": "time_limit",
        "roundLimit": "round_limit",
        "scoreLimit": "score_limit",
    }
    while parser.peek() is not None and parser.peek().value != "}":
        field = parser.take(kind="word")
        unique_field(seen, field.value, field.line)
        if field.value in string_fields:
            values[string_fields[field.value]] = parser.take(kind="string").value
        elif field.value in int_fields:
            values[int_fields[field.value]] = int(parser.take(kind="int").value)
        elif field.value == "bots":
            values["bots"] = parse_bots(parser)
        else:
            raise AssertionError(
                f"{relative(CAMPAIGN_PATH)}:{field.line}: unknown match field "
                f"{field.value!r}"
            )
    parser.take("}")

    expected = set(string_fields) | set(int_fields) | {"bots"}
    if seen != expected:
        missing = sorted(expected - seen)
        extra = sorted(seen - expected)
        raise AssertionError(
            f"campaign match fields differ: missing={missing}, extra={extra}"
        )
    return Match(**values)  # type: ignore[arg-type]


def parse_tier(parser: Parser) -> Tier:
    parser.take("{")
    seen: set[str] = set()
    name = ""
    description = ""
    matches: list[Match] = []
    while parser.peek() is not None and parser.peek().value != "}":
        field = parser.take(kind="word")
        if field.value == "match":
            matches.append(parse_match(parser))
        elif field.value == "name":
            unique_field(seen, field.value, field.line)
            name = parser.take(kind="string").value
        elif field.value == "description":
            unique_field(seen, field.value, field.line)
            description = parser.take(kind="string").value
        else:
            raise AssertionError(
                f"{relative(CAMPAIGN_PATH)}:{field.line}: unknown tier field "
                f"{field.value!r}"
            )
    parser.take("}")
    if seen != {"name", "description"}:
        raise AssertionError("every campaign tier requires one name and description")
    return Tier(name, description, tuple(matches))


def parse_campaign() -> Campaign:
    parser = Parser(tokenize(read(CAMPAIGN_PATH)))
    parser.take("campaign")
    version = int(parser.take(kind="int").value)
    parser.take("{")
    tiers: list[Tier] = []
    while parser.peek() is not None and parser.peek().value != "}":
        parser.take("tier")
        tiers.append(parse_tier(parser))
    parser.take("}")
    if parser.peek() is not None:
        token = parser.peek()
        raise AssertionError(
            f"{relative(CAMPAIGN_PATH)}:{token.line}: data after campaign block"
        )
    return Campaign(version, tuple(tiers))


def validate_manifest(campaign: Campaign) -> None:
    if campaign.version != 1:
        raise AssertionError(f"campaign version is {campaign.version}, expected 1")
    if len(campaign.tiers) != 5:
        raise AssertionError(f"campaign has {len(campaign.tiers)} tiers, expected 5")

    matches: list[Match] = []
    for tier_index, tier in enumerate(campaign.tiers):
        expected_name = f"#str_{42050 + tier_index}"
        expected_description = f"#str_{42055 + tier_index}"
        if tier.name != expected_name or tier.description != expected_description:
            raise AssertionError(
                f"tier {tier_index + 1} uses {tier.name}/{tier.description}, "
                f"expected {expected_name}/{expected_description}"
            )
        if len(tier.matches) != 4:
            raise AssertionError(
                f"tier {tier_index + 1} has {len(tier.matches)} matches, expected 4"
            )

        for match_index, match in enumerate(tier.matches):
            global_index = tier_index * 4 + match_index
            expected_name = f"#str_{42100 + global_index * 2}"
            expected_description = f"#str_{42101 + global_index * 2}"
            if match.name != expected_name or match.description != expected_description:
                raise AssertionError(
                    f"match {global_index + 1} localization IDs drifted: "
                    f"{match.name}/{match.description}"
                )
            if match.base_skill != tier_index + 1:
                raise AssertionError(
                    f"tier {tier_index + 1} match {match_index + 1} baseSkill is "
                    f"{match.base_skill}, expected {tier_index + 1}"
                )
            if not 1 <= match.frag_limit <= 100:
                raise AssertionError(f"invalid fragLimit in match {global_index + 1}")
            if not 0 <= match.time_limit <= 60:
                raise AssertionError(f"invalid timeLimit in match {global_index + 1}")
            if not 1 <= match.round_limit <= 99:
                raise AssertionError(f"invalid roundLimit in match {global_index + 1}")
            if not 1 <= match.score_limit <= 9999:
                raise AssertionError(f"invalid scoreLimit in match {global_index + 1}")
            if not match.bots or len(match.bots) >= 16:
                raise AssertionError(f"invalid bot count in match {global_index + 1}")
            if len({bot.name.casefold() for bot in match.bots}) != len(match.bots):
                raise AssertionError(f"duplicate bot in match {global_index + 1}")
            for bot in match.bots:
                if bot.offset not in {-1, 0, 1}:
                    raise AssertionError(
                        f"{bot.name} has invalid offset {bot.offset} in "
                        f"match {global_index + 1}"
                    )
                if not 1 <= match.base_skill + bot.offset <= 5:
                    raise AssertionError(
                        f"{bot.name} falls outside base skill 1..5 in "
                        f"match {global_index + 1}"
                    )
            matches.append(match)

    actual_maps_and_modes = tuple((match.map, match.game_type) for match in matches)
    if actual_maps_and_modes != EXPECTED_MAPS_AND_MODES:
        raise AssertionError(
            "campaign stock-map progression drifted:\n"
            f"  actual:   {actual_maps_and_modes}\n"
            f"  expected: {EXPECTED_MAPS_AND_MODES}"
        )

    actual_rosters = tuple(
        tuple((bot.name, bot.offset) for bot in match.bots) for match in matches
    )
    if actual_rosters != EXPECTED_ROSTERS:
        raise AssertionError("campaign bot roster or skill offsets drifted")

    used_roster = {bot.name for match in matches for bot in match.bots}
    if used_roster != REQUIRED_ROSTER:
        raise AssertionError(
            f"campaign roster differs: missing={sorted(REQUIRED_ROSTER - used_roster)}, "
            f"extra={sorted(used_roster - REQUIRED_ROSTER)}"
        )

    character_dir = (
        ROOT / "content" / "baseoq4" / "pak0" / "botfiles" / "characters"
    )
    available_characters = {path.stem.casefold() for path in character_dir.glob("*.bot")}
    for bot_name in sorted(used_roster):
        if bot_name.casefold() not in available_characters:
            raise AssertionError(f"campaign bot {bot_name!r} has no character file")

    for index, match in enumerate(matches):
        if match.game_type not in MODE_MAP_KEY:
            raise AssertionError(
                f"match {index + 1} uses objective/unknown mode {match.game_type!r}"
            )
        if match.map.startswith("mp/q4cmp"):
            raise AssertionError("core campaign must not require the optional q4cmp pack")
        supported_keys = STOCK_MAP_KEYS.get(match.map)
        if supported_keys is None:
            raise AssertionError(f"match {index + 1} uses a non-contract stock map")
        needed_key = MODE_MAP_KEY[match.game_type]
        if needed_key not in supported_keys:
            raise AssertionError(
                f"{match.map} does not advertise {needed_key} for {match.game_type}"
            )
        if match.game_type in {"Team DM", "Red Rover", "Clan Arena"}:
            if len(match.bots) != 5:
                raise AssertionError(
                    f"team match {index + 1} needs five bots plus the local player"
                )

    for tier_index in range(5):
        boss = matches[tier_index * 4 + 3]
        if boss.game_type != "Duel" or len(boss.bots) != 1:
            raise AssertionError(
                f"tier {tier_index + 1} boss must be a one-bot Duel"
            )
    if matches[-1].bots[0].name != "Makron":
        raise AssertionError("final campaign boss must be Makron")

    # Preserve the ladder's authored breadth and escalation even if the pinned
    # map table is deliberately revised later.
    mode_counts = {
        mode: sum(match.game_type == mode for match in matches)
        for mode in MODE_MAP_KEY
    }
    if set(mode for mode, count in mode_counts.items() if count) != set(MODE_MAP_KEY):
        raise AssertionError(f"campaign mode variety regressed: {mode_counts}")
    for mode in ("DM", "Team DM", "Red Rover", "Clan Arena"):
        if mode_counts[mode] < 2:
            raise AssertionError(f"campaign needs at least two {mode} matches")

    unique_maps = {match.map for match in matches}
    if len(unique_maps) < len(matches) - 1:
        raise AssertionError(
            f"campaign repeats too many maps ({len(unique_maps)} unique for {len(matches)} matches)"
        )

    for match_index in range(4):
        slot = [campaign.tiers[tier].matches[match_index] for tier in range(5)]
        frag_limits = [match.frag_limit for match in slot]
        time_limits = [match.time_limit for match in slot]
        if frag_limits != sorted(frag_limits) or time_limits != sorted(time_limits):
            raise AssertionError(
                f"match slot {match_index + 1} no longer escalates its limits"
            )

    boss_names = [campaign.tiers[tier].matches[3].bots[0].name for tier in range(5)]
    if len(set(boss_names)) != len(boss_names):
        raise AssertionError(f"campaign boss roster repeats a character: {boss_names}")


LANG_ENTRY_RE = re.compile(r'^\s*"(#str_\d+)"\s+"((?:[^"\\]|\\.)*)"', re.MULTILINE)


def parse_language(path: Path) -> dict[str, str]:
    data = path.read_bytes()
    if data.startswith(b"\xef\xbb\xbf"):
        raise AssertionError(f"{relative(path)} must remain UTF-8 without a BOM")
    text = data.decode("utf-8")
    try:
        text.encode("cp1252")
    except UnicodeEncodeError as exc:
        raise AssertionError(
            f"{relative(path)} contains a character outside the CP1252 font set"
        ) from exc

    entries: dict[str, str] = {}
    for match in LANG_ENTRY_RE.finditer(text):
        key, value = match.groups()
        if key in entries:
            raise AssertionError(f"{relative(path)} contains duplicate {key}")
        entries[key] = value
    return entries


def validate_localization(campaign: Campaign) -> None:
    language_paths = {
        locale: STRINGS_DIR / f"{locale}_openq4.lang"
        for locale in ("english", "french", "italian", "spanish")
    }
    languages = {
        locale: parse_language(path) for locale, path in language_paths.items()
    }

    required_ids = (
        {f"#str_{value}" for value in range(42000, 42096)}
        | {f"#str_{value}" for value in range(42100, 42140)}
        | {f"#str_{value}" for value in range(42200, 42205)}
    )
    referenced_ids = {
        tier.name for tier in campaign.tiers
    } | {
        tier.description for tier in campaign.tiers
    } | {
        value
        for tier in campaign.tiers
        for match in tier.matches
        for value in (match.name, match.description)
    } | set(re.findall(r"#str_\d+", read(ARENA_MENU)))

    for locale, entries in languages.items():
        missing = sorted(required_ids - entries.keys())
        if missing:
            raise AssertionError(f"{locale} arena strings missing: {missing}")
        empty = sorted(key for key in required_ids if not entries[key].strip())
        if empty:
            raise AssertionError(f"{locale} arena strings are empty: {empty}")
        if not referenced_ids <= entries.keys():
            raise AssertionError(f"{locale} lacks campaign-authored string references")

    english_expected = {
        "#str_42000": "SINGLE PLAYER",
        "#str_42001": "MISSION",
        "#str_42002": "ARENA",
        "#str_42009": "DIFFICULTY",
        "#str_42010": "START MATCH",
        "#str_42011": "RESET PROGRESS",
        "#str_42012": "CAMPAIGN PROGRESS",
        "#str_42022": "LOCKED",
        "#str_42023": "COMPLETE",
        "#str_42030": "VICTORY",
        "#str_42031": "DEFEAT",
        "#str_42033": "RETRY",
        "#str_42041": "Frags",
        "#str_42042": "Minutes",
        "#str_42043": "Rounds",
        "#str_42044": "Points",
        "#str_42045": "BOSS READY",
        "#str_42046": "NEW TIER UNLOCKED",
        "#str_42047": "FINAL TIER CONQUERED",
        "#str_42048": "MISSING REQUIRED MAP",
        "#str_42049": "Win all three qualifiers to reveal the boss, then win the boss match to unlock the next tier.",
        "#str_42060": "TRAINING",
        "#str_42061": "ROOKIE",
        "#str_42062": "VETERAN",
        "#str_42063": "NIGHTMARE",
        "#str_42064": "IMPOSSIBLE",
        "#str_42065": "AVAILABLE",
        "#str_42066": "Arena victory. The circuit opens ahead.",
        "#str_42067": "Arena defeated. Adjust your skill or replay the match.",
        "#str_42068": "MATCH ABORTED",
        "#str_42069": "One or more bots could not join. Check the console log and try again.",
        "#str_42070": "TIER BRIEFING",
        "#str_42071": "3 QUALIFIERS  >  BOSS  >  NEXT TIER",
        "#str_42072": "RESET ARENA PROGRESS?",
        "#str_42074": "CANCEL",
        "#str_42075": "RESET",
        "#str_42076": "REPLAY",
        "#str_42077": "DRAW",
        "#str_42078": "The match ended level. Replay it to settle the score.",
        "#str_42088": "QUALIFIER 1",
        "#str_42089": "QUALIFIER 2",
        "#str_42090": "QUALIFIER 3",
        "#str_42091": "TIER 1",
        "#str_42092": "TIER 2",
        "#str_42093": "TIER 3",
        "#str_42094": "TIER 4",
        "#str_42095": "TIER 5",
        "#str_42200": "Deathmatch",
        "#str_42201": "Duel",
        "#str_42202": "Team Deathmatch",
        "#str_42203": "Red Rover",
        "#str_42204": "Clan Arena",
    }
    english = languages["english"]
    for key, expected in english_expected.items():
        if english[key] != expected:
            raise AssertionError(
                f"English {key} is {english[key]!r}, expected {expected!r}"
            )

    for locale in ("french", "italian", "spanish"):
        translated = sum(
            languages[locale][key] != english[key] for key in required_ids
        )
        if translated < 40:
            raise AssertionError(
                f"{locale} arena table looks untranslated ({translated} distinct values)"
            )


def validate_menu_hooks() -> None:
    main_menu = read(MAIN_MENU)
    arena_menu = read(ARENA_MENU)
    mp_hud = read(ROOT / "content" / "baseoq4" / "pak0" / "guis" / "mphud.gui")
    session_menu = read(ROOT / "src" / "framework" / "Session_menu.cpp")

    for needle in (
        'set "desktop::dest" "37" ;',
        'resettime "anim_mainOut" "0" ;',
        'set "cmd" "singlePlayerOpen" ;',
        "onNamedEvent returnFromSinglePlayer",
        "singleplayer_topbar_home",
        "singleplayer_btmbar_home",
    ):
        require(main_menu, needle, "animated Single Player selector entry/return")
    require(
        main_menu,
        'set "cmd" "play main_menu_selection ; startMap game/airdefense1" ;',
        "unchanged Mission start",
    )
    for command in (
        "arenaMission",
        "arenaBrowse",
        "arenaClose",
        "arenaBackToModes",
        "arenaDifficulty -1",
        "arenaDifficulty 1",
        "arenaReset",
        "arenaResetConfirm",
        "arenaResetCancel",
        "arenaStart",
        "arenaRetry",
        "arenaDismissResult",
    ):
        require(arena_menu, command, "Arena Campaign GUI commands")
    for tier in range(5):
        require(
            arena_menu,
            f"arenaSelectTier {tier}",
            "Arena Campaign tier buttons",
        )
    for match in range(4):
        require(
            arena_menu,
            f"arenaSelectMatch {match}",
            "Arena Campaign match buttons",
        )
    for string_id in (
        "#str_42000",
        "#str_42001",
        "#str_42002",
        "#str_42009",
        "#str_42010",
        "#str_42011",
        "#str_42026",
        "#str_42027",
        "#str_42028",
        "#str_42029",
        "#str_42032",
        "#str_42033",
        "#str_42034",
        "#str_42049",
        "#str_42070",
        "#str_42071",
        "#str_42072",
        "#str_42073",
        "#str_42074",
        "#str_42075",
        "#str_42076",
        "#str_42079",
        "#str_42080",
        "#str_42081",
        "#str_42082",
        "#str_42083",
        "#str_42084",
        "#str_42085",
        "#str_42086",
        "#str_42087",
        "#str_42088",
        "#str_42089",
        "#str_42090",
        "#str_42091",
        "#str_42092",
        "#str_42093",
        "#str_42094",
        "#str_42095",
    ):
        require(arena_menu + mp_hud, string_id, "Arena Campaign GUI localization")

    for state_name in (
        "arenaCampaignProgress",
        "arena_resetConfirm",
        "arena_resultCanRetry",
        "arena_selectedTierDescription",
        "arena_selectedTierName",
        "arena_selectedSkill",
        "arena_previousCampaignProgress",
        "arena_progressChanged",
        "arena_resultTier",
        "arena_resultMatch",
        "arena_resultNewlyCompleted",
        "arena_resultUnlockedBoss",
        "arena_resultUnlockedTier",
        "arena_resultUnlockedCampaign",
        "arena_resultMatchName",
    ):
        require(arena_menu, state_name, "Arena Campaign GUI state")
    focus_handler = function_body(arena_menu, "onEvent")
    for focus_target in (
        "arena_reset_cancel",
        "arena_result_next",
        "arena_result_next_full",
        "arena_result_return",
        "arena_result_return_full",
        "arena_result_retry",
        "arena_result_replay",
        "arena_result_continue_only",
        "arena_mode_mission_card",
        "arena_start_enabled",
        "arena_browser_back",
        "arena_transition_blocker",
    ):
        require(
            focus_handler,
            f'setfocus "{focus_target}"',
            "Arena recommended keyboard/controller focus",
        )
    require_regex(
        arena_menu,
        r"windowDef\s+arena_result_draw_accent\s*\{.*?arena_result\"\s*==\s*4",
        "distinct Arena draw presentation",
    )
    require_regex(
        arena_menu,
        r"windowDef\s+arena_match3_locked_name\s*\{.*?arena_match3_locked\"\s*==\s*1.*?#str_42024",
        "locked Arena boss concealment",
    )
    require_regex(
        arena_menu,
        r"windowDef\s+arena_browser_reset_disabled\s*\{.*?arenaCampaignProgress\"\s*==\s*0",
        "disabled zero-progress reset",
    )

    esc_handler = function_body(arena_menu, "onESC")
    for event_name in (
        "arenaResetCancelTransition",
        "arenaReturnToModes",
        "arenaCloseSelector",
    ):
        require(esc_handler, event_name, "animated Arena Escape/back navigation")
    require(
        esc_handler,
        'resetTime "arena_result_exit_timeline" "0"',
        "animated Arena result Escape/back navigation",
    )

    for event_name in (
        "arenaMatchLaunch",
        "arenaResultReveal",
        "arenaProgressReveal",
        "arenaBrowserReveal",
        "arenaResetReveal",
    ):
        require(
            arena_menu,
            f"onNamedEvent {event_name}",
            "Arena cinematic menu lifecycle",
        )
    for timeline_name in (
        "arena_mode_reveal_timeline",
        "arena_mode_exit_timeline",
        "arena_browser_entry_timeline",
        "arena_browser_exit_timeline",
        "arena_mode_return_timeline",
        "arena_reset_reveal_timeline",
        "arena_reset_exit_timeline",
        "arena_launch_timeline",
        "arena_result_reveal_timeline",
        "arena_result_exit_timeline",
        "arena_progress_reveal_timeline",
        "arena_browser_reveal_timeline",
    ):
        require(arena_menu, timeline_name, "Arena cinematic menu animation")
    for overlay_name in (
        "arena_launch_entry_curtain",
        "arena_launch_exit_curtain",
        "arena_progress_entry_curtain",
        "arena_progress_exit_curtain",
        "arena_reset_entry_curtain",
        "arena_reset_exit_curtain",
        "arena_result_curtain",
    ):
        require(arena_menu, overlay_name, "Arena layered modal exit")
    for material in (
        "gfx/guis/mainmenu/popup_bg",
        "gfx/guis/mainmenu/popup_top",
        "gfx/guis/mainmenu/popup_mid",
        "gfx/guis/mainmenu/popup_btm",
        "gfx/guis/loading/bg_grid_tiled",
        "gfx/guis/mainmenu/topbar_tile_left",
        "gfx/guis/mainmenu/topbar_tile_right",
        "gfx/guis/mainmenu/btmbar_tile_left",
        "gfx/guis/mainmenu/btmbar_tile_right",
    ):
        require(arena_menu, material, "Arena shared legacy popup treatment")
    for state_name in ("arena_navigationAnimating", "arena_resetAnimating"):
        require(arena_menu, state_name, "Arena transition input blocking")
    require(
        arena_menu,
        '"gui::arena_resultMatchName"',
        "Arena post-match identity review",
    )
    require(
        arena_menu,
        '"gui::arenaCampaignProgress" - "gui::arena_previousCampaignProgress"',
        "Arena visible campaign progress delta",
    )
    require(
        arena_menu,
        "arena_progress_boss_pulse",
        "Arena newly unlocked boss-card reveal",
    )
    require(arena_menu, "arena_result_curtain", "Arena wipe-to-report continuity curtain")
    require(
        arena_menu,
        'set "Desktop::arena_launchVisible" "0"',
        "stale Arena launch-card cleanup",
    )
    require(
        arena_menu,
        "backcolor\t0.015,0.022,0.014,0.88",
        "legible Arena result ledger",
    )
    for needle in (
        'set "arena_result_panel::rect" "320,240,0,0" ;',
        'transition "arena_result_panel::rect" "320,240,0,0" "122,91,396,298" "520" ;',
        'set "arena_progress_update_panel::rect" "320,231,0,0" ;',
        'transition "arena_progress_update_panel::rect" "320,231,0,0" "110,169,420,124" "480" ;',
        "arena_reset_shell",
        'transition "arena_reset_shell::rect" "320,233,0,0" "140,33,360,400" "260" ;',
    ):
        require(arena_menu, needle, "two-axis Arena popup expansion")

    for event_name in (
        "arenaCampaignEntrance",
        "arenaCampaignVictory",
        "arenaCampaignPresentationClear",
    ):
        require(mp_hud, f"onNamedEvent {event_name}", "Arena in-world presentation HUD")
    for state_name in (
        "arena_presentTitle",
        "arena_presentSubtitle",
        "arena_presentVictor",
        "arena_presentOutcome",
        "arena_presentScore",
    ):
        require(mp_hud, state_name, "Arena in-world presentation HUD state")
    require(mp_hud, "oq4_arena_present_victory", "Arena victory presentation timeline")
    for timer_rect in ("23,59,200,20", "23,96,200,20", "23,94,200,20"):
        require(mp_hud, timer_rect, "readable localized Arena entrance clock")
    if '"gui::arena_presentVictor" != ""' in mp_hud:
        raise AssertionError("Arena HUD visibility must not depend on numeric GUI string comparison")

    if arena_menu.count('"gui::virtual_screen_x_expand"') < 4:
        raise AssertionError("Arena modal backdrops must cover widescreen expansion")
    if arena_menu.count('"gui::virtual_screen_y_expand"') < 4:
        raise AssertionError("Arena modal backdrops must cover vertical expansion")
    require_regex(
        arena_menu,
        r"windowDef\s+arena_screen_tint\s*\{.*?virtual_screen_x_expand.*?virtual_screen_y_expand",
        "Arena screen tint must cover both expanded axes",
    )
    text_scales = [
        float(value)
        for value in re.findall(r"\btextscale\s+([0-9]+(?:\.[0-9]+)?)", arena_menu)
    ]
    if not text_scales or min(text_scales) < 0.13:
        raise AssertionError("Arena GUI contains unreadably small authored text")

    require(
        session_menu,
        'if ( !idStr::Icmp( cmd, "singlePlayerOpen" ) )',
        "Single Player selector dispatch",
    )
    require(
        session_menu,
        "arenaCampaign.OpenSelector();",
        "Single Player selector dispatch",
    )
    require(
        session_menu,
        "} else if ( arenaCampaign.IsGui( gui ) ) {",
        "Arena Campaign GUI dispatch",
    )
    require(
        session_menu,
        "arenaCampaign.HandleGuiCommand( menuCommand );",
        "Arena Campaign GUI dispatch",
    )


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Missing function {signature!r}")
    brace = source.find("{", start)
    if brace < 0:
        raise AssertionError(f"Missing body for {signature!r}")
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : index]
    raise AssertionError(f"Unterminated function {signature!r}")


def validate_engine_hooks() -> None:
    arena = read(ROOT / "src" / "framework" / "ArenaCampaign.cpp")
    arena_header = read(ROOT / "src" / "framework" / "ArenaCampaign.h")
    common = read(ROOT / "src" / "framework" / "Common.cpp")
    session = read(ROOT / "src" / "framework" / "Session.cpp")
    session_local = read(ROOT / "src" / "framework" / "Session_local.h")
    async_network = read(ROOT / "src" / "framework" / "async" / "AsyncNetwork.cpp")
    async_client = read(ROOT / "src" / "framework" / "async" / "AsyncClient.cpp")
    draw_common = read(ROOT / "src" / "renderer" / "draw_common.cpp")
    vulkan_gui = read(ROOT / "src" / "renderer" / "Vulkan" / "vk_GuiExecutor.cpp")
    vulkan_blur_shader = read(
        ROOT / "src" / "renderer" / "Vulkan" / "shaders" / "material_depth_aware_blur.frag"
    )

    smooth_slow_time = function_body(
        common, "static bool openQ4_ShouldUseSmoothSingleplayerSlowTime( void )"
    )
    require(
        common,
        "static std::atomic<bool> openQ4_singleplayerGameModuleReady( false );",
        "Arena-safe async module state",
    )
    require(
        smooth_slow_time,
        "openQ4_singleplayerGameModuleReady.load( std::memory_order_acquire )",
        "Arena-safe async module transition",
    )
    if (
        "cvarSystem->" in smooth_slow_time
        or "GetCVarString" in smooth_slow_time
        or "com_activeGameModule" in smooth_slow_time
    ):
        raise AssertionError(
            "async slow-time detection must not read mutable CVar/module strings during a module swap"
        )

    # Arena's victor-focus DOF uses Raven's normalized controller contract.
    # GL consumes normalized depth directly; Vulkan converts focus/range to
    # world distance but must linearize its raw depth with the same historical
    # special-depth near-plane convention, not parm 7's distance scale.
    gl_blur = function_body(draw_common, "static bool RB_CompositeRVSpecialBlur( void )")
    require(
        gl_blur,
        "idMath::ClampFloat( 0.0f, 1.0f, tr.specialEffectParms[ SPECIAL_EFFECT_BLUR ][5] )",
        "OpenGL normalized special-blur focus",
    )
    vk_blur = function_body(
        vulkan_gui, "static bool VK_Exec_DrawRVSpecialBlur( const viewDef_t *viewDef )"
    )
    require(
        vulkan_gui,
        "VK_RVSPECIAL_DEPTH_ZNEAR = 0.25f",
        "Vulkan Raven special-depth convention",
    )
    require(
        vk_blur,
        "tr.specialEffectParms[ SPECIAL_EFFECT_BLUR ][ 5 ] )\n\t\t\t\t\t* distanceScale",
        "Vulkan normalized-focus translation",
    )
    require(
        vk_blur,
        "parms[ 5 ][ 0 ] = VK_RVSPECIAL_DEPTH_ZNEAR;",
        "Vulkan raw-depth linearization near plane",
    )
    require(
        vulkan_blur_shader,
        "5 specialDepthZNear (controller distance scale is consumed at the C++ boundary)",
        "Vulkan special-blur shader parameter contract",
    )
    require(
        vulkan_blur_shader,
        "writes the special-depth near plane here after consuming distanceScale",
        "Vulkan special-blur shader translated near-plane input",
    )
    if "parms[ 5 ][ 0 ] = distanceScale;" in vk_blur:
        raise AssertionError(
            "Vulkan special blur must not use the controller distance scale as zNear"
        )
    load_game_dll = function_body(common, "void idCommonLocal::LoadGameDLL( void )")
    unload_game_dll = function_body(common, "void idCommonLocal::UnloadGameDLL( void )")
    shutdown_game = function_body(common, "void idCommonLocal::ShutdownGame( bool reloading )")
    require(
        load_game_dll,
        "openQ4_singleplayerGameModuleReady.store( false, std::memory_order_release );",
        "Arena-safe module load transition",
    )
    require(
        load_game_dll,
        'game != NULL && idStr::Icmp( gameModuleBaseName, "game_sp" ) == 0,',
        "Arena-safe module ready transition",
    )
    require(
        shutdown_game,
        "openQ4_singleplayerGameModuleReady.store( false, std::memory_order_release );",
        "Arena-safe module shutdown transition",
    )
    if "game->Shutdown();" in unload_game_dll:
        raise AssertionError("Arena binary-only module unload still shuts down the game object")

    for token in (
        'ARENA_CAMPAIGN_FILE = "arena/openq4_campaign.cfg"',
        "ARENA_TIER_COUNT = 5",
        "ARENA_MATCHES_PER_TIER = 4",
        "ARENA_BOSS_MATCH = ARENA_MATCHES_PER_TIER - 1",
        "ARENA_PROGRESS_MASK",
        "ARENA_OUTCOME_DRAW = 2",
        "ARENA_ENTRANCE_MSEC = 1250",
        "enum arenaTransitionPhase_t",
        "ARENA_TRANSITION_ENTRANCE",
        "ARENA_TRANSITION_RETURN",
        "ARENA_TRANSITION_RESULT",
        '"ui_arenaProgress", "0"',
        "CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER",
        "CVAR_SYSTEM | CVAR_BOOL | CVAR_NOCHEAT",
        "CVAR_SYSTEM | CVAR_INTEGER | CVAR_NOCHEAT",
        "static bool ArenaTierUnlocked",
        "static bool ArenaBossUnlocked",
        "static int ArenaCanonicalProgress",
        "ArenaRepairPersistedProgress",
        "match < ARENA_BOSS_MATCH || ArenaBossUnlocked",
        "LoadCampaign()",
        "bool campaignClosed = false",
        "lexer.HadError() || !campaignClosed",
        'uiManager->FindGui( "guis/arena_menu.gui"',
        "ArenaMapSupportsMatch",
        'if ( !idStr::Icmp( cmd, "arenaMission" ) )',
        'mainMenu->HandleNamedEvent( "openMission" )',
        'if ( !idStr::Icmp( cmd, "arenaBrowse" ) )',
        'va( "addbot \\"%s\\" %d exact\\n"',
        "match->baseSkill + difficulty - 3 + match->bots[i].skillOffset",
        'va( "spawnServer \\"%s\\"\\n"',
        "ArenaMarkUniqueField",
        "MATCH_REQUIRED_FIELDS",
        "ArenaStringIsLocalizationKey",
        "match.bots.Num() & 1",
        "duplicate bot",
        "ArenaSaveCurrentCVars",
        "ArenaRestoreSavedCVars",
        "resetCVars",
        'va( "reset %s\\n", name )',
        'idStr::Icmp( name, "si_pure" ) == 0',
        "clientsBeforePopulation",
        "clientsBeforePopulation != 1",
        "idAsyncNetwork::server.GetNumClients()",
        "botsAdded != match->bots.Num()",
        'state->result = showBotFailure ? 3 : 0',
        '"si_minPlayers"',
        'ArenaSetMatchCVar( "si_minPlayers", 2 )',
        'cvarSystem->SetCVarBool( "si_isBuyingEnabled", false )',
        'cvarSystem->SetCVarBool( "si_dropWeaponsInBuyingModes", false )',
        'cvarSystem->SetCVarBool( "si_allowVoting", false )',
        'cvarSystem->SetCVarBool( "si_teamDamage", false )',
        'cvarSystem->SetCVarBool( "si_weaponStay", false )',
        'ArenaSetMatchCVar( "si_overtime", 120 )',
        'ArenaSetMatchCVar( "si_fps", 60 )',
        '"ui_autoJoin"',
        'cvarSystem->SetCVarBool( "ui_autoJoin", true )',
        'cvarSystem->SetCVarString( "ui_spectate", "Play" )',
        'ArenaSetMatchCVar( "si_arenaCampaign", token )',
        "progress |= ArenaMatchBit( tier, match )",
        'state->resultUnlock = "#str_42047"',
        'state->resultUnlock = "#str_42046"',
        'state->resultUnlock = "#str_42045"',
        "const bool newlyCompleted",
        "!ArenaBossUnlocked( previousProgress, tier )",
        'if ( !idStr::Icmp( cmd, "arenaRetry" ) )',
        'if ( !idStr::Icmp( cmd, "arenaResetConfirm" ) )',
        'if ( !idStr::Icmp( cmd, "arenaResetCancel" ) )',
        '"arena_selectedTierDescription"',
        '"arena_selectedSkill"',
        '"arena_resultCanRetry"',
        '"arena_resetConfirm"',
        'cmdSystem->RemoveCommand( "arenaCampaignFinish" )',
        'cmdSystem->BufferCommandText( CMD_EXEC_INSERT, "arenaCampaignFinish\\n" )',
        '"arena campaign: accepted result token %d',
        "moduleTransitionPending",
        'state->result == 4 ? ArenaLocalized( "#str_42077" )',
        'state->result == 4 ? ArenaLocalized( "#str_42078" )',
        'HandleNamedEvent( "arenaMatchLaunch" )',
        'HandleNamedEvent( "arenaResultReveal" )',
        '"arenaProgressReveal"',
        '"arenaBrowserReveal"',
        '"arena_transitionPhase"',
        '"arena_transitionRemainingMsec"',
        '"arena_previousCampaignProgress"',
        '"arena_progressChanged"',
        '"arena_resultTier"',
        '"arena_resultMatch"',
        '"arena_resultNewlyCompleted"',
        '"arena_resultUnlockedBoss"',
        '"arena_resultUnlockedTier"',
        '"arena_resultUnlockedCampaign"',
        '"arena_resultTierName"',
        '"arena_resultMatchName"',
    ):
        require(arena, token, "Arena Campaign framework")

    saved_cvars_match = re.search(
        r"ARENA_SAVED_CVARS\[\]\s*=\s*\{(?P<body>.*?)\bNULL\s*\n\s*\};",
        arena,
        re.DOTALL,
    )
    if saved_cvars_match is None:
        raise AssertionError("Could not inspect Arena overridden-CVar snapshot list")
    saved_cvars = set(re.findall(r'"([A-Za-z0-9_]+)"', saved_cvars_match.group("body")))
    prepare_body = function_body(arena, "bool idArenaCampaign::PrepareServer()")
    overridden_cvars = set(
        re.findall(
            r'(?:ArenaSetMatchCVar|cvarSystem->SetCVar(?:Bool|Float|Integer|String))'
            r'\(\s*"([A-Za-z0-9_]+)"',
            prepare_body,
        )
    )
    unsaved_overrides = overridden_cvars - saved_cvars - {"si_arenaCampaign"}
    if unsaved_overrides:
        raise AssertionError(
            f"Arena match overrides are not restored: {sorted(unsaved_overrides)}"
        )

    for forbidden_clamp in (
        "match.baseSkill = idMath::ClampInt",
        "match.fragLimit = idMath::ClampInt",
        "match.timeLimit = idMath::ClampInt",
        "match.roundLimit = idMath::ClampInt",
        "match.scoreLimit = idMath::ClampInt",
        "bot.skillOffset = idMath::ClampInt",
    ):
        if forbidden_clamp in arena:
            raise AssertionError(
                f"Arena manifest must reject authored range errors, found {forbidden_clamp!r}"
            )

    difficulty_body = function_body(
        arena, "static const char *ArenaDifficultyStringKey"
    )
    difficulty_ids = re.findall(r"#str_\d+", difficulty_body)
    if difficulty_ids != [
        "#str_42060",
        "#str_42061",
        "#str_42062",
        "#str_42063",
        "#str_42064",
    ]:
        raise AssertionError(
            f"Arena difficulty localization map drifted: {difficulty_ids}"
        )

    game_type_body = function_body(
        arena, "static const char *ArenaGameTypeStringKey"
    )
    expected_game_type_ids = {
        "DM": "#str_42200",
        "Duel": "#str_42201",
        "Team DM": "#str_42202",
        "Red Rover": "#str_42203",
        "Clan Arena": "#str_42204",
    }
    for game_type, string_id in expected_game_type_ids.items():
        require_regex(
            game_type_body,
            rf'Icmp\(\s*"{re.escape(game_type)}"\s*\).*?return\s+"{string_id}"',
            f"{game_type} localization mapping",
        )

    tier_unlock_body = function_body(arena, "static bool ArenaTierUnlocked")
    require(
        tier_unlock_body,
        "tier == 0 || ArenaMatchComplete( progress, tier - 1, ARENA_BOSS_MATCH )",
        "boss win unlocks the next tier",
    )
    boss_unlock_body = function_body(arena, "static bool ArenaBossUnlocked")
    require(
        boss_unlock_body,
        "match < ARENA_BOSS_MATCH",
        "first three matches gate the boss",
    )
    require(
        boss_unlock_body,
        "!ArenaMatchComplete( progress, tier, match )",
        "every preliminary win gates the boss",
    )

    canonical_progress_body = function_body(
        arena, "static int ArenaCanonicalProgress"
    )
    for token in (
        "progress &= ARENA_PROGRESS_MASK",
        "!ArenaMatchComplete( canonical, tier - 1, ARENA_BOSS_MATCH )",
        "qualifiersComplete = false",
        "qualifiersComplete && ArenaMatchComplete",
    ):
        require(canonical_progress_body, token, "canonical Arena progression repair")

    def canonical_progress(progress: int) -> int:
        progress &= (1 << 20) - 1
        canonical = 0
        for tier in range(5):
            if tier > 0:
                previous_boss = 1 << (tier * 4 - 1)
                if not canonical & previous_boss:
                    break
            qualifiers = 0b111 << (tier * 4)
            canonical |= progress & qualifiers
            boss = 1 << (tier * 4 + 3)
            if canonical & qualifiers == qualifiers and progress & boss:
                canonical |= boss
        return canonical

    progress_repair_cases = {
        0: 0,
        1 << 3: 0,
        0b0111: 0b0111,
        0b1111: 0b1111,
        1 << 4: 0,
        0b1111 | (1 << 4): 0b1111 | (1 << 4),
        0b1111 | (1 << 7): 0b1111,
        0xFF | (1 << 8): 0xFF | (1 << 8),
        (1 << 20) - 1: (1 << 20) - 1,
        (1 << 24) | 0b1111: 0b1111,
    }
    for persisted, expected in progress_repair_cases.items():
        actual = canonical_progress(persisted)
        if actual != expected:
            raise AssertionError(
                "Arena progress repair drifted: "
                f"0x{persisted:x} became 0x{actual:x}, expected 0x{expected:x}"
            )

    load_body = function_body(arena, "bool idArenaCampaign::LoadCampaign()")
    require(
        load_body,
        "ArenaRepairPersistedProgress();",
        "same-version Arena saves are canonicalized on load",
    )
    shutdown_body = function_body(arena, "void idArenaCampaign::Shutdown()")
    require(
        shutdown_body,
        "state->moduleTransitionPending",
        "only the intended game_sp to game_mp reload preserves an Arena launch",
    )
    abort_body = function_body(arena, "void idArenaCampaign::AbortMatch()")
    require(
        abort_body,
        "state->moduleTransitionPending = false;",
        "Arena abort clears pending module-transition state",
    )
    require(
        abort_body,
        "if ( state->result != 0 )",
        "Arena abort detects an unfinished result presentation",
    )
    require(
        abort_body,
        "ArenaClearResultMetadata( state );",
        "Arena abort cannot strand an unrevealed result overlay",
    )
    for token in (
        "state->completionPending = false;",
        "state->pendingToken = 0;",
        "state->botsPopulated = false;",
        "state->moduleTransitionPending = false;",
        "state->transitionPhase = ARENA_TRANSITION_IDLE;",
        "state->transitionDeadlineMsec = 0;",
        "state->launchToken = 0;",
        "state->resultRevealPending = false;",
        "ui_arenaCampaignActive.SetBool( false );",
        "ui_arenaActiveMatch.SetInteger( 0 );",
        "ArenaRestoreSavedCVars( state );",
    ):
        require(abort_body, token, "atomic Arena cancellation cleanup")
    for token in (
        "if ( state->returnWipeHeld )",
        "sessLocal.ClearWipe();",
        "state->returnWipeHeld = false;",
    ):
        require(abort_body, token, "Arena cancellation releases a held return wipe")
    for signature in (
        "void idArenaCampaign::RequestResetProgress()",
        "void idArenaCampaign::ResetProgress()",
    ):
        reset_body = function_body(arena, signature)
        require(
            reset_body,
            "ui_arenaProgress.GetInteger() == 0",
            "zero-progress Arena reset guard",
        )
    request_reset_body = function_body(
        arena, "void idArenaCampaign::RequestResetProgress()"
    )
    require(
        request_reset_body,
        'HandleNamedEvent( "arenaResetReveal" )',
        "event-driven Arena reset reveal",
    )
    queue_body = function_body(
        arena, "bool idArenaCampaign::QueueCompletion( const idCmdArgs &args )"
    )
    for token in (
        "args.Argc() != 5",
        "ArenaParseBoundedInteger",
        "ARENA_OUTCOME_DRAW",
        "state->pendingOutcome = outcome",
    ):
        require(queue_body, token, "strict Arena completion handoff")
    if "atoi(" in queue_body:
        raise AssertionError("Arena completion handoff must not accept partial integers")
    require(
        queue_body,
        "QueueFailedCompletionCleanup();",
        "invalid active Arena result fails closed",
    )
    failed_completion = function_body(
        arena, "void idArenaCampaign::QueueFailedCompletionCleanup()"
    )
    for token in (
        "!IsActive()",
        "state->completionPending = true;",
        "state->pendingToken = 0;",
        'BufferCommandText( CMD_EXEC_INSERT, "arenaCampaignFinish\\n" )',
    ):
        require(failed_completion, token, "deferred invalid-result cleanup")

    arena_frame = function_body(arena, "void idArenaCampaign::Frame()")
    for token in (
        "state->transitionPhase == ARENA_TRANSITION_RETURN",
        "state->completionPending",
        "FinishPendingCompletion();",
        "state->transitionPhase != ARENA_TRANSITION_ENTRANCE",
    ):
        require(arena_frame, token, "Arena transition frame orchestration")
    result_finish = arena_frame.find("FinishPendingCompletion();")
    entrance_commit = arena_frame.find("CommitPendingMatch();")
    if not 0 <= result_finish < entrance_commit:
        raise AssertionError(
            "Arena result return must finish before considering an entrance commit"
        )
    require(arena_frame, "CommitPendingMatch();", "Arena entrance fallback commit")

    start_match = function_body(arena, "void idArenaCampaign::StartSelectedMatch()")
    for token in (
        "state->transitionPhase = ARENA_TRANSITION_ENTRANCE;",
        "state->transitionDeadlineMsec = Sys_Milliseconds() + ARENA_ENTRANCE_MSEC;",
        'HandleNamedEvent( "arenaMatchLaunch" )',
    ):
        require(start_match, token, "Arena entrance orchestration")
    if "spawnServer" in start_match:
        raise AssertionError("Arena entrance must complete before spawnServer is queued")

    commit_match = function_body(arena, "void idArenaCampaign::CommitPendingMatch()")
    for token in (
        "ArenaMatchForToken( state, token, &tier, &matchIndex )",
        "state->transitionPhase = ARENA_TRANSITION_MATCH;",
        "ui_arenaCampaignActive.SetBool( true );",
        'va( "spawnServer \\"%s\\"\\n", match->map.c_str() )',
    ):
        require(commit_match, token, "Arena entrance commit")

    open_browser = function_body(arena, "void idArenaCampaign::OpenBrowser()")
    open_selector = function_body(arena, "void idArenaCampaign::OpenSelector()")
    for name, body, view in (
        ("selector", open_selector, "state->view = 0;"),
        ("browser", open_browser, "state->view = 1;"),
    ):
        view_index = body.find(view)
        update_index = body.find("UpdateGui();", view_index)
        set_gui_index = body.find("sessLocal.SetGUI( state->gui, NULL );", view_index)
        if not 0 <= view_index < update_index < set_gui_index:
            raise AssertionError(
                f"Arena {name} GUI state must be published before synchronous activation"
            )
    update_before_reveal = open_browser.find("UpdateGui();")
    result_reveal = open_browser.find('HandleNamedEvent( "arenaResultReveal" )')
    if not 0 <= update_before_reveal < result_reveal:
        raise AssertionError("Arena result reveal must fire after post-match GUI state is published")

    finish_completion = function_body(
        arena, "void idArenaCampaign::FinishPendingCompletion()"
    )
    for token in (
        "state->resultPreviousProgress = previousProgress;",
        "state->resultTier = tier;",
        "state->resultMatch = match;",
        "state->resultNewlyCompleted = newlyCompleted;",
        "state->resultTierName = state->tiers[tier].name;",
        "state->resultMatchName = completedMatch->name;",
        "state->transitionPhase = ARENA_TRANSITION_RESULT;",
        'sessLocal.StartWipe( "gfx/wipes/fade", true );',
        "sessLocal.CompleteWipe();",
    ):
        require(finish_completion, token, "Arena cinematic result return")
    wipe_capture = finish_completion.find('sessLocal.StartWipe( "gfx/wipes/fade", true );')
    wipe_complete = finish_completion.find("sessLocal.CompleteWipe();", wipe_capture)
    stop_server = finish_completion.find("sessLocal.StopPreservingWipe();", wipe_complete)
    clear_server_token = finish_completion.find(
        'cvarSystem->SetCVarInteger( "si_arenaCampaign", 0 );', stop_server
    )
    open_result_browser = finish_completion.find("OpenBrowser();", clear_server_token)
    clear_return_wipe = finish_completion.find("sessLocal.ClearWipe();", open_result_browser)
    if not (
        0
        <= wipe_capture
        < wipe_complete
        < stop_server
        < clear_server_token
        < open_result_browser
        < clear_return_wipe
    ):
        raise AssertionError(
            "Arena return must hold black through teardown/result setup, then release it to the GUI"
        )

    stop_internal = function_body(
        session, "void idSessionLocal::StopInternal( bool preserveWipe )"
    )
    for token in (
        "const bool outermostStop = stopDepth == 0;",
        "preserveWipeDuringStop = preserveWipe;",
        "if ( !preserveWipeDuringStop )",
        "ClearWipe();",
        "const auto finishStopPolicy",
        "catch( ... )",
        "finishStopPolicy();",
    ):
        require(stop_internal, token, "nested-safe session wipe preservation")
    require(
        session_local,
        "void\t\t\t\tStopPreservingWipe();",
        "Arena return wipe session interface",
    )

    dismiss_result = function_body(arena, "void idArenaCampaign::DismissResult()")
    update_before_progress = dismiss_result.find("UpdateGui();")
    progress_reveal = dismiss_result.find('"arenaProgressReveal"')
    browser_reveal = dismiss_result.find('"arenaBrowserReveal"')
    if not 0 <= update_before_progress < progress_reveal < browser_reveal:
        raise AssertionError("Arena progression reveal must fire after the result panel clears")
    for forbidden in (
        "state->resultToken = 0;",
        "state->resultTier = -1;",
        "state->resultMatch = -1;",
        "state->resultTierName.Clear();",
        "state->resultMatchName.Clear();",
    ):
        if forbidden in dismiss_result:
            raise AssertionError("DismissResult must preserve completed-match metadata")

    update_body = function_body(arena, "void idArenaCampaign::UpdateGui()")
    require(
        update_body,
        'SetStateInt( "arenaCampaignProgress"',
        "Arena Campaign progress-bar state",
    )
    for string_id in (
        "#str_42012",
        "#str_42022",
        "#str_42023",
        "#str_42030",
        "#str_42031",
        "#str_42045",
        "#str_42048",
        "#str_42065",
        "#str_42066",
        "#str_42067",
        "#str_42068",
        "#str_42069",
    ):
        require(update_body, string_id, "Arena Campaign GUI state mapping")
    require(
        update_body,
        'const char *status = !unlocked ? "#str_42022"',
        "locked Arena status",
    )
    require(
        update_body,
        'complete ? "#str_42023"',
        "completed Arena status",
    )
    require(
        update_body,
        'bossReady ? "#str_42045" : "#str_42065"',
        "boss-ready and available Arena statuses",
    )
    require(
        update_body,
        'state->result == 1 ? ArenaLocalized( "#str_42030" )',
        "Arena victory title",
    )
    require(
        update_body,
        'state->result == 2 ? ArenaLocalized( "#str_42031" )',
        "Arena defeat title",
    )
    require(
        update_body,
        'state->result == 1 ? ArenaLocalized( "#str_42066" )',
        "Arena victory message",
    )
    require(
        update_body,
        'state->result == 2 ? ArenaLocalized( "#str_42067" )',
        "Arena defeat message",
    )
    require(
        update_body,
        'state->result == 3 ? ArenaLocalized( "#str_42068" )',
        "Arena bot-population failure title",
    )
    require(
        update_body,
        'state->result == 3 ? ArenaLocalized( "#str_42069" )',
        "Arena bot-population failure message",
    )

    for token in (
        "class idArenaCampaign",
        "OpenSelector",
        "StartSelectedMatch",
        "PrepareServer",
        "PopulateBots",
        "QueueCompletion",
        "FinishPendingCompletion",
        "Frame",
        "CommitPendingMatch",
        "AbortMatch",
        "NeedsCleanup",
    ):
        require(arena_header, token, "Arena Campaign public interface")

    for token in (
        "arenaCampaign.Init();",
        "arenaCampaign.Shutdown();",
        "arenaCampaign.AbortMatch();",
    ):
        require(session, token, "Arena Campaign session lifecycle")

    common_frame = function_body(common, "void idCommonLocal::Frame( void )")
    network_frame = common_frame.find("idAsyncNetwork::RunFrame();")
    arena_transition_frame = common_frame.find("arenaCampaign.Frame();")
    network_branch = common_frame.find("if ( idAsyncNetwork::IsActive() )")
    if not 0 <= network_frame < arena_transition_frame < network_branch:
        raise AssertionError(
            "Arena transition frame must run after async results and before the network/session split"
        )

    reload_module = function_body(common, "void Com_ReloadGameModule_f( const idCmdArgs &args )")
    swap_failure = reload_module.find("if ( swapFailed )")
    swap_cleanup = reload_module.find("arenaCampaign.AbortMatch();", swap_failure)
    swap_menu = reload_module.find("session->StartMenu();", swap_failure)
    if not 0 <= swap_failure < swap_cleanup < swap_menu:
        raise AssertionError(
            "failed Arena game-module swaps must restore their transaction before opening the menu"
        )
    post_reload_failure = reload_module.find("if ( !cmdSystem->PostReloadEngine() )")
    abandoned_launch = reload_module.find(
        "const bool abandonedArenaLaunch = arenaCampaign.NeedsCleanup();",
        post_reload_failure,
    )
    post_reload_abort = reload_module.find("arenaCampaign.AbortMatch();", abandoned_launch)
    post_reload_browser = reload_module.find("arenaCampaign.OpenBrowser();", post_reload_abort)
    if not 0 <= post_reload_failure < abandoned_launch < post_reload_abort < post_reload_browser:
        raise AssertionError(
            "Arena must recover to its browser when the post-swap launch command is lost"
        )

    reliable_messages = function_body(
        async_client, "void idAsyncClient::ProcessReliableServerMessages( void )"
    )
    disconnect_case = reliable_messages.find("case SERVER_RELIABLE_MESSAGE_DISCONNECT")
    reliable_abort = reliable_messages.find("arenaCampaign.AbortMatch();", disconnect_case)
    reliable_stop = reliable_messages.find("session->Stop();", reliable_abort)
    if not 0 <= disconnect_case < reliable_abort < reliable_stop:
        raise AssertionError(
            "server-directed Arena disconnect must restore the campaign before session teardown"
        )
    disconnect_packet = function_body(
        async_client,
        "void idAsyncClient::ProcessDisconnectMessage( const netadr_t from, const idBitMsg &msg )",
    )
    packet_abort = disconnect_packet.find("arenaCampaign.AbortMatch();")
    packet_stop = disconnect_packet.find("session->Stop();", packet_abort)
    if not 0 <= packet_abort < packet_stop:
        raise AssertionError(
            "Arena disconnect packets must restore the campaign before session teardown"
        )
    connect_to_server = function_body(
        async_client, "void idAsyncClient::ConnectToServer( const netadr_t adr )"
    )
    connect_abort = connect_to_server.find("arenaCampaign.AbortMatch();")
    connect_stop = connect_to_server.find("session->Stop();", connect_abort)
    if not 0 <= connect_abort < connect_stop:
        raise AssertionError(
            "connect/reconnect must cancel a pending Arena transaction before replacing its session"
        )

    quit_body = function_body(common, "void idCommonLocal::Quit( void )")
    write_configuration_body = function_body(
        common, "void idCommonLocal::WriteConfiguration( void )"
    )
    require(
        common,
        'com_activeGameModule", "", CVAR_SYSTEM | CVAR_NOCHEAT',
        "active game-module identity survives multiplayer cheat reset",
    )
    require(
        common,
        'com_nextGameModule", "", CVAR_SYSTEM | CVAR_NOCHEAT',
        "pending game-module identity survives multiplayer cheat reset",
    )
    require(
        quit_body,
        "arenaCampaign.AbortMatch();",
        "Arena settings restored before clean shutdown",
    )
    if quit_body.find("arenaCampaign.AbortMatch();") > quit_body.find(
        "WriteConfiguration();"
    ):
        raise AssertionError(
            "Arena clean-quit restore must run before the final configuration write"
        )
    require(
        write_configuration_body,
        "arenaCampaign.NeedsCleanup()",
        "temporary Arena server rules are never serialized",
    )

    for token in (
        "!arenaCampaign.PrepareServer()",
        "arenaCampaign.PopulateBots();",
        '"arenaComplete"',
        "arenaCampaign.QueueCompletion( args );",
    ):
        require(async_network, token, "Arena Campaign async handoff")
    completion_dispatch = function_body(
        async_network, "void idAsyncNetwork::ExecuteSessionCommand( const char *sessCmd )"
    )
    if "atoi( args.Argv" in completion_dispatch:
        raise AssertionError("Async Arena result dispatch must preserve strict parsing")


def validate_game_hooks() -> None:
    sys_cvar = GAME_LIBS_ROOT / "src" / "mpgame" / "gamesys" / "SysCvar.cpp"
    multiplayer = GAME_LIBS_ROOT / "src" / "mpgame" / "MultiplayerGame.cpp"
    multiplayer_header = GAME_LIBS_ROOT / "src" / "mpgame" / "MultiplayerGame.h"
    network_runtime = GAME_LIBS_ROOT / "src" / "mpgame" / "Game_network.cpp"
    sp_multiplayer = GAME_LIBS_ROOT / "src" / "game" / "MultiplayerGame.cpp"
    sp_network_runtime = GAME_LIBS_ROOT / "src" / "game" / "Game_network.cpp"
    bot_runtime = GAME_LIBS_ROOT / "src" / "mpgame" / "bots" / "Bot.cpp"
    duel_runtime = GAME_LIBS_ROOT / "src" / "mpgame" / "mp" / "Duel.cpp"
    state_runtime = GAME_LIBS_ROOT / "src" / "mpgame" / "mp" / "GameState.cpp"
    round_runtime = GAME_LIBS_ROOT / "src" / "mpgame" / "mp" / "RoundGameState.cpp"
    round_modes = GAME_LIBS_ROOT / "src" / "mpgame" / "mp" / "RoundModes.cpp"
    if not (
        sys_cvar.is_file()
        and multiplayer.is_file()
        and multiplayer_header.is_file()
        and network_runtime.is_file()
        and sp_multiplayer.is_file()
        and sp_network_runtime.is_file()
        and bot_runtime.is_file()
        and duel_runtime.is_file()
        and state_runtime.is_file()
        and round_runtime.is_file()
        and round_modes.is_file()
    ):
        print(
            "arena_campaign: skipped GameLibs hook check "
            f"(no companion checkout at {GAME_LIBS_ROOT})"
        )
        return

    cvars = read(sys_cvar)
    game = read(multiplayer)
    header = read(multiplayer_header)
    network = read(network_runtime)
    sp_game = read(sp_multiplayer)
    sp_network = read(sp_network_runtime)
    bots = read(bot_runtime)
    duel = read(duel_runtime)
    state = read(state_runtime)
    rounds = read(round_runtime)
    modes = read(round_modes)
    require(cvars, 'idCVar si_arenaCampaign(', "Arena Campaign server token")
    require(cvars, "CVAR_GAME | CVAR_SERVERINFO | CVAR_INTEGER", "Arena server token flags")
    require(cvars, '"single-player arena campaign match token', "Arena server token purpose")

    for token in (
        "BeginArenaCampaignResult",
        "UpdateArenaCampaignResult",
        'gameLocal.serverInfo.GetInt( "si_arenaCampaign" )',
        "gameLocal.IsTeamGame()",
        "gameLocal.gameType != GAME_REDROVER",
        "arenaResultPlayerScore > arenaResultOpponentScore",
        "ARENA_RESULT_DRAW",
        "ARENA_SCORE_UNAVAILABLE",
        "arenaResultOutcome",
        "arenaResultOpponentScore = 0;",
        'gameLocal.sessionCommand = va( "arenaComplete %d %d %d %d"',
        "currentState == GAMEREVIEW",
        '"arena campaign: queued result token %d',
        '"arena campaign: reporting result token %d',
    ):
        require(game, token, "Arena Campaign authoritative result")
    require(header, "BeginArenaCampaignResult", "Arena result declarations")
    require(header, "UpdateArenaCampaignResult", "Arena result declarations")

    result_capture = function_body(
        game, "void idMultiplayerGame::BeginArenaCampaignResult( void )"
    )
    require(
        result_capture,
        "gameLocal.gameType == GAME_DUEL && player->spectating",
        "Arena Duel result excludes waiting queue",
    )

    score_tie = function_body(
        game, "bool idMultiplayerGame::ScoreIsTied( int *leadingScore )"
    )
    require(score_tie, "topScoreCount > 1", "Arena live tie detection")
    require(score_tie, "scoreFound ? topScore : 0", "Arena signed leading score")
    if "GetRankedPlayer" in score_tie:
        raise AssertionError("Arena tie detection must not depend on stale ranked players")

    dm_run = function_body(state, "void rvDMGameState::Run( void )")
    require(dm_run, "ScoreIsTied( &leadingScore )", "Duel/DM authoritative overtime tie")
    require(
        state,
        '"overtime: period %d extended match by %d seconds',
        "Arena overtime diagnostic",
    )

    schedule_round = function_body(
        rounds, "bool rvRoundGameState::ScheduleNextRound( void )"
    )
    prepare_round = schedule_round.find("PrepareNextRound();")
    reset_round = schedule_round.find("ResetRound();")
    if not 0 <= prepare_round < reset_round:
        raise AssertionError("round modes must normalize state before the next respawn")

    rover_prepare = function_body(
        modes, "void rvRedRoverGameState::PrepareNextRound( void )"
    )
    for token in (
        "player->team = targetTeam;",
        "player->latchedTeam = targetTeam;",
        'BufferCommandText( CMD_EXEC_NOW, va( "updateUI %d',
        '"red rover: prepared round %d with %d Marine and %d Strogg players',
    ):
        require(rover_prepare, token, "Red Rover between-round rebalance")

    bot_match_end = function_body(bots, "void rvBotManager::OnMatchEnd( void )")
    require(bot_match_end, "topScoreCount > 1", "Arena bot draw detection")
    require(
        bot_match_end,
        "gameLocal.gameType == GAME_DUEL && player->spectating",
        "Arena bot result excludes Duel waiting queue",
    )
    duel_waiter_skip = bot_match_end.rfind(
        "gameLocal.gameType == GAME_DUEL && player->spectating"
    )
    bot_result_chat = bot_match_end.find("bots[i].OnMatchEnd( won );")
    if not 0 <= duel_waiter_skip < bot_result_chat:
        raise AssertionError("Duel waiting bots must be skipped before result chat")
    require(bot_match_end, "gameLocal.mpGame.ForfeitTeam()", "Arena bot forfeit result")
    require(bot_match_end, "forfeitWinner < 0", "Arena bot true-draw detection")
    require(
        bot_match_end,
        "player->team == forfeitWinner",
        "Arena bot forfeit winner chatter",
    )
    require(bot_match_end, "if ( matchTied )", "Arena bot draw-chat suppression")

    for module, module_game, module_network in (
        ("game_mp", game, network),
        ("game_sp", sp_game, sp_network),
    ):
        message_writer = function_body(
            module_game,
            "void idMultiplayerGame::PrintMessageEvent( int to, msg_evt_t evt, "
            "int parm1, int parm2 )",
        )
        message_reader = function_body(
            module_network,
            "void idGameLocal::ClientProcessReliableMessage( int clientNum, "
            "const idBitMsg &msg )",
        )
        require(
            message_writer,
            "outMsg.WriteChar( parm1 );",
            f"{module} signed multiplayer message parameter write",
        )
        require(
            message_writer,
            "outMsg.WriteChar( parm2 );",
            f"{module} signed multiplayer message parameter write",
        )
        require(
            message_reader,
            "parm1 = msg.ReadChar( );",
            f"{module} signed multiplayer message parameter read",
        )
        require(
            message_reader,
            "parm2 = msg.ReadChar( );",
            f"{module} signed multiplayer message parameter read",
        )
        for unsigned_form in (
            "outMsg.WriteByte( parm1",
            "outMsg.WriteByte( parm2",
            "parm1 = msg.ReadByte",
            "parm2 = msg.ReadByte",
        ):
            if unsigned_form in message_writer or unsigned_form in message_reader:
                raise AssertionError(
                    f"{module} must not encode optional message parameters as unsigned bytes"
                )

    result_update = function_body(
        game, "void idMultiplayerGame::UpdateArenaCampaignResult( void )"
    )
    benign_review_handoff = result_update.find(
        'idStr::Icmp( gameLocal.sessionCommand.c_str(), "game_startmenu" ) == 0'
    )
    unrelated_handoff = result_update.find(
        "because session command '%s' owns the handoff"
    )
    unrelated_handoff_cancel = result_update.find(
        "arenaResultPending = false;", unrelated_handoff
    )
    result_report = result_update.find(
        'gameLocal.sessionCommand = va( "arenaComplete %d %d %d %d"'
    )
    if not (
        0
        <= benign_review_handoff
        < unrelated_handoff
        < unrelated_handoff_cancel
        < result_report
    ):
        raise AssertionError(
            "Arena completion must retry the review menu but preserve unrelated handoffs"
        )

    stat_summary = function_body(game, "void idMultiplayerGame::ShowStatSummary( void )")
    campaign_guard = stat_summary.find("if ( arenaCampaignToken > 0 )")
    stock_review_menu = stat_summary.find('gameLocal.sessionCommand = "game_startmenu";')
    if not 0 <= campaign_guard < stock_review_menu:
        raise AssertionError(
            "Arena review must suppress only the stock multiplayer stat-summary command"
        )
    require(
        stat_summary,
        '"arena campaign: suppressed multiplayer stat menu for result token %d',
        "Arena review-menu collision diagnostic",
    )
    require(
        stat_summary,
        '} else {\n\t\tnextMenu = 3;\n\t\tgameLocal.sessionCommand = "game_startmenu";',
        "Stock multiplayer stat summary remains unchanged outside Arena",
    )
    require(
        bots,
        "botCharacterManager.FindCharacter( this->name.c_str() )",
        "Arena exact character roster binding",
    )
    require(
        bots,
        "botCharacterManager.MarkCharacterUsed( character )",
        "Arena character roster claim",
    )
    require(
        bots,
        "player->spectating && player->wantSpectate",
        "Arena bot team balancing ignores only genuine spectators",
    )
    require(
        bots,
        'gameLocal.userInfo[i].GetString( "ui_team" )',
        "Arena bot team balancing counts pending play-intent teams",
    )

    duel_run = function_body(duel, "void rvDuelGameState::Run( void )")
    queue_update = duel_run.find("UpdateQueue();")
    queue_promotion = duel_run.find("PromoteFromQueue();")
    base_state_run = duel_run.find("rvDMGameState::Run();")
    if not 0 <= queue_update < queue_promotion < base_state_run:
        raise AssertionError(
            "Duel must seed contenders before the base WARMUP state can advance"
        )
    require(
        duel_run,
        "if ( p->wantSpectate )",
        "ordinary Duel spectators remain outside contender seeding",
    )


def validate_docs() -> None:
    user_doc = read(ROOT / "docs" / "user" / "arena-campaign.md")
    readme = read(ROOT / "README.md")
    bot_doc = read(ROOT / "docs" / "dev" / "mp-bots.md")
    release_completion = read(ROOT / "docs" / "dev" / "release-completion.md")
    release_notes = read(ROOT / "docs" / "dev" / "releases" / "v0.10.000.md")

    for token in (
        "The first three matches in a tier are available together.",
        "Winning all three unlocks that tier's boss match.",
        "Winning the boss match unlocks the next tier.",
        "A draw does not award a win or unlock progress",
        "recommended action receives focus",
        "controller back action",
        "five tiers",
        "twenty",
        "Mission",
        "q4cmp",
        "do not yet pursue flags or map objectives",
    ):
        require(user_doc, token, "Arena Campaign player guide")
    require(
        readme,
        "[Arena Campaign](docs/user/arena-campaign.md)",
        "Arena Campaign README link",
    )
    for token in (
        "arena/openq4_campaign.cfg",
        "Match indexes 0 through",
        "Only kill-driven modes appear",
    ):
        require(bot_doc, token, "Arena Campaign bot documentation")
    require(
        release_completion,
        "Single-player now includes an Arena Campaign",
        "Arena Campaign candidate changelog",
    )
    for token in (
        "# openQ4 0.10.000",
        "A new Arena Campaign joins single-player.",
        "No extra maps or replacement assets are required.",
        "explicit draw results",
        "The campaign currently focuses on combat-driven modes.",
    ):
        require(release_notes, token, "Arena Campaign release notes")


def main() -> int:
    campaign = parse_campaign()
    validate_manifest(campaign)
    validate_localization(campaign)
    validate_menu_hooks()
    validate_engine_hooks()
    validate_game_hooks()
    validate_docs()
    print("arena_campaign: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
