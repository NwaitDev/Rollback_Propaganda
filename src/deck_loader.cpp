#include "../include/game_concepts.hpp"
#include <fstream>
#include <map>
#include <stdexcept>
#include <utility>
#include <nlohmann/json.hpp>

// Card sets are JSON documents listing how many copies of each card a set
// holds:
//   decks/*.json      { "name": "implemented", "cards": { "ETHER": 6, ... } }
//                     card names: those registered in make_card()
//                     (shenanigans.cpp)
//   contracts/*.json  { "name": "two_per_faction",
//                       "contracts": { "RED": 2, "YELLOW": 2, "BLUE": 2 } }
//
// Event sets list their cards with a number of copies:
//   events/*.json     { "name": "base", "events": [
//                         { "count": 10, "kind": "ELECTION", "stealth": 3 },
//                         { "count": 10, "kind": "PROPAGANDA",
//                           "faction": "RED", "value": 2, "stealth": 4 },
//                         { "count": 7, "kind": "AND",   (or "OR")
//                           "left":  { "faction": "ANY", "value": 1, "stealth": 1 },
//                           "right": { "faction": "RED", "value": 2, "stealth": 2 } },
//                         { "count": 5, "kind": "MISTAKE", "value": 2 } ] }
//   kinds: PROPAGANDA (faction RED, YELLOW, BLUE or ANY; value; stealth),
//          AND / OR (left and right: events; a branch without "kind" is a
//          propaganda), ELECTION, COLLAPSE, EXPLOSION, MISTAKE (stealth and
//          value are optional, 0 by default)

namespace {

nlohmann::ordered_json read_json_file(const std::string& path){
	std::ifstream file(path);
	if (!file) {
		throw std::runtime_error("cannot open file " + path);
	}
	nlohmann::ordered_json json;
	try {
		file >> json;
	} catch (const nlohmann::json::parse_error& e) {
		throw std::runtime_error(path + ": invalid JSON (" + e.what() + ")");
	}
	return json;
}

struct card_counts {
	std::string name;
	std::vector<std::pair<std::string, unsigned>> counts; // in file order
};

// reads { "name": ..., "<list_key>": { "<CARD>": count, ... } }
card_counts read_card_counts(const std::string& path, const std::string& list_key){

	nlohmann::ordered_json json = read_json_file(path);

	if (!json.is_object() || !json.contains("name") || !json["name"].is_string()) {
		throw std::runtime_error(path + ": missing \"name\" (a string)");
	}
	if (!json.contains(list_key) || !json[list_key].is_object()) {
		throw std::runtime_error(path + ": missing \"" + list_key
		                         + "\" (an object { \"NAME\": count })");
	}

	card_counts result;
	result.name = json["name"].get<std::string>();
	for (const auto& entry : json[list_key].items()) {
		if (!entry.value().is_number_unsigned()) {
			throw std::runtime_error(path + ": the count of \"" + entry.key()
			                         + "\" must be a non-negative integer");
		}
		result.counts.emplace_back(entry.key(), entry.value().get<unsigned>());
	}
	return result;
}

}

starting_deck load_deck(const std::string& path){
	card_counts file = read_card_counts(path, "cards");
	starting_deck deck;
	deck.name = file.name;
	for (const auto& [card_name, count] : file.counts) {
		if (make_card(card_name) == nullptr) {
			throw std::runtime_error(path + ": unknown card \"" + card_name + "\"");
		}
		for (unsigned i = 0; i < count; ++i) {
			deck.cards.push_back(make_card(card_name));
		}
	}
	return deck;
}

std::vector<faction> load_contracts(const std::string& path){
	static const std::map<std::string, faction> factions = {
		{"RED", RED}, {"YELLOW", YELLOW}, {"BLUE", BLUE},
	};
	card_counts file = read_card_counts(path, "contracts");
	std::vector<faction> contracts;
	for (const auto& [faction_name, count] : file.counts) {
		auto f = factions.find(faction_name);
		if (f == factions.end()) {
			throw std::runtime_error(path + ": unknown faction \"" + faction_name
			                         + "\" (RED, YELLOW or BLUE)");
		}
		contracts.insert(contracts.end(), count, f->second);
	}
	return contracts;
}


// --- events ------------------------------------------------------------------------

namespace {

int optional_int(const nlohmann::ordered_json& j, const char* key, const std::string& where){
	if (!j.contains(key)) {
		return 0;
	}
	if (!j[key].is_number_integer()) {
		throw std::runtime_error(where + ": \"" + key + "\" must be an integer");
	}
	return j[key].get<int>();
}

event parse_event(const nlohmann::ordered_json& j, const std::string& where, bool branch){
	if (!j.is_object()) {
		throw std::runtime_error(where + ": an event must be an object");
	}
	std::string kind;
	if (j.contains("kind")) {
		if (!j["kind"].is_string()) {
			throw std::runtime_error(where + ": \"kind\" must be a string");
		}
		kind = j["kind"].get<std::string>();
	} else if (branch) {
		kind = "PROPAGANDA";
	} else {
		throw std::runtime_error(where + ": missing \"kind\"");
	}
	int value = optional_int(j, "value", where);
	int stealth = optional_int(j, "stealth", where);

	if (kind == "PROPAGANDA") {
		static const std::map<std::string, faction> factions = {
			{"RED", RED}, {"YELLOW", YELLOW}, {"BLUE", BLUE}, {"ANY", ANY},
		};
		if (!j.contains("faction") || !j["faction"].is_string()
		    || factions.count(j["faction"].get<std::string>()) == 0) {
			throw std::runtime_error(where + ": a propaganda needs a \"faction\" "
			                         "(RED, YELLOW, BLUE or ANY)");
		}
		if (!j.contains("value")) {
			throw std::runtime_error(where + ": a propaganda needs a \"value\"");
		}
		return event(factions.at(j["faction"].get<std::string>()), value, stealth);
	}
	if (kind == "AND" || kind == "OR") {
		if (!j.contains("left") || !j.contains("right")) {
			throw std::runtime_error(where + ": " + kind + " needs \"left\" and \"right\"");
		}
		return event((kind == "AND") ? CONVERGENCE : DIVERGENCE,
		             parse_event(j["left"], where + " (left)", true),
		             parse_event(j["right"], where + " (right)", true));
	}
	static const std::map<std::string, event_kind> others = {
		{"ELECTION", ELECTION}, {"COLLAPSE", COLLAPSE},
		{"EXPLOSION", EXPLOSION}, {"MISTAKE", MISTAKE},
	};
	auto other = others.find(kind);
	if (other == others.end()) {
		throw std::runtime_error(where + ": unknown kind \"" + kind + "\"");
	}
	return event(other->second, stealth, value);
}

}

std::vector<event> load_events(const std::string& path){
	nlohmann::ordered_json json = read_json_file(path);
	if (!json.is_object() || !json.contains("events") || !json["events"].is_array()) {
		throw std::runtime_error(path + ": missing \"events\" (a list)");
	}
	std::vector<event> events;
	size_t index = 0;
	for (const auto& entry : json["events"]) {
		std::string where = path + ", event n°" + std::to_string(index++);
		if (!entry.is_object() || !entry.contains("count") || !entry["count"].is_number_unsigned()) {
			throw std::runtime_error(where + ": missing \"count\" (a non-negative integer)");
		}
		event card = parse_event(entry, where, false);
		events.insert(events.end(), entry["count"].get<unsigned>(), card);
	}
	return events;
}
