// Contracts: ranks and ties, points, rating, and the smart player's choices
// for each kind of contract.
#include "test_support.hpp"

using test::make_contract;

namespace {

// 3 smart players: player 0 holds `mine`, the others YELLOW and BLUE
game game_with(const contract& mine, std::vector<int> popularity) {
	game g = test::make_game<smart_player>(1, popularity);
	g.players[0]->sign_contract(mine);
	return g;
}

std::vector<int> delta(game& g, const event& e, std::vector<int> before) {
	return test::delta(g, e, 0, before);
}

const contract favourite = make_contract({RED}, 1, 2);
const contract outsider = make_contract({YELLOW}, 2, 3);
const contract duo = make_contract({RED, BLUE}, 1, 1);

}

TEST_CASE("ranks: a faction tied with another one holds no rank") {
	CHECK(rank_of(RED, {5, 3, 1}) == 1);
	CHECK(rank_of(YELLOW, {5, 3, 1}) == 2);
	CHECK(rank_of(BLUE, {5, 3, 1}) == 3);
	CHECK(rank_of(RED, {5, 5, 2}) == 0);
	CHECK(rank_of(BLUE, {5, 5, 2}) == 3);      // third, not second
	CHECK(rank_of(YELLOW, {5, 3, 3}) == 0);    // nobody is second
	CHECK(rank_of(RED, {5, 3, 3}) == 1);
}

TEST_CASE("points of the favourite, outsider and duo contracts") {
	CHECK(favourite.points_earned({5, 3, 1}) == 2);
	CHECK(favourite.points_earned({3, 5, 1}) == 0);
	CHECK(favourite.points_earned({5, 5, 1}) == 0);
	CHECK(outsider.points_earned({5, 3, 1}) == 3);
	CHECK(outsider.points_earned({3, 5, 1}) == 0);
	CHECK(outsider.points_earned({5, 3, 3}) == 0);
	CHECK(duo.points_earned({5, 3, 1}) == 1);
	CHECK(duo.points_earned({1, 3, 5}) == 1);
	CHECK(duo.points_earned({1, 5, 3}) == 0);
	CHECK(duo.points_earned({5, 1, 5}) == 0);
}

TEST_CASE("rating: a satisfied contract rates better, then the margin decides") {
	CHECK(favourite.rate({9, 3, 1}) > favourite.rate({5, 3, 1}));
	CHECK(favourite.rate({5, 3, 1}) > favourite.rate({3, 3, 1}));
	CHECK(favourite.rate({3, 3, 1}) > favourite.rate({1, 3, 1}));
	CHECK(outsider.rate({6, 3, 1}) > outsider.rate({4, 3, 1}));   // second with room on both sides
	CHECK(outsider.rate({4, 3, 1}) > outsider.rate({2, 3, 1}));   // first: the contract fails
}

TEST_CASE("election: each mage scores with their own contract") {
	test::quiet quiet;
	game g = game_with(outsider, {5, 3, 1});
	g.players[1]->sign_contract(favourite);
	g.players[2]->sign_contract(duo);
	g.resolve_event(event(ELECTION, 3, 0), *g.players[0]);
	CHECK(g.players[0]->get_score() == 3);
	CHECK(g.players[1]->get_score() == 2);
	CHECK(g.players[2]->get_score() == 1);
}

TEST_CASE("smart player with a contract in the lead (RED)") {
	test::quiet quiet;
	game g = game_with(make_contract({RED}), {0, 5, 1});        // YELLOW leads, BLUE trails
	std::vector<int> s = {0, 5, 1};
	CHECK(delta(g, event(ANY, 3, 5), s) == std::vector<int>{3, 0, 0});     // a positive ANY: for itself
	CHECK(delta(g, event(ANY, -3, 5), s) == std::vector<int>{0, -3, 0});   // a negative one: the leader
	CHECK(delta(g, event(DIVERGENCE, event(YELLOW, 2, 1), event(RED, 1, 1)), s) == std::vector<int>{1, 0, 0});
	CHECK(delta(g, event(DIVERGENCE, event(ANY, 3, 1), event(RED, 1, 1)), s) == std::vector<int>{3, 0, 0});
}

TEST_CASE("smart player with an outsider contract (YELLOW second)") {
	test::quiet quiet;
	game g = game_with(outsider, {6, 4, 1});
	CHECK(delta(g, event(ANY, 2, 1), {6, 4, 1}) == std::vector<int>{2, 0, 0});    // RED 8: room on both sides
	CHECK(delta(g, event(ANY, 1, 1), {6, 4, 1}) == std::vector<int>{1, 0, 0});
	CHECK(delta(g, event(ANY, 2, 1), {9, 4, 3}) == std::vector<int>{0, 2, 0});    // here YELLOW +2 is safest
	CHECK(delta(g, event(ANY, -2, 1), {6, 4, 1}) == std::vector<int>{0, 0, -2});  // widens the gap below
	// boosting YELLOW would put it ahead of RED: it boosts RED instead
	g.scores = {6, 4, 1};
	test::reveal(g, event(YELLOW, 2, 1));
	test::reveal(g, event(RED, 1, 1));
	test::reveal(g, event(ELECTION, 3, 0));
	event* target = g.players[0]->choose_propaganda(g, +1);
	REQUIRE(target != nullptr);
	CHECK(target->get_faction() == RED);
}

TEST_CASE("smart player with a duo contract (RED or BLUE in the lead)") {
	test::quiet quiet;
	game g = game_with(duo, {2, 5, 4});
	CHECK(delta(g, event(ANY, 3, 1), {2, 5, 4}) == std::vector<int>{0, 0, 3});    // BLUE takes the lead
	CHECK(delta(g, event(DIVERGENCE, event(RED, 2, 1), event(YELLOW, -2, 1)), {2, 5, 4})
	      == std::vector<int>{0, -2, 0});
}

TEST_CASE("smart player spends the least ether that secures the election") {
	test::quiet quiet;
	game g = game_with(favourite, {0, 3, 0});
	player& p = *g.players[0];
	test::reveal(g, event(RED, 1, 1));
	test::reveal(g, event(ELECTION, 3, 0));
	test::give_ether(p, 5);
	auto euphoria = make_card("EUPHORIA");
	const spell& s = dynamic_cast<const spell&>(*euphoria);
	CHECK(p.wants_to_reveal(g, *euphoria));
	CHECK(p.choose_ether_to_spend(g, s) == 2);   // 1 + (1 + 2) = 4 > 3 before the election
	g.scores = {0, 30, 0};
	CHECK(p.choose_ether_to_spend(g, s) == 0);   // out of reach: it keeps its ether
}

TEST_CASE("contract files: clear errors, default names") {
	auto error = [](const std::string& content) {
		test::temporary_file file(content, "contracts");
		return test::error_of([&] { load_contracts(file.path); });
	};
	CHECK(error(R"({"contracts":[{"count":1,"factions":["RED"],"rank":4,"points":1}]})").find("rank") != std::string::npos);
	CHECK(error(R"({"contracts":[{"count":1,"factions":["RED","RED"],"rank":1,"points":1}]})").find("twice") != std::string::npos);
	CHECK(error(R"({"contracts":[{"count":1,"factions":["GREEN"],"rank":1,"points":1}]})").find("RED, YELLOW or BLUE") != std::string::npos);
	CHECK(error(R"({"contracts":[{"count":1,"factions":["RED"],"rank":1,"pts":1}]})").find("unknown key") != std::string::npos);
	test::temporary_file file(R"({"contracts":[{"count":2,"factions":["BLUE","RED"],"rank":1,"points":1}]})", "contracts");
	std::vector<contract> loaded = load_contracts(file.path);
	REQUIRE(loaded.size() == 2);
	CHECK(loaded[0].name == "BLUE+RED rank 1, 1 pt");
}
