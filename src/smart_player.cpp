#include "../include/game_concepts.hpp"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>

// The smart player follows simple, readable heuristics. Its goal is that the
// faction of its contract becomes (and stays) the most popular one.

smart_player::smart_player(stealth greed) : player(greed, SMART){}

namespace {

// the two opponents' factions, the most popular first (random on a tie)
std::pair<faction, faction> leader_and_trailer(game& g, faction mine){
	faction other1 = (faction)((mine+1)%3);
	faction other2 = (faction)((mine+2)%3);
	std::vector<int>& scores = g.get_scores_ref();
	bool other1_leads = (scores[other1] == scores[other2])
		? (g.random_below(2) == 0)
		: (scores[other1] > scores[other2]);
	return other1_leads ? std::make_pair(other1, other2) : std::make_pair(other2, other1);
}

// How much the player wants the factions printed on a propaganda to become
// popular: +2 its own faction, +1 an ANY (it may place it), -1 the leading
// opponent, 0 the trailing one. A multiple propaganda adds up its branches.
int propaganda_interest(const event& e, faction mine, faction leader){
	if (e.get_kind() == OPERATOR) {
		return propaganda_interest(e.get_left(), mine, leader)
		     + propaganda_interest(e.get_right(), mine, leader);
	}
	faction f = e.get_faction();
	if (f == mine) return 2;
	if (f == ANY) return 1;
	if (f == leader) return -1;
	return 0;
}

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

// Popularity the factions would have once the present is resolved, if
// `target` received `modification`. Approximation: the ANY propagandas and
// the divergences (OR) are left out, since their outcome depends on choices.
std::vector<int> projected_scores(game& g, const event* target, int modification){
	std::vector<int> scores = g.get_scores_ref();
	std::function<void(const event&, bool)> add = [&](const event& e, bool modified) {
		if (e.get_kind() == PROPAGANDA) {
			faction f = e.get_faction();
			if (f != ANY && f != NONE) {
				scores[f] += e.get_value() + (modified ? modification : 0);
			}
		} else if (e.get_kind() == OPERATOR && e.get_operator() == CONVERGENCE) {
			add(e.get_left(), modified);
			add(e.get_right(), modified);
		}
	};
	for (const event& e : g.present_events.show_events()) {
		add(e, &e == target);
	}
	return scores;
}

bool strictly_leads(const std::vector<int>& scores, faction f){
	for (faction other : {RED, YELLOW, BLUE}) {
		if (other != f && scores[other] >= scores[f]) return false;
	}
	return true;
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
		faction mine = get_contract();
		faction leader = leader_and_trailer(g, mine).first;
		for (event* e : present_propaganda(g)) {
			int interest = propaganda_interest(*e, mine, leader);
			if ((name == "EUPHORIA") ? (interest > 0) : (interest < 0)) {
				return true;
			}
		}
		return false;
	}
	if (name == "IMMUNITY") {
		return &g.get_current_player() == this;
	}
	if (name == "ETHER_LEAK") {
		return !opponents_with_ether(g).empty();
	}
	return true; // ETHER, MEMORY_LAPSE and the other spells
}

// Euphoria / Neurasthenia: the least ether that makes its faction strictly
// lead once the present is resolved (none if it leads anyway or cannot).
// Ether leak: enough to leak every ether of the opponents.
// Other spells: none.
unsigned smart_player::choose_ether_to_spend(game& g, const spell& s){
	const std::string name = s.name();
	if (name == "EUPHORIA" || name == "NEURASTHENIA") {
		int sign = (name == "EUPHORIA") ? 1 : -1;
		event* target = choose_propaganda(g, sign);
		if (target == nullptr) {
			return 0;
		}
		faction mine = get_contract();
		for (unsigned ether = 0; ether <= available_ether(); ++ether) {
			int modification = sign * (1 + static_cast<int>(ether));
			if (strictly_leads(projected_scores(g, target, modification), mine)) {
				return ether;
			}
		}
		return 0;
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

// Boosts the propaganda that helps its own faction the most, weakens the one
// that helps the leading opponent the most.
event* smart_player::choose_propaganda(game& g, int modification){
	std::vector<event*> candidates = present_propaganda(g);
	if (candidates.empty()) {
		return nullptr;
	}
	faction mine = get_contract();
	faction leader = leader_and_trailer(g, mine).first;
	event* best = candidates[0];
	for (event* candidate : candidates) {
		int candidate_interest = propaganda_interest(*candidate, mine, leader);
		int best_interest = propaganda_interest(*best, mine, leader);
		if ((modification > 0) ? (candidate_interest > best_interest)
		                       : (candidate_interest < best_interest)) {
			best = candidate;
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

// Gives a positive ANY to its own faction, then to the trailing opponent;
// a negative one to the leading opponent, then to the trailing one.
faction smart_player::choose_faction_for_any(game& g, int value,
                                             const std::vector<faction>& allowed){
	faction mine = get_contract();
	auto [leader, trailer] = leader_and_trailer(g, mine);
	std::vector<faction> preferences = (value >= 0)
		? std::vector<faction>{mine, trailer, leader}
		: std::vector<faction>{leader, trailer, mine};
	for (faction f : preferences) {
		if (std::find(allowed.begin(), allowed.end(), f) != allowed.end()) {
			return f;
		}
	}
	return allowed.front();
}

// Picks the branch with the best expected gain: a point of popularity is
// worth +2 for its own faction, -2 for the leading opponent, -1 for the
// trailing one, and an ANY is worth +2 per point (it will be placed well).
size_t smart_player::choose_divergence_branch(game& g, const event& divergence){
	faction mine = get_contract();
	faction leader = leader_and_trailer(g, mine).first;
	std::function<int(const event&)> gain = [&](const event& e) {
		if (e.get_kind() == OPERATOR) {
			int left = gain(e.get_left());
			int right = gain(e.get_right());
			return (e.get_operator() == CONVERGENCE) ? left + right : std::max(left, right);
		}
		if (e.get_kind() != PROPAGANDA) {
			return 0;
		}
		int value = e.get_value();
		faction f = e.get_faction();
		if (f == ANY) return 2 * std::abs(value);
		if (f == mine) return 2 * value;
		if (f == leader) return -2 * value;
		if (f == NONE) return 0;
		return -value;
	};
	return (gain(divergence.get_right()) > gain(divergence.get_left())) ? 1 : 0;
}
