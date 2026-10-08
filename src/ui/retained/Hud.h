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
}
