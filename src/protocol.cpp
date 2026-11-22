#include "../include/protocol.hpp"

#include <algorithm>
#include <string>
#include <utility>

// --- names -----------------------------------------------------------------------

std::string faction_name(faction f) {
	switch (f) {
		case RED: return "RED";
		case YELLOW: return "YELLOW";
		case BLUE: return "BLUE";
		case ANY: return "ANY";
		case NONE: return "NONE";
	}
	return "NONE";
}

faction faction_from_name(const std::string &name) {
	for (faction f : {RED, YELLOW, BLUE, ANY, NONE}) {
		if (faction_name(f) == name) {
			return f;
		}
	}
	throw std::runtime_error("unknown faction: " + name);
}

std::string phase_name(game_phase p) {
	switch (p) {
		case game_phase::SCHEMES: return "schemes";
		case game_phase::REVEAL: return "reveal";
		case game_phase::SPELLS: return "spells";
		case game_phase::RESOLUTION: return "resolution";
		case game_phase::CLEANUP: return "cleanup";
	}
	return "schemes";
}

namespace {

std::string outcome_name(turn_outcome o) {
	switch (o) {
		case turn_outcome::RESOLVED: return "resolved";
		case turn_outcome::STEALTH_ROLLBACK: return "stealth_rollback";
		case turn_outcome::EXPLOSION_ROLLBACK: return "explosion_rollback";
	}
	return "resolved";
}

std::string tiebreak_name(tiebreak t) {
	switch (t) {
		case tiebreak::NONE: return "none";
		case tiebreak::GRIMORY: return "grimory";
		case tiebreak::DRAW: return "draw";
	}
	return "none";
}

json popularity_json(const std::vector<int> &popularity) {
	json j = json::object();
	for (faction f : {RED, YELLOW, BLUE}) {
		j[faction_name(f)] = popularity[f];
	}
	return j;
}

json cards_of(const hand &cards) {
	json list = json::array();
	for (size_t i = 0; i < cards.get_nb_cards(); ++i) {
		list.push_back(card_json(cards.card_at(i)));
	}
	return list;
}

} // namespace

json card_json(const shenanigan &card) {
	return json{{"id", card.get_id()}, {"name", card.name()}};
}

card_ptr card_from_json(const json &j) {
	const std::string name = j.at("name").get<std::string>();
	card_ptr card = make_card(name);
	if (!card) {
		throw std::runtime_error("unknown grimory card: " + name);
	}
	card->set_id(j.at("id").get<long>());
	return card;
}

json contract_json(const contract &c) {
	json factions = json::array();
	for (faction f : c.factions) {
		factions.push_back(faction_name(f));
	}
	return json{{"name", c.name}, {"factions", factions}, {"rank", c.rank}, {"points", c.points}};
}

contract contract_from_json(const json &j) {
	contract c;
	c.name = j.at("name").get<std::string>();
	for (const json &f : j.at("factions")) {
		c.factions.push_back(faction_from_name(f.get<std::string>()));
	}
	c.rank = j.at("rank").get<int>();
	c.points = j.at("points").get<int>();
	return c;
}


// --- events ----------------------------------------------------------------------

json state_codec::event_json(const event &e) {
	json j = json::object();
	if (e.id >= 0) {
		j["id"] = e.id;
	}
	switch (e.kind) {
		case OPERATOR:
			j["kind"] = (e.type.op == CONVERGENCE) ? "AND" : "OR";
			j["stealth"] = e.get_stealth();
			j["modification"] = e.modification;
			j["left"] = event_json(*e.left);
			j["right"] = event_json(*e.right);
			break;
		case PROPAGANDA:
			j["kind"] = "PROPAGANDA";
			j["faction"] = faction_name(e.type.faction);
			j["value"] = e.get_value();
			j["base_value"] = e.value;
			j["modification"] = e.modification;
			j["stealth"] = e.st;
			break;
		case ELECTION:
			j["kind"] = "ELECTION";
			j["stealth"] = e.st;
			break;
		case EXPLOSION:
			j["kind"] = "EXPLOSION";
			j["stealth"] = e.st;
			break;
		case COLLAPSE:
			j["kind"] = "COLLAPSE";
			j["stealth"] = e.st;
			break;
		case MISTAKE:
			j["kind"] = "MISTAKE";
			j["stealth"] = e.st;
			j["value"] = e.value;
			break;
	}
	return j;
}

event state_codec::event_from_json(const json &j) {
	const std::string kind = j.at("kind").get<std::string>();
	int stealth = j.value("stealth", 0);
	auto with_identity = [&j](event e) {
		e.id = j.value("id", -1L);
		e.modification = j.value("modification", 0);
		return e;
	};
	if (kind == "AND" || kind == "OR") {
		return with_identity(event((kind == "AND") ? CONVERGENCE : DIVERGENCE,
		                           event_from_json(j.at("left")), event_from_json(j.at("right"))));
	}
	if (kind == "PROPAGANDA") {
		return with_identity(event(faction_from_name(j.at("faction").get<std::string>()),
		                           j.at("base_value").get<int>(), stealth));
	}
	if (kind == "MISTAKE") {
		return with_identity(event(MISTAKE, stealth, j.at("value").get<int>()));
	}
	for (event_kind k : {ELECTION, EXPLOSION, COLLAPSE}) {
		if (kind == event_json(event(k, 0, 0)).at("kind")) {
			return with_identity(event(k, stealth, 0));
		}
	}
	throw std::runtime_error("unknown event kind: " + kind);
}


// --- the whole game --------------------------------------------------------------

namespace {

json events_json(const std::vector<event> &events) {
	json list = json::array();
	for (const event &e : events) {
		list.push_back(state_codec::event_json(e));
	}
	return list;
}

// the controller of a mage in a rebuilt game: it only describes the strategy
class described_controller : public controller {
public:
	described_controller(std::string strategy, stealth greed)
		: strategy(std::move(strategy)), greed(greed) {}
	std::string strategy_name() const override { return strategy; }
	stealth get_greed() const override { return greed; }

	bool reveal_more(const decision &) override { return refuse(); }
	size_t scheme_card(const decision &) override { return refuse(); }
	size_t next_shenanigan(const decision &) override { return refuse(); }
	bool reveal_shenanigan(const decision &, const shenanigan &) override { return refuse(); }
	unsigned ether_to_spend(const decision &, const spell &) override { return refuse(); }
	size_t propaganda_target(const decision &, int, const std::vector<size_t> &) override { return refuse(); }
	size_t opponent_target(const decision &, const std::vector<size_t> &) override { return refuse(); }
	size_t cosmos_card(const decision &, unsigned) override { return refuse(); }
	size_t ether_leak_target(const decision &, const std::vector<size_t> &, unsigned) override { return refuse(); }
	size_t divergence_branch(const decision &, const event &) override { return refuse(); }
	size_t any_faction(const decision &, int, const std::vector<faction> &) override { return refuse(); }

private:
	static size_t refuse() {
		throw std::logic_error("a rebuilt game cannot take decisions");
	}
	std::string strategy;
	stealth greed;
};

} // namespace

json state_codec::state(const game &g) {
	json s = json::object();
	s["seed"] = g.seed;
	s["turn"] = g.elapsed_turns;
	s["current"] = g.current_player;
	s["phase"] = phase_name(g.phase);
	s["max_stealth"] = g.max_stealth;
	s["stealth"] = g.present_events.get_stealth();
	s["popularity"] = popularity_json(g.scores);

	json present = json::array();
	for (const event &e : g.present_events.show_events()) {
		present.push_back(event_json(e));
	}
	s["present"] = present;
	std::vector<event> future = g.future_events.show_events();
	std::reverse(future.begin(), future.end()); // top first
	s["future"] = events_json(future);
	s["past"] = events_json(g.past_events.show_events());
	s["oblivion"] = events_json(g.oblivion);
	json cosmos = json::array();
	for (size_t i = 0; i < g.cosmos_cards.size(); ++i) {
		cosmos.push_back(card_json(g.cosmos_cards.card_at(i)));
	}
	s["cosmos"] = cosmos;
	s["contracts_left"] = g.contract_pile.size();

	json seats = json::array();
	for (size_t seat = 0; seat < g.players.size(); ++seat) {
		const player &p = *g.players[seat];
		json grimory = json::array();
		for (auto it = p.deck.cards.rbegin(); it != p.deck.cards.rend(); ++it) {
			grimory.push_back(card_json(**it)); // top first
		}
		json prepared = json::array();
		for (size_t i = 0; i < p.cards_in_play.nb_prepared(); ++i) {
			prepared.push_back(card_json(p.cards_in_play.prepared_at(i)));
		}
		json ether = json::array();
		for (size_t i = 0; i < p.cards_in_play.available_ether(); ++i) {
			ether.push_back(card_json(p.cards_in_play.ether_at(i)));
		}
		json known = json::array();
		for (const event &e : g.future_events.show_events()) {
			if (p.known_event(e.get_id()) != nullptr) {
				known.push_back(e.get_id());
			}
		}
		std::reverse(known.begin(), known.end()); // top first
		seats.push_back(json{
			{"seat", seat},
			{"controller", {{"strategy", p.get_controller().strategy_name()},
			                {"greed", p.get_controller().get_greed()}}},
			{"deck", p.get_deck_name()},
			{"contract", contract_json(p.get_contract())},
			{"score", p.get_score()},
			{"hand", cards_of(p.get_hand())},
			{"hand_limit", p.cards_in_hand.cards_limit},
			{"prepared", prepared},
			{"ether", ether},
			{"immunity", p.get_immunity()},
			{"grimory", grimory},
			{"known_future", known},
		});
	}
	s["seats"] = seats;
	return s;
}

game state_codec::rebuild(const json &s) {
	game g(s.at("max_stealth").get<stealth>(), s.at("seed").get<unsigned>());
	const json &popularity = s.at("popularity");
	for (faction f : {RED, YELLOW, BLUE}) {
		g.scores[f] = popularity.at(faction_name(f)).get<int>();
	}
	g.elapsed_turns = s.at("turn").get<long>();
	g.current_player = s.at("current").get<size_t>();
	for (game_phase p : {game_phase::SCHEMES, game_phase::REVEAL, game_phase::SPELLS,
	                     game_phase::RESOLUTION, game_phase::CLEANUP}) {
		if (phase_name(p) == s.at("phase").get<std::string>()) {
			g.phase = p;
		}
	}

	for (const json &e : s.at("present")) {
		g.present_events.present.push_back(event_from_json(e));
	}
	g.present_events.current_stealth = s.at("stealth").get<stealth>();
	const json &future = s.at("future");
	for (auto it = future.rbegin(); it != future.rend(); ++it) { // bottom first
		g.future_events.future.push_back(event_from_json(*it));
	}
	for (const json &e : s.at("past")) {
		g.past_events.past.push_back(event_from_json(e));
	}
	for (const json &e : s.at("oblivion")) {
		g.oblivion.push_back(event_from_json(e));
	}
	for (const json &card : s.at("cosmos")) {
		g.cosmos_cards.absorb(card_from_json(card));
	}
	// the undealt contracts are face down: only their number is known
	g.contract_pile.resize(s.at("contracts_left").get<size_t>());

	for (const json &seat : s.at("seats")) {
		const json &c = seat.at("controller");
		g.players.push_back(std::make_unique<player>(std::make_unique<described_controller>(
			c.at("strategy").get<std::string>(), c.at("greed").get<stealth>())));
		player &p = *g.players.back();
		p.deck_name = seat.at("deck").get<std::string>();
		p.signed_contract = contract_from_json(seat.at("contract"));
		p.points = seat.at("score").get<int>();
		p.cards_in_hand.cards_limit = seat.at("hand_limit").get<size_t>();
		for (const json &card : seat.at("hand")) {
			p.cards_in_hand.add_card(card_from_json(card));
		}
		for (const json &card : seat.at("prepared")) {
			p.cards_in_play.prepare_shenanigan(card_from_json(card));
		}
		for (const json &card : seat.at("ether")) {
			p.cards_in_play.store_ether(card_from_json(card));
		}
		p.immunity_charges = seat.at("immunity").get<unsigned>();
		for (const json &card : seat.at("grimory")) { // top first
			p.deck.cards.push_front(card_from_json(card));
		}
		for (const json &id : seat.at("known_future")) {
			for (const event &e : g.future_events.future) {
				if (e.get_id() == id.get<long>()) {
					p.known_events.push_back(e);
				}
			}
		}
	}
	return g;
}


// --- questions -------------------------------------------------------------------

namespace questions {

question reveal_more(const decision &d) {
	return {"reveal_more",
	        {{"stealth", d.g.present_events.get_stealth()}, {"max_stealth", d.g.get_max_stealth()}},
	        json::array({"reveal", "stop"})};
}

question scheme_card(const decision &d) {
	question q{"scheme_card"};
	const hand &cards = d.self.get_hand();
	for (size_t i = 0; i < cards.get_nb_cards(); ++i) {
		q.options.push_back({{"hand", i}, {"card", card_json(cards.card_at(i))}});
	}
	return q;
}

question next_shenanigan(const decision &d) {
	question q{"next_shenanigan"};
	const lair &schemed = d.self.get_lair();
	for (size_t i = 0; i < schemed.nb_prepared(); ++i) {
		q.options.push_back({{"prepared", i}, {"card", card_json(schemed.prepared_at(i))}});
	}
	return q;
}

question reveal_shenanigan(const shenanigan &card) {
	return {"reveal_shenanigan", {{"card", card_json(card)}}, json::array({"reveal", "bury"})};
}

question ether_to_spend(const decision &d, const spell &s) {
	question q{"ether_to_spend", {{"card", card_json(s)}}};
	for (unsigned ether = 0; ether <= d.self.available_ether(); ++ether) {
		q.options.push_back(ether);
	}
	return q;
}

question propaganda_target(const decision &d, int modification, const std::vector<size_t> &present) {
	question q{"propaganda_target",
	           {{"spell", (modification > 0) ? "EUPHORIA" : "NEURASTHENIA"},
	            {"modification", modification}}};
	const std::deque<event> &events = d.g.present_events.show_events();
	for (size_t position : present) {
		q.options.push_back({{"present", position},
		                     {"event", state_codec::event_json(events.at(position))}});
	}
	return q;
}

question opponent_target(const std::vector<size_t> &seats) {
	question q{"opponent_target", {{"spell", "MEMORY_LAPSE"}}};
	for (size_t seat : seats) {
		q.options.push_back({{"seat", seat}});
	}
	return q;
}

question cosmos_card(const decision &d, unsigned remaining) {
	question q{"cosmos_card", {{"spell", "ENTROPY_REVERSAL"}, {"remaining", remaining}}};
	for (size_t i = 0; i < d.g.cosmos_cards.size(); ++i) {
		q.options.push_back({{"cosmos", i}, {"card", card_json(d.g.cosmos_cards.card_at(i))}});
	}
	return q;
}

question ether_leak_target(const decision &d, const std::vector<size_t> &seats, unsigned remaining) {
	question q{"ether_leak_target", {{"spell", "ETHER_LEAK"}, {"remaining", remaining}}};
	for (size_t seat : seats) {
		q.options.push_back({{"seat", seat}, {"ether", d.g.player_at(seat).available_ether()}});
	}
	q.options.push_back("stop");
	return q;
}

question divergence_branch(const event &divergence) {
	return {"divergence_branch", {{"event", state_codec::event_json(divergence)}}, json::array({"left", "right"})};
}

question any_faction(int value, const std::vector<faction> &allowed) {
	question q{"any_faction", {{"value", value}}};
	for (faction f : allowed) {
		q.options.push_back(faction_name(f));
	}
	return q;
}

} // namespace questions


// --- output ----------------------------------------------------------------------

void protocol_writer::write(const json &message) {
	out << message.dump() << '\n';
	out.flush();
}

void protocol_writer::announce(const game &g, json message) {
	if (with_state) {
		message["state"] = state_codec::state(g);
	}
	write(message);
}


// --- announcements ---------------------------------------------------------------

void protocol_announcer::announce_setup(game &g) {
	json future = state_codec::state(g).at("future");
	size_t hand_size = g.nb_players() ? g.player_at(0).get_hand().get_nb_cards() : 0;
	w.announce(g, {{"type", "game_start"}, {"protocol", protocol_version}, {"seed", g.get_seed()},
	               {"seats", g.nb_players()}, {"max_stealth", g.get_max_stealth()},
	               {"hand_size", hand_size}, {"popularity", popularity_json(g.get_popularity())},
	               {"future", future}});
	json state = state_codec::state(g);
	for (size_t seat = 0; seat < g.nb_players(); ++seat) {
		const json &s = state.at("seats").at(seat);
		w.announce(g, {{"type", "seat_setup"}, {"seat", seat}, {"controller", s.at("controller")},
		               {"contract", s.at("contract")}, {"deck", s.at("deck")},
		               {"hand", s.at("hand")}, {"grimory", s.at("grimory")}});
	}
}

void protocol_announcer::announce_game_over(game &g, bool finished) {
	tiebreak how = tiebreak::NONE;
	size_t winner = g.determine_winner(&how);
	json scores = json::array();
	json contracts = json::array();
	for (size_t seat = 0; seat < g.nb_players(); ++seat) {
		scores.push_back(g.player_at(seat).get_score());
		contracts.push_back(contract_json(g.player_at(seat).get_contract()));
	}
	w.announce(g, {{"type", "game_over"}, {"finished", finished}, {"scores", scores},
	               {"winner", winner}, {"tiebreak", tiebreak_name(how)}, {"contracts", contracts}});
}

void protocol_announcer::turn_started(game &g) {
	w.announce(g, {{"type", "turn_start"}, {"turn", g.get_elapsed_turns()},
	               {"current", g.current_player}, {"future_size", g.future_events.remaining_cards()}});
}

void protocol_announcer::phase_started(game &g, game_phase phase) {
	w.announce(g, {{"type", "phase"}, {"name", phase_name(phase)}});
}

void protocol_announcer::turn_ended(game &g, turn_outcome outcome) {
	w.announce(g, {{"type", "turn_end"}, {"outcome", outcome_name(outcome)}});
}

void protocol_announcer::event_revealed(game &g, const event &e) {
	w.announce(g, {{"type", "event_revealed"}, {"event", state_codec::event_json(e)},
	               {"present", g.present_events.show_events().size() - 1},
	               {"stealth", g.present_events.get_stealth()},
	               {"future_size", g.future_events.remaining_cards()}});
}

void protocol_announcer::collapse_revealed(game &g) {
	w.announce(g, {{"type", "collapse"}, {"stealth", g.present_events.get_stealth()}});
}

void protocol_announcer::rolled_back(game &g, turn_outcome cause, const std::vector<long> &returned) {
	json oblivion = (cause == turn_outcome::EXPLOSION_ROLLBACK)
		? state_codec::event_json(g.oblivion.back()) : json(nullptr);
	w.announce(g, {{"type", "rollback"},
	               {"cause", (cause == turn_outcome::EXPLOSION_ROLLBACK) ? "explosion" : "stealth"},
	               {"to_oblivion", oblivion}, {"returned", returned}});
}

void protocol_announcer::shenanigan_returned(game &g, const player &p, const shenanigan &card) {
	w.announce(g, {{"type", "shenanigan_returned"}, {"seat", g.seat_of(p)}, {"card", card_json(card)}});
}

void protocol_announcer::propaganda_applied(game &g, faction f, int value) {
	applied.push_back({{"faction", faction_name(f)}, {"value", value}});
}

void protocol_announcer::event_resolved(game &g, const event &e) {
	w.announce(g, {{"type", "event_resolved"}, {"event", state_codec::event_json(e)},
	               {"applied", applied}, {"popularity", popularity_json(g.get_popularity())}});
	applied = json::array();
}

void protocol_announcer::election_resolved(game &g, const std::vector<int> &points) {
	json scores = json::array();
	for (size_t seat = 0; seat < g.nb_players(); ++seat) {
		scores.push_back(g.player_at(seat).get_score());
	}
	w.announce(g, {{"type", "election"}, {"points", points}, {"scores", scores},
	               {"popularity", popularity_json(g.get_popularity())}});
}

void protocol_announcer::card_schemed(game &g, const player &p, const shenanigan &card) {
	w.announce(g, {{"type", "card_schemed"}, {"seat", g.seat_of(p)}, {"card", card_json(card)}});
}

void protocol_announcer::shenanigan_buried(game &g, const player &p, const shenanigan &card) {
	w.announce(g, {{"type", "shenanigan_buried"}, {"seat", g.seat_of(p)}, {"card", card_json(card)},
	               {"grimory_size", p.grimory_size()}});
}

void protocol_announcer::card_revealed(game &g, const player &p, const shenanigan &card) {
	w.announce(g, {{"type", "shenanigan_revealed"}, {"seat", g.seat_of(p)}, {"card", card_json(card)}});
}

void protocol_announcer::ether_stored(game &g, const player &p) {
	const lair &l = p.get_lair();
	w.announce(g, {{"type", "ether_stored"}, {"seat", g.seat_of(p)},
	               {"card", card_json(l.ether_at(l.available_ether() - 1))},
	               {"ether", p.available_ether()}});
}

namespace {

// the last `count` cards that reached the cosmos
json last_in_cosmos(const game &g, size_t count) {
	json cards = json::array();
	for (size_t i = g.cosmos_cards.size() - count; i < g.cosmos_cards.size(); ++i) {
		cards.push_back(card_json(g.cosmos_cards.card_at(i)));
	}
	return cards;
}

}

void protocol_announcer::ether_spent(game &g, const player &p, unsigned amount) {
	w.announce(g, {{"type", "ether_spent"}, {"seat", g.seat_of(p)}, {"amount", amount},
	               {"cards", last_in_cosmos(g, amount)}, {"ether", p.available_ether()}});
}

void protocol_announcer::spell_failed(game &g, const player &p, const shenanigan &card) {
	w.announce(g, {{"type", "spell_failed"}, {"seat", g.seat_of(p)}, {"card", card_json(card)}});
}

void protocol_announcer::spell_discarded(game &g, const player &p, const shenanigan &card) {
	w.announce(g, {{"type", "spell_discarded"}, {"seat", g.seat_of(p)}, {"card", card_json(card)},
	               {"cosmos_size", g.cosmos_cards.size()}});
}

void protocol_announcer::propaganda_modified(game &g, size_t position, int modification) {
	w.announce(g, {{"type", "propaganda_modified"}, {"present", position},
	               {"modification", modification},
	               {"event", state_codec::event_json(g.present_events.show_events().at(position))}});
}

void protocol_announcer::memory_lapse_received(game &g, const player &caster, const player &target) {
	const hand &h = target.get_hand();
	w.announce(g, {{"type", "memory_lapse"}, {"from", g.seat_of(caster)}, {"to", g.seat_of(target)},
	               {"card", card_json(h.card_at(h.get_nb_cards() - 1))}});
}

void protocol_announcer::cosmos_card_taken(game &g, const player &p, const shenanigan &card) {
	w.announce(g, {{"type", "cosmos_taken"}, {"seat", g.seat_of(p)}, {"card", card_json(card)}});
}

void protocol_announcer::ether_leaked(game &g, const player &caster, const player &target) {
	w.announce(g, {{"type", "ether_leaked"}, {"seat", g.seat_of(target)}, {"by", g.seat_of(caster)},
	               {"card", last_in_cosmos(g, 1).at(0)}, {"ether", target.available_ether()}});
}

void protocol_announcer::immunity_gained(game &g, const player &p, unsigned charges) {
	w.announce(g, {{"type", "immunity_gained"}, {"seat", g.seat_of(p)}, {"gained", charges},
	               {"charges", p.get_immunity()}});
}

void protocol_announcer::immunity_triggered(game &g, const player &immune, const player &chooser) {
	w.announce(g, {{"type", "choice_cancelled"}, {"immune", g.seat_of(immune)},
	               {"by", g.seat_of(chooser)}, {"charges_left", immune.get_immunity()}});
}

void protocol_announcer::immunity_expired(game &g, const player &p) {
	w.announce(g, {{"type", "immunity_expired"}, {"seat", g.seat_of(p)}});
}

void protocol_announcer::future_read(game &g, const player &p, const std::vector<event> &events) {
	w.announce(g, {{"type", "future_read"}, {"seat", g.seat_of(p)}, {"events", events_json(events)}});
}

void protocol_announcer::event_skipped(game &g, const event &e) {
	w.announce(g, {{"type", "event_skipped"}, {"event", state_codec::event_json(e)},
	               {"future_size", g.future_events.remaining_cards()}});
}

void protocol_announcer::forced_reveal(game &g, int count) {
	w.announce(g, {{"type", "forced_reveal"}, {"count", count}});
}

void protocol_announcer::cards_drawn(game &g, const player &p, size_t count) {
	json cards = json::array();
	const hand &h = p.get_hand();
	for (size_t i = h.get_nb_cards() - count; i < h.get_nb_cards(); ++i) {
		cards.push_back(card_json(h.card_at(i)));
	}
	w.announce(g, {{"type", "cards_drawn"}, {"seat", g.seat_of(p)}, {"cards", cards},
	               {"grimory_size", p.grimory_size()}});
}


// --- remote controller -----------------------------------------------------------

size_t remote_controller::ask(const decision &d, const question &q) {
	json message = {{"type", "ask"}, {"id", w.next_id()}, {"seat", d.seat},
	                {"question", q.name}, {"context", q.context}, {"options", q.options},
	                {"state", state_codec::state(d.g)}};
	const long id = message["id"];
	for (int attempt = 1; ; ++attempt) {
		w.write(message);
		std::string line;
		if (!std::getline(in, line)) {
			throw protocol_abort(2, nullptr, "the standard input is closed");
		}
		std::string reason;
		try {
			json answer = json::parse(line);
			if (!answer.is_object() || !answer.contains("seat") || !answer.contains("id")
			    || !answer.contains("choice")) {
				reason = "an answer needs seat, id and choice";
			} else if (answer["seat"] != d.seat) {
				reason = "seat " + std::to_string(d.seat) + " must answer";
			} else if (answer["id"] != id) {
				reason = "the question pending is id " + std::to_string(id);
			} else if (!answer["choice"].is_number_integer() || answer["choice"].get<long>() < 0
			           || answer["choice"].get<long>() >= static_cast<long>(q.options.size())) {
				reason = "choice must be between 0 and " + std::to_string(q.options.size() - 1);
			} else {
				return answer["choice"].get<size_t>();
			}
		} catch (const json::parse_error &) {
			reason = "an answer must be one line of JSON";
		}
		w.write({{"type", "error"}, {"id", id}, {"seat", d.seat}, {"reason", reason}});
		if (attempt == 2) {
			throw protocol_abort(3, d.seat, "two invalid answers to question " + std::to_string(id)
			                                + ": " + reason);
		}
	}
}

bool remote_controller::reveal_more(const decision &d) {
	return ask(d, questions::reveal_more(d)) == 0;
}
size_t remote_controller::scheme_card(const decision &d) {
	return ask(d, questions::scheme_card(d));
}
size_t remote_controller::next_shenanigan(const decision &d) {
	return ask(d, questions::next_shenanigan(d));
}
bool remote_controller::reveal_shenanigan(const decision &d, const shenanigan &card) {
	return ask(d, questions::reveal_shenanigan(card)) == 0;
}
unsigned remote_controller::ether_to_spend(const decision &d, const spell &s) {
	return static_cast<unsigned>(ask(d, questions::ether_to_spend(d, s)));
}
size_t remote_controller::propaganda_target(const decision &d, int modification,
                                            const std::vector<size_t> &present) {
	return ask(d, questions::propaganda_target(d, modification, present));
}
size_t remote_controller::opponent_target(const decision &d, const std::vector<size_t> &seats) {
	return ask(d, questions::opponent_target(seats));
}
size_t remote_controller::cosmos_card(const decision &d, unsigned remaining) {
	return ask(d, questions::cosmos_card(d, remaining));
}
size_t remote_controller::ether_leak_target(const decision &d, const std::vector<size_t> &seats,
                                            unsigned remaining) {
	return ask(d, questions::ether_leak_target(d, seats, remaining));
}
size_t remote_controller::divergence_branch(const decision &d, const event &divergence) {
	return ask(d, questions::divergence_branch(divergence));
}
size_t remote_controller::any_faction(const decision &d, int value, const std::vector<faction> &allowed) {
	return ask(d, questions::any_faction(value, allowed));
}


// --- announced controller --------------------------------------------------------

void announced_controller::decided(const decision &d, const question &q, size_t choice) {
	w.announce(d.g, {{"type", "decided"}, {"id", w.next_id()}, {"seat", d.seat},
	                 {"question", q.name}, {"context", q.context}, {"options", q.options},
	                 {"choice", choice}});
}

bool announced_controller::reveal_more(const decision &d) {
	bool more = inner->reveal_more(d);
	decided(d, questions::reveal_more(d), more ? 0 : 1);
	return more;
}
size_t announced_controller::scheme_card(const decision &d) {
	size_t choice = inner->scheme_card(d);
	decided(d, questions::scheme_card(d), choice);
	return choice;
}
size_t announced_controller::next_shenanigan(const decision &d) {
	size_t choice = inner->next_shenanigan(d);
	decided(d, questions::next_shenanigan(d), choice);
	return choice;
}
bool announced_controller::reveal_shenanigan(const decision &d, const shenanigan &card) {
	bool reveal = inner->reveal_shenanigan(d, card);
	decided(d, questions::reveal_shenanigan(card), reveal ? 0 : 1);
	return reveal;
}
unsigned announced_controller::ether_to_spend(const decision &d, const spell &s) {
	unsigned ether = inner->ether_to_spend(d, s);
	decided(d, questions::ether_to_spend(d, s), ether);
	return ether;
}
size_t announced_controller::propaganda_target(const decision &d, int modification,
                                               const std::vector<size_t> &present) {
	size_t choice = inner->propaganda_target(d, modification, present);
	decided(d, questions::propaganda_target(d, modification, present), choice);
	return choice;
}
size_t announced_controller::opponent_target(const decision &d, const std::vector<size_t> &seats) {
	size_t choice = inner->opponent_target(d, seats);
	decided(d, questions::opponent_target(seats), choice);
	return choice;
}
size_t announced_controller::cosmos_card(const decision &d, unsigned remaining) {
	size_t choice = inner->cosmos_card(d, remaining);
	decided(d, questions::cosmos_card(d, remaining), choice);
	return choice;
}
size_t announced_controller::ether_leak_target(const decision &d, const std::vector<size_t> &seats,
                                               unsigned remaining) {
	size_t choice = inner->ether_leak_target(d, seats, remaining);
	decided(d, questions::ether_leak_target(d, seats, remaining), choice);
	return choice;
}
size_t announced_controller::divergence_branch(const decision &d, const event &divergence) {
	size_t choice = inner->divergence_branch(d, divergence);
	decided(d, questions::divergence_branch(divergence), choice);
	return choice;
}
size_t announced_controller::any_faction(const decision &d, int value, const std::vector<faction> &allowed) {
	size_t choice = inner->any_faction(d, value, allowed);
	decided(d, questions::any_faction(value, allowed), choice);
	return choice;
}
