// The smart player's heuristics (its contract here: RED in the lead, 1 point).
#include "test_support.hpp"

using test::give_ether;
using test::hand_of;
using test::reveal;

TEST_CASE("scheming: an ether while less than 2 in reserve, otherwise the best ranked spell") {
	game g = test::make_game<smart_player>();
	player& p = *g.players[0];
	hand_of(p, {"PRECIPITATION", "ETHER", "NEURASTHENIA"});
	CHECK(p.choose_card_to_scheme(g) == 1);
	give_ether(p, 2);
	CHECK(p.choose_card_to_scheme(g) == 2);
	hand_of(p, {"ETHER", "ETHER"});
	CHECK(p.choose_card_to_scheme(g) == 0);   // only ethers
}

TEST_CASE("revealing only what is useful now") {
	test::quiet quiet;
	game g = test::make_game<smart_player>(1, {0, 5, 1});   // YELLOW leads
	player& p = *g.players[0];
	auto euphoria = make_card("EUPHORIA"), neurasthenia = make_card("NEURASTHENIA");
	auto immunity = make_card("IMMUNITY"), leak = make_card("ETHER_LEAK"), ether = make_card("ETHER");
	CHECK_FALSE(p.wants_to_reveal(g, *euphoria));       // empty present
	CHECK_FALSE(p.wants_to_reveal(g, *neurasthenia));
	reveal(g, event(BLUE, 2, 1));                       // neither for it nor for the leader
	CHECK_FALSE(p.wants_to_reveal(g, *euphoria));
	CHECK_FALSE(p.wants_to_reveal(g, *neurasthenia));
	reveal(g, event(YELLOW, 2, 1));                     // the leader's propaganda
	CHECK_FALSE(p.wants_to_reveal(g, *euphoria));
	CHECK(p.wants_to_reveal(g, *neurasthenia));
	reveal(g, event(RED, 2, 1));
	CHECK(p.wants_to_reveal(g, *euphoria));
	g.current_player = 0;
	CHECK(p.wants_to_reveal(g, *immunity));             // only on its own turn
	g.current_player = 1;
	CHECK_FALSE(p.wants_to_reveal(g, *immunity));
	CHECK_FALSE(p.wants_to_reveal(g, *leak));           // no opponent ether
	give_ether(*g.players[2], 1);
	CHECK(p.wants_to_reveal(g, *leak));
	CHECK(p.wants_to_reveal(g, *ether));
}

TEST_CASE("ether: the least that makes its faction lead") {
	test::quiet quiet;
	game g = test::make_game<smart_player>(1, {0, 3, 0});
	player& p = *g.players[0];
	reveal(g, event(RED, 1, 1));                        // RED would end at 1, YELLOW at 3
	give_ether(p, 5);
	auto euphoria = make_card("EUPHORIA");
	const spell& s = dynamic_cast<const spell&>(*euphoria);
	CHECK(p.choose_ether_to_spend(g, s) == 2);          // 1 + (1 + 2) = 4 > 3
	g.scores = {5, 3, 0};
	CHECK(p.choose_ether_to_spend(g, s) == 0);          // leads anyway
	g.scores = {0, 30, 0};
	CHECK(p.choose_ether_to_spend(g, s) == 0);          // out of reach: keeps its ether
	auto leak = make_card("ETHER_LEAK");
	give_ether(*g.players[1], 2);
	give_ether(*g.players[2], 1);
	CHECK(p.choose_ether_to_spend(g, dynamic_cast<const spell&>(*leak)) == 2);   // 1 + 2 = 3 opponent ethers
	auto chapter = make_card("CHAPTER_SKIP");
	CHECK(p.choose_ether_to_spend(g, dynamic_cast<const spell&>(*chapter)) == 0);
}

TEST_CASE("memory lapse to the mage with the most points; the best card from the cosmos") {
	game g = test::make_game<smart_player>();
	player& p = *g.players[0];
	g.players[2]->score_points(1);
	CHECK(p.choose_opponent(g) == g.players[2].get());
	g.cosmos_cards.absorb(make_card("READING"));
	g.cosmos_cards.absorb(make_card("IMMUNITY"));
	g.cosmos_cards.absorb(make_card("ETHER"));
	CHECK(p.choose_cosmos_card(g, 1) == 2);   // no ether in reserve: the ether
	give_ether(p, 2);
	CHECK(p.choose_cosmos_card(g, 1) == 1);   // otherwise Immunity, ranked above Reading
}

TEST_CASE("careful reveal thanks to Reading") {
	game g = test::make_game<smart_player>();
	player& p = *g.players[0];
	g.add_event(event(EXPLOSION, 0, 0));
	CHECK(p.wants_to_reveal_more(g));            // unknown card: the greed decides
	p.remember_future(g.future_events.peek(1));
	CHECK_FALSE(p.wants_to_reveal_more(g));      // a known explosion: it stops
	g.add_event(event(RED, 2, 8));               // an unknown card on top
	CHECK(p.wants_to_reveal_more(g));
	p.remember_future(g.future_events.peek(1));
	g.present_events.set_stealth(9);
	CHECK(p.wants_to_reveal_more(g));            // 9 + 8 = 17 <= 21
	test::controller_of<smart_player>(g, 0).greed = 20;
	g.present_events.set_stealth(14);
	CHECK_FALSE(p.wants_to_reveal_more(g));      // 14 + 8 = 22 > 21
}

TEST_CASE("what Reading showed follows the cards through a rollback") {
	game g = test::make_game<smart_player>();
	player& p = *g.players[0];
	g.add_event(event(EXPLOSION, 0, 0));
	g.add_event(event(BLUE, 1, 1));
	g.add_event(event(RED, 1, 1));
	p.remember_future(g.future_events.peek(3));
	g.present_events.append_from_future(g.future_events);
	g.present_events.append_from_future(g.future_events);
	g.present_events.rollback(g.future_events);
	g.present_events.append_from_future(g.future_events);
	g.present_events.append_from_future(g.future_events);
	CHECK_FALSE(p.wants_to_reveal_more(g));      // the next one is still the explosion
}
