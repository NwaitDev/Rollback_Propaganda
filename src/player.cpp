#include "../include/game_concepts.hpp"
#include <iostream>

player::player(enum faction faction, stealth greed, strategy strat)
    : contract(faction), greed(greed), strat(strat) {}

void player::execute(const event, game &) const {
  std::cout << "shouldn't appear" << std::endl;
}

void player::execute_any_any(game &, const event, const event) const {
  std::cout << "shouldn't appear" << std::endl;
}

void player::execute_any_else(game &, const event, const event) const {
  std::cout << "shouldn't appear" << std::endl;
}

void prepare() { std::cout << "shouldn't appear" << std::endl; };
void do_shenanigans(game &g) { std::cout << "shouldn't appear" << std::endl; };

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
  std::cout << " / greed: " << greed << " (score :" << points << ")";
}

event player::spell_access(game &g, event_pile ep, size_t position) {
  switch (ep) {
  case PAST:
    return g.past_events.past[position];
  case PRESENT:
    return g.present_events.present[position];
  case FUTURE:
    return g.future_events.future[position];
  }
}

shenanigan player::spell_cosmos_access(game &g, size_t position){
  return g.cosmos.cosmos[position];
}