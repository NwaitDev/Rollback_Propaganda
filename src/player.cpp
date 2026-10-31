#include "../include/game_concepts.hpp"
#include <iostream>
#include <stdexcept>
#include <utility>

player::player(stealth greed, strategy strat) : greed(greed), strat(strat) {}

void player::sign_contract(enum faction f) { contract = f; }

void player::score_a_point() { this->points++; }

int player::get_score() { return points; }

faction player::get_contract() const { return contract; }

int player::get_greed() { return greed; }

void player::view() const {
  std::cout << "faction: ";
  switch (contract) {

  case RED:
    std::cout << "RED";
    break;
  case YELLOW:
    std::cout << "YELLOW";
    break;
  case BLUE:
    std::cout << "BLUE";
    break;
  case ANY:
    std::cout << "ANY?";
    break;
  case NONE:
    std::cout << "NONE???";
    break;
  }
  std::cout << " / strategy: ";
  switch (strat) {
  case RANDOM:
    std::cout << "RANDOM";
    break;
  case SMART:
    std::cout << "SMART";
    break;
  }
  std::cout << " / deck: " << deck_name;
  std::cout << " / greed: " << greed << " (score :" << points << ")";
  std::cout << " / grimory: " << deck.size()
            << ", hand: " << cards_in_hand.get_nb_cards();
}

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

event &player::spell_access(game &g, event_pile ep, size_t position) {
  switch (ep) {
  case PAST:
    return g.past_events.past[position];
  case PRESENT:
    return g.present_events.present[position];
  case FUTURE:
    return g.future_events.future[position];
  }
  throw std::runtime_error("unknown event pile");
}

std::vector<event *> player::present_propaganda(game &g) {
  std::vector<event *> propaganda;
  for (size_t i = 0; i < g.present_events.show_events().size(); ++i) {
    event &e = spell_access(g, PRESENT, i);
    if (e.is_propaganda_card()) {
      propaganda.push_back(&e);
    }
  }
  return propaganda;
}

std::vector<player *> player::opponents_with_ether(game &g) {
  std::vector<player *> targets;
  for (player *p : opponents(g)) {
    if (p->available_ether() > 0) {
      targets.push_back(p);
    }
  }
  return targets;
}

std::vector<player *> player::opponents(game &g) {
  std::vector<player *> others;
  for (std::unique_ptr<player> &p : g.get_players_ref()) {
    if (p.get() != this) {
      others.push_back(p.get());
    }
  }
  return others;
}


// --- shenanigan phases -------------------------------------------------------

void player::prepare(game &g) {
  if (cards_in_hand.get_nb_cards() == 0) {
    return; // nothing to scheme with this turn
  }
  size_t index = choose_card_to_scheme(g);
  cards_in_play.prepare_shenanigan(cards_in_hand.play_card(index));
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

bool player::cancels_choice_by(const player &chooser) {
  if (&chooser == this || immunity_charges == 0) {
    return false;
  }
  --immunity_charges;
  return true;
}

void player::end_turn() {
  immunity_charges = 0; // immunity only lasts for the current turn
}
