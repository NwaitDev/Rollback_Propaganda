#ifndef __CAMPAIGN__
#define __CAMPAIGN__

#include "game_concepts.hpp"
#include <map>
#include <string>
#include <vector>

// A campaign plays many games with the same settings, described by an
// experiment file (see experiments/), and records their results and metrics.
// The metrics are defined in metrics/columns.csv (M1 to M22).

enum class game_type { RANDOM_ONLY, WITH_SMART };

std::string type_name(game_type type);

struct experiment {
  std::string name;
  unsigned seed;            // seed of the campaign: every game seed derives from it
  long random_only_games;   // games between random players only
  long with_smart_games;    // games between random players and one smart player
  size_t players;
  // greed levels of the random players: the campaign plays its games once
  // for each level (with the same seeds)
  std::vector<stealth> random_greeds;
  bool smart_greed_given;   // optional; by default, the smart player's greed
  stealth smart_greed_fixed;//   is the random players' one
  size_t hand_size;
  stealth max_stealth;      // optional, 21 by default
  int popularity;           // optional, starting popularity, 0 by default
  long max_turns;           // optional, 1000 by default: beyond, a game is blocked
  std::string output;       // optional, results/<name> by default
  // optional: a card added to the grimory of a single player (M8), the only
  // difference between the grimories; empty if none
  std::string extra_card;

  std::string deck_path, events_path, contracts_path;
  // the files' content, loaded once for the whole campaign
  std::string deck_name;
  std::vector<std::string> deck_cards;
  std::vector<event> events;
  std::vector<contract> contracts;

  // greed of the games being played (one of random_greeds)
  stealth random_greed = 0;
  stealth smart_greed = 0;
  // a copy of this experiment playing at this greed level
  experiment with_greed(stealth greed) const;
};

// reads and checks an experiment file, and loads the files it refers to
experiment load_experiment(const std::string &path);


// --- metrics of one game ---------------------------------------------------------

struct card_stats {
  long schemed = 0;
  long revealed = 0;
  long failed = 0;          // revealed spells without effect, for lack of a target
  long dead_leading = 0;    // turns kept in hand while its holder leads alone (points)
  long dead_trailing = 0;   // turns kept in hand otherwise
};

// Counts what happens during a game (see game_observer).
class game_stats : public game_observer {
public:
  game_stats(const std::vector<int> &starting_popularity, size_t players);

  void card_schemed(game &g, const player &p, const shenanigan &card) override;
  void card_kept_in_hand(game &g, const player &p, const shenanigan &card) override;
  void card_revealed(game &g, const player &p, const shenanigan &card) override;
  void spell_failed(game &g, const player &p, const shenanigan &card) override;
  void event_resolved(game &g, const event &e) override;
  void memory_lapse_received(game &g, const player &caster, const player &target) override;
  void ether_leaked(game &g, const player &caster, const player &target) override;
  void immunity_triggered(game &g, const player &immune, const player &chooser) override;
  void turn_ended(game &g, turn_outcome outcome) override;

  long resolved_turns = 0;      // turns without rollback
  long stealth_rollbacks = 0;   // rollbacks for exceeding the stealth limit
  long elections = 0;           // elections resolved
  long events_resolved = 0;
  long lead_changes = 0;        // changes of the leading faction (a tie counts as one)
  int max_popularity;           // highest popularity reached by a faction
  long memory_lapses = 0;
  long ethers_leaked = 0;
  long immunities = 0;
  // by seat: last turn at the end of which the mage did not have the most
  // points (ties included in "the most"); 0 if none
  std::vector<long> last_turn_behind;
  std::map<std::string, card_stats> cards;

private:
  faction leader;               // the faction leading alone, NONE on a tie
};


// --- results of one game -----------------------------------------------------------

struct game_result {
  game_type type;
  long index;               // index of the game among the games of its type
  unsigned seed;
  stealth random_greed;
  long smart_seat;          // seat of the smart player, -1 if there is none
  long holder_seat;         // seat holding the extra card, -1 if there is none
  bool finished;            // false if the game was blocked (see max_turns)
  long turns;
  long winner;              // seat of the winner, -1 for a blocked game
  tiebreak how_won = tiebreak::NONE;
  std::vector<std::string> strategies;  // by seat
  std::vector<std::string> contracts;   // names, by seat
  std::vector<int> points;              // by seat
  std::vector<int> popularity;          // final popularity, by faction
  std::vector<size_t> end_grimory;      // by seat
  std::vector<size_t> end_hand;         // by seat
  size_t end_cosmos = 0;
  game_stats stats;

  game_result(const std::vector<int> &starting_popularity, size_t players)
    : stats(starting_popularity, players) {}
};

// seed of every game of the campaign, in the order the games are played:
// first the random-only games, then the games with a smart player (the same
// seeds for every greed level)
std::vector<unsigned> game_seeds(const experiment &exp);

// plays one game; with verbose, the game prints what happens
game_result run_game(const experiment &exp, game_type type, long index,
                     unsigned seed, bool verbose);

// plays every game, writes in exp.output games.csv (one line per game),
// campaigns.csv and details.csv (the metrics), and prints a summary
void run_campaign(const experiment &exp, const std::string &experiment_path);

#endif
