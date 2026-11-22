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

// a controller whose every choice is scripted by the test
class scripted : public controller {
public:
	explicit scripted(stealth greed) : greed(greed) {}
	stealth greed;
	bool reveal_cards = true;          // reveal the schemed cards
	size_t opponent = 0;               // seat targeted by Memory lapse and Ether leak
	std::string strategy_name() const override { return "SCRIPTED"; }
	stealth get_greed() const override { return greed; }
	bool reveal_more(const decision& d) override { return d.g.present_events.get_stealth() < greed; }
	size_t scheme_card(const decision&) override { return 0; }
	size_t next_shenanigan(const decision&) override { return 0; }
	bool reveal_shenanigan(const decision&, const shenanigan&) override { return reveal_cards; }
	unsigned ether_to_spend(const decision&, const spell&) override { return 0; }
	size_t propaganda_target(const decision&, int, const std::vector<size_t>&) override { return 0; }
	// the scripted opponent (the first one if it is not an opponent)
	size_t opponent_target(const decision&, const std::vector<size_t>& seats) override {
		auto it = std::find(seats.begin(), seats.end(), opponent);
		return (it == seats.end()) ? 0 : static_cast<size_t>(it - seats.begin());
	}
	size_t cosmos_card(const decision&) override { return 0; }
	// the scripted opponent while it has ether, then stop
	size_t ether_leak_target(const decision&, const std::vector<size_t>& seats) override {
		return static_cast<size_t>(std::find(seats.begin(), seats.end(), opponent) - seats.begin());
	}
	size_t divergence_branch(const decision&, const event&) override { return 0; }
	size_t any_faction(const decision&, int, const std::vector<faction>&) override { return 0; }
};

// the controller of a seat, as its concrete type
template <class C>
C& controller_of(game& g, size_t seat) {
	return static_cast<C&>(g.players.at(seat)->get_controller());
}

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
