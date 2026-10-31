#include "../include/campaign.hpp"
#include <exception>
#include <iostream>
#include <string>

// usage: campaign <experiment.json>
//        campaign <experiment.json> --replay <random_only|with_smart> <index>
// The first form plays every game of the experiment and writes the results.
// The second one replays a single game of the campaign, printing every step.
int main(int argc, char* argv[]){
	const std::string usage = std::string("usage: ") + argv[0] + " <experiment.json>"
		+ " [--replay <random_only|with_smart> <index>]";
	if (argc != 2 && argc != 5) {
		std::cerr<<usage<<std::endl;
		return 1;
	}
	try {
		experiment exp = load_experiment(argv[1]);
		if (argc == 2) {
			run_campaign(exp, argv[1]);
			return 0;
		}

		std::string type = argv[3];
		if (std::string(argv[2]) != "--replay" || (type != "random_only" && type != "with_smart")) {
			std::cerr<<usage<<std::endl;
			return 1;
		}
		long index = std::stol(argv[4]);
		game_type t = (type == "random_only") ? game_type::RANDOM_ONLY : game_type::WITH_SMART;
		long count = (t == game_type::RANDOM_ONLY) ? exp.random_only_games : exp.with_smart_games;
		if (index < 0 || index >= count) {
			std::cerr<<"error: the campaign has "<<count<<" "<<type<<" games"<<std::endl;
			return 1;
		}
		long position = (t == game_type::RANDOM_ONLY) ? index : exp.random_only_games + index;
		unsigned seed = game_seeds(exp)[position];
		std::cout<<"replaying "<<type<<" game n°"<<index<<" (seed "<<seed<<")"<<std::endl;
		run_game(exp, t, index, seed, true);
	} catch (const std::exception& e) {
		std::cerr<<"error: "<<e.what()<<std::endl;
		return 1;
	}
	return 0;
}
