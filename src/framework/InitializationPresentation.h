// Copyright (C) 2026 DarkMatter Productions. GPL-3.0-or-later.
#pragma once

#include <algorithm>

namespace openq4 {
// Cooperative startup presentation: no sleeping, catch-up frames or worker
// rendering. Charge completed frame time to the next interval so expensive
// presentation cannot consume initialization. Times are monotonic milliseconds.
class InitializationPresentation {
public:
	bool Due(double now, int presentationCap) const {
		const double interval = 1000.0 / (presentationCap > 0 ? (std::min)(30, presentationCap) : 30);
		return ready && now - completed >= (std::max)(interval - cost, cost * 9);
	}
	void Presented(double start, double end) {
		// The first frame includes one-time document/geometry creation. Its cost
		// must not freeze an already warm screen for the rest of startup.
		cost = ready ? (std::max)(0.0, end - start) : 0;
		completed = end;
		ready = true;
	}
private:
	bool ready = false;
	double completed = 0, cost = 0;
};
}
