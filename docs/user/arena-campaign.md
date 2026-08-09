# Arena Campaign

The Arena Campaign is a single-player ladder built from Quake 4's stock
multiplayer maps and openQ4's character bots. It sits beside the original story:
choose **Single Player**, then choose **Mission** for the Quake 4 campaign or
**Arena** for the new circuit.

No internet connection, public server, community map pack, or replacement game
content is required. Arena matches run as private local multiplayer games so
the bots can use the same scoring, team, and match rules as human players.

## How the ladder works

The circuit contains five tiers with four matches in each tier, for twenty
matches in total:

1. The first three matches in a tier are available together.
2. Winning all three unlocks that tier's boss match.
3. Winning the boss match unlocks the next tier.
4. Defeated matches remain available for another attempt.
5. A draw does not award a win or unlock progress; replay it to settle the match.
6. Defeating the Makron in the final boss duel completes the campaign.

Completed matches can be replayed without losing progress. After a result,
**Retry Match** immediately reruns a defeat, **Replay Match** repeats a victory
or draw, and **Next Match** moves to the next available challenge after a win.
The browser's progress bar, completion marks, boss accent, and tier briefing
keep the route to the next unlock visible while choosing a match. A locked boss
keeps its identity hidden until all three qualifiers are won.

Arena screens use the standard menu confirm and back controls on keyboard and
controller. The recommended action receives focus when a screen opens: next
match after a victory, retry after a defeat, replay after a draw, and cancel in
the destructive reset prompt. **Escape** or the controller back action dismisses
a result, cancels reset confirmation, or moves back one menu level.

## Match presentation

Arena has its own beginning-to-end match ceremony rather than exposing the
ordinary multiplayer lobby and score-summary flow:

1. **Challenge launch.** Confirming a match expands its challenge card over the
   selected map and then fades into the arena. Input is held during this short
   transition so one confirm press cannot accidentally perform two actions.
2. **Arena entrance.** As soon as the authored roster is present, the match
   begins a four-second cinematic countdown automatically. There is no Ready
   button or warmup vote. Combatants are locked in place while a
   collision-aware camera sweeps in toward the player, with the match name and
   rules framed over the world. Normal control returns on **Fight**.
3. **Final tableau.** The terminal score freezes every combatant where the
   match ended. For six seconds the camera makes a slow, collision-aware orbit
   around the unique victor while a restrained depth-of-field effect keeps that
   character sharp. The score and champion are staged over the live arena; a
   draw uses a neutral presentation and does not declare a victor.
4. **Match report.** A dramatic fade takes the arena down before a dedicated
   result ledger expands in the Arena browser. Outcome, final score, unlocks,
   and the recommended replay, retry, or next action appear in deliberate
   stages instead of all at once.
5. **Campaign update.** Leaving a newly won report reveals the campaign board
   beneath it, then visibly records the completed match, fills the progress
   segment, and pulses any new boss or tier gate. Control returns only after
   the update has settled, with the next useful selection ready.

The cinematic framing used by the entrance and the final tableau is sized from
the actual display. Its bars always reach every screen edge, and they thicken on
narrower or taller displays so the framed picture stays close to a 16:9
cinematic window instead of shrinking to a fixed slice of a 4:3 layout. On 16:9
and wider displays the frame keeps its usual thin profile, and on every aspect
it stops short of the match clock and the surrounding interface. The frame lifts
on **Fight**, exactly as control returns.

These ceremonies are exclusive to the single-player Arena Campaign. Private
matches opened from the multiplayer menus retain their normal ready-up,
scoreboard, and review behavior.

**Reset Progress** is unavailable until at least one match has been won. It
clears every Arena win and tier unlock, but asks for confirmation before
changing anything. It does not affect Mission saves, ordinary multiplayer
settings, or achievements.

Each tier raises the base bot skill from 1 to 5. Individual opponents may be
one step easier or harder than the tier baseline to preserve their
personalities and make a roster feel less uniform. The **Easier** and **Harder**
controls adjust the campaign challenge while keeping that relative spread,
with effective bot skill kept inside the normal 1-to-5 range. The match panel
shows the resulting bot skill or skill range, so the selected tier, campaign
difficulty, and opponent offsets are visible before the match starts.

## The circuit

| Tier | Match | Map | Game type | Featured roster |
| --- | --- | --- | --- | --- |
| Boot Camp | First Blood | The Fragging Yard (`mp/q4dm1`) | Duel | Bagby |
| Boot Camp | Crossfire | Sandstorm (`mp/q4dm2`) | Deathmatch | Anderson, Bagby, Tetzlaff |
| Boot Camp | Fireteam | The Lost Fleet (`mp/q4dm3`) | Team Deathmatch | Anderson, Bagby, Tetzlaff, Sorg, Strauss |
| Boot Camp | Transfer Test | Bloodwork (`mp/q4dm4`) | Duel | Sorg |
| The Ironworks | Thorns | The Rose (`mp/q4dm5`) | Deathmatch | Sorg, Strauss, Hollenbeck |
| The Ironworks | Shifting Lines | No Doctors (`mp/q4dm6`) | Red Rover | Sorg, Strauss, Hollenbeck, Marsh, Anderson |
| The Ironworks | Last Squad Standing | Over The Edge (`mp/q4dm7`) | Clan Arena | Sorg, Strauss, Hollenbeck, Marsh, Tetzlaff |
| The Ironworks | The Marshal | Railed (`mp/q4tourney1`) | Duel | Marsh |
| Blood Circuit | Longest Odds | The Longest Day (`mp/q4dm8`) | Deathmatch | Bidwell, Cortez, Morris, Rhodes |
| Blood Circuit | Campground Clash | Campgrounds Redux (`mp/q4dm9`) | Team Deathmatch | Bidwell, Cortez, Morris, Rhodes, Voss |
| Blood Circuit | Skeleton Crew | Skeleton Crew (`mp/q4dm11`) | Clan Arena | Bidwell, Cortez, Morris, Rhodes, Voss |
| Blood Circuit | Officer's Challenge | The Fragging Yard 1v1 (`mp/q4dm11v1`) | Duel | Voss |
| Strogg Trials | Central Pressure | Central Industrial (`mp/q4xdm10`) | Deathmatch | Gunner, Rhodes, Cortez, Voss |
| Strogg Trials | Red Shift | Stroyent Red (`mp/q4xdm13`) | Red Rover | Gunner, Rhodes, Cortez, Voss, Kane |
| Strogg Trials | Warforged | Warforged (`mp/q4xdm11`) | Team Deathmatch | Gunner, Kane, Sledge, Voss, Rhodes |
| Strogg Trials | Sledgehammer | Verticon (`mp/q4xtourney2`) | Duel | Sledge |
| Final Gauntlet | Last Alliance | Retrophobopolis (`mp/q4xdm14`) | Team Deathmatch | Kane, Sledge, Gunner, Morris, Voss |
| Final Gauntlet | Firewall | Firewall (`mp/q4xdm15`) | Clan Arena | Kane, Sledge, Gunner, Morris, Voss |
| Final Gauntlet | Champion's Circle | Skeleton Crew (`mp/q4dm11`) | Deathmatch | Kane, Sledge, Gunner, Morris, Voss |
| Final Gauntlet | The Makron | Stroggenomenon (`mp/q4xtourney1`) | Duel | Makron |

The team modes use five bots plus the player, giving the matchmaker enough
players for balanced three-versus-three sides. Red Rover moves players between
teams as frags are scored. Clan Arena gives each player one life for the round,
so positioning and survival matter as much as aim.

## Stock-content compatibility

Every campaign map comes from the official Quake 4 files required by openQ4.
The optional `q4cmp` community map pack is deliberately not part of the
progression. The campaign also avoids CTF, DeadZone, and other objective modes:
the current bots fight, navigate, collect items, and obey combat match rules,
but do not yet pursue flags or map objectives.

Bots build navigation from the loaded map's collision at runtime. The first
bot match on a map can therefore spend a brief moment preparing navigation;
there is no separate bot-map download or AAS file to install.

If the Arena screen reports **Missing Required Map**, verify the retail Quake 4
installation and openQ4 content paths using the
[Getting Started guide](getting-started.md). Do not install loose replacement
maps over the campaign maps.
