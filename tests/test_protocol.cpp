// The protocol of ./server (protocol.hpp): cards and games in JSON, rebuilt
// games, and the questions.
#include "test_support.hpp"
#include "protocol.hpp"
#include "setup.hpp"

namespace {

// checks, at every step of a game, that the state rebuilt from the state
// gives the same state again
class round_trip : public game_observer {
public:
	long checked = 0;
	void check(game& g) {
		json state = state_codec::state(g);
		CHECK(state_codec::state(state_codec::rebuild(state)) == state);
		checked++;
	}
	void phase_started(game& g, game_phase) override { check(g); }
	void event_revealed(game& g, const event&) override { check(g); }
	void card_revealed(game& g, const player&, const shenanigan&) override { check(g); }
	void spell_discarded(game& g, const player&, const shenanigan&) override { check(g); }
	void turn_ended(game& g, turn_outcome) override { check(g); }
};

} // namespace

TEST_CASE("an event survives JSON, with its id and modifications") {
	event multiple(DIVERGENCE, event(ANY, 1, 1), event(RED, 2, 2));
	multiple.set_id(42);
	multiple.modify(+2);
	event back = state_codec::event_from_json(state_codec::event_json(multiple));
	CHECK(back.get_id() == 42);
	CHECK(back.get_operator() == DIVERGENCE);
	CHECK(back.get_left().get_value() == 3);
	CHECK(back.get_right().get_value() == 4);
	CHECK(back.get_right().get_base_value() == 2);
	CHECK(state_codec::event_json(back) == state_codec::event_json(multiple));

	for (const event& e : {event(ELECTION, 3, 0), event(MISTAKE, 0, 2), event(EXPLOSION, 0, 0),
	                       event(COLLAPSE, 0, 0), event(BLUE, -1, 4)}) {
		CHECK(state_codec::event_json(state_codec::event_from_json(state_codec::event_json(e)))
		      == state_codec::event_json(e));
	}
}

TEST_CASE("a game rebuilt from its state has the same state, at every step") {
	for (unsigned seed = 1; seed <= 4; ++seed) {
		game g(21, seed);
		round_trip check;
		std::vector<std::unique_ptr<controller>> seats;
		seats.push_back(std::make_unique<smart_player>(12));
		seats.push_back(std::make_unique<random_player>(10));
		seats.push_back(std::make_unique<random_player>(14));
		game_files files;
		files.deck_paths = {"decks/all_spells.json"};
		files.contracts_path = "contracts/mixed.json";
		prepare_game(g, std::move(seats), files);
		g.add_observer(&check);
		g.play_game(200);
		CHECK(check.checked > 100);
	}
}

TEST_CASE("the state: future top first, known cards, controllers") {
	game g = test::make_game<smart_player>(1, {0, 0, 0}, 12);
	g.add_event(event(RED, 1, 1));
	g.add_event(event(BLUE, 2, 2));               // added last: on top
	g.players[0]->remember_future(g.future_events.peek(1));
	json state = state_codec::state(g);
	CHECK(state["future"][0]["faction"] == "BLUE");
	CHECK(state["seats"][0]["known_future"] == json::array({state["future"][0]["id"]}));
	CHECK(state["seats"][0]["controller"]["strategy"] == "SMART");
	CHECK(state["seats"][0]["controller"]["greed"] == 12);
	CHECK(state["phase"] == "schemes");
}

TEST_CASE("questions: options in the order of the game, yes first") {
	game g = test::make_game<random_player>(10);
	player& p = *g.players[0];
	p.receive_card(make_card("EUPHORIA"));
	p.receive_card(make_card("ETHER"));
	decision d{g, p, 0, g.players[0]->get_controller().rng()};
	p.cards_in_hand.cards[0]->set_id(7);
	CHECK(questions::scheme_card(d).options
	      == json::array({{{"hand", 0}, {"card", {{"id", 7}, {"name", "EUPHORIA"}}}},
	                      {{"hand", 1}, {"card", {{"id", -1}, {"name", "ETHER"}}}}}));
	CHECK(questions::reveal_more(d).options == json::array({"reveal", "stop"}));
	CHECK(questions::ether_leak_target(d, {}, 2).options == json::array({"stop"}));
	CHECK(questions::any_faction(2, {RED, BLUE}).options == json::array({"RED", "BLUE"}));
}

TEST_CASE("every grimory card of a game has its own id") {
	game g(21, 3);
	std::vector<std::unique_ptr<controller>> seats;
	for (int i = 0; i < 3; ++i) seats.push_back(std::make_unique<random_player>(10));
	game_files files;
	files.deck_paths = {"decks/all_spells.json"};
	files.contracts_path = "contracts/mixed.json";
	prepare_game(g, std::move(seats), files);
	std::set<long> ids;
	size_t cards = 0;
	json state = state_codec::state(g);
	for (const json& seat : state["seats"]) {
		for (const char* zone : {"hand", "grimory"}) {
			for (const json& card : seat[zone]) {
				ids.insert(card["id"].get<long>());
				cards++;
			}
		}
	}
	CHECK(cards > 0);
	CHECK(ids.size() == cards);
	CHECK(*ids.begin() == 0);
	CHECK(*ids.rbegin() == static_cast<long>(cards) - 1);
}
