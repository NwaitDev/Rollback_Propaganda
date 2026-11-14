#include "../include/campaign.hpp"
#include <algorithm>
#include <exception>
#include <iostream>
#include <string>

// usage: campaign --cards
//        campaign <experiment.json>
//        campaign <experiment.json> --replay <random_only|with_smart> <index> [<greed>]
// --cards lists the card names a deck or \"extra_card\" can use.
// The first form with an experiment plays every game of the experiment and writes the results.
// The second one replays a single game of the campaign, printing every step;
// <greed> picks the greed level when the experiment lists several.
int main(int argc, char* argv[]){
	const std::string usage = std::string("usage: ") + argv[0] + " <experiment.json>"
		+ " [--replay <random_only|with_smart> <index> [<greed>]]";
	if (argc == 2 && std::string(argv[1]) == "--cards") {
		for (const std::string& name : card_names()) {
			std::cout<<name<<std::endl;
		}
		return 0;
	}
	if (argc != 2 && argc != 5 && argc != 6) {
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
		stealth greed = exp.random_greeds.front();
		if (argc == 6) {
			greed = std::stoi(argv[5]);
		} else if (exp.random_greeds.size() > 1) {
			std::cerr<<"error: the campaign has several greed levels; give one of them"<<std::endl;
			return 1;
		}
		if (std::find(exp.random_greeds.begin(), exp.random_greeds.end(), greed) == exp.random_greeds.end()) {
			std::cerr<<"error: the campaign has no greed level "<<greed<<std::endl;
			return 1;
		}
		long position = (t == game_type::RANDOM_ONLY) ? index : exp.random_only_games + index;
		unsigned seed = game_seeds(exp)[position];
		std::cout<<"replaying "<<type<<" game n°"<<index<<" at greed "<<greed
		         <<" (seed "<<seed<<")"<<std::endl;
		run_game(exp.with_greed(greed), t, index, seed, true);
	} catch (const std::exception& e) {
		std::cerr<<"error: "<<e.what()<<std::endl;
		return 1;
	}
	return 0;
}
