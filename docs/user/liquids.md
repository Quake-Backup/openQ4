# Liquids

openQ4 has water, slime and lava that behave the way they do in the older Quake games: you wade,
you swim, you drown, and lava and slime hurt. Retail Quake 4 has none of this — everything that
looks like water in the stock maps is decoration with a `trigger_hurt` floating over it where it is
meant to be dangerous — so liquids are something you author, and this page is how.

## What a liquid does

| | water | slime | lava |
| --- | --- | --- | --- |
| wade and swim | yes | yes | yes |
| drown when submerged | yes | yes | yes |
| damage while in it | no | 10 per water level | 30 per water level |
| screen tint | blue | green | orange |
| muffled audio when submerged | yes | yes | yes |
| bots route through it | yes | never | never |

Water level is the Quake ladder: feet, waist, head. Damage scales with it, so standing ankle-deep
in lava is a third of the punishment of being under it — and being under it kills in about a second.

Movement matches Quake 3. Wading clamps your ground speed, swimming runs at half speed, and pushing
into a ledge while waist-deep does the water jump that pops you out of the pool. Falling damage is
halved at feet depth, quartered at waist depth, and gone entirely once your head is under.

## Building a liquid volume

Texture a brush with one of the shipped materials and that brush *is* the liquid:

```
textures/openq4/liquids/water         moving surface with heat-haze distortion
textures/openq4/liquids/water_calm    same volume, still surface, cheaper
textures/openq4/liquids/slime
textures/openq4/liquids/lava
```

Texture the whole brush, all six sides. The material clears the solid flag itself, so you do not
need to also mark it non-solid, and you must not add `solid` afterwards.

If you had been faking lava with a `trigger_hurt` over a decorative brush, delete the trigger when
you convert it. The two damage sources do not know about each other and you will take both.

## Writing your own liquid material

The keyword that makes a liquid is `water`, `lava` or `slime`. It sets the content flag and clears
the solid flag in one step. Then:

```
textures/mymap/greenwater
{
	qer_editorimage	textures/mymap/greenwater.tga

	water           // the liquid keyword: water, lava or slime
	liquid          // surface type, so footsteps and impacts sound wet
	translucent     // REQUIRED - see below
	twosided        // so the surface is visible from underneath

	materialType	water   // impact reaction: water, lava or slime

	{
		blend	diffusemap
		map		textures/mymap/greenwater.tga
		translate	time * 0.03 , time * 0.02
	}
}
```

**`translucent` is not optional, even for lava.** dmap seals any brush whose sides are all opaque,
and a sealed volume has no space inside it — the player can never get in, and none of the liquid
behaviour will ever run. Lava and slime look solid but must still be declared translucent or your
pool will be a solid block.

`materialType` picks which impact effect and sound a weapon uses when it hits the surface. `water`
ships with Quake 4; `lava` and `slime` are added by openQ4 in
`materials/types/liquids_openq4.mtt`.

## A liquid volume without a brush

If you just want a box of liquid — or you are adding one to a map you cannot recompile — there is an
entity for it:

```
func_liquid_openq4_water
func_liquid_openq4_slime
func_liquid_openq4_lava
```

Set `size` for the box, which sits on the entity origin, or give explicit `mins`/`maxs`. It works
from the console too, which is the quickest way to see any of this in a stock map:

```bash
spawn func_liquid_openq4_lava size "512 512 256"
```

## Tuning

| cvar | default | what it does |
| --- | --- | --- |
| `pm_waterAir` | 720 | frames underwater before drowning starts — 720 is Quake 3's twelve seconds |
| `g_drownDamageMax` | 15 | ceiling on the rising drown damage |
| `g_liquidDamageInterval` | 500 | milliseconds between lava and slime damage ticks |
| `g_liquidScreenTint` | 1 | strength of the underwater screen tint, `0` turns it off |
| `g_debugLiquid` | 0 | log water level changes, the air reservoir and every damage tick |
| `g_liquidTestVolume` | "" | dev aid: drop a `water`/`slime`/`lava` volume around the player on spawn |
| `g_liquidTestVolumeSize` | 640 | cube size of that test volume |

Drowning shares the air reservoir and HUD readout with Quake 4's vacuum areas. Underwater the
reservoir drains faster, so a full bar is twelve seconds of swimming but still the full `pm_air`
worth of vacuum.

## Retargeting the sounds and effects

Every liquid sound and splash is keyed off a single def, `liquid_openq4` in
`def/liquids_openq4.def`. Override that def in your mod to change the whole set at once. Any key you
leave out is simply silent, so a partial set is fine.

Keys are `<what>_<liquid>`: `snd_enter_water`, `snd_under_lava`, `fx_splash_slime`,
`fx_impact_water`, `fx_bubbles_water`, and so on, plus `snd_drown` and `snd_wade`.

A weapon that defines its own `fx_impact_water` keeps it — the shared def only fills in for things
that specify nothing.

## Monsters and NPCs

Monsters are in the same water you are. Lava and slime burn them at the same rate and with the same
water-level scaling, and anything with its head under drowns after twelve seconds. Breaking the
surface splashes and sounds for them exactly as it does for the player.

Quake 4's invulnerable story marines are unaffected, because they already ignore damage — nothing
special was needed for that.

Two def keys opt a monster out:

```
"canBreatheLiquid"  "1"   // never drowns: anything aquatic, or that does not breathe
"liquidImmune"      "1"   // lava and slime do nothing: anything already made of fire
```

## Bots

Bots swim, surface before they drown, and climb when something is burning them. They will not route
through lava or slime: those cells are refused when the navigation mesh is built, and a bot that is
somehow pushed toward one will refuse the step. Water is left in the graph, because it is crossable.
