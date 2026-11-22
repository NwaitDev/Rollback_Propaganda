#include "../include/campaign.hpp"
#include "../include/text_view.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
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
	               "popularity", "max_turns", "output", "extra_card"}, path);
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
	// random_greed: one level, or a list of levels
	if (j.contains("random_greed") && j["random_greed"].is_array()) {
		for (const auto& level : j["random_greed"]) {
			if (!level.is_number_integer() || level.get<long long>() < 0) {
				throw std::runtime_error(path + ": \"random_greed\" must be an integer >= 0 "
				                         "or a list of them");
			}
			stealth greed = level.get<stealth>();
			if (std::find(exp.random_greeds.begin(), exp.random_greeds.end(), greed)
			    != exp.random_greeds.end()) {
				throw std::runtime_error(path + ": \"random_greed\" lists "
				                         + std::to_string(greed) + " twice");
			}
			exp.random_greeds.push_back(greed);
		}
		if (exp.random_greeds.empty()) {
			throw std::runtime_error(path + ": \"random_greed\" is an empty list");
		}
	} else {
		exp.random_greeds.push_back(integer_field(j, "random_greed", 0, path));
	}
	exp.smart_greed_given = j.contains("smart_greed");
	exp.smart_greed_fixed = optional_integer(j, "smart_greed", 0, 0, path);
	exp.hand_size = integer_field(j, "hand_size", 1, path);
	exp.max_stealth = optional_integer(j, "max_stealth", 0, 21, path);
	exp.popularity = optional_integer(j, "popularity", -1000000, 0, path);
	exp.max_turns = optional_integer(j, "max_turns", 1, 1000, path);
	exp.output = j.contains("output") ? string_field(j, "output", path) : "results/" + exp.name;
	exp.deck_path = string_field(j, "deck", path);
	exp.events_path = string_field(j, "events", path);
	exp.contracts_path = string_field(j, "contracts", path);
	if (j.contains("extra_card")) {
		exp.extra_card = string_field(j, "extra_card", path);
		if (make_card(exp.extra_card) == nullptr) {
			throw std::runtime_error(path + ": unknown card \"" + exp.extra_card + "\" in \"extra_card\"");
		}
	}

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
	return exp.with_greed(exp.random_greeds.front());
}

experiment experiment::with_greed(stealth greed) const {
	experiment copy = *this;
	copy.random_greed = greed;
	copy.smart_greed = smart_greed_given ? smart_greed_fixed : greed;
	return copy;
}


// --- games ---------------------------------------------------------------------

std::vector<unsigned> game_seeds(const experiment& exp){
	std::mt19937 campaign_rng(exp.seed);
	std::vector<unsigned> seeds;
	for (long i = 0; i < exp.random_only_games + exp.with_smart_games; ++i) {
		seeds.push_back(campaign_rng());
	}
	return seeds;
}


// --- metrics of one game ---------------------------------------------------------

namespace {

// the faction that leads alone, NONE on a tie for the lead
faction leading_faction(const std::vector<int>& popularity){
	faction best = RED;
	bool tied = false;
	for (faction f : {YELLOW, BLUE}) {
		if (popularity[f] > popularity[best]) {
			best = f;
			tied = false;
		} else if (popularity[f] == popularity[best]) {
			tied = true;
		}
	}
	return tied ? NONE : best;
}

// true if this mage has strictly more points than every other one
bool leads_alone(game& g, const player& p){
	for (std::unique_ptr<player>& other : g.get_players_ref()) {
		if (other.get() != &p && other->get_score() >= p.get_score()) {
			return false;
		}
	}
	return true;
}

}

game_stats::game_stats(const std::vector<int>& starting_popularity, size_t players)
: max_popularity(*std::max_element(starting_popularity.begin(), starting_popularity.end())),
  last_turn_behind(players, 0), leader(leading_faction(starting_popularity)) {}

void game_stats::card_schemed(game& g, const player& p, const shenanigan& card){
	cards[card.name()].schemed++;
}

void game_stats::card_kept_in_hand(game& g, const player& p, const shenanigan& card){
	card_stats& c = cards[card.name()];
	(leads_alone(g, p) ? c.dead_leading : c.dead_trailing)++;
}

void game_stats::card_revealed(game& g, const player& p, const shenanigan& card){
	cards[card.name()].revealed++;
}

void game_stats::spell_failed(game& g, const player& p, const shenanigan& card){
	cards[card.name()].failed++;
}

void game_stats::event_resolved(game& g, const event& e){
	events_resolved++;
	if (e.get_kind() == ELECTION) {
		elections++;
	}
	const std::vector<int>& popularity = g.get_scores_ref();
	max_popularity = std::max(max_popularity,
	                          *std::max_element(popularity.begin(), popularity.end()));
	faction now = leading_faction(popularity);
	if (now != leader) {
		lead_changes++;
		leader = now;
	}
}

void game_stats::memory_lapse_received(game& g, const player& caster, const player& target){ memory_lapses++; }
void game_stats::ether_leaked(game& g, const player& caster, const player& target){ ethers_leaked++; }
void game_stats::immunity_triggered(game& g, const player& immune, const player& chooser){ immunities++; }

void game_stats::turn_ended(game& g, turn_outcome outcome){
	if (outcome == turn_outcome::RESOLVED) {
		resolved_turns++;
	} else if (outcome == turn_outcome::STEALTH_ROLLBACK) {
		stealth_rollbacks++;
	}
	std::vector<std::unique_ptr<player>>& players = g.get_players_ref();
	int most = 0;
	for (size_t seat = 0; seat < players.size(); ++seat) {
		most = (seat == 0) ? players[seat]->get_score() : std::max(most, players[seat]->get_score());
	}
	for (size_t seat = 0; seat < players.size(); ++seat) {
		if (players[seat]->get_score() < most) {
			last_turn_behind[seat] = g.get_elapsed_turns();
		}
	}
}


// --- games ---------------------------------------------------------------------

game_result run_game(const experiment& exp, game_type type, long index,
                     unsigned seed, bool verbose){
	game g(exp.max_stealth, seed, exp.popularity);
	// a replayed game is written as ./simulation does
	text_logger logger(std::cout);
	if (verbose) {
		g.add_observer(&logger);
	}
	game_result result(g.get_scores_ref(), exp.players);
	result.type = type;
	result.index = index;
	result.seed = seed;
	result.random_greed = exp.random_greed;
	// the smart player changes seat at every game, to cancel the advantage
	// of a seat; the extra card goes to the smart player when there is one,
	// to a seat that changes at every game otherwise
	long rotating_seat = static_cast<long>(index % exp.players);
	result.smart_seat = (type == game_type::WITH_SMART) ? rotating_seat : -1;
	result.holder_seat = exp.extra_card.empty() ? -1 : rotating_seat;

	for (size_t seat = 0; seat < exp.players; ++seat) {
		if (static_cast<long>(seat) == result.smart_seat) {
			g.add_player(std::make_unique<smart_player>(exp.smart_greed));
			result.strategies.push_back("smart");
		} else {
			g.add_player(std::make_unique<random_player>(exp.random_greed));
			result.strategies.push_back("random");
		}
	}
	std::vector<std::unique_ptr<player>>& players = g.get_players_ref();
	for (size_t seat = 0; seat < exp.players; ++seat) {
		starting_deck deck;
		deck.name = exp.deck_name;
		for (const std::string& card : exp.deck_cards) {
			deck.cards.push_back(make_card(card));
		}
		players[seat]->give_starting_deck(std::move(deck));
		if (static_cast<long>(seat) == result.holder_seat) {
			players[seat]->add_to_grimory(make_card(exp.extra_card));
		}
		players[seat]->set_hand_size(exp.hand_size);
	}
	for (const event& e : exp.events) {
		g.add_event(e);
	}
	g.set_contract_pile(exp.contracts);

	g.shuffle();
	g.setup();
	g.add_observer(&result.stats);
	if (verbose) {
		print_game(std::cout, g);
	}
	result.finished = g.play_game(exp.max_turns);
	g.set_observer(nullptr);
	result.turns = g.get_elapsed_turns();
	result.winner = result.finished ? static_cast<long>(g.determine_winner(&result.how_won)) : -1;
	for (std::unique_ptr<player>& p : players) {
		result.contracts.push_back(p->get_contract().name);
		result.points.push_back(p->get_score());
		result.end_grimory.push_back(p->grimory_size());
		result.end_hand.push_back(p->get_hand().get_nb_cards());
	}
	result.popularity = g.get_scores_ref();
	result.end_cosmos = g.cosmos_cards.size();
	if (verbose) {
		for (std::unique_ptr<player>& p : players) {
			std::cout<<describe(*p)<<std::endl;
		}
		if (result.finished) {
			std::cout<<"winner: player n°"<<result.winner<<std::endl;
		}
	}
	return result;
}


// --- campaign ------------------------------------------------------------------

namespace {

// quotes a text field when it holds a comma or a quote
std::string csv_field(const std::string& text){
	if (text.find_first_of(",\"\n") == std::string::npos) {
		return text;
	}
	std::string quoted = "\"";
	for (char c : text) {
		quoted += (c == '"') ? std::string("\"\"") : std::string(1, c);
	}
	return quoted + "\"";
}

void write_games_header(std::ostream& csv, size_t players){
	csv<<"random_greed,type,index,seed,smart_seat,holder_seat,finished,turns,winner,winner_strategy";
	for (size_t s = 0; s < players; ++s) csv<<",strategy_"<<s;
	for (size_t s = 0; s < players; ++s) csv<<",contract_"<<s;
	for (size_t s = 0; s < players; ++s) csv<<",points_"<<s;
	csv<<",popularity_RED,popularity_YELLOW,popularity_BLUE\n";
}

void write_games_line(std::ostream& csv, const game_result& r){
	csv<<r.random_greed<<","<<type_name(r.type)<<","<<r.index<<","<<r.seed<<","
	   <<r.smart_seat<<","<<r.holder_seat<<","<<(r.finished ? 1 : 0)<<","<<r.turns<<","
	   <<r.winner<<","<<(r.winner >= 0 ? r.strategies[r.winner] : "");
	for (const std::string& s : r.strategies) csv<<","<<s;
	for (const std::string& c : r.contracts) csv<<","<<csv_field(c);
	for (int p : r.points) csv<<","<<p;
	for (int p : r.popularity) csv<<","<<p;
	csv<<"\n";
}

// sum and sum of squares of values, for their mean and standard deviation
struct moments {
	double n = 0, sum = 0, squares = 0;
	void add(double x){ n++; sum += x; squares += x * x; }
	void add(const moments& m){ n += m.n; sum += m.sum; squares += m.squares; }
};

std::string text_of(double value){
	std::ostringstream text;
	text<<std::setprecision(6)<<value;
	return text.str();
}

// mean, or an empty field when there is no value
std::string mean_of(const moments& m){
	return (m.n == 0) ? "" : text_of(m.sum / m.n);
}

// sample standard deviation, or an empty field with less than two values
std::string sd_of(const moments& m){
	if (m.n < 2) {
		return "";
	}
	double variance = (m.squares - m.sum * m.sum / m.n) / (m.n - 1);
	return text_of(std::sqrt(std::max(variance, 0.0)));
}

// a ratio, or an empty field when it is undefined (division by zero)
std::string number(double numerator, double denominator){
	return (denominator == 0) ? "" : text_of(numerator / denominator);
}

struct contract_totals {
	long occurrences = 0, wins = 0;
	moments points;                       // one value per mage holding it
};

struct card_totals {
	long occurrences = 0;                 // copies dealt
	long schemed = 0, revealed = 0, failed = 0;
	// one value per game: turns kept in hand per copy dealt
	moments dead_leading, dead_trailing;
};

// metrics of the finished games of one type at one greed level
struct aggregate {
	long games = 0, finished = 0, blocked = 0;
	long tiebreak_grimory = 0, tiebreak_draw = 0;
	long smart_wins = 0, holder_wins = 0;
	long all_turns = 0, all_resolved_turns = 0, stealth_rollbacks = 0, events_resolved = 0;
	// one value per finished game (per mage for end_grimory and end_hand)
	moments turns, resolved_turns, elections, lead_changes, max_popularity;
	moments decided_turn, decided_ratio, final_margin;
	moments end_grimory, end_hand, end_cosmos;
	moments memory_lapses, ethers_leaked, immunities;
	std::vector<int> max_popularities;    // for the percentiles
	std::vector<long> seat_wins;
	std::map<std::string, contract_totals> contracts;
	std::map<std::string, card_totals> cards;

	void add(const experiment& exp, const game_result& r){
		games++;
		if (!r.finished) {
			blocked++;
			return;
		}
		finished++;
		if (r.how_won == tiebreak::GRIMORY) tiebreak_grimory++;
		if (r.how_won == tiebreak::DRAW) tiebreak_draw++;
		if (r.winner == r.smart_seat) smart_wins++;
		if (r.winner == r.holder_seat) holder_wins++;
		const game_stats& s = r.stats;
		all_turns += r.turns;
		all_resolved_turns += s.resolved_turns;
		stealth_rollbacks += s.stealth_rollbacks;
		events_resolved += s.events_resolved;
		turns.add(r.turns);
		resolved_turns.add(s.resolved_turns);
		elections.add(s.elections);
		lead_changes.add(s.lead_changes);
		max_popularity.add(s.max_popularity);
		max_popularities.push_back(s.max_popularity);
		// the turn from which the winner keeps the most points for good
		long last_behind = s.last_turn_behind[r.winner];
		long decided = (last_behind == 0) ? 0 : last_behind + 1;
		decided_turn.add(decided);
		decided_ratio.add((r.turns > 0) ? static_cast<double>(decided) / r.turns : 0.0);
		std::vector<int> sorted = r.points;
		std::sort(sorted.rbegin(), sorted.rend());
		final_margin.add((sorted.size() > 1) ? sorted[0] - sorted[1] : 0);
		for (size_t seat = 0; seat < r.points.size(); ++seat) {
			end_grimory.add(r.end_grimory[seat]);
			end_hand.add(r.end_hand[seat]);
		}
		end_cosmos.add(r.end_cosmos);
		memory_lapses.add(s.memory_lapses);
		ethers_leaked.add(s.ethers_leaked);
		immunities.add(s.immunities);

		seat_wins.resize(exp.players, 0);
		seat_wins[r.winner]++;
		for (size_t seat = 0; seat < r.points.size(); ++seat) {
			contract_totals& c = contracts[r.contracts[seat]];
			c.occurrences++;
			c.points.add(r.points[seat]);
			if (static_cast<long>(seat) == r.winner) c.wins++;
		}
		// copies of each card dealt in this game
		std::map<std::string, long> copies;
		for (const std::string& card : exp.deck_cards) {
			copies[card] += static_cast<long>(exp.players);
		}
		if (!exp.extra_card.empty()) {
			copies[exp.extra_card]++;
		}
		for (const auto& [name, n] : copies) {
			card_totals& t = cards[name];
			t.occurrences += n;
			auto played = s.cards.find(name);
			card_stats c = (played == s.cards.end()) ? card_stats() : played->second;
			t.schemed += c.schemed;
			t.revealed += c.revealed;
			t.failed += c.failed;
			t.dead_leading.add(static_cast<double>(c.dead_leading) / n);
			t.dead_trailing.add(static_cast<double>(c.dead_trailing) / n);
		}
		// a card can reach a game without being dealt (none for now): keep it
		for (const auto& [name, c] : s.cards) {
			if (copies.count(name) == 0) {
				card_totals& t = cards[name];
				t.schemed += c.schemed;
				t.revealed += c.revealed;
				t.failed += c.failed;
			}
		}
	}
};

// nearest-rank percentile of the values (not empty)
int percentile(std::vector<int> values, int p){
	std::sort(values.begin(), values.end());
	size_t rank = static_cast<size_t>(std::ceil(p / 100.0 * values.size()));
	return values[std::max<size_t>(rank, 1) - 1];
}

const char* campaigns_header =
	"campaign,game_type,games,seed,players,random_greed,smart_greed,hand_size,deck,events,"
	"contracts,extra_card,max_stealth,popularity,max_turns,finished_games,blocked_games,"
	"tiebreak_grimory_games,tiebreak_draw_games,smart_wins,holder_wins,turns_mean,turns_sd,"
	"resolved_turns_mean,resolved_turns_sd,elections_resolved_mean,elections_resolved_sd,"
	"stealth_rollbacks_per_turn,events_per_resolved_turn,lead_changes_mean,lead_changes_sd,"
	"max_popularity_mean,max_popularity_sd,max_popularity_p10,max_popularity_p50,"
	"max_popularity_p90,decided_turn_mean,decided_turn_sd,decided_turn_ratio_mean,"
	"decided_turn_ratio_sd,final_margin_mean,final_margin_sd,end_grimory_mean,end_grimory_sd,"
	"end_hand_mean,end_hand_sd,end_cosmos_mean,end_cosmos_sd,memory_lapses_received_mean,"
	"memory_lapses_received_sd,ethers_leaked_mean,ethers_leaked_sd,"
	"immunities_triggered_mean,immunities_triggered_sd\n";

const char* details_header =
	"campaign,random_greed,game_type,kind,item,occurrences,wins,points_mean,points_sd,"
	"schemed,revealed,failed,dead_turns_leading_mean,dead_turns_leading_sd,"
	"dead_turns_trailing_mean,dead_turns_trailing_sd\n";

void write_campaigns_line(std::ostream& csv, const experiment& exp, game_type type,
                          const aggregate& a){
	auto both = [](const moments& m) { return mean_of(m) + "," + sd_of(m); };
	csv<<csv_field(exp.name)<<","<<type_name(type)<<","<<a.games<<","<<exp.seed<<","
	   <<exp.players<<","<<exp.random_greed<<","
	   <<((type == game_type::WITH_SMART) ? std::to_string(exp.smart_greed) : "")<<","
	   <<exp.hand_size<<","<<csv_field(exp.deck_path)<<","<<csv_field(exp.events_path)<<","
	   <<csv_field(exp.contracts_path)<<","<<csv_field(exp.extra_card)<<","
	   <<exp.max_stealth<<","<<exp.popularity<<","<<exp.max_turns<<","
	   <<a.finished<<","<<a.blocked<<","<<a.tiebreak_grimory<<","<<a.tiebreak_draw<<","
	   <<((type == game_type::WITH_SMART) ? std::to_string(a.smart_wins) : "")<<","
	   <<(exp.extra_card.empty() ? "" : std::to_string(a.holder_wins))<<","
	   <<both(a.turns)<<","<<both(a.resolved_turns)<<","<<both(a.elections)<<","
	   <<number(a.stealth_rollbacks, a.all_turns)<<","
	   <<number(a.events_resolved, a.all_resolved_turns)<<","
	   <<both(a.lead_changes)<<","<<both(a.max_popularity)<<",";
	if (a.finished > 0) {
		csv<<percentile(a.max_popularities, 10)<<","<<percentile(a.max_popularities, 50)<<","
		   <<percentile(a.max_popularities, 90)<<",";
	} else {
		csv<<",,,";
	}
	csv<<both(a.decided_turn)<<","<<both(a.decided_ratio)<<","<<both(a.final_margin)<<","
	   <<both(a.end_grimory)<<","<<both(a.end_hand)<<","<<both(a.end_cosmos)<<","
	   <<both(a.memory_lapses)<<","<<both(a.ethers_leaked)<<","<<both(a.immunities)<<"\n";
}

void write_details_lines(std::ostream& csv, const experiment& exp, game_type type,
                         const aggregate& a){
	std::string key = csv_field(exp.name) + "," + std::to_string(exp.random_greed) + ","
		+ type_name(type);
	for (size_t seat = 0; seat < a.seat_wins.size(); ++seat) {
		csv<<key<<",seat,"<<seat<<","<<a.finished<<","<<a.seat_wins[seat]<<",,,,,,,,,\n";
	}
	for (const auto& [name, c] : a.contracts) {
		csv<<key<<",contract,"<<csv_field(name)<<","<<c.occurrences<<","<<c.wins<<","
		   <<mean_of(c.points)<<","<<sd_of(c.points)<<",,,,,,,\n";
	}
	for (const auto& [name, c] : a.cards) {
		csv<<key<<",card,"<<csv_field(name)<<","<<c.occurrences<<",,,,"<<c.schemed<<","
		   <<c.revealed<<","<<((name == "ETHER") ? "" : std::to_string(c.failed))<<","
		   <<mean_of(c.dead_leading)<<","<<sd_of(c.dead_leading)<<","
		   <<mean_of(c.dead_trailing)<<","<<sd_of(c.dead_trailing)<<"\n";
	}
}

std::ofstream open_output(const std::string& path){
	std::ofstream file(path);
	if (!file) {
		throw std::runtime_error("cannot write " + path);
	}
	return file;
}

}

void run_campaign(const experiment& exp, const std::string& experiment_path){
	std::filesystem::create_directories(exp.output);
	// the experiment is kept with its results, to know how they were obtained
	std::filesystem::copy_file(experiment_path, exp.output + "/experiment.json",
	                           std::filesystem::copy_options::overwrite_existing);
	std::ofstream games_csv = open_output(exp.output + "/games.csv");
	std::ofstream campaigns_csv = open_output(exp.output + "/campaigns.csv");
	std::ofstream details_csv = open_output(exp.output + "/details.csv");
	write_games_header(games_csv, exp.players);
	campaigns_csv<<campaigns_header;
	details_csv<<details_header;

	std::vector<unsigned> seeds = game_seeds(exp);
	long total = (exp.random_only_games + exp.with_smart_games)
		* static_cast<long>(exp.random_greeds.size());
	long played = 0;
	std::cout<<std::fixed<<std::setprecision(1);
	std::cout<<"campaign \""<<exp.name<<"\": "<<exp.players<<" players, deck "
	         <<exp.deck_name<<", hand "<<exp.hand_size
	         <<(exp.extra_card.empty() ? "" : ", extra card " + exp.extra_card)<<std::endl;
	for (stealth greed : exp.random_greeds) {
		experiment level = exp.with_greed(greed);
		std::cout<<"  greed "<<level.random_greed<<" (smart "<<level.smart_greed<<")"<<std::endl;
		long seed_index = 0;
		for (game_type type : {game_type::RANDOM_ONLY, game_type::WITH_SMART}) {
			long count = (type == game_type::RANDOM_ONLY) ? exp.random_only_games : exp.with_smart_games;
			aggregate a;
			for (long index = 0; index < count; ++index) {
				game_result r = run_game(level, type, index, seeds[seed_index++], false);
				write_games_line(games_csv, r);
				a.add(level, r);
				played++;
				if (total >= 10 && played % (total / 10) == 0) {
					std::cerr<<"\r"<<(100 * played / total)<<" %"<<std::flush;
				}
			}
			if (count == 0) {
				continue;
			}
			write_campaigns_line(campaigns_csv, level, type, a);
			write_details_lines(details_csv, level, type, a);

			std::cerr<<"\r";
			std::cout<<"    "<<type_name(type)<<": "<<a.games<<" games, "<<a.blocked<<" blocked";
			if (a.finished > 0) {
				std::cout<<", "<<(double)a.all_turns / a.finished<<" turns on average";
				if (type == game_type::WITH_SMART) {
					std::cout<<", smart player wins "<<100.0 * a.smart_wins / a.finished<<" %";
				}
				if (!exp.extra_card.empty()) {
					std::cout<<", holder of the extra card wins "<<100.0 * a.holder_wins / a.finished<<" %";
				}
			}
			std::cout<<std::endl;
		}
	}
	std::cerr<<"\r";
	std::cout<<"results: "<<exp.output<<"/ (games.csv, campaigns.csv, details.csv)"<<std::endl;
}
