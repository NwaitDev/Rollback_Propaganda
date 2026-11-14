// Configuration files: decks, contracts (see test_contracts.cpp), events and
// experiments. Every mistake must give a clear message.
#include "test_support.hpp"

namespace {

std::string deck_error(const std::string& content) {
	test::temporary_file file(content, "deck");
	return test::error_of([&] { load_deck(file.path); });
}

std::string events_error(const std::string& content) {
	test::temporary_file file(content, "events");
	return test::error_of([&] { load_events(file.path); });
}

// a valid experiment, with some keys changed or added
std::string experiment_error(const std::string& changes) {
	nlohmann::json e = {{"name", "t"}, {"seed", 1}, {"games", {{"random_only", 1}, {"with_smart", 1}}},
	                    {"players", 3}, {"random_greed", 10}, {"hand_size", 4},
	                    {"deck", "decks/empty.json"}, {"events", "events/control.json"},
	                    {"contracts", "contracts/two_per_faction.json"}};
	e.update(nlohmann::json::parse(changes));
	test::temporary_file file(e.dump(), "experiment");
	return test::error_of([&] { load_experiment(file.path); });
}

bool has(const std::string& message, const std::string& part) {
	return message.find(part) != std::string::npos;
}

}

TEST_CASE("the files of the project load") {
	for (const char* deck : {"decks/all_spells.json", "decks/implemented.json",
	                         "decks/modifiers.json", "decks/empty.json"}) {
		CHECK_NOTHROW(load_deck(deck));
	}
	CHECK(load_deck("decks/empty.json").cards.empty());
	CHECK(load_events("events/control.json").size() == 16 * 5);
	CHECK(load_contracts("contracts/two_per_faction.json").size() == 6);
	CHECK(load_contracts("contracts/mixed.json").size() == 9);
}

TEST_CASE("every card name of make_card() is listed by card_names()") {
	std::vector<std::string> names = card_names();
	CHECK(names.size() == 10);
	for (const std::string& name : names) {
		CHECK(make_card(name) != nullptr);
	}
	CHECK(make_card("FIREBALL") == nullptr);
}

TEST_CASE("deck files: errors") {
	CHECK(has(deck_error(R"({"name":"d","cards":{"ETHER":2,"FIREBALL":1}})"), "unknown card \"FIREBALL\""));
	CHECK(has(deck_error(R"({"name":"d","cards":{"ETHER":-1}})"), "non-negative integer"));
	CHECK(has(deck_error(R"({"name":"d", "cards": )"), "invalid JSON"));
	CHECK(has(deck_error(R"({"cards":{"ETHER":1}})"), "missing \"name\""));
	CHECK(has(test::error_of([] { load_deck("no_such_file.json"); }), "cannot open"));
}

TEST_CASE("event files: errors") {
	CHECK(has(events_error(R"({"events":[{"kind":"ELECTION","stealth":3}]})"), "missing \"count\""));
	CHECK(has(events_error(R"({"events":[{"count":1,"kind":"FIREWORKS"}]})"), "unknown kind"));
	CHECK(has(events_error(R"({"events":[{"count":1,"kind":"PROPAGANDA","faction":"GREEN","value":1}]})"), "faction"));
	CHECK(has(events_error(R"({"events":[{"count":1,"kind":"PROPAGANDA","faction":"RED"}]})"), "\"value\""));
	CHECK(has(events_error(R"({"events":[{"count":1,"kind":"AND","left":{"faction":"RED","value":1}}]})"), "\"right\""));
}

TEST_CASE("experiment files: valid, and errors") {
	CHECK(experiment_error("{}").empty());
	CHECK(experiment_error(R"({"random_greed":[8,13,17],"extra_card":"EUPHORIA"})").empty());
	CHECK(has(experiment_error(R"({"hand_sise":4})"), "unknown key \"hand_sise\""));
	CHECK(has(experiment_error(R"({"players":7})"), "6 contracts for 7 players"));
	CHECK(has(experiment_error(R"({"random_greed":-2})"), "random_greed"));
	CHECK(has(experiment_error(R"({"random_greed":[8,8]})"), "twice"));
	CHECK(has(experiment_error(R"({"random_greed":[]})"), "empty list"));
	CHECK(has(experiment_error(R"({"extra_card":"FIREBALL"})"), "unknown card"));
	CHECK(has(experiment_error(R"({"games":{"random_only":1}})"), "with_smart"));
}

TEST_CASE("an experiment with several greed levels plays each one with the same seeds") {
	test::temporary_file file(R"({"name":"t","seed":4,"games":{"random_only":2,"with_smart":2},
		"players":3,"random_greed":[8,13],"hand_size":4,"deck":"decks/empty.json",
		"events":"events/control.json","contracts":"contracts/two_per_faction.json"})", "experiment");
	experiment exp = load_experiment(file.path);
	CHECK(exp.random_greeds == std::vector<stealth>{8, 13});
	CHECK(exp.with_greed(13).random_greed == 13);
	CHECK(exp.with_greed(13).smart_greed == 13);   // follows the random greed by default
	CHECK(game_seeds(exp).size() == 4);
	CHECK(game_seeds(exp) == game_seeds(exp.with_greed(13)));
}
