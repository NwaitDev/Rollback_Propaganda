#ifndef __PROTOCOL__
#define __PROTOCOL__

#include "game_concepts.hpp"

#include <istream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// The protocol of the game engine (see docs/protocol.md): JSON lines written
// by ./server on its standard output (announcements, questions) and read on
// its standard input (answers).

using json = nlohmann::ordered_json;

const int protocol_version = 1;

// --- the game in JSON ------------------------------------------------------------

std::string faction_name(faction f);
faction faction_from_name(const std::string &name);
std::string phase_name(game_phase p);

// a grimory card: {"id": 12, "name": "EUPHORIA"}
json card_json(const shenanigan &card);
card_ptr card_from_json(const json &j);

json contract_json(const contract &c);
contract contract_from_json(const json &j);

// Reads and writes every zone of a game, private ones included.
class state_codec {
public:
  static json event_json(const event &e);
  static event event_from_json(const json &j);
  // the whole game, as in the "state" of the protocol
  static json state(const game &g);
  // a game rebuilt from a "state": every mage gets a controller that only
  // knows its strategy's name and greed, and cannot decide
  static game rebuild(const json &state);
};


// --- questions -------------------------------------------------------------------

// a question as it is asked: the answer is an index in `options`
struct question {
  std::string name;
  json context = json::object();
  json options = json::array();
};

// The questions of the protocol, built from what a controller receives.
// For yes/no decisions, option 0 is "yes" (reveal) and 1 is "no".
namespace questions {
question reveal_more(const decision &d);
question scheme_card(const decision &d);
question next_shenanigan(const decision &d);
question reveal_shenanigan(const shenanigan &card);
question ether_to_spend(const decision &d, const spell &s);
question propaganda_target(const decision &d, int modification, const std::vector<size_t> &present);
question opponent_target(const std::vector<size_t> &seats);
question cosmos_card(const decision &d, unsigned remaining);
question ether_leak_target(const decision &d, const std::vector<size_t> &seats, unsigned remaining);
question divergence_branch(const event &divergence);
question any_faction(int value, const std::vector<faction> &allowed);
}


// --- output ----------------------------------------------------------------------

// Writes the messages, one line each, flushed at once. The questions and the
// decisions share one counter of ids.
class protocol_writer {
public:
  // with_state: every announcement carries the whole state (the questions
  // always do)
  protocol_writer(std::ostream &out, bool with_state) : out(out), with_state(with_state) {}
  void write(const json &message);
  // an announcement: adds the state if asked
  void announce(const game &g, json message);
  inline long next_id() { return id++; }

private:
  std::ostream &out;
  bool with_state;
  long id = 0;
};

// stops the game: the server writes game_aborted and exits with `exit_code`
struct protocol_abort : std::runtime_error {
  protocol_abort(int exit_code, json seat, const std::string &reason)
    : std::runtime_error(reason), exit_code(exit_code), seat(std::move(seat)) {}
  int exit_code;
  json seat; // the faulty seat, or null
};

// Every announcement of the protocol, written as the game unfolds.
class protocol_announcer : public game_observer {
public:
  explicit protocol_announcer(protocol_writer &w) : w(w) {}

  // before the first turn: game_start, then seat_setup for every seat
  void announce_setup(game &g);
  // after the last turn: game_over (draws the winner of a tie)
  void announce_game_over(game &g, bool finished);

  void turn_started(game &g) override;
  void phase_started(game &g, game_phase phase) override;
  void turn_ended(game &g, turn_outcome outcome) override;
  void event_revealed(game &g, const event &e) override;
  void collapse_revealed(game &g) override;
  void rolled_back(game &g, turn_outcome cause, const std::vector<long> &returned) override;
  void shenanigan_returned(game &g, const player &p, const shenanigan &card) override;
  void propaganda_applied(game &g, faction f, int value) override;
  void event_resolved(game &g, const event &e) override;
  void election_resolved(game &g, const std::vector<int> &points) override;
  void card_schemed(game &g, const player &p, const shenanigan &card) override;
  void shenanigan_buried(game &g, const player &p, const shenanigan &card) override;
  void card_revealed(game &g, const player &p, const shenanigan &card) override;
  void ether_stored(game &g, const player &p) override;
  void ether_spent(game &g, const player &p, unsigned amount) override;
  void spell_failed(game &g, const player &p, const shenanigan &card) override;
  void spell_discarded(game &g, const player &p, const shenanigan &card) override;
  void propaganda_modified(game &g, size_t position, int modification) override;
  void memory_lapse_received(game &g, const player &caster, const player &target) override;
  void cosmos_card_taken(game &g, const player &p, const shenanigan &card) override;
  void ether_leaked(game &g, const player &caster, const player &target) override;
  void immunity_gained(game &g, const player &p, unsigned charges) override;
  void immunity_triggered(game &g, const player &immune, const player &chooser) override;
  void immunity_expired(game &g, const player &p) override;
  void future_read(game &g, const player &p, const std::vector<event> &events) override;
  void event_skipped(game &g, const event &e) override;
  void forced_reveal(game &g, int count) override;
  void cards_drawn(game &g, const player &p, size_t count) override;

private:
  protocol_writer &w;
  json applied = json::array(); // propagandas of the event being resolved
};


// --- controllers -----------------------------------------------------------------

// A mage played outside of the engine: every decision is a question written
// on the output, answered on the input. An invalid answer gets an error and
// the question again; a second one stops the game (protocol_abort, exit 3).
// A closed input stops it too (exit 2).
class remote_controller : public controller {
public:
  remote_controller(protocol_writer &w, std::istream &in) : w(w), in(in) {}
  std::string strategy_name() const override { return "REMOTE"; }

  bool reveal_more(const decision &d) override;
  size_t scheme_card(const decision &d) override;
  size_t next_shenanigan(const decision &d) override;
  bool reveal_shenanigan(const decision &d, const shenanigan &card) override;
  unsigned ether_to_spend(const decision &d, const spell &s) override;
  size_t propaganda_target(const decision &d, int modification,
                           const std::vector<size_t> &present) override;
  size_t opponent_target(const decision &d, const std::vector<size_t> &seats) override;
  size_t cosmos_card(const decision &d, unsigned remaining) override;
  size_t ether_leak_target(const decision &d, const std::vector<size_t> &seats,
                           unsigned remaining) override;
  size_t divergence_branch(const decision &d, const event &divergence) override;
  size_t any_faction(const decision &d, int value,
                     const std::vector<faction> &allowed) override;

private:
  size_t ask(const decision &d, const question &q);
  protocol_writer &w;
  std::istream &in;
};

// A mage played by the engine (random_player, smart_player...): it decides
// as `inner` does, and every decision is announced ("decided").
class announced_controller : public controller {
public:
  announced_controller(std::unique_ptr<controller> inner, protocol_writer &w)
    : inner(std::move(inner)), w(w) {}
  std::string strategy_name() const override { return inner->strategy_name(); }
  stealth get_greed() const override { return inner->get_greed(); }

  bool reveal_more(const decision &d) override;
  size_t scheme_card(const decision &d) override;
  size_t next_shenanigan(const decision &d) override;
  bool reveal_shenanigan(const decision &d, const shenanigan &card) override;
  unsigned ether_to_spend(const decision &d, const spell &s) override;
  size_t propaganda_target(const decision &d, int modification,
                           const std::vector<size_t> &present) override;
  size_t opponent_target(const decision &d, const std::vector<size_t> &seats) override;
  size_t cosmos_card(const decision &d, unsigned remaining) override;
  size_t ether_leak_target(const decision &d, const std::vector<size_t> &seats,
                           unsigned remaining) override;
  size_t divergence_branch(const decision &d, const event &divergence) override;
  size_t any_faction(const decision &d, int value,
                     const std::vector<faction> &allowed) override;

private:
  void decided(const decision &d, const question &q, size_t choice);
  std::unique_ptr<controller> inner;
  protocol_writer &w;
};

#endif
