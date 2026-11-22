#ifndef __SETUP__
#define __SETUP__

#include "game_concepts.hpp"

#include <memory>
#include <string>
#include <vector>

// Setup of a game, shared by ./simulation, ./server and ./bot: the same seed
// and the same files give the same game, whatever program plays it.

const std::string default_deck = "decks/implemented.json";
const std::string default_contracts = "contracts/two_per_faction.json";
const std::string default_events = "events/base.json";

struct game_files {
  // either a single deck for every mage, or one deck per mage
  std::vector<std::string> deck_paths = {default_deck};
  std::string contracts_path = default_contracts;
  std::string events_path = default_events;
};

// Adds one mage per controller (in seat order), gives them their decks, fills
// the future, shuffles it, deals the contracts and the hands. The observers
// of the game are notified from the shuffle on.
void prepare_game(game &g, std::vector<std::unique_ptr<controller>> seats,
                  const game_files &files);

// parses an integer in [min, max], or throws std::invalid_argument
long long parse_integer(const std::string &value, long long min, long long max);

#endif
