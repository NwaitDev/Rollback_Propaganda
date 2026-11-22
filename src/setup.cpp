#include "../include/setup.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

void prepare_game(game &g, std::vector<std::unique_ptr<controller>> seats,
                  const game_files &files) {
	for (std::unique_ptr<controller> &c : seats) {
		g.add_player(std::move(c));
	}

	std::vector<std::unique_ptr<player>> &players = g.get_players_ref();
	const std::vector<std::string> &decks = files.deck_paths;
	if (decks.size() != 1 && decks.size() != players.size()) {
		throw std::runtime_error("give either one --deck for every mage, or one per mage ("
		                         + std::to_string(players.size()) + ")");
	}
	for (size_t i = 0; i < players.size(); ++i) {
		players[i]->give_starting_deck(load_deck((decks.size() == 1) ? decks[0] : decks[i]));
	}
	for (const event &e : load_events(files.events_path)) {
		g.add_event(e);
	}
	// the contracts are dealt at random by game::setup()
	g.set_contract_pile(load_contracts(files.contracts_path));
	g.shuffle();
	g.setup();
}

long long parse_integer(const std::string &value, long long min, long long max) {
	size_t parsed = 0;
	long long n = 0;
	try {
		n = std::stoll(value, &parsed);
	} catch (const std::exception &) {
		parsed = 0;
	}
	if (value.empty() || parsed != value.size() || n < min || n > max) {
		throw std::invalid_argument("expects an integer between " + std::to_string(min)
		                            + " and " + std::to_string(max));
	}
	return n;
}
