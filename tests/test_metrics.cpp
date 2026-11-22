// Metrics counted during a game (game_stats, see campaign.hpp), on games
// built by hand with scripted players.
#include "test_support.hpp"

namespace {

// two scripted players, contracts RED and YELLOW; events given top of the
// future first
struct small_game {
	game g{21, 1};
	game_stats stats;
	player* a;
	player* b;
	test::scripted* ca;   // their controllers
	test::scripted* cb;
	explicit small_game(std::vector<event> future, stealth greed = 21) : stats({0, 0, 0}, 2) {
		g.add_player(std::make_unique<test::scripted>(greed));
		g.add_player(std::make_unique<test::scripted>(greed));
		a = g.players[0].get();
		b = g.players[1].get();
		ca = &test::controller_of<test::scripted>(g, 0);
		cb = &test::controller_of<test::scripted>(g, 1);
		a->sign_contract(test::make_contract({RED}));
		b->sign_contract(test::make_contract({YELLOW}));
		for (auto it = future.rbegin(); it != future.rend(); ++it) g.add_event(*it);
		g.set_observer(&stats);
	}
};

// event(MISTAKE, 0, 1) has a stealth of 0 (the constructor takes the stealth first)
event mistake() { return event(MISTAKE, 0, 1); }

}

TEST_CASE("turn outcomes: resolved, stealth rollback, explosion (not counted)") {
	test::quiet quiet;
	small_game resolved({event(RED, 1, 5), event(RED, 1, 5)});
	resolved.g.play_turn();
	CHECK(resolved.stats.resolved_turns == 1);
	CHECK(resolved.stats.stealth_rollbacks == 0);

	small_game stealth({event(RED, 1, 15), event(RED, 1, 15)}, 30);   // 30 > 21
	stealth.g.play_turn();
	CHECK(stealth.stats.resolved_turns == 0);
	CHECK(stealth.stats.stealth_rollbacks == 1);

	small_game explosion({event(RED, 1, 1), event(EXPLOSION, 0, 0), event(RED, 1, 1)});
	explosion.g.play_turn();
	CHECK(explosion.stats.resolved_turns == 0);
	CHECK(explosion.stats.stealth_rollbacks == 0);
	CHECK(explosion.g.oblivion.size() == 1);
}

TEST_CASE("elections, resolved events, lead changes and highest popularity") {
	test::quiet quiet;
	small_game s({event(RED, 2, 1), event(ELECTION, 1, 0), event(YELLOW, 2, 1),
	              event(YELLOW, 3, 1), event(ELECTION, 1, 0)});
	s.g.play_turn();
	// tie -> RED (1) -> tie (2) -> YELLOW (3)
	CHECK(s.stats.events_resolved == 5);
	CHECK(s.stats.elections == 2);
	CHECK(s.stats.lead_changes == 3);
	CHECK(s.stats.max_popularity == 5);
	CHECK(s.a->get_score() == 1);
	CHECK(s.b->get_score() == 1);
}

TEST_CASE("M18: last turn at which each mage did not have the most points") {
	test::quiet quiet;
	small_game s({event(RED, 2, 1), event(ELECTION, 1, 0), mistake(), event(YELLOW, 5, 1),
	              event(ELECTION, 1, 0), mistake()}, 2);
	s.g.play_turn();
	s.g.play_turn();
	CHECK(s.stats.last_turn_behind[0] == 0);
	CHECK(s.stats.last_turn_behind[1] == 1);   // behind at the end of turn 1 only
	s.g.play_turn();
	s.g.play_turn();
	CHECK(s.stats.last_turn_behind[1] == 1);   // tied afterwards: not behind
	CHECK(s.a->get_score() == 1);
	CHECK(s.b->get_score() == 1);
}

TEST_CASE("cards: schemed, revealed, failed, dead turns while leading or not") {
	test::quiet quiet;
	small_game s({mistake(), mistake()}, 1);
	s.a->receive_card(make_card("EUPHORIA"));      // no propaganda: fails
	s.a->receive_card(make_card("ETHER"));         // stays in hand this turn
	s.b->receive_card(make_card("NEURASTHENIA"));
	s.cb->reveal_cards = false;                     // put under the grimory
	s.b->score_points(1);                          // B leads alone
	s.g.play_turn();
	auto& c = s.stats.cards;
	CHECK(c["EUPHORIA"].schemed == 1);
	CHECK(c["EUPHORIA"].revealed == 1);
	CHECK(c["EUPHORIA"].failed == 1);
	CHECK(c["NEURASTHENIA"].schemed == 1);
	CHECK(c["NEURASTHENIA"].revealed == 0);
	CHECK(c["ETHER"].dead_trailing == 1);
	CHECK(c["ETHER"].dead_leading == 0);
	s.b->receive_card(make_card("READING"));
	s.b->cards_in_hand.cards.insert(s.b->cards_in_hand.cards.begin(), make_card("ETHER"));
	s.g.play_turn();
	CHECK(c["READING"].dead_leading == 1);         // kept by B, who leads alone
}

TEST_CASE("a rollback gives the schemes back: neither revealed nor failed") {
	test::quiet quiet;
	small_game s({event(RED, 1, 15), event(RED, 1, 15)}, 30);
	s.a->receive_card(make_card("EUPHORIA"));
	s.g.play_turn();
	CHECK(s.stats.cards["EUPHORIA"].schemed == 1);
	CHECK(s.stats.cards["EUPHORIA"].revealed == 0);
	CHECK(s.a->get_hand().get_nb_cards() == 1);
}

TEST_CASE("interactions: memory lapse received, ether leaked, immunity triggered") {
	test::quiet quiet;
	small_game lapse({mistake()}, 1);
	lapse.ca->opponent = 1;
	lapse.a->receive_card(make_card("MEMORY_LAPSE"));
	lapse.g.play_turn();
	CHECK(lapse.stats.memory_lapses == 1);
	CHECK(lapse.b->get_hand().get_nb_cards() == 1);

	small_game leak({mistake()}, 1);
	leak.ca->opponent = 1;
	test::give_ether(*leak.b, 2);
	leak.a->receive_card(make_card("ETHER_LEAK"));
	leak.g.play_turn();
	CHECK(leak.stats.ethers_leaked == 1);
	CHECK(leak.b->available_ether() == 1);

	small_game immune({mistake()}, 1);
	immune.ca->opponent = 1;
	immune.b->gain_immunity(1);
	immune.a->receive_card(make_card("MEMORY_LAPSE"));
	immune.g.play_turn();
	CHECK(immune.stats.immunities == 1);
	CHECK(immune.stats.memory_lapses == 0);
}

TEST_CASE("tie-breaks of the winner") {
	small_game s({}, 1);
	s.a->score_points(2);
	s.b->score_points(2);
	s.a->add_to_grimory(make_card("ETHER"));
	tiebreak how;
	CHECK(s.g.determine_winner(&how) == 0);
	CHECK(how == tiebreak::GRIMORY);
	s.b->add_to_grimory(make_card("ETHER"));
	s.g.determine_winner(&how);
	CHECK(how == tiebreak::DRAW);
	s.b->score_points(1);
	CHECK(s.g.determine_winner(&how) == 1);
	CHECK(how == tiebreak::NONE);
}

TEST_CASE("observing a game never changes it") {
	auto play = [](bool observed) {
		test::quiet quiet;
		game g(21, 7);
		for (int i = 0; i < 3; ++i) g.add_player(std::make_unique<random_player>(10));
		for (auto& p : g.players) p->give_starting_deck(load_deck("decks/all_spells.json"));
		for (const event& e : load_events("events/base.json")) g.add_event(e);
		g.set_contract_pile(load_contracts("contracts/mixed.json"));
		g.shuffle();
		g.setup();
		game_stats stats(g.get_scores_ref(), 3);
		if (observed) g.set_observer(&stats);
		g.play_game();
		std::vector<int> outcome = g.get_scores_ref();
		for (auto& p : g.players) outcome.push_back(p->get_score());
		outcome.push_back(static_cast<int>(g.get_elapsed_turns()));
		return outcome;
	};
	CHECK(play(true) == play(false));
}
