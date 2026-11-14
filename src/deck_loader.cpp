#include "../include/game_concepts.hpp"
#include <fstream>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>
#include <nlohmann/json.hpp>

// Card sets are JSON documents listing how many copies of each card a set
// holds:
//   decks/*.json      { "name": "implemented", "cards": { "ETHER": 6, ... } }
//                     card names: those registered in make_card()
//                     (shenanigans.cpp)
//
// Contract and event sets list their cards with a number of copies:
//   contracts/*.json  { "name": "mixed", "contracts": [
//                         { "count": 2, "name": "red favourite",
//                           "factions": ["RED"], "rank": 1, "points": 2 },
//                         { "count": 1, "name": "red or blue",
//                           "factions": ["RED", "BLUE"], "rank": 1, "points": 1 } ] }
//                     factions: one or more of RED, YELLOW, BLUE; rank: 1 to 3
//                     (1: the most popular); name: optional
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

std::vector<contract> load_contracts(const std::string& path){
	static const std::map<std::string, faction> factions = {
		{"RED", RED}, {"YELLOW", YELLOW}, {"BLUE", BLUE},
	};
	static const std::map<faction, std::string> names = {
		{RED, "RED"}, {YELLOW, "YELLOW"}, {BLUE, "BLUE"},
	};
	nlohmann::ordered_json json = read_json_file(path);
	if (!json.is_object() || !json.contains("contracts") || !json["contracts"].is_array()) {
		throw std::runtime_error(path + ": missing \"contracts\" (a list)");
	}
	std::vector<contract> contracts;
	size_t index = 0;
	for (const auto& entry : json["contracts"]) {
		std::string where = path + ", contract n°" + std::to_string(index++);
		if (!entry.is_object()) {
			throw std::runtime_error(where + ": a contract must be an object");
		}
		for (const auto& key : entry.items()) {
			if (key.key() != "count" && key.key() != "name" && key.key() != "factions"
			    && key.key() != "rank" && key.key() != "points") {
				throw std::runtime_error(where + ": unknown key \"" + key.key() + "\"");
			}
		}
		if (!entry.contains("count") || !entry["count"].is_number_unsigned()) {
			throw std::runtime_error(where + ": missing \"count\" (a non-negative integer)");
		}
		contract c;
		if (!entry.contains("factions") || !entry["factions"].is_array() || entry["factions"].empty()) {
			throw std::runtime_error(where + ": missing \"factions\" (a non-empty list)");
		}
		for (const auto& f : entry["factions"]) {
			if (!f.is_string() || factions.count(f.get<std::string>()) == 0) {
				throw std::runtime_error(where + ": factions are RED, YELLOW or BLUE");
			}
			faction parsed = factions.at(f.get<std::string>());
			if (std::find(c.factions.begin(), c.factions.end(), parsed) != c.factions.end()) {
				throw std::runtime_error(where + ": a faction is listed twice");
			}
			c.factions.push_back(parsed);
		}
		if (!entry.contains("rank") || !entry["rank"].is_number_integer()
		    || entry["rank"].get<int>() < 1 || entry["rank"].get<int>() > factions_count) {
			throw std::runtime_error(where + ": \"rank\" must be an integer from 1 to "
			                         + std::to_string(factions_count));
		}
		c.rank = entry["rank"].get<int>();
		if (!entry.contains("points") || !entry["points"].is_number_integer()) {
			throw std::runtime_error(where + ": missing \"points\" (an integer)");
		}
		c.points = entry["points"].get<int>();
		if (entry.contains("name")) {
			if (!entry["name"].is_string()) {
				throw std::runtime_error(where + ": \"name\" must be a string");
			}
			c.name = entry["name"].get<std::string>();
		} else {
			// default name, e.g. "RED+BLUE rank 1, 1 pt"
			for (faction f : c.factions) {
				c.name += (c.name.empty() ? "" : "+") + names.at(f);
			}
			c.name += " rank " + std::to_string(c.rank) + ", " + std::to_string(c.points) + " pt";
		}
		contracts.insert(contracts.end(), entry["count"].get<unsigned>(), c);
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
