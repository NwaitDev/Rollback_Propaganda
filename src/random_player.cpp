#include "../include/game_concepts.hpp"
#include <vector>

random_player::random_player(stealth greed) : player(greed, RANDOM) {}

// --- decisions: every choice is made at random ------------------------------

// the only decision that is not random: reveals until the stealth reaches
// the player's greed
bool random_player::wants_to_reveal_more(game &g) {
  return g.present_events.get_stealth() < get_greed();
}

size_t random_player::choose_card_to_scheme(game &g) {
  return g.random_below(cards_in_hand.get_nb_cards());
}

size_t random_player::choose_next_shenanigan(game &g) {
  return g.random_below(cards_in_play.nb_prepared());
}

bool random_player::wants_to_reveal(game &g, const shenanigan &card) {
  return g.random_below(2) == 0;
}

unsigned random_player::choose_ether_to_spend(game &g, const spell &s) {
  return g.random_below(available_ether() + 1);
}

event *random_player::choose_propaganda(game &g, int modification) {
  std::vector<event *> candidates = present_propaganda(g);
  if (candidates.empty()) {
    return nullptr;
  }
  return candidates[g.random_below(candidates.size())];
}

player *random_player::choose_opponent(game &g) {
  std::vector<player *> candidates = opponents(g);
  if (candidates.empty()) {
    return nullptr;
  }
  return candidates[g.random_below(candidates.size())];
}

size_t random_player::choose_cosmos_card(game &g) {
  return g.random_below(g.cosmos_cards.size());
}

// leaks an ether of a random opponent, or stops at random
player *random_player::choose_ether_to_leak(game &g) {
  std::vector<player *> candidates = opponents_with_ether(g);
  size_t choice = g.random_below(candidates.size() + 1);
  if (choice == candidates.size()) {
    return nullptr;
  }
  return candidates[choice];
}

size_t random_player::choose_divergence_branch(game &g, const event &divergence) {
  return g.random_below(2);
}

faction random_player::choose_faction_for_any(game &g, int value,
                                              const std::vector<faction> &allowed) {
  return allowed[g.random_below(allowed.size())];
}
