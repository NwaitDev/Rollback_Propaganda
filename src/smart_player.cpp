#include "../include/game_concepts.hpp"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>

// The smart player follows simple, readable heuristics. Every choice that
// changes the popularities is made the same way: it projects the outcome of
// each option and keeps the one its contract rates best (see contract::rate).

smart_player::smart_player(stealth greed) : player(greed, SMART){}

namespace {

// The grimory cards from the most to the least useful (ethers apart).
const std::vector<std::string> spell_ranking = {
	"EUPHORIA", "NEURASTHENIA", "IMMUNITY", "ETHER_LEAK", "ENTROPY_REVERSAL",
	"CHAPTER_SKIP", "PRECIPITATION", "READING", "MEMORY_LAPSE",
};

size_t spell_rank(const std::string& name){
	auto it = std::find(spell_ranking.begin(), spell_ranking.end(), name);
	return static_cast<size_t>(it - spell_ranking.begin()); // unknown: last
}

// Index of the most useful card among `names`: an ether while the player
// has less than 2 ethers in reserve, otherwise the best ranked spell (an
// ether only when there is nothing else).
size_t most_useful(const std::vector<std::string>& names, unsigned available_ether){
	if (available_ether < 2) {
		for (size_t i = 0; i < names.size(); ++i) {
			if (names[i] == "ETHER") return i;
		}
	}
	size_t best = 0;
	for (size_t i = 1; i < names.size(); ++i) {
		bool best_is_ether = (names[best] == "ETHER");
		bool i_is_ether = (names[i] == "ETHER");
		if (best_is_ether && !i_is_ether) {
			best = i;
		} else if (best_is_ether == i_is_ether && spell_rank(names[i]) < spell_rank(names[best])) {
			best = i;
		}
	}
	return best;
}

// --- projections ---------------------------------------------------------------

// The ways a propaganda card can apply: one list of simple propagandas per
// combination of the branches of its divergences (OR).
std::vector<std::vector<const event*>> expand(const event& e){
	if (e.get_kind() == PROPAGANDA) {
		return {{&e}};
	}
	if (e.get_kind() != OPERATOR) {
		return {{}};
	}
	std::vector<std::vector<const event*>> left = expand(e.get_left());
	std::vector<std::vector<const event*>> right = expand(e.get_right());
	if (e.get_operator() == DIVERGENCE) {
		left.insert(left.end(), right.begin(), right.end());
		return left;
	}
	std::vector<std::vector<const event*>> both;
	for (const auto& l : left) {
		for (const auto& r : right) {
			std::vector<const event*> combination = l;
			combination.insert(combination.end(), r.begin(), r.end());
			both.push_back(combination);
		}
	}
	return both;
}

// Faction among `allowed` where `value` rates best for the contract.
faction best_faction(const contract& c, const std::vector<int>& popularity, int value,
                     const std::vector<faction>& allowed){
	faction best = allowed.front();
	long best_rate = 0;
	for (size_t i = 0; i < allowed.size(); ++i) {
		std::vector<int> after = popularity;
		after[allowed[i]] += value;
		long r = c.rate(after);
		if (i == 0 || r > best_rate) {
			best = allowed[i];
			best_rate = r;
		}
	}
	return best;
}

// Applies simple propagandas like game::apply_propagandas, placing the ANY
// where they rate best. `extra` is added to every value (a modification
// being considered).
std::vector<int> apply(const contract& c, std::vector<int> popularity,
                       const std::vector<const event*>& propagandas, int extra){
	std::vector<faction> targeted;
	std::vector<int> anys;
	for (const event* p : propagandas) {
		int value = p->get_value() + extra;
		if (p->get_faction() == ANY) {
			anys.push_back(value);
		} else if (p->get_faction() != NONE) {
			popularity[p->get_faction()] += value;
			targeted.push_back(p->get_faction());
		}
	}
	std::stable_sort(anys.begin(), anys.end(), [](int a, int b) { return std::abs(a) > std::abs(b); });
	for (int value : anys) {
		std::vector<faction> allowed;
		for (faction f : {RED, YELLOW, BLUE}) {
			if (std::find(targeted.begin(), targeted.end(), f) == targeted.end()) allowed.push_back(f);
		}
		if (allowed.empty()) allowed = {RED, YELLOW, BLUE};
		faction f = best_faction(c, popularity, value, allowed);
		popularity[f] += value;
		targeted.push_back(f);
	}
	return popularity;
}

// Popularity after the event, with the choices (OR branch, ANY factions)
// that rate best for the contract.
std::vector<int> best_application(const contract& c, const std::vector<int>& popularity,
                                  const event& e, int extra){
	std::vector<int> best = popularity;
	bool first = true;
	for (const auto& option : expand(e)) {
		std::vector<int> after = apply(c, popularity, option, extra);
		if (first || c.rate(after) > c.rate(best)) {
			best = after;
			first = false;
		}
	}
	return best;
}

// What the resolution of the present would bring if `target` received
// `modification`: the points the contract would earn at the elections of the
// present, and the popularities at the end. Approximation: every choice
// (OR branch, ANY faction) is assumed to be made in favour of the contract,
// although the mage whose turn it is makes them.
struct projection {
	int earned = 0;
	std::vector<int> popularity;
};

projection project_present(game& g, const contract& c, const event* target, int modification){
	projection p;
	p.popularity = g.get_scores_ref();
	for (const event& e : g.present_events.show_events()) {
		if (e.get_kind() == ELECTION) {
			p.earned += c.points_earned(p.popularity);
		} else if (e.is_propaganda_card()) {
			p.popularity = best_application(c, p.popularity, e, (&e == target) ? modification : 0);
		}
	}
	return p;
}

// Full value of a projection: the points earned first, then the rating.
long value(const contract& c, const projection& p){
	return p.earned * 2000000L + c.rate(p.popularity);
}

// Coarse value: what the projection secures (points earned, contract
// satisfied at the end), ignoring the margins.
long coarse_value(const contract& c, const projection& p){
	return p.earned * 2L + ((c.points_earned(p.popularity) > 0) ? 1 : 0);
}

}


// --- reveal phase --------------------------------------------------------------

// Reveals like a greedy player, but stops before a card it knows (seen with
// Reading) would be an explosion or push the stealth over the limit.
bool smart_player::wants_to_reveal_more(game& g){
	stealth current = g.present_events.get_stealth();
	if (current >= get_greed()) {
		return false;
	}
	const event* next = known_event(g.future_events.top().get_id());
	if (next != nullptr) {
		if (next->get_kind() == EXPLOSION) return false;
		if (current + next->get_stealth() > g.get_max_stealth()) return false;
	}
	return true;
}


// --- shenanigans -----------------------------------------------------------------

size_t smart_player::choose_card_to_scheme(game& g){
	std::vector<std::string> names;
	for (size_t i = 0; i < cards_in_hand.get_nb_cards(); ++i) {
		names.push_back(cards_in_hand.card_at(i).name());
	}
	return most_useful(names, available_ether());
}

// ethers first (so that later spells can spend them), then immunity (to
// protect the rest of the turn), then the other cards
size_t smart_player::choose_next_shenanigan(game& g){
	for (const char* wanted : {"ETHER", "IMMUNITY"}) {
		for (size_t i = 0; i < cards_in_play.nb_prepared(); ++i) {
			if (cards_in_play.prepared_at(i).name() == wanted) {
				return i;
			}
		}
	}
	return 0;
}

// Reveals a card only if it is useful right now; otherwise it goes back
// under the grimory, to be drawn again later.
bool smart_player::wants_to_reveal(game& g, const shenanigan& card){
	const std::string name = card.name();
	if (name == "EUPHORIA" || name == "NEURASTHENIA") {
		// useful if the weakest version of the spell improves the outcome
		const contract& c = get_contract();
		int modification = (name == "EUPHORIA") ? 1 : -1;
		event* target = choose_propaganda(g, modification);
		return target != nullptr
			&& value(c, project_present(g, c, target, modification))
			   > value(c, project_present(g, c, nullptr, 0));
	}
	if (name == "IMMUNITY") {
		return &g.get_current_player() == this;
	}
	if (name == "ETHER_LEAK") {
		return !opponents_with_ether(g).empty();
	}
	return true; // ETHER, MEMORY_LAPSE and the other spells
}

// Euphoria / Neurasthenia: the least ether that secures the best outcome
// (points earned at the elections of the present, contract satisfied
// afterwards); none if spending ether secures nothing more.
// Ether leak: enough to leak every ether of the opponents.
// Other spells: none.
unsigned smart_player::choose_ether_to_spend(game& g, const spell& s){
	const std::string name = s.name();
	if (name == "EUPHORIA" || name == "NEURASTHENIA") {
		const contract& c = get_contract();
		int sign = (name == "EUPHORIA") ? 1 : -1;
		unsigned best_ether = 0;
		long best = 0;
		for (unsigned ether = 0; ether <= available_ether(); ++ether) {
			int modification = sign * (1 + static_cast<int>(ether));
			event* target = choose_propaganda(g, modification);
			if (target == nullptr) {
				return 0;
			}
			long secured = coarse_value(c, project_present(g, c, target, modification));
			if (ether == 0 || secured > best) {
				best = secured;
				best_ether = ether;
			}
		}
		return best_ether;
	}
	if (name == "ETHER_LEAK") {
		unsigned opponents_ether = 0;
		for (player* p : opponents_with_ether(g)) {
			opponents_ether += p->available_ether();
		}
		if (opponents_ether == 0) {
			return 0;
		}
		return std::min(available_ether(), opponents_ether - 1);
	}
	return 0;
}

// The propaganda of the present whose modification rates best.
event* smart_player::choose_propaganda(game& g, int modification){
	const contract& c = get_contract();
	event* best = nullptr;
	long best_value = 0;
	for (event* candidate : present_propaganda(g)) {
		long v = value(c, project_present(g, c, candidate, modification));
		if (best == nullptr || v > best_value) {
			best = candidate;
			best_value = v;
		}
	}
	return best;
}

// Memory lapse: a dead card in a hand, given to the mage with the most points
// (random among the best)
player* smart_player::choose_opponent(game& g){
	std::vector<player*> best;
	for (player* p : opponents(g)) {
		if (best.empty() || p->get_score() > best.front()->get_score()) {
			best.assign(1, p);
		} else if (p->get_score() == best.front()->get_score()) {
			best.push_back(p);
		}
	}
	if (best.empty()) {
		return nullptr;
	}
	return best[g.random_below(best.size())];
}

// Entropy reversal: takes back the most useful card of the cosmos
size_t smart_player::choose_cosmos_card(game& g){
	std::vector<std::string> names;
	for (size_t i = 0; i < g.cosmos_cards.size(); ++i) {
		names.push_back(g.cosmos_cards.card_at(i).name());
	}
	return most_useful(names, available_ether());
}

// leaks the opponent with the most ether, as long as one has some
player* smart_player::choose_ether_to_leak(game& g){
	player* target = nullptr;
	for (player* p : opponents_with_ether(g)) {
		if (target == nullptr || p->available_ether() > target->available_ether()) {
			target = p;
		}
	}
	return target;
}


// --- resolution of the events ----------------------------------------------------

// The faction where the ANY rates best for the contract.
faction smart_player::choose_faction_for_any(game& g, int value,
                                             const std::vector<faction>& allowed){
	return best_faction(get_contract(), g.get_scores_ref(), value, allowed);
}

// The branch whose best application rates best for the contract (the left
// one on a tie).
size_t smart_player::choose_divergence_branch(game& g, const event& divergence){
	const contract& c = get_contract();
	const std::vector<int>& now = g.get_scores_ref();
	long left = c.rate(best_application(c, now, divergence.get_left(), 0));
	long right = c.rate(best_application(c, now, divergence.get_right(), 0));
	return (right > left) ? 1 : 0;
}
