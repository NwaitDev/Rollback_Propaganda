#include "../include/game_concepts.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

const std::string default_deck = "decks/implemented.json";
const std::string default_contracts = "contracts/two_per_faction.json";
const std::string default_events = "events/base.json";

// deck_paths: either a single deck for every mage, or one deck per mage
void init_game(game& g, const std::vector<std::string>& deck_paths,
               const std::string& events_path){
	// the contracts (factions) are dealt at random by game::setup()
	g.add_player(std::make_unique<random_player>(10));
	g.add_player(std::make_unique<random_player>(10));
	g.add_player(std::make_unique<random_player>(10));

	std::vector<std::unique_ptr<player>>& players = g.get_players_ref();
	if (deck_paths.size() != 1 && deck_paths.size() != players.size()) {
		throw std::runtime_error("give either one --deck for every mage, or one per mage ("
		                         + std::to_string(players.size()) + ")");
	}
	for (size_t i = 0; i < players.size(); ++i) {
		const std::string& path = (deck_paths.size() == 1) ? deck_paths[0] : deck_paths[i];
		players[i]->give_starting_deck(load_deck(path));
	}

	for (const event& e : load_events(events_path)) {
		g.add_event(e);
	}
}


// parses an integer in [min, max], or throws std::invalid_argument
long long parse_integer(const std::string& value, long long min, long long max){
	size_t parsed = 0;
	long long n = 0;
	try {
		n = std::stoll(value, &parsed);
	} catch (const std::exception&) {
		parsed = 0;
	}
	if (value.empty() || parsed != value.size() || n < min || n > max) {
		throw std::invalid_argument("expects an integer between " + std::to_string(min)
		                            + " and " + std::to_string(max));
	}
	return n;
}

// usage: simulation [--play] [--deck <file.json>]... [--contracts <file.json>]
//                   [--events <file.json>]
//                   [--popularity <n>] [--seed <n>]
//   --play        plays a whole game, prints the final scores and the winner
//   --deck        starting deck: once for every mage, or once per mage (in order)
//   --contracts   pile of contract cards (default: contracts/two_per_faction.json)
//   --events      event cards of the future (default: events/base.json)
//   --popularity  starting popularity of every faction (default: 0)
//   --seed        seed of the random generator; without it, a seed is drawn and
//                 printed, so that any game can be replayed
int main(int argc, char* argv[]){

	const std::string usage = std::string("usage: ") + argv[0]
		+ " [--play] [--deck <file.json>]... [--contracts <file.json>] [--events <file.json>]"
		+ " [--popularity <n>] [--seed <n>]";
	bool play = false;
	std::vector<std::string> deck_paths;
	std::string contracts_path = default_contracts;
	std::string events_path = default_events;
	int popularity = 0;
	unsigned seed = std::random_device{}();
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		bool has_value = (i + 1 < argc);
		try {
			if (arg == "--play") {
				play = true;
			} else if (arg == "--deck" && has_value) {
				deck_paths.push_back(argv[++i]);
			} else if (arg == "--contracts" && has_value) {
				contracts_path = argv[++i];
			} else if (arg == "--events" && has_value) {
				events_path = argv[++i];
			} else if (arg == "--popularity" && has_value) {
				popularity = static_cast<int>(parse_integer(argv[++i], -1000000, 1000000));
			} else if (arg == "--seed" && has_value) {
				seed = static_cast<unsigned>(parse_integer(argv[++i], 0, 4294967295LL));
			} else {
				std::cerr<<usage<<std::endl;
				return 1;
			}
		} catch (const std::invalid_argument& e) {
			std::cerr<<"error: "<<arg<<" "<<e.what()<<std::endl;
			return 1;
		}
	}
	if (deck_paths.empty()) {
		deck_paths.push_back(default_deck);
	}

	std::cout<<"seed: "<<seed<<std::endl;

	game g = game(21, seed, popularity);

	try {
		init_game(g, deck_paths, events_path);
		g.set_contract_pile(load_contracts(contracts_path));
		g.shuffle();
		g.setup();
	} catch (const std::exception& e) {
		std::cerr<<"error: "<<e.what()<<std::endl;
		return 1;
	}

	g.view();

	if (play) {
		g.play_game();
		std::vector<std::unique_ptr<player>>& players = g.get_players_ref();
		for (std::unique_ptr<player>& p : players) {
			p->view(); std::cout<<std::endl;
		}
		size_t winner = g.determine_winner();
		std::cout<<"winner: player n°"<<winner<<" (";
		players[winner]->view();
		std::cout<<")"<<std::endl;
	}

	return 0;
}
