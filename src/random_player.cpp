#include "../include/game_concepts.hpp"
#include <vector>

// --- decisions: every choice is made at random ------------------------------

// the only decision that is not random: reveals until the stealth reaches
// the player's greed
bool random_player::reveal_more(const decision &d) {
  return d.g.present_events.get_stealth() < greed;
}

size_t random_player::scheme_card(const decision &d) {
  return d.random_below(d.self.get_hand().get_nb_cards());
}

size_t random_player::next_shenanigan(const decision &d) {
  return d.random_below(d.self.get_lair().nb_prepared());
}

bool random_player::reveal_shenanigan(const decision &d, const shenanigan &card) {
  return d.random_below(2) == 0;
}

unsigned random_player::ether_to_spend(const decision &d, const spell &s) {
  return d.random_below(d.self.available_ether() + 1);
}

size_t random_player::propaganda_target(const decision &d, int modification,
                                        const std::vector<size_t> &present) {
  return d.random_below(present.size());
}

size_t random_player::opponent_target(const decision &d, const std::vector<size_t> &seats) {
  return d.random_below(seats.size());
}

size_t random_player::cosmos_card(const decision &d) {
  return d.random_below(d.g.cosmos_cards.size());
}

// leaks an ether of a random opponent, or stops at random
size_t random_player::ether_leak_target(const decision &d, const std::vector<size_t> &seats) {
  return d.random_below(seats.size() + 1);
}

size_t random_player::divergence_branch(const decision &d, const event &divergence) {
  return d.random_below(2);
}

size_t random_player::any_faction(const decision &d, int value,
                                  const std::vector<faction> &allowed) {
  return d.random_below(allowed.size());
}
