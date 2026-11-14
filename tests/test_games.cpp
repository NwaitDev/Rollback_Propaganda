// Properties every complete game must satisfy, checked on many games.
// These tests are slower: build/check/tests -tce="*[slow]*" skips them.
#include "test_support.hpp"

namespace {

void add_events(game& g, const char* path = "events/base.json") {
	for (const event& e : load_events(path)) g.add_event(e);
}

void give_deck(game& g, const starting_deck& deck) {
	for (auto& p : g.players) {
		starting_deck copy;
		copy.name = deck.name;
		for (const card_ptr& card : deck.cards) copy.cards.push_back(make_card(card->name()));
		p->give_starting_deck(std::move(copy));
	}
}

}

TEST_CASE("[slow] each spell: no card lost or duplicated, immunity expires every turn") {
	test::quiet quiet;
	for (const char* spell : {"EUPHORIA", "NEURASTHENIA", "PRECIPITATION", "MEMORY_LAPSE", "READING",
	                          "ENTROPY_REVERSAL", "ETHER_LEAK", "IMMUNITY", "CHAPTER_SKIP"}) {
		CAPTURE(spell);
		test::temporary_file file(std::string(R"({"name":"t","cards":{"ETHER":4,"IMMUNITY":3,")")
		                          + spell + "\":3}}", "deck");
		starting_deck deck = load_deck(file.path);
		size_t biggest_hand = 0;
		bool something_known = false;
		for (unsigned seed = 0; seed < 40; ++seed) {
			game g(21, seed);
			for (int i = 0; i < 3; ++i) g.add_player(std::make_unique<random_player>(10));
			give_deck(g, deck);
			add_events(g);
			g.set_contract_pile(load_contracts("contracts/two_per_faction.json"));
			g.shuffle();
			g.setup();
			size_t total = test::grimory_cards(g);
			REQUIRE(total == 3 * deck.cards.size());
			while (g.future_events.remaining_cards() > 0) {
				g.play_turn();
				CHECK(test::grimory_cards(g) == total);
				for (auto& p : g.players) {
					CHECK(p->immunity_charges == 0);
					biggest_hand = std::max(biggest_hand, p->get_hand().get_nb_cards());
					something_known = something_known || !p->known_events.empty();
				}
			}
		}
		// the spells that leave a trace in the hands or the memory
		if (std::string(spell) == "MEMORY_LAPSE" || std::string(spell) == "ENTROPY_REVERSAL") {
			CHECK(biggest_hand > 4);
		}
		if (std::string(spell) == "READING") {
			CHECK(something_known);
		}
	}
}

TEST_CASE("[slow] rollbacks: nothing resolved, cards kept, turn passed") {
	test::quiet quiet;
	long rollbacks = 0;
	for (unsigned seed = 0; seed < 60; ++seed) {
		game g(21, seed);
		for (int i = 0; i < 3; ++i) g.add_player(std::make_unique<random_player>(12 + seed % 6));
		give_deck(g, load_deck("decks/all_spells.json"));
		add_events(g);
		g.set_contract_pile(load_contracts("contracts/two_per_faction.json"));
		g.shuffle();
		g.setup();
		size_t total = test::grimory_cards(g);
		while (g.future_events.remaining_cards() > 0 && g.get_elapsed_turns() < 2000) {
			std::vector<int> popularity = g.get_scores_ref();
			std::vector<int> points;
			for (auto& p : g.players) points.push_back(p->get_score());
			size_t before = g.current_player;
			g.play_turn();
			CHECK(test::grimory_cards(g) == total);
			CHECK(g.present_events.is_empty());
			CHECK(g.current_player == (before + 1) % 3);
			for (auto& p : g.players) {
				CHECK(p->cards_in_play.nb_prepared() == 0);
				CHECK(p->immunity_charges == 0);
			}
			if (g.turn_was_rolled_back()) {
				rollbacks++;
				CHECK(g.get_scores_ref() == popularity);   // no resolution
				for (size_t i = 0; i < 3; ++i) CHECK(g.players[i]->get_score() == points[i]);
			}
		}
	}
	CHECK(rollbacks > 0);
}

TEST_CASE("[slow] a game that loops forever stops at the turn limit") {
	test::quiet quiet;
	game g(21, 1);
	for (int i = 0; i < 3; ++i) g.add_player(std::make_unique<random_player>(21));
	add_events(g);
	g.set_contract_pile(load_contracts("contracts/two_per_faction.json"));
	g.shuffle();
	g.setup();
	CHECK_FALSE(g.play_game(300));      // greed 21: every turn ends in a rollback
	CHECK(g.get_elapsed_turns() == 300);
}

TEST_CASE("[slow] mixed smart and random players: contracts, explosions and cards") {
	test::quiet quiet;
	for (const char* path : {"decks/all_spells.json", "decks/implemented.json", "decks/modifiers.json"}) {
		CAPTURE(path);
		starting_deck deck = load_deck(path);
		for (unsigned seed = 0; seed < 30; ++seed) {
			game g(21, seed);
			g.add_player(std::make_unique<smart_player>(10));
			g.add_player(std::make_unique<random_player>(10));
			g.add_player(std::make_unique<smart_player>(10));
			give_deck(g, deck);
			add_events(g);
			size_t events = g.future_events.remaining_cards();
			g.set_contract_pile(load_contracts("contracts/two_per_faction.json"));
			g.shuffle();
			g.setup();
			CHECK(g.contract_pile.size() == 3);   // the contracts not dealt stay face down
			size_t total = test::grimory_cards(g);
			REQUIRE(g.play_game(2000));
			CHECK(test::grimory_cards(g) == total);
			size_t explosions = g.oblivion.size();
			for (const event& e : g.past_events.past) explosions += (e.get_kind() == EXPLOSION);
			CHECK(explosions == 5);
			CHECK(g.oblivion.size() + g.past_events.past.size() == events);
		}
	}
}

TEST_CASE("[slow] metrics of complete games stay consistent") {
	for (const char* deck : {"decks/all_spells.json", "decks/implemented.json", "decks/empty.json"})
	for (const char* contracts : {"contracts/two_per_faction.json", "contracts/mixed.json"})
	for (const char* extra : {"", "IMMUNITY"}) {
		CAPTURE(deck);
		CAPTURE(contracts);
		CAPTURE(extra);
		nlohmann::json e = {{"name", "t"}, {"seed", 3}, {"games", {{"random_only", 0}, {"with_smart", 0}}},
		                    {"players", 3}, {"random_greed", {8, 13, 17}}, {"hand_size", 4},
		                    {"deck", deck}, {"events", "events/control.json"}, {"contracts", contracts}};
		if (*extra) e["extra_card"] = extra;
		test::temporary_file file(e.dump(), "experiment");
		experiment exp = load_experiment(file.path);
		for (stealth greed : exp.random_greeds) {
			experiment level = exp.with_greed(greed);
			for (game_type type : {game_type::RANDOM_ONLY, game_type::WITH_SMART})
			for (long i = 0; i < 4; ++i) {
				game_result r = run_game(level, type, i, 1000 + static_cast<unsigned>(i), false);
				const game_stats& s = r.stats;
				REQUIRE(r.finished);
				CHECK(s.resolved_turns + s.stealth_rollbacks <= r.turns);
				CHECK(r.turns - s.resolved_turns - s.stealth_rollbacks <= 5);   // 5 explosions at most
				CHECK(s.elections <= 5);
				CHECK(s.events_resolved <= 80);
				for (const auto& [name, c] : s.cards) {
					CHECK(c.revealed <= c.schemed);
					CHECK(c.failed <= c.revealed);
					if (name == "ETHER") CHECK(c.failed == 0);
				}
				CHECK(r.holder_seat == (level.extra_card.empty() ? -1 : i % 3));
				if (type == game_type::WITH_SMART && !level.extra_card.empty()) {
					CHECK(r.holder_seat == r.smart_seat);
				}
				CHECK(s.last_turn_behind[r.winner] < r.turns);
				CHECK(r.points[r.winner] == *std::max_element(r.points.begin(), r.points.end()));
			}
		}
	}
}
