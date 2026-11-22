#include "../include/game_concepts.hpp"
#include "../include/protocol.hpp"
#include "../include/setup.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// A player of the simulation (random_player or smart_player) playing one seat
// through the protocol (see docs/protocol.md): it reads every message of the
// engine on its standard input, and answers the questions of its seat on its
// standard output. Its random draws are seeded as in the engine, so that it
// plays exactly as the same strategy would inside ./simulation or ./server.
//
// usage: bot --seat <k> [--strategy random|smart] [--greed <n>]
//   --strategy   random by default
//   --greed      10 by default
//
// The bot decides from the "state" of each question, like the strategies do
// inside the engine: they only read what their mage may know (their hand and
// contract, the faces up, the sizes of the other hands and grimories, and
// the cards of the future they saw with Reading).

namespace {

// the answer of `c` to a question, as an index in its options
size_t answer(controller &c, const json &ask, size_t seat) {
	game g = state_codec::rebuild(ask.at("state"));
	decision d{g, g.player_at(seat), seat, c.rng()};
	const std::string q = ask.at("question").get<std::string>();
	const json &context = ask.at("context");
	const json &options = ask.at("options");

	if (q == "reveal_more") {
		return c.reveal_more(d) ? 0 : 1;
	}
	if (q == "scheme_card") {
		return c.scheme_card(d);
	}
	if (q == "next_shenanigan") {
		return c.next_shenanigan(d);
	}
	if (q == "reveal_shenanigan") {
		card_ptr card = card_from_json(context.at("card"));
		return c.reveal_shenanigan(d, *card) ? 0 : 1;
	}
	if (q == "ether_to_spend") {
		card_ptr card = card_from_json(context.at("card"));
		return c.ether_to_spend(d, dynamic_cast<const spell &>(*card)); // option i is i ether
	}
	if (q == "propaganda_target") {
		std::vector<size_t> present;
		for (const json &o : options) {
			present.push_back(o.at("present").get<size_t>());
		}
		return c.propaganda_target(d, context.at("modification").get<int>(), present);
	}
	if (q == "opponent_target") {
		std::vector<size_t> seats;
		for (const json &o : options) {
			seats.push_back(o.at("seat").get<size_t>());
		}
		return c.opponent_target(d, seats);
	}
	if (q == "cosmos_card") {
		return c.cosmos_card(d, context.at("remaining").get<unsigned>());
	}
	if (q == "ether_leak_target") {
		std::vector<size_t> seats;
		for (const json &o : options) {
			if (o.is_object()) {
				seats.push_back(o.at("seat").get<size_t>());
			}
		}
		return c.ether_leak_target(d, seats, context.at("remaining").get<unsigned>());
	}
	if (q == "divergence_branch") {
		return c.divergence_branch(d, state_codec::event_from_json(context.at("event")));
	}
	if (q == "any_faction") {
		std::vector<faction> allowed;
		for (const json &o : options) {
			allowed.push_back(faction_from_name(o.get<std::string>()));
		}
		return c.any_faction(d, context.at("value").get<int>(), allowed);
	}
	throw std::runtime_error("unknown question: " + q);
}

} // namespace

int main(int argc, char *argv[]) {
	const std::string usage = std::string("usage: ") + argv[0]
		+ " --seat <k> [--strategy random|smart] [--greed <n>]";
	long seat = -1;
	std::string strategy = "random";
	stealth greed = 10;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		bool has_value = (i + 1 < argc);
		try {
			if (arg == "--seat" && has_value) {
				seat = static_cast<long>(parse_integer(argv[++i], 0, 63));
			} else if (arg == "--strategy" && has_value) {
				strategy = argv[++i];
				if (strategy != "random" && strategy != "smart") {
					throw std::invalid_argument("expects random or smart");
				}
			} else if (arg == "--greed" && has_value) {
				greed = static_cast<stealth>(parse_integer(argv[++i], 0, 1000));
			} else {
				std::cerr << usage << std::endl;
				return 1;
			}
		} catch (const std::invalid_argument &e) {
			std::cerr << "error: " << arg << " " << e.what() << std::endl;
			return 1;
		}
	}
	if (seat < 0) {
		std::cerr << usage << std::endl;
		return 1;
	}

	std::unique_ptr<controller> me;
	if (strategy == "smart") {
		me = std::make_unique<smart_player>(greed);
	} else {
		me = std::make_unique<random_player>(greed);
	}

	std::ios::sync_with_stdio(false);
	std::string line;
	try {
		while (std::getline(std::cin, line)) {
			json message = json::parse(line);
			const std::string type = message.at("type").get<std::string>();
			if (type == "game_start") {
				// the same generator as this seat's controller in the engine
				me->seed_for(message.at("seed").get<unsigned>(), static_cast<size_t>(seat));
			} else if (type == "ask" && message.at("seat").get<long>() == seat) {
				size_t choice = answer(*me, message, static_cast<size_t>(seat));
				json reply = {{"seat", seat}, {"id", message.at("id")}, {"choice", choice}};
				std::cout << reply.dump() << '\n';
				std::cout.flush();
			} else if (type == "game_over" || type == "game_aborted") {
				return 0;
			}
		}
	} catch (const std::exception &e) {
		std::cerr << "bot " << seat << ": " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
