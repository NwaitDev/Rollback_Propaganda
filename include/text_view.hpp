#ifndef __TEXT_VIEW__
#define __TEXT_VIEW__

#include "game_concepts.hpp"

#include <ostream>
#include <string>

// Human-readable text of a game, as printed by ./simulation. The game itself
// prints nothing: it notifies its observers, and text_logger is the one that
// writes what happens.

// "contract: ... / strategy: ... / deck: ... / greed: ... (score :...) / ..."
std::string describe(const player &p);

// the mages, then the future, the present, the past and the oblivion
void print_game(std::ostream &out, game &g);

// writes every step of a game as it unfolds
class text_logger : public game_observer {
public:
  explicit text_logger(std::ostream &out) : out(out) {}

  void future_shuffled(game &g) override;
  void game_started(game &g) override;
  void game_finished(game &g) override;
  void game_blocked(game &g) override;
  void turn_started(game &g) override;
  void event_revealed(game &g, const event &e) override;
  void rolled_back(game &g, turn_outcome cause, const std::vector<long> &returned) override;
  void collapse_revealed(game &g) override;
  void election_resolved(game &g, const std::vector<int> &points) override;

private:
  std::ostream &out;
};

#endif
