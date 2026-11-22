#include "../include/game_concepts.hpp"

// observer_list: every notification goes to every observer, in order

void observer_list::future_shuffled(game &g) {
  for (game_observer *o : observers) o->future_shuffled(g);
}
void observer_list::game_started(game &g) {
  for (game_observer *o : observers) o->game_started(g);
}
void observer_list::game_finished(game &g) {
  for (game_observer *o : observers) o->game_finished(g);
}
void observer_list::game_blocked(game &g) {
  for (game_observer *o : observers) o->game_blocked(g);
}
void observer_list::turn_started(game &g) {
  for (game_observer *o : observers) o->turn_started(g);
}
void observer_list::phase_started(game &g, game_phase phase) {
  for (game_observer *o : observers) o->phase_started(g, phase);
}
void observer_list::turn_ended(game &g, turn_outcome outcome) {
  for (game_observer *o : observers) o->turn_ended(g, outcome);
}
void observer_list::event_revealed(game &g, const event &e) {
  for (game_observer *o : observers) o->event_revealed(g, e);
}
void observer_list::collapse_revealed(game &g) {
  for (game_observer *o : observers) o->collapse_revealed(g);
}
void observer_list::rolled_back(game &g, turn_outcome cause, const std::vector<long> &returned) {
  for (game_observer *o : observers) o->rolled_back(g, cause, returned);
}
void observer_list::shenanigan_returned(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->shenanigan_returned(g, p, card);
}
void observer_list::propaganda_applied(game &g, faction f, int value) {
  for (game_observer *o : observers) o->propaganda_applied(g, f, value);
}
void observer_list::event_resolved(game &g, const event &e) {
  for (game_observer *o : observers) o->event_resolved(g, e);
}
void observer_list::election_resolved(game &g, const std::vector<int> &points) {
  for (game_observer *o : observers) o->election_resolved(g, points);
}
void observer_list::card_schemed(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->card_schemed(g, p, card);
}
void observer_list::card_kept_in_hand(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->card_kept_in_hand(g, p, card);
}
void observer_list::shenanigan_buried(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->shenanigan_buried(g, p, card);
}
void observer_list::card_revealed(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->card_revealed(g, p, card);
}
void observer_list::ether_stored(game &g, const player &p) {
  for (game_observer *o : observers) o->ether_stored(g, p);
}
void observer_list::ether_spent(game &g, const player &p, unsigned amount) {
  for (game_observer *o : observers) o->ether_spent(g, p, amount);
}
void observer_list::spell_failed(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->spell_failed(g, p, card);
}
void observer_list::spell_discarded(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->spell_discarded(g, p, card);
}
void observer_list::propaganda_modified(game &g, size_t position, int modification) {
  for (game_observer *o : observers) o->propaganda_modified(g, position, modification);
}
void observer_list::memory_lapse_received(game &g, const player &caster, const player &target) {
  for (game_observer *o : observers) o->memory_lapse_received(g, caster, target);
}
void observer_list::cosmos_card_taken(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->cosmos_card_taken(g, p, card);
}
void observer_list::ether_leaked(game &g, const player &caster, const player &target) {
  for (game_observer *o : observers) o->ether_leaked(g, caster, target);
}
void observer_list::immunity_gained(game &g, const player &p, unsigned charges) {
  for (game_observer *o : observers) o->immunity_gained(g, p, charges);
}
void observer_list::immunity_triggered(game &g, const player &immune, const player &chooser) {
  for (game_observer *o : observers) o->immunity_triggered(g, immune, chooser);
}
void observer_list::immunity_expired(game &g, const player &p) {
  for (game_observer *o : observers) o->immunity_expired(g, p);
}
void observer_list::future_read(game &g, const player &p, const std::vector<event> &events) {
  for (game_observer *o : observers) o->future_read(g, p, events);
}
void observer_list::event_skipped(game &g, const event &e) {
  for (game_observer *o : observers) o->event_skipped(g, e);
}
void observer_list::forced_reveal(game &g, int count) {
  for (game_observer *o : observers) o->forced_reveal(g, count);
}
void observer_list::cards_drawn(game &g, const player &p, size_t count) {
  for (game_observer *o : observers) o->cards_drawn(g, p, count);
}
