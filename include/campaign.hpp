#ifndef __CAMPAIGN__
#define __CAMPAIGN__

#include "game_concepts.hpp"
#include <string>
#include <vector>

// A campaign plays many games with the same settings, described by an
// experiment file (see experiments/), and records their results.

enum class game_type { RANDOM_ONLY, WITH_SMART };

std::string type_name(game_type type);

struct experiment {
  std::string name;
  unsigned seed;            // seed of the campaign: every game seed derives from it
  long random_only_games;   // games between random players only
  long with_smart_games;    // games between random players and one smart player
  size_t players;
  stealth random_greed;
  stealth smart_greed;      // optional, random_greed by default
  size_t hand_size;
  stealth max_stealth;      // optional, 21 by default
  int popularity;           // optional, starting popularity, 0 by default
  long max_turns;           // optional, 1000 by default: beyond, a game is blocked
  std::string output;       // optional, results/<name> by default

  std::string deck_path, events_path, contracts_path;
  // the files' content, loaded once for the whole campaign
  std::string deck_name;
  std::vector<std::string> deck_cards;
  std::vector<event> events;
  std::vector<faction> contracts;
};

// reads and checks an experiment file, and loads the files it refers to
experiment load_experiment(const std::string &path);

struct game_result {
  game_type type;
  long index;               // index of the game among the games of its type
  unsigned seed;
  long smart_seat;          // seat of the smart player, -1 if there is none
  bool finished;            // false if the game was blocked (see max_turns)
  long turns;
  long winner;              // seat of the winner, -1 for a blocked game
  std::vector<std::string> strategies;  // by seat
  std::vector<faction> contracts;       // by seat
  std::vector<int> points;              // by seat
  std::vector<int> popularity;          // final popularity, by faction
};

// seed of every game of the campaign, in the order the games are played:
// first the random-only games, then the games with a smart player
std::vector<unsigned> game_seeds(const experiment &exp);

// plays one game; with verbose, the game prints what happens
game_result run_game(const experiment &exp, game_type type, long index,
                     unsigned seed, bool verbose);

// plays every game, writes the results in exp.output and prints a summary
void run_campaign(const experiment &exp, const std::string &experiment_path);

#endif
