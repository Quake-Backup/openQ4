// Copyright (C) 2026 DarkMatter Productions. GPL-3.0-or-later.
#pragma once
#include <array>
#include <string>
#include <vector>

namespace openq4::ui {
// An evaluated HUD draw, in the engine's 640x480 submission space. The quad
// already includes GUI anchoring, clipping and transforms. UVs retain the
// original unclipped artwork extent, including mirrored and tiled artwork.
struct HudPrimitive {
	std::string material, artwork;
	std::array<float,8> quad{};
	std::array<float,4> uv{0,0,1,1}, colour{1,1,1,1};
	bool glyph = false;
};

// Compare the displayed values, rather than the inventory pickup pulse:
// reloading increases the clip, while a pickup can affect only the reserve.
// Joining, changing weapon, or entering/leaving unlimited ammo never pops.
struct HudAmmoReadout {
	std::string weapon;
	int clip = -1, total = -1;
	bool clipped = false;
};
enum HudAmmoGain { HudAmmoGainNone = 0, HudAmmoGainClip = 1, HudAmmoGainReserve = 2, HudAmmoGainTotal = 4 };
inline int HudAmmoGains(const HudAmmoReadout& before, const HudAmmoReadout& after) {
	if (before.weapon.empty() || before.weapon != after.weapon || before.clipped != after.clipped ||
		before.total < 0 || after.total < 0) return HudAmmoGainNone;
	int gains = HudAmmoGainNone;
	if (after.clipped && before.clip >= 0 && after.clip > before.clip) gains |= HudAmmoGainClip;
	if (after.total > before.total) gains |= after.clipped ? HudAmmoGainReserve : HudAmmoGainTotal;
	return gains;
}
}
