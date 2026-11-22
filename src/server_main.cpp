#include "../include/game_concepts.hpp"
#include "../include/protocol.hpp"
#include "../include/setup.hpp"
#include "../include/text_view.hpp"

#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// The game engine as a server (see docs/protocol.md): plays one game, writes
// the announcements and the questions on the standard output, reads the
// answers on the standard input, and writes the game as text on the
// standard error.
//
// usage: server [--players <n>] [--seat <k>=remote|random[:greed]|smart[:greed]]...
//               [--deck <file.json>]... [--contracts <file.json>] [--events <file.json>]
//               [--popularity <n>] [--max-stealth <n>] [--max-turns <n>] [--seed <n>]
//               [--light] [--quiet]
//   --players     number of mages (default: 3)
//   --seat        who plays a seat: remote (default: questions on the standard
//                 output), or a strategy of the engine, whose decisions are
//                 announced (greed: 10 by default)
//   --deck, --contracts, --events, --popularity, --seed: as ./simulation
//   --max-stealth stealth limit (default: 21)
//   --max-turns   the game stops after this number of turns (default: 0, no limit)
//   --light       the announcements do not carry the state (the questions do)
//   --quiet       nothing on the standard error

namespace {

struct seat_spec {
	std::string strategy = "remote";
	stealth greed = 10;
};

// "random", "smart:12", "remote"
seat_spec parse_seat(const std::string &text) {
	seat_spec spec;
	size_t colon = text.find(':');
	spec.strategy = text.substr(0, colon);
	if (spec.strategy != "remote" && spec.strategy != "random" && spec.strategy != "smart") {
		throw std::invalid_argument("expects remote, random[:greed] or smart[:greed]");
	}
	if (colon != std::string::npos) {
		if (spec.strategy == "remote") {
			throw std::invalid_argument("expects no greed for a remote seat");
		}
		spec.greed = static_cast<stealth>(parse_integer(text.substr(colon + 1), 0, 1000));
	}
	return spec;
}

} // namespace

int main(int argc, char *argv[]) {
	const std::string usage = std::string("usage: ") + argv[0]
		+ " [--players <n>] [--seat <k>=remote|random[:greed]|smart[:greed]]..."
		+ " [--deck <file.json>]... [--contracts <file.json>] [--events <file.json>]"
		+ " [--popularity <n>] [--max-stealth <n>] [--max-turns <n>] [--seed <n>]"
		+ " [--light] [--quiet]";
	size_t players = 3;
	std::map<size_t, seat_spec> seats;
	game_files files;
	files.deck_paths.clear();
	int popularity = 0;
	stealth max_stealth = 21;
	long max_turns = 0;
	unsigned seed = std::random_device{}();
	bool light = false;
	bool quiet = false;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		bool has_value = (i + 1 < argc);
		try {
			if (arg == "--players" && has_value) {
				players = static_cast<size_t>(parse_integer(argv[++i], 1, 64));
			} else if (arg == "--seat" && has_value) {
				std::string value = argv[++i];
				size_t equal = value.find('=');
				if (equal == std::string::npos) {
					throw std::invalid_argument("expects <seat>=<who>");
				}
				size_t seat = static_cast<size_t>(parse_integer(value.substr(0, equal), 0, 63));
				seats[seat] = parse_seat(value.substr(equal + 1));
			} else if (arg == "--deck" && has_value) {
				files.deck_paths.push_back(argv[++i]);
			} else if (arg == "--contracts" && has_value) {
				files.contracts_path = argv[++i];
			} else if (arg == "--events" && has_value) {
				files.events_path = argv[++i];
			} else if (arg == "--popularity" && has_value) {
				popularity = static_cast<int>(parse_integer(argv[++i], -1000000, 1000000));
			} else if (arg == "--max-stealth" && has_value) {
				max_stealth = static_cast<stealth>(parse_integer(argv[++i], 1, 1000000));
			} else if (arg == "--max-turns" && has_value) {
				max_turns = static_cast<long>(parse_integer(argv[++i], 0, 1000000000));
			} else if (arg == "--seed" && has_value) {
				seed = static_cast<unsigned>(parse_integer(argv[++i], 0, 4294967295LL));
			} else if (arg == "--light") {
				light = true;
			} else if (arg == "--quiet") {
				quiet = true;
			} else {
				std::cerr << usage << std::endl;
				return 1;
			}
		} catch (const std::invalid_argument &e) {
			std::cerr << "error: " << arg << " " << e.what() << std::endl;
			return 1;
		}
	}
	if (files.deck_paths.empty()) {
		files.deck_paths.push_back(default_deck);
	}
	for (const auto &entry : seats) {
		if (entry.first >= players) {
			std::cerr << "error: --seat " << entry.first << ": there are only "
			          << players << " seats (see --players)" << std::endl;
			return 1;
		}
	}

	std::ios::sync_with_stdio(false);
	protocol_writer writer(std::cout, !light);
	game g(max_stealth, seed, popularity);
	text_logger logger(std::cerr);
	if (!quiet) {
		std::cerr << "seed: " << seed << std::endl;
		g.add_observer(&logger);
	}

	try {
		std::vector<std::unique_ptr<controller>> controllers;
		for (size_t seat = 0; seat < players; ++seat) {
			seat_spec spec = seats.count(seat) ? seats[seat] : seat_spec();
			if (spec.strategy == "remote") {
				controllers.push_back(std::make_unique<remote_controller>(writer, std::cin));
			} else if (spec.strategy == "random") {
				controllers.push_back(std::make_unique<announced_controller>(
					std::make_unique<random_player>(spec.greed), writer));
			} else {
				controllers.push_back(std::make_unique<announced_controller>(
					std::make_unique<smart_player>(spec.greed), writer));
			}
		}
		prepare_game(g, std::move(controllers), files);
	} catch (const std::exception &e) {
		std::cerr << "error: " << e.what() << std::endl;
		return 1;
	}

	protocol_announcer announcer(writer);
	g.add_observer(&announcer);
	try {
		announcer.announce_setup(g);
		bool finished = g.play_game(max_turns);
		announcer.announce_game_over(g, finished);
	} catch (const protocol_abort &e) {
		writer.write({{"type", "game_aborted"}, {"reason", e.what()}, {"seat", e.seat}});
		if (!quiet) {
			std::cerr << "game aborted: " << e.what() << std::endl;
		}
		return e.exit_code;
	}
	return 0;
}
