// Rules of the game: resolution of the propagandas (ANY, AND, OR), elections,
// several schemed cards, rollbacks and the winner.
#include "test_support.hpp"

using test::delta;

TEST_CASE("random player: the ANY of a card go to distinct factions, every one possible") {
	test::quiet quiet;
	std::set<std::vector<int>> any_any, any_else;
	for (unsigned seed = 0; seed < 400; ++seed) {
		game g = test::make_game<random_player>(seed);
		auto d = delta(g, event(CONVERGENCE, event(ANY, 1, 1), event(ANY, 2, 2)), 0, {0, 0, 0});
		CHECK(std::count(d.begin(), d.end(), 1) == 1);
		CHECK(std::count(d.begin(), d.end(), 2) == 1);
		any_any.insert(d);
		d = delta(g, event(CONVERGENCE, event(ANY, 1, 1), event(RED, 2, 2)), 0, {0, 0, 0});
		CHECK(d[RED] == 2);
		CHECK(d[YELLOW] + d[BLUE] == 1);   // the ANY never goes to RED, already targeted
		any_else.insert(d);
		d = delta(g, event(DIVERGENCE, event(YELLOW, 1, 1), event(BLUE, 2, 2)), 0, {0, 0, 0});
		CHECK(((d == std::vector<int>{0, 1, 0}) || (d == std::vector<int>{0, 0, 2})));
	}
	CHECK(any_any.size() == 6);
	CHECK(any_else.size() == 2);
}

TEST_CASE("election: every mage scores what their contract pays") {
	test::quiet quiet;
	game g = test::make_game<random_player>(1, {3, 1, 1});
	g.resolve_event(event(ELECTION, 3, 0), *g.players[0]);
	CHECK(g.players[0]->get_score() == 1);
	CHECK(g.players[1]->get_score() == 0);
	g.scores = {3, 3, 1};                   // tie for the lead: nobody leads
	g.resolve_event(event(ELECTION, 3, 0), *g.players[0]);
	CHECK(g.players[0]->get_score() == 1);
	CHECK(g.players[1]->get_score() == 0);
}

TEST_CASE("several schemed cards: all handled, or all given back by a rollback") {
	test::quiet quiet;
	game g = test::make_game<random_player>(5);
	player& p = *g.players[0];
	p.cards_in_play.prepare_shenanigan(make_card("ETHER"));
	p.cards_in_play.prepare_shenanigan(make_card("IMMUNITY"));
	p.cards_in_play.prepare_shenanigan(make_card("ETHER"));
	size_t hand = p.cards_in_hand.get_nb_cards();
	p.take_back_shenanigan();
	CHECK(p.cards_in_hand.get_nb_cards() == hand + 3);
	CHECK(p.cards_in_play.nb_prepared() == 0);
	for (int i = 0; i < 3; ++i) {
		p.cards_in_play.prepare_shenanigan(p.cards_in_hand.play_card(0));
	}
	p.do_shenanigans(g);
	CHECK(p.cards_in_play.nb_prepared() == 0);
	CHECK(p.deck.size() + p.available_ether() + g.cosmos_cards.size() == 3);
}

TEST_CASE("smart player: ethers first, then immunity, then the other cards") {
	game g = test::make_game<smart_player>(5);
	player& p = *g.players[0];
	p.cards_in_play.prepare_shenanigan(make_card("EUPHORIA"));
	p.cards_in_play.prepare_shenanigan(make_card("IMMUNITY"));
	p.cards_in_play.prepare_shenanigan(make_card("ETHER"));
	CHECK(p.choose_next_shenanigan(g) == 2);
	p.cards_in_play.take_prepared_shenanigan(2);
	CHECK(p.choose_next_shenanigan(g) == 1);
}

TEST_CASE("winner: points, then the biggest grimory, then a draw") {
	game g = test::make_game<random_player>(9);
	g.players[1]->score_points(1);
	CHECK(g.determine_winner() == 1);
	g.players[2]->score_points(1);
	g.players[2]->add_to_grimory(make_card("ETHER"));
	tiebreak how;
	CHECK(g.determine_winner(&how) == 2);
	CHECK(how == tiebreak::GRIMORY);
	g.players[1]->add_to_grimory(make_card("ETHER"));
	std::set<size_t> winners;
	for (unsigned seed = 0; seed < 50; ++seed) {
		g.generator.seed(seed);
		winners.insert(g.determine_winner(&how));
		CHECK(how == tiebreak::DRAW);
	}
	CHECK(winners == std::set<size_t>{1, 2});
}

TEST_CASE("a rollback ends the turn: no resolution, schemes back in hand, next mage") {
	test::quiet quiet;
	game g(21, 1);
	for (int i = 0; i < 3; ++i) g.add_player(std::make_unique<test::scripted>(30));
	for (auto& p : g.players) p->sign_contract(test::make_contract({RED}));
	g.add_event(event(RED, 1, 15));
	g.add_event(event(RED, 1, 15));             // 30 > 21: forced rollback
	g.players[0]->receive_card(make_card("EUPHORIA"));
	g.play_turn();
	CHECK(g.turn_was_rolled_back());
	CHECK(g.get_scores_ref() == std::vector<int>{0, 0, 0});
	CHECK(g.present_events.is_empty());
	CHECK(g.future_events.remaining_cards() == 2);
	CHECK(g.players[0]->get_hand().get_nb_cards() == 1);
	CHECK(g.current_player == 1);
}

TEST_CASE("an explosion goes to oblivion and rolls the turn back") {
	test::quiet quiet;
	game g(21, 1);
	g.add_player(std::make_unique<test::scripted>(21));
	g.players[0]->sign_contract(test::make_contract({RED}));
	g.add_event(event(RED, 1, 1));
	g.add_event(event(EXPLOSION, 0, 0));
	g.add_event(event(RED, 1, 1));   // added last: on top of the future
	g.play_turn();
	CHECK(g.turn_was_rolled_back());
	CHECK(g.oblivion.size() == 1);
	CHECK(g.future_events.remaining_cards() == 2);
}

TEST_CASE("the same seed plays the same game") {
	auto play = [](unsigned seed) {
		test::quiet quiet;
		game g(21, seed);
		for (int i = 0; i < 3; ++i) g.add_player(std::make_unique<random_player>(10));
		for (auto& p : g.players) p->give_starting_deck(load_deck("decks/all_spells.json"));
		for (const event& e : load_events("events/base.json")) g.add_event(e);
		g.set_contract_pile(load_contracts("contracts/mixed.json"));
		g.shuffle();
		g.setup();
		g.play_game();
		std::vector<int> outcome = g.get_scores_ref();
		for (auto& p : g.players) outcome.push_back(p->get_score());
		outcome.push_back(static_cast<int>(g.get_elapsed_turns()));
		return outcome;
	};
	CHECK(play(3) == play(3));
	CHECK(play(3) != play(4));
}
