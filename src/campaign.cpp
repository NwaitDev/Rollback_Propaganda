#include "../include/campaign.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <nlohmann/json.hpp>

std::string type_name(game_type type){
	return (type == game_type::RANDOM_ONLY) ? "random_only" : "with_smart";
}


// --- experiment file -----------------------------------------------------------

namespace {

using json = nlohmann::ordered_json;

long long integer_field(const json& j, const std::string& key, long long min,
                        const std::string& path){
	if (!j.contains(key)) {
		throw std::runtime_error(path + ": missing \"" + key + "\"");
	}
	if (!j[key].is_number_integer() || j[key].get<long long>() < min) {
		throw std::runtime_error(path + ": \"" + key + "\" must be an integer >= "
		                         + std::to_string(min));
	}
	return j[key].get<long long>();
}

long long optional_integer(const json& j, const std::string& key, long long min,
                           long long default_value, const std::string& path){
	return j.contains(key) ? integer_field(j, key, min, path) : default_value;
}

std::string string_field(const json& j, const std::string& key, const std::string& path){
	if (!j.contains(key) || !j[key].is_string()) {
		throw std::runtime_error(path + ": missing \"" + key + "\" (a string)");
	}
	return j[key].get<std::string>();
}

// a misspelled key would silently keep a default value: refuse it instead
void check_keys(const json& j, const std::set<std::string>& allowed, const std::string& where){
	for (const auto& entry : j.items()) {
		if (allowed.count(entry.key()) == 0) {
			throw std::runtime_error(where + ": unknown key \"" + entry.key() + "\"");
		}
	}
}

}

experiment load_experiment(const std::string& path){
	std::ifstream file(path);
	if (!file) {
		throw std::runtime_error("cannot open file " + path);
	}
	json j;
	try {
		file >> j;
	} catch (const json::parse_error& e) {
		throw std::runtime_error(path + ": invalid JSON (" + e.what() + ")");
	}
	if (!j.is_object()) {
		throw std::runtime_error(path + ": an experiment must be an object");
	}
	check_keys(j, {"name", "seed", "games", "players", "random_greed", "smart_greed",
	               "hand_size", "deck", "events", "contracts", "max_stealth",
	               "popularity", "max_turns", "output"}, path);
	if (!j.contains("games") || !j["games"].is_object()) {
		throw std::runtime_error(path + ": missing \"games\" ({ \"random_only\": n, \"with_smart\": n })");
	}
	check_keys(j["games"], {"random_only", "with_smart"}, path + ", \"games\"");

	experiment exp;
	exp.name = string_field(j, "name", path);
	exp.seed = static_cast<unsigned>(integer_field(j, "seed", 0, path));
	exp.random_only_games = integer_field(j["games"], "random_only", 0, path + ", \"games\"");
	exp.with_smart_games = integer_field(j["games"], "with_smart", 0, path + ", \"games\"");
	exp.players = integer_field(j, "players", 2, path);
	exp.random_greed = integer_field(j, "random_greed", 0, path);
	exp.smart_greed = optional_integer(j, "smart_greed", 0, exp.random_greed, path);
	exp.hand_size = integer_field(j, "hand_size", 1, path);
	exp.max_stealth = optional_integer(j, "max_stealth", 0, 21, path);
	exp.popularity = optional_integer(j, "popularity", -1000000, 0, path);
	exp.max_turns = optional_integer(j, "max_turns", 1, 1000, path);
	exp.output = j.contains("output") ? string_field(j, "output", path) : "results/" + exp.name;
	exp.deck_path = string_field(j, "deck", path);
	exp.events_path = string_field(j, "events", path);
	exp.contracts_path = string_field(j, "contracts", path);

	starting_deck deck = load_deck(exp.deck_path);
	exp.deck_name = deck.name;
	for (const card_ptr& card : deck.cards) {
		exp.deck_cards.push_back(card->name());
	}
	exp.events = load_events(exp.events_path);
	exp.contracts = load_contracts(exp.contracts_path);
	if (exp.contracts.size() < exp.players) {
		throw std::runtime_error(path + ": " + exp.contracts_path + " holds "
		                         + std::to_string(exp.contracts.size()) + " contracts for "
		                         + std::to_string(exp.players) + " players");
	}
	return exp;
}


// --- games ---------------------------------------------------------------------

namespace {

// silences std::cout while it exists (the game prints every step)
struct silence {
	std::streambuf* saved;
	silence() : saved(std::cout.rdbuf(nullptr)) {}
	~silence() { std::cout.rdbuf(saved); std::cout.clear(); }
};

}

std::vector<unsigned> game_seeds(const experiment& exp){
	std::mt19937 campaign_rng(exp.seed);
	std::vector<unsigned> seeds;
	for (long i = 0; i < exp.random_only_games + exp.with_smart_games; ++i) {
		seeds.push_back(campaign_rng());
	}
	return seeds;
}

game_result run_game(const experiment& exp, game_type type, long index,
                     unsigned seed, bool verbose){
	std::unique_ptr<silence> quiet = verbose ? nullptr : std::make_unique<silence>();

	game_result result;
	result.type = type;
	result.index = index;
	result.seed = seed;
	// the smart player changes seat at every game, to cancel the advantage
	// of a seat
	result.smart_seat = (type == game_type::WITH_SMART)
		? static_cast<long>(index % exp.players) : -1;

	game g(exp.max_stealth, seed, exp.popularity);
	for (size_t seat = 0; seat < exp.players; ++seat) {
		if (static_cast<long>(seat) == result.smart_seat) {
			g.add_player(std::make_unique<smart_player>(exp.smart_greed));
			result.strategies.push_back("smart");
		} else {
			g.add_player(std::make_unique<random_player>(exp.random_greed));
			result.strategies.push_back("random");
		}
	}
	for (std::unique_ptr<player>& p : g.get_players_ref()) {
		starting_deck deck;
		deck.name = exp.deck_name;
		for (const std::string& card : exp.deck_cards) {
			deck.cards.push_back(make_card(card));
		}
		p->give_starting_deck(std::move(deck));
		p->set_hand_size(exp.hand_size);
	}
	for (const event& e : exp.events) {
		g.add_event(e);
	}
	g.set_contract_pile(exp.contracts);

	g.shuffle();
	g.setup();
	if (verbose) {
		g.view();
	}
	result.finished = g.play_game(exp.max_turns);
	result.turns = g.get_elapsed_turns();
	result.winner = result.finished ? static_cast<long>(g.determine_winner()) : -1;
	for (std::unique_ptr<player>& p : g.get_players_ref()) {
		result.contracts.push_back(p->get_contract());
		result.points.push_back(p->get_score());
	}
	result.popularity = g.get_scores_ref();
	if (verbose) {
		for (std::unique_ptr<player>& p : g.get_players_ref()) {
			p->view(); std::cout<<std::endl;
		}
		if (result.finished) {
			std::cout<<"winner: player n°"<<result.winner<<std::endl;
		}
	}
	return result;
}


// --- campaign ------------------------------------------------------------------

namespace {

const char* faction_name(faction f){
	switch (f) {
		case RED: return "RED";
		case YELLOW: return "YELLOW";
		case BLUE: return "BLUE";
		case ANY: return "ANY";
		case NONE: return "NONE";
	}
	return "?";
}

void write_csv_header(std::ostream& csv, size_t players){
	csv<<"type,index,seed,smart_seat,finished,turns,winner,winner_strategy";
	for (size_t s = 0; s < players; ++s) csv<<",strategy_"<<s;
	for (size_t s = 0; s < players; ++s) csv<<",contract_"<<s;
	for (size_t s = 0; s < players; ++s) csv<<",points_"<<s;
	csv<<",popularity_RED,popularity_YELLOW,popularity_BLUE\n";
}

void write_csv_line(std::ostream& csv, const game_result& r){
	csv<<type_name(r.type)<<","<<r.index<<","<<r.seed<<","<<r.smart_seat<<","
	   <<(r.finished ? 1 : 0)<<","<<r.turns<<","<<r.winner<<","
	   <<(r.winner >= 0 ? r.strategies[r.winner] : "");
	for (const std::string& s : r.strategies) csv<<","<<s;
	for (faction f : r.contracts) csv<<","<<faction_name(f);
	for (int p : r.points) csv<<","<<p;
	for (int p : r.popularity) csv<<","<<p;
	csv<<"\n";
}

struct type_summary {
	long games = 0, blocked = 0, turns = 0;
	long smart_wins = 0;
	std::vector<long> seat_wins;
};

}

void run_campaign(const experiment& exp, const std::string& experiment_path){
	std::filesystem::create_directories(exp.output);
	// the experiment is kept with its results, to know how they were obtained
	std::filesystem::copy_file(experiment_path, exp.output + "/experiment.json",
	                           std::filesystem::copy_options::overwrite_existing);
	std::ofstream csv(exp.output + "/games.csv");
	if (!csv) {
		throw std::runtime_error("cannot write " + exp.output + "/games.csv");
	}
	write_csv_header(csv, exp.players);

	std::vector<unsigned> seeds = game_seeds(exp);
	long total = exp.random_only_games + exp.with_smart_games;
	long played = 0;
	std::map<game_type, type_summary> summaries;
	for (game_type type : {game_type::RANDOM_ONLY, game_type::WITH_SMART}) {
		long count = (type == game_type::RANDOM_ONLY) ? exp.random_only_games : exp.with_smart_games;
		type_summary& summary = summaries[type];
		summary.seat_wins.assign(exp.players, 0);
		for (long index = 0; index < count; ++index) {
			game_result r = run_game(exp, type, index, seeds[played], false);
			write_csv_line(csv, r);
			summary.games++;
			summary.turns += r.turns;
			if (!r.finished) {
				summary.blocked++;
			} else {
				summary.seat_wins[r.winner]++;
				if (r.winner == r.smart_seat) summary.smart_wins++;
			}
			played++;
			if (total >= 10 && played % (total / 10) == 0) {
				std::cerr<<"\r"<<(100 * played / total)<<" %"<<std::flush;
			}
		}
	}
	std::cerr<<"\r";

	std::cout<<std::fixed<<std::setprecision(1);
	std::cout<<"campaign \""<<exp.name<<"\": "<<exp.players<<" players, deck "
	         <<exp.deck_name<<", hand "<<exp.hand_size<<", greed "<<exp.random_greed
	         <<" (smart "<<exp.smart_greed<<")"<<std::endl;
	for (const auto& [type, s] : summaries) {
		if (s.games == 0) continue;
		long finished = s.games - s.blocked;
		std::cout<<"  "<<type_name(type)<<": "<<s.games<<" games, "<<s.blocked<<" blocked, "
		         <<(double)s.turns / s.games<<" turns on average"<<std::endl;
		if (finished == 0) continue;
		std::cout<<"    wins by seat:";
		for (size_t seat = 0; seat < s.seat_wins.size(); ++seat) {
			std::cout<<" "<<seat<<": "<<100.0 * s.seat_wins[seat] / finished<<" %";
		}
		std::cout<<std::endl;
		if (type == game_type::WITH_SMART) {
			std::cout<<"    smart player wins "<<100.0 * s.smart_wins / finished
			         <<" % of the finished games (even chances: "
			         <<100.0 / exp.players<<" %)"<<std::endl;
		}
	}
	std::cout<<"results: "<<exp.output<<"/games.csv"<<std::endl;
}
