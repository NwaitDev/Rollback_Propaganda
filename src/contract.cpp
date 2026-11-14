#include "../include/game_concepts.hpp"
#include <algorithm>
#include <climits>

int rank_of(faction f, const std::vector<int>& popularity){
	int above = 0;
	for (faction other : {RED, YELLOW, BLUE}) {
		if (other == f) continue;
		if (popularity[other] == popularity[f]) return 0;   // tied: no rank
		if (popularity[other] > popularity[f]) ++above;
	}
	return above + 1;
}

int contract::points_earned(const std::vector<int>& popularity) const {
	for (faction f : factions) {
		if (rank_of(f, popularity) == rank) {
			return points;
		}
	}
	return 0;
}

namespace {

// Margin of faction f for the rank: f must stay strictly below the factions
// ranked above and strictly above the others. Positive: f holds the rank,
// and the margin is how much popularity it would take to lose it. Zero or
// negative: f does not hold it, and the margin tells how far it is.
long margin(faction f, int rank, const std::vector<int>& popularity){
	std::vector<int> others;
	for (faction other : {RED, YELLOW, BLUE}) {
		if (other != f) others.push_back(popularity[other]);
	}
	std::sort(others.rbegin(), others.rend());   // most popular first
	long above = (rank >= 2) ? others[rank - 2] : LONG_MAX;   // the faction just above
	long below = (rank - 1 < (int)others.size()) ? others[rank - 1] : LONG_MIN;
	long room_above = (above == LONG_MAX) ? LONG_MAX : above - popularity[f];
	long room_below = (below == LONG_MIN) ? LONG_MAX : popularity[f] - below;
	return std::min(room_above, room_below);
}

}

long contract::rate(const std::vector<int>& popularity) const {
	long best = LONG_MIN;
	for (faction f : factions) {
		best = std::max(best, margin(f, rank, popularity));
	}
	// keeps "satisfied" above "unsatisfied" whatever the margins
	const long satisfied_bonus = 1000000;
	return (points_earned(popularity) > 0) ? satisfied_bonus + best : std::min(best, 0L);
}
