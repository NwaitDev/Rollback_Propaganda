#include "../include/game_concepts.hpp"
#include "../include/setup.hpp"
#include "../include/text_view.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

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
	game_files files;
	files.deck_paths.clear();
	int popularity = 0;
	unsigned seed = std::random_device{}();
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		bool has_value = (i + 1 < argc);
		try {
			if (arg == "--play") {
				play = true;
			} else if (arg == "--deck" && has_value) {
				files.deck_paths.push_back(argv[++i]);
			} else if (arg == "--contracts" && has_value) {
				files.contracts_path = argv[++i];
			} else if (arg == "--events" && has_value) {
				files.events_path = argv[++i];
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
	if (files.deck_paths.empty()) {
		files.deck_paths.push_back(default_deck);
	}

	std::cout<<"seed: "<<seed<<std::endl;

	game g = game(21, seed, popularity);
	text_logger logger(std::cout);
	g.add_observer(&logger);

	try {
		std::vector<std::unique_ptr<controller>> seats;
		for (int i = 0; i < 3; ++i) {
			seats.push_back(std::make_unique<random_player>(10));
		}
		prepare_game(g, std::move(seats), files);
	} catch (const std::exception& e) {
		std::cerr<<"error: "<<e.what()<<std::endl;
		return 1;
	}

	print_game(std::cout, g);

	if (play) {
		g.play_game();
		std::vector<std::unique_ptr<player>>& players = g.get_players_ref();
		for (std::unique_ptr<player>& p : players) {
			std::cout<<describe(*p)<<std::endl;
		}
		size_t winner = g.determine_winner();
		std::cout<<"winner: player n°"<<winner<<" ("<<describe(*players[winner])<<")"<<std::endl;
	}

	return 0;
}
