// Events: loading, copies, modifications (Euphoria / Neurasthenia) and their
// reset when a card changes zone.
#include "test_support.hpp"

TEST_CASE("events/base.json loads every card, AND and OR included") {
	std::vector<event> events = load_events("events/base.json");
	CHECK(events.size() == 10 + 40 + 28 + 28 + 15);
	size_t and_cards = 0, or_cards = 0;
	for (const event& e : events) {
		if (e.get_kind() == OPERATOR) {
			(e.get_operator() == CONVERGENCE ? and_cards : or_cards)++;
		}
	}
	CHECK(and_cards == 28);
	CHECK(or_cards == 28);
}

TEST_CASE("copies of a multiple propaganda have their own branches") {
	std::vector<event> events = load_events("events/base.json");
	std::vector<event*> multiple;
	for (event& e : events) {
		if (e.get_kind() == OPERATOR) multiple.push_back(&e);
	}
	REQUIRE(multiple.size() >= 2);
	multiple[0]->modify(+3);
	CHECK(multiple[0]->get_left().get_value() == 4);
	CHECK(multiple[1]->get_left().get_value() == 1);   // another copy of the same card
	event copy = *multiple[0];
	copy.modify(+1);
	CHECK(multiple[0]->get_left().get_value() == 4);
	CHECK(copy.get_left().get_value() == 5);
}

TEST_CASE("a multiple propaganda is modified on every branch, AND as OR") {
	for (event_operator op : {CONVERGENCE, DIVERGENCE}) {
		event e(op, event(ANY, 1, 1), event(RED, 2, 2));
		CHECK(e.is_propaganda_card());
		e.modify(+2);
		CHECK(e.get_left().get_value() == 3);
		CHECK(e.get_right().get_value() == 4);
		e.reset();
		CHECK(e.get_left().get_value() == 1);
		CHECK(e.get_right().get_value() == 2);
		e.modify(-3);
		CHECK(e.get_left().get_value() == -2);
		CHECK(e.get_right().get_value() == -1);
	}
	CHECK_FALSE(event(ELECTION, 3, 0).is_propaganda_card());
}

TEST_CASE("modifications add up") {
	event simple(BLUE, 2, 4);
	simple.modify(+2);
	simple.modify(+1);
	simple.modify(-4);
	CHECK(simple.get_value() == 1);
	event multiple(CONVERGENCE, event(ANY, 1, 1), event(RED, 2, 2));
	multiple.modify(+1);
	multiple.modify(+2);
	CHECK(multiple.get_left().get_value() == 4);
	CHECK(multiple.get_right().get_value() == 5);
}

TEST_CASE("a resolved card counts with its modifications, then loses them in the past") {
	test::quiet quiet;
	game g(21, 1);
	g.add_player(std::make_unique<random_player>(10));
	g.players[0]->sign_contract(test::make_contract({RED}));
	test::reveal(g, event(BLUE, 2, 4));
	g.present_events.present.front().modify(+3);
	g.present_events.present.front().modify(+1);
	g.execution_phase();
	CHECK(g.get_scores_ref()[BLUE] == 6);
	CHECK(g.past_events.past.back().get_value() == 2);
}

TEST_CASE("a rollback puts the cards back in the future without their modifications") {
	game g(21, 1);
	test::reveal(g, event(CONVERGENCE, event(YELLOW, 1, 1), event(RED, 2, 2)));
	g.present_events.present.front().modify(-5);
	g.present_events.rollback(g.future_events);
	const event& back = g.future_events.top();
	CHECK(back.get_left().get_value() == 1);
	CHECK(back.get_right().get_value() == 2);
}

TEST_CASE("every card of a game gets its own id, kept by its copies") {
	game g(21, 1);
	g.add_event(event(RED, 1, 1));
	g.add_event(event(RED, 1, 1));
	std::vector<event> top = g.future_events.peek(2);
	CHECK(top[0].get_id() != top[1].get_id());
	event copy = top[0];
	CHECK(copy.get_id() == top[0].get_id());
}
