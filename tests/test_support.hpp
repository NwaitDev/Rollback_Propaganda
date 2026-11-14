// Shared helpers of the C++ tests (see tests/README.md).
//
// The tests look inside the classes (a game's piles, a player's hand...), so
// this header opens their private and protected parts. The standard headers
// are included first, so that only the project's classes are opened.
#ifndef __TEST_SUPPORT__
#define __TEST_SUPPORT__

#include <algorithm>
#include <cstddef>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <doctest/doctest.h>

// a failed CHECK on vectors prints their content instead of {?}
namespace doctest {
template <class T>
struct StringMaker<std::vector<T>> {
	static String convert(const std::vector<T>& values) {
		std::ostringstream out;
		out << "{";
		for (size_t i = 0; i < values.size(); ++i) out << (i ? ", " : "") << values[i];
		out << "}";
		return out.str().c_str();
	}
};
}
#include <nlohmann/json.hpp>

#define private public
#define protected public
#include "game_concepts.hpp"
#include "campaign.hpp"
#undef private
#undef protected

namespace test {

// silences std::cout while it exists (a game prints every step)
struct quiet {
	std::streambuf* saved;
	quiet() : saved(std::cout.rdbuf(nullptr)) {}
	~quiet() { std::cout.rdbuf(saved); std::cout.clear(); }
};

inline contract make_contract(std::vector<faction> factions, int rank = 1, int points = 1) {
	contract c;
	c.factions = std::move(factions);
	c.rank = rank;
	c.points = points;
	for (faction f : c.factions) {
		c.name += (c.name.empty() ? "" : "+") + std::string(f == RED ? "RED" : f == YELLOW ? "YELLOW" : "BLUE");
	}
	return c;
}

// a game with 3 players of type P, contracts RED, YELLOW and BLUE (1 point in
// the lead) in seat order, and these popularities
template <class P>
game make_game(unsigned seed = 1, std::vector<int> popularity = {0, 0, 0}, stealth greed = 10) {
	game g(21, seed);
	for (int i = 0; i < 3; ++i) {
		g.add_player(std::make_unique<P>(greed));
	}
	g.players[0]->sign_contract(make_contract({RED}));
	g.players[1]->sign_contract(make_contract({YELLOW}));
	g.players[2]->sign_contract(make_contract({BLUE}));
	g.scores = popularity;
	return g;
}

// puts the event on top of the future, then reveals it into the present
inline void reveal(game& g, const event& e) {
	g.add_event(e);
	g.present_events.append_from_future(g.future_events);
}

inline void hand_of(player& p, std::vector<const char*> names) {
	p.cards_in_hand.cards.clear();
	for (const char* name : names) {
		p.receive_card(make_card(name));
	}
}

inline void give_ether(player& p, int count) {
	for (int i = 0; i < count; ++i) {
		p.store_ether(make_card("ETHER"));
	}
}

// change of every faction's popularity when `controller` resolves the event
inline std::vector<int> delta(game& g, const event& e, size_t controller, std::vector<int> before) {
	g.scores = before;
	g.resolve_event(e, *g.players[controller]);
	std::vector<int> change(3);
	for (int i = 0; i < 3; ++i) {
		change[i] = g.scores[i] - before[i];
	}
	return change;
}

// every grimory card of the game, wherever it is
inline size_t grimory_cards(game& g) {
	size_t total = g.cosmos_cards.size();
	for (auto& p : g.players) {
		total += p->deck.size() + p->cards_in_hand.get_nb_cards()
		       + p->cards_in_play.available_ether() + p->cards_in_play.nb_prepared();
	}
	return total;
}

// a player whose every choice is scripted by the test
class scripted : public player {
public:
	explicit scripted(stealth greed) : player(greed, RANDOM) {}
	bool reveal_cards = true;          // reveal the schemed cards
	player* opponent = nullptr;        // target of Memory lapse and Ether leak
	bool wants_to_reveal_more(game& g) override { return g.present_events.get_stealth() < get_greed(); }
	size_t choose_card_to_scheme(game&) override { return 0; }
	size_t choose_next_shenanigan(game&) override { return 0; }
	bool wants_to_reveal(game&, const shenanigan&) override { return reveal_cards; }
	unsigned choose_ether_to_spend(game&, const spell&) override { return 0; }
	event* choose_propaganda(game& g, int) override {
		auto candidates = present_propaganda(g);
		return candidates.empty() ? nullptr : candidates[0];
	}
	player* choose_opponent(game&) override { return opponent; }
	size_t choose_cosmos_card(game&) override { return 0; }
	player* choose_ether_to_leak(game&) override {
		return (opponent && opponent->available_ether() > 0) ? opponent : nullptr;
	}
	size_t choose_divergence_branch(game&, const event&) override { return 0; }
	faction choose_faction_for_any(game&, int, const std::vector<faction>& allowed) override {
		return allowed.front();
	}
};

// a JSON file written for a test, removed afterwards
struct temporary_file {
	std::string path;
	explicit temporary_file(const std::string& content, const std::string& name = "test") {
		path = "build/check/tmp_" + name + "_" + std::to_string(std::random_device{}()) + ".json";
		std::ofstream(path) << content;
	}
	~temporary_file() { std::remove(path.c_str()); }
};

// the message of the exception thrown by f, "" if none
inline std::string error_of(const std::function<void()>& f) {
	try {
		f();
	} catch (const std::exception& e) {
		return e.what();
	}
	return "";
}

}

#endif
