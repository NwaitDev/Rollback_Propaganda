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
void observer_list::event_revealed(game &g, const event &e) {
  for (game_observer *o : observers) o->event_revealed(g, e);
}
void observer_list::rolled_back(game &g, turn_outcome cause) {
  for (game_observer *o : observers) o->rolled_back(g, cause);
}
void observer_list::collapse_revealed(game &g) {
  for (game_observer *o : observers) o->collapse_revealed(g);
}
void observer_list::election_resolved(game &g) {
  for (game_observer *o : observers) o->election_resolved(g);
}
void observer_list::card_schemed(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->card_schemed(g, p, card);
}
void observer_list::card_kept_in_hand(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->card_kept_in_hand(g, p, card);
}
void observer_list::card_revealed(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->card_revealed(g, p, card);
}
void observer_list::spell_failed(game &g, const player &p, const shenanigan &card) {
  for (game_observer *o : observers) o->spell_failed(g, p, card);
}
void observer_list::event_resolved(game &g, const event &e) {
  for (game_observer *o : observers) o->event_resolved(g, e);
}
void observer_list::memory_lapse_received(game &g, const player &target) {
  for (game_observer *o : observers) o->memory_lapse_received(g, target);
}
void observer_list::ether_leaked(game &g, const player &target) {
  for (game_observer *o : observers) o->ether_leaked(g, target);
}
void observer_list::immunity_triggered(game &g, const player &immune) {
  for (game_observer *o : observers) o->immunity_triggered(g, immune);
}
void observer_list::turn_ended(game &g, turn_outcome outcome) {
  for (game_observer *o : observers) o->turn_ended(g, outcome);
}
