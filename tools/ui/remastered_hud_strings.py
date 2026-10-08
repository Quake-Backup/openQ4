#!/usr/bin/env python3
"""Language-table entries for the Remastered HUD (ui-visual-design.md 14).

The Remastered HUD documents and the game's Remastered fields use the
#str_2312xx block. Running this script adds missing entries to every
content/baseoq4/pak0/strings/*_openq4.lang table (UTF-8, CRLF) and updates
entries whose text changed; `--check` reports tables that are out of date.

Ordinals: #str_231201 is first place exactly, then 231202-231205 are the
places ending in 1, 2 and 3 (not 11-13) and every other place, each a format
with one %d.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STRINGS = ROOT / "content/baseoq4/pak0/strings"
LANGUAGES = ("english", "brazilian", "czech", "french", "german", "hungarian", "italian", "polish", "russian", "spanish",
             "turkish", "ukrainian")

# key: (english, brazilian, czech, french, german, hungarian, italian, polish, russian, spanish, turkish, ukrainian)
ENTRIES = {
    "#str_231200": ("TIED", "EMPATE", "REMÍZA", "ÉGALITÉ", "GLEICHSTAND", "DÖNTETLEN", "PARI", "REMIS", "НИЧЬЯ", "EMPATE",
                    "BERABERE", "НІЧИЯ"),
    "#str_231201": ("%dST", "%dº", "%d.", "%dER", "%d.", "%d.", "%dº", "%d.", "%d-Е", "%dº", "%d.", "%d-Е"),
    "#str_231202": ("%dST", "%dº", "%d.", "%dE", "%d.", "%d.", "%dº", "%d.", "%d-Е", "%dº", "%d.", "%d-Е"),
    "#str_231203": ("%dND", "%dº", "%d.", "%dE", "%d.", "%d.", "%dº", "%d.", "%d-Е", "%dº", "%d.", "%d-Е"),
    "#str_231204": ("%dRD", "%dº", "%d.", "%dE", "%d.", "%d.", "%dº", "%d.", "%d-Е", "%dº", "%d.", "%d-Е"),
    "#str_231205": ("%dTH", "%dº", "%d.", "%dE", "%d.", "%d.", "%dº", "%d.", "%d-Е", "%dº", "%d.", "%d-Е"),
    "#str_231206": ("WARM-UP", "AQUECIMENTO", "ROZCVIČKA", "ÉCHAUFFEMENT", "AUFWÄRMEN", "BEMELEGÍTÉS", "RISCALDAMENTO",
                    "ROZGRZEWKA", "РАЗМИНКА", "CALENTAMIENTO", "ISINMA", "РОЗМИНКА"),
    "#str_231207": ("STARTS IN", "COMEÇA EM", "ZAČÁTEK ZA", "DÉBUT DANS", "START IN", "KEZDÉS", "INIZIO TRA", "START ZA",
                    "СТАРТ ЧЕРЕЗ", "EMPIEZA EN", "BAŞLAMAYA", "СТАРТ ЧЕРЕЗ"),
    "#str_231208": ("SUDDEN DEATH", "MORTE SÚBITA", "NÁHLÁ SMRT", "MORT SUBITE", "SUDDEN DEATH", "HIRTELEN HALÁL",
                    "MORTE IMPROVVISA", "NAGŁA ŚMIERĆ", "ВНЕЗАПНАЯ СМЕРТЬ", "MUERTE SÚBITA", "ANİ ÖLÜM", "РАПТОВА СМЕРТЬ"),
    "#str_231209": ("VOTE", "VOTAÇÃO", "HLASOVÁNÍ", "VOTE", "ABSTIMMUNG", "SZAVAZÁS", "VOTAZIONE", "GŁOSOWANIE", "ГОЛОСОВАНИЕ",
                    "VOTACIÓN", "OYLAMA", "ГОЛОСУВАННЯ"),
    "#str_231210": ("YES", "SIM", "ANO", "OUI", "JA", "IGEN", "SÌ", "TAK", "ДА", "SÍ", "EVET", "ТАК"),
    "#str_231211": ("NO", "NÃO", "NE", "NON", "NEIN", "NEM", "NO", "NIE", "НЕТ", "NO", "HAYIR", "НІ"),
    "#str_231212": ("FOLLOWING", "SEGUINDO", "SLEDUJETE", "SUIVI", "FOLGEN", "KÖVETÉS", "SEGUI", "OBSERWUJESZ", "НАБЛЮДЕНИЕ",
                    "SIGUIENDO", "TAKİP", "СПОСТЕРЕЖЕННЯ"),
    "#str_231213": ("SPECTATING", "ESPECTADOR", "DIVÁK", "SPECTATEUR", "ZUSCHAUER", "NÉZŐ", "SPETTATORE", "WIDZ", "ЗРИТЕЛЬ",
                    "ESPECTADOR", "İZLEYİCİ", "ГЛЯДАЧ"),
    "#str_231214": ("Free camera", "Câmera livre", "Volná kamera", "Caméra libre", "Freie Kamera", "Szabad kamera",
                    "Telecamera libera", "Wolna kamera", "Свободная камера", "Cámara libre", "Serbest kamera", "Вільна камера"),
    "#str_231215": ("Next", "Próximo", "Další", "Suivant", "Weiter", "Következő", "Successivo", "Następny", "Далее",
                    "Siguiente", "Sonraki", "Далі"),
    "#str_231216": ("Follow a player", "Seguir um jogador", "Sledovat hráče", "Suivre un joueur", "Spieler folgen",
                    "Játékos követése", "Segui un giocatore", "Obserwuj gracza", "Следить за игроком", "Seguir a un jugador",
                    "Bir oyuncuyu izle", "Стежити за гравцем"),
    "#str_231217": ("Scores", "Placar", "Skóre", "Scores", "Punkte", "Eredmények", "Punteggi", "Wyniki", "Счёт", "Puntuaciones",
                    "Skorlar", "Рахунок"),
    "#str_231218": ("ZONE", "ZONA", "ZÓNA", "ZONE", "ZONE", "ZÓNA", "ZONA", "STREFA", "ЗОНА", "ZONA", "BÖLGE", "ЗОНА"),
    "#str_231219": ("CONTESTED", "DISPUTADA", "SPORNÁ", "DISPUTÉE", "UMKÄMPFT", "VITATOTT", "CONTESA", "SPORNA", "ОСПАРИВАЕТСЯ",
                    "EN DISPUTA", "ÇEKİŞMELİ", "ОСКАРЖУЄТЬСЯ"),
    "#str_231220": ("EMPTY", "VAZIA", "PRÁZDNÁ", "VIDE", "LEER", "ÜRES", "VUOTA", "PUSTA", "ПУСТО", "VACÍA", "BOŞ", "ПОРОЖНЬО"),
    "#str_231221": ("YOU", "VOCÊ", "VY", "VOUS", "DU", "TE", "TU", "TY", "ВЫ", "TÚ", "SEN", "ВИ"),
    # Symbols the HUD composes around numbers and names; the same in every language.
    "#str_231222": ("/",) * 12,
    "#str_231223": ("+",) * 12,
    "#str_231224": ("−",) * 12,
    "#str_231225": ("!",) * 12,
    "#str_231226": ("?",) * 12,
    "#str_231227": (": ",) * 12,
    "#str_231228": ("ms",) * 12,
    # The vehicle HUD prompt (14.8): leaving a vehicle or gun, and a locked one.
    "#str_231229": ("Exit", "Sair", "Opustit", "Sortir", "Verlassen", "Kilépés", "Esci", "Wyjdź", "Выйти", "Salir", "Çık",
                    "Вийти"),
    "#str_231230": ("Vehicle locked", "Veículo travado", "Vozidlo zamčeno", "Véhicule verrouillé", "Fahrzeug gesperrt",
                    "Jármű zárolva", "Veicolo bloccato", "Pojazd zablokowany", "Транспорт заблокирован", "Vehículo bloqueado",
                    "Araç kilitli", "Транспорт заблоковано"),
    # The scoreboard (14.14): the bot tag and your place, an ordinal (231201-231205) of the players.
    "#str_231231": ("BOT", "BOT", "BOT", "BOT", "BOT", "BOT", "BOT", "BOT", "БОТ", "BOT", "BOT", "БОТ"),
    "#str_231232": ("%s OF %d", "%s DE %d", "%s Z %d", "%s SUR %d", "%s VON %d", "%s / %d", "%s SU %d", "%s Z %d", "%s ИЗ %d",
                    "%s DE %d", "%s / %d", "%s З %d"),
    # The multiplayer respawn prompt (14.9): the attack control's action, and the hold's countdown label.
    "#str_231233": ("Respawn", "Renascer", "Oživit", "Réapparaître", "Wiedereinstieg", "Újraéledés", "Rinasci", "Odrodzenie",
                    "Возродиться", "Reaparecer", "Yeniden doğ", "Відродитися"),
    "#str_231234": ("RESPAWN IN", "RENASCE EM", "OŽIVENÍ ZA", "RÉAPPARITION DANS", "WIEDEREINSTIEG IN", "ÚJRAÉLEDÉSIG",
                    "RINASCITA TRA", "ODRODZENIE ZA", "ВОЗРОЖДЕНИЕ ЧЕРЕЗ", "REAPARICIÓN EN", "YENİDEN DOĞMAYA",
                    "ВІДРОДЖЕННЯ ЧЕРЕЗ"),
}

LINE = re.compile(r'^\s*"(#str_\d+)"\s*"(.*)"\s*$')


def update(path: Path, language: str, check: bool) -> bool:
    raw = path.read_bytes().decode("utf-8")
    lines = raw.replace("\r\n", "\n").split("\n")
    index = LANGUAGES.index(language)
    present = {}
    for i, line in enumerate(lines):
        match = LINE.match(line)
        if match:
            present[match.group(1)] = i
    changed = False
    additions = []
    for key, values in ENTRIES.items():
        text = values[index].replace('"', '\\"')
        entry = f'\t"{key}"\t"{text}"'
        if key in present:
            if lines[present[key]] != entry:
                lines[present[key]] = entry
                changed = True
        else:
            additions.append(entry)
    if additions:
        close = max(i for i, line in enumerate(lines) if line.strip() == "}")
        lines[close:close] = additions
        changed = True
    if changed and not check:
        path.write_bytes("\r\n".join(lines).encode("utf-8"))
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    stale = [language for language in LANGUAGES if update(STRINGS / f"{language}_openq4.lang", language, args.check)]
    if args.check and stale:
        print("stale language tables:", ", ".join(stale))
        return 1
    print("updated:" if stale else "current", ", ".join(stale))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
