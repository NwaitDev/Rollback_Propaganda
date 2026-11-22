#include "../include/text_view.hpp"

#include <vector>

std::string describe(const player &p) {
  const contract &c = p.get_contract();
  std::string text = "contract: " + (c.name.empty() ? std::string("none") : c.name);
  text += " / strategy: ";
  text += p.get_controller().strategy_name();
  text += " / deck: " + p.get_deck_name();
  text += " / greed: " + std::to_string(p.get_controller().get_greed())
          + " (score :" + std::to_string(p.get_score()) + ")";
  text += " / grimory: " + std::to_string(p.grimory_size())
          + ", hand: " + std::to_string(p.get_hand().get_nb_cards());
  return text;
}

namespace {

template <class cards>
void print_pile(std::ostream &out, const std::string &title, const cards &pile) {
  out << "(" << title << ")\n";
  size_t i = 0;
  for (const event &card : pile) {
    out << "card n°" << i++ << ": " << card.describe() << std::endl;
  }
}

} // namespace

void print_game(std::ostream &out, game &g) {
  out << "nb players: " << g.get_players_ref().size() << "." << std::endl;
  for (const std::unique_ptr<player> &p : g.get_players_ref()) {
    out << describe(*p) << std::endl;
  }
  print_pile(out, "future", g.future_events.show_events());
  print_pile(out, "present", g.present_events.show_events());
  print_pile(out, "past", g.past_events.show_events());
  print_pile(out, "oblivion", g.oblivion);
}


// --- text_logger ---------------------------------------------------------------

void text_logger::future_shuffled(game &g) {
  out << "shuffling the future_events!" << std::endl;
  out << "shuffling the future!" << std::endl;
}

void text_logger::game_started(game &g) {
  out << "The game is running..." << std::endl;
}

void text_logger::game_finished(game &g) {
  out << "the game is over!" << std::endl;
}

void text_logger::game_blocked(game &g) {
  out << "the game is blocked after " << g.get_elapsed_turns() << " turns!" << std::endl;
}

void text_logger::turn_started(game &g) {
  out << "turn:" << g.get_elapsed_turns()
      << ".\tremaining cards: " << g.future_events.remaining_cards() << std::endl;
}

// an event that pushes the stealth over the limit is announced by the
// rollback only
void text_logger::event_revealed(game &g, const event &e) {
  if (g.present_events.get_stealth() > g.get_max_stealth()) {
    return;
  }
  switch (e.get_kind()) {
  case OPERATOR:
    out << "multi_propaganda!" << std::endl;
    break;
  case PROPAGANDA:
    out << "propaganda!" << std::endl;
    break;
  case ELECTION:
    out << "election!" << std::endl;
    break;
  case MISTAKE:
    out << "mistake!" << std::endl;
    break;
  case EXPLOSION:
    out << "explosion!" << std::endl;
    break;
  case COLLAPSE:
    break; // see collapse_revealed
  }
}

void text_logger::rolled_back(game &g, turn_outcome cause, const std::vector<long> &returned) {
  if (cause == turn_outcome::STEALTH_ROLLBACK) {
    out << "forced rollback!" << std::endl;
  }
}

void text_logger::collapse_revealed(game &g) {
  out << "forced execute!" << std::endl;
}

void text_logger::election_resolved(game &g, const std::vector<int> &points) {
  out << "ELECTION EXECUTEE" << std::endl;
}
