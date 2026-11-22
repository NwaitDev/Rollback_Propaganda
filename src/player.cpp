#include "../include/game_concepts.hpp"
#include <stdexcept>
#include <utility>

player::player(std::unique_ptr<controller> c) : ctrl(std::move(c)) {
  if (!ctrl) {
    throw std::logic_error("a mage needs a controller");
  }
}

void controller::seed_for(unsigned game_seed, size_t seat) {
  std::seed_seq seq{game_seed, static_cast<unsigned>(seat), 0x524f4c4cu};
  generator.seed(seq);
}

void player::sign_contract(const contract &c) { signed_contract = c; }

void player::score_points(int amount) { points += amount; }

int player::get_score() const { return points; }

const contract &player::get_contract() const { return signed_contract; }


void player::give_starting_deck(starting_deck d) {
  deck_name = d.name;
  for (card_ptr &card : d.cards) {
    deck.put_at_bottom(std::move(card));
  }
}

void player::setup(game &g) {
  deck.shuffle(g.rng());
  refill_hand();
}

// --- shenanigan phases -------------------------------------------------------

void player::prepare(game &g) {
  if (cards_in_hand.get_nb_cards() == 0) {
    return; // nothing to scheme with this turn
  }
  size_t index = choose_card_to_scheme(g);
  card_ptr card = cards_in_hand.play_card(index);
  g.observer().card_schemed(g, *this, *card);
  cards_in_play.prepare_shenanigan(std::move(card));
}

void player::take_back_shenanigan() {
  for (card_ptr &card : cards_in_play.take_all_prepared()) {
    cards_in_hand.add_card(std::move(card));
  }
}

// Handles every schemed card, in the order chosen by the mage. A rollback
// during a spell gives the remaining ones back to the hand, which ends the
// loop.
void player::do_shenanigans(game &g) {
  while (cards_in_play.nb_prepared() > 0) {
    size_t index = (cards_in_play.nb_prepared() == 1) ? 0 : choose_next_shenanigan(g);
    card_ptr card = cards_in_play.take_prepared_shenanigan(index);
    if (!wants_to_reveal(g, *card)) {
      deck.put_at_bottom(std::move(card));
      continue;
    }
    g.observer().card_revealed(g, *this, *card);
    shenanigan &revealed = *card;
    revealed.resolve(std::move(card), g, *this);
  }
}


// --- used by the cards while they resolve ------------------------------------

void player::receive_card(card_ptr card) {
  cards_in_hand.add_card(std::move(card));
}

void player::store_ether(card_ptr card) {
  cards_in_play.store_ether(std::move(card));
}

unsigned player::available_ether() const {
  return cards_in_play.available_ether();
}

std::vector<card_ptr> player::spend_ether(unsigned amount) {
  return cards_in_play.spend_ether(amount);
}

card_ptr player::lose_ether() {
  return std::move(cards_in_play.spend_ether(1).front());
}

void player::remember_future(const std::vector<event> &seen) {
  for (const event &e : seen) {
    if (known_event(e.get_id()) == nullptr) {
      known_events.push_back(e);
    }
  }
}

const event *player::known_event(long id) const {
  for (const event &e : known_events) {
    if (e.get_id() == id) {
      return &e;
    }
  }
  return nullptr;
}

void player::gain_immunity(unsigned charges) {
  immunity_charges += charges;
}

bool player::cancels_choice_by(game &g, const player &chooser) {
  if (&chooser == this || immunity_charges == 0) {
    return false;
  }
  --immunity_charges;
  g.observer().immunity_triggered(g, *this);
  return true;
}

void player::end_turn() {
  immunity_charges = 0; // immunity only lasts for the current turn
}


// --- decisions -----------------------------------------------------------------
// Every answer of the controller is checked: a wrong answer is a bug of the
// controller (the server checks the answers of remote players beforehand).

decision player::decide(game &g) const {
  return decision{g, *this, g.seat_of(*this), ctrl->rng()};
}

bool player::wants_to_reveal_more(game &g) {
  return ctrl->reveal_more(decide(g));
}

size_t player::choose_card_to_scheme(game &g) {
  size_t index = ctrl->scheme_card(decide(g));
  if (index >= cards_in_hand.get_nb_cards()) {
    throw std::logic_error("a player chose a card out of their hand");
  }
  return index;
}

size_t player::choose_next_shenanigan(game &g) {
  size_t index = ctrl->next_shenanigan(decide(g));
  if (index >= cards_in_play.nb_prepared()) {
    throw std::logic_error("a player chose a schemed card that does not exist");
  }
  return index;
}

bool player::wants_to_reveal(game &g, const shenanigan &card) {
  return ctrl->reveal_shenanigan(decide(g), card);
}

unsigned player::choose_ether_to_spend(game &g, const spell &s) {
  unsigned ether = ctrl->ether_to_spend(decide(g), s);
  if (ether > available_ether()) {
    throw std::logic_error("a player chose to spend more ether than available");
  }
  return ether;
}

event *player::choose_propaganda(game &g, int modification) {
  std::vector<size_t> present = g.propaganda_in_present();
  if (present.empty()) {
    return nullptr;
  }
  size_t choice = ctrl->propaganda_target(decide(g), modification, present);
  if (choice >= present.size()) {
    throw std::logic_error("a player chose a propaganda that does not exist");
  }
  return &g.present_event_at(present[choice]);
}

player *player::choose_opponent(game &g) {
  std::vector<size_t> seats = g.opponents_of(g.seat_of(*this));
  if (seats.empty()) {
    return nullptr;
  }
  size_t choice = ctrl->opponent_target(decide(g), seats);
  if (choice >= seats.size()) {
    throw std::logic_error("a player chose an opponent that does not exist");
  }
  return g.get_players_ref()[seats[choice]].get();
}

size_t player::choose_cosmos_card(game &g) {
  size_t choice = ctrl->cosmos_card(decide(g));
  if (choice >= g.cosmos_cards.size()) {
    throw std::logic_error("a player chose a card out of the cosmos");
  }
  return choice;
}

player *player::choose_ether_to_leak(game &g) {
  std::vector<size_t> seats = g.opponents_with_ether(g.seat_of(*this));
  size_t choice = ctrl->ether_leak_target(decide(g), seats);
  if (choice > seats.size()) {
    throw std::logic_error("a player chose an invalid ether to leak");
  }
  return (choice == seats.size()) ? nullptr : g.get_players_ref()[seats[choice]].get();
}

size_t player::choose_divergence_branch(game &g, const event &divergence) {
  size_t branch = ctrl->divergence_branch(decide(g), divergence);
  if (branch > 1) {
    throw std::logic_error("a player chose an invalid branch of a divergence");
  }
  return branch;
}

faction player::choose_faction_for_any(game &g, int value, const std::vector<faction> &allowed) {
  size_t choice = ctrl->any_faction(decide(g), value, allowed);
  if (choice >= allowed.size()) {
    throw std::logic_error("a player chose a forbidden faction for an ANY propaganda");
  }
  return allowed[choice];
}
