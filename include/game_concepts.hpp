#ifndef __GAME_CONCEPTS__
#define __GAME_CONCEPTS__

#include <cstddef>
#include <list>
#include <memory>
#include <vector>

using stealth = int;

class event;
class future;
class past;
class present;
class shenanigan;
class game;
class player;

enum event_kind {
  OPERATOR,
  PROPAGANDA,
  ELECTION,
  EXPLOSION,
  COLLAPSE,
  MISTAKE
};

const int factions_count = 3;
enum faction {
  RED,    // the fellowship
  YELLOW, // the empire
  BLUE,   // the architects
  ANY,    // any one of the previously mentionned factions
  NONE    // neutral event
};

enum event_operator {
  CONVERGENCE, // AND operator for events
  DIVERGENCE   // exclusive OR operator for events
};

enum strategy { RANDOM, SMART };

enum shenanigan_type { NOTHING, ETHER, SPELL };

enum spell_type {
  EUPHORIA,
  NEURASTHENIA,
  PRECIPITATION,
  MEMORY_LAPSE,
  READING,
  ENTROPY_REVERSAL,
  ETHER_LEAK,
  IMMUNITY,
  CHAPTER_SKIP
};

enum event_pile { PAST, PRESENT, FUTURE };

class event {
public:
  event(faction faction, int value, stealth stealth);
  event(event_kind ekind, stealth stealth, int value);
  event(event_operator etype, event &left, event &right);

  void gets_targeted_by(shenanigan spell, int spent_ether);
  void view() const;
  stealth get_stealth() const;
  int get_value() const;
  event_kind get_kind() const;
  enum faction get_faction() const;
  enum event_operator get_operator() const;
  event &get_left();
  event &get_right();

  void modify(const int modif);
  inline bool is_modified() { return modification != 0; };
  void reset();

private:
  int value;
  stealth st;
  event_kind kind;
  union type {
    enum faction faction;
    event_operator op;
  } type;
  event *left;
  event *right;
  int modification;
};

class shenanigan {
public:
  shenanigan(int ether_val);
  shenanigan(spell_type spell);
  shenanigan();
  shenanigan_type get_shtype();
  int get_ether_value();
  spell_type get_spell_type();

private:
  shenanigan_type shtype;
  union {
    int ether_value;
    spell_type spell;
  } value;
};

class grimory {
public:
  grimory();
  shenanigan draw();
  void shuffle();
  void put_at_bottom(shenanigan card);
  inline bool is_empty() { return cards.empty(); }

private:
  std::list<shenanigan> cards = std::list<shenanigan>();
};

class hand {
public:
  hand(size_t cards_limit);
  void add_card(shenanigan card);
  shenanigan play_card(size_t index);
  size_t get_nb_cards();
  void draw_up_to_limit(grimory);

private:
  size_t cards_limit;
  std::vector<shenanigan> cards = std::vector<shenanigan>();
};

class lair {
public:
  void store_ether(shenanigan card);
  shenanigan discard_ether();
  std::list<shenanigan> cast_spell(shenanigan card, size_t consumed_ether);
  void prepare_shenanigan(shenanigan card);
  shenanigan reveal_shenanigan();
  inline unsigned int available_ether() { return ether_pool.size(); }

private:
  std::list<shenanigan> ether_pool = std::list<shenanigan>();
  shenanigan prepared_shenanigan = NULL;
};


class cosmos{
	friend class player;
public:
	void absorb(shenanigan);
	const std::vector<shenanigan> show_shenanigans(){return cosmos;}
private:
	std::vector<shenanigan> cosmos = std::vector<shenanigan>();
};

class player {
public:
  player(faction faction, stealth greed, strategy strat);
  void score_a_point();
  void view() const;
  int get_score();
  enum faction get_contract() const;
  int get_greed();
  event spell_access(game &g, event_pile ep, size_t position);
  shenanigan spell_cosmos_access(game &g, size_t position);
  virtual void execute(const event, game &) const;
  virtual void prepare();
  virtual void do_shenanigans(game &g);

protected:
  grimory deck;
  hand cards_in_hand = hand(4);
  lair cards_in_play;
  enum faction contract;

  virtual bool cast_euphoria(game &g, int ether);
  virtual bool cast_neurasthenia(game &g, int ether);
  virtual bool cast_precipitation(game &g, int ether);
  virtual bool cast_memorylapse(game &g);
  virtual bool cast_reading(game &g, int ether) const;
  virtual bool cast_entropyreversal(game &g, int ether);
  virtual bool cast_etherleak(game &g, int ether);
  virtual bool cast_immunity(game &g, int ether);
  virtual bool cast_chapterskip(game &g, int ether);

private:
  stealth greed;
  int points = 0;
  strategy strat;
  virtual void execute_any_any(game &, const event, const event) const;
  virtual void execute_any_else(game &, const event, const event) const;
};

class random_player : public player {
public:
  random_player(faction f, stealth greed);
  void execute(event, game &) const override;
  void prepare() override;
  void do_shenanigans(game &g) override;

protected:
  virtual bool cast_euphoria(game &g, int ether) override;
  virtual bool cast_neurasthenia(game &g, int ether) override;
  virtual bool cast_precipitation(game &g, int ether) override;
  virtual bool cast_memorylapse(game &g) override;
  virtual bool cast_reading(game &g, int ether) const override;
  virtual bool cast_entropyreversal(game &g, int ether) override;
  virtual bool cast_etherleak(game &g, int ether) override;
  virtual bool cast_immunity(game &g, int ether) override;
  virtual bool cast_chapterskip(game &g, int ether) override;

private:
  void execute_any_any(game &, const event, const event) const override;
  void execute_any_else(game &, const event, const event) const override;
};

class smart_player : public player {
public:
  smart_player(faction f, stealth greed);
  void execute(event, game &) const override;

private:
  void execute_any_any(game &, const event, const event) const override;
  void execute_any_else(game &, const event, const event) const override;
  void prepare() override;
  void do_shenanigans(game &g) override;
};

class present {
  friend class player;

public:
  stealth append_from_future(future &future);
  void rollback(future &future);
  const event execute(game &g, player &controller);
  inline stealth get_stealth() const {return current_stealth;}
  inline void set_stealth(const stealth &st) {current_stealth = st;}
  const event get_last_event();
  void discard_last();
  const event draw_last();
  inline bool is_empty() const {return present.empty();};
  void view();
  inline const std::vector<event> show_events() {return present;};

private:
  std::vector<event> present = std::vector<event>();
  stealth current_stealth = 0;
  bool internally_reversed_for_reallocation_avoidance_purpose;
  void reverse_for_reallocation_avoidance_purpose();
};

class future {
  friend class player;

public:
  const event draw();
  void put_on_top(const event &event);
  size_t remaining_cards();
  void shuffle();
  void view();

private:
  std::vector<event> future = std::vector<event>();
};

class past {
  friend class player;

public:
  void pass(const event &executed_event);
  void view() const;

private:
  std::vector<event> past = std::vector<event>();
};

class game {
public:
  game(stealth max_stealth);
  bool current_player_wants_to_play_and_can();
  void add_player(std::unique_ptr<player> p);
  void add_event(const event &e);
  void play_turn();
  void play_game();
  void view();

  inline std::vector<int> &get_scores_ref() { return scores; };
  inline std::vector<std::unique_ptr<player>> &get_players_ref() {
    return players;
  };

  size_t current_player = 0;

  void shuffle();

  past past_events;
  present present_events;
  future future_events;
  cosmos cosmos;

private:
  void preparation_phase();
  void reveal_phase();
  void shenanigan_phase();
  void execution_phase();
  void cleanup_phase();

  std::vector<int> scores = std::vector<int>({0, 0, 0});
  std::vector<std::unique_ptr<player>> players =
      std::vector<std::unique_ptr<player>>();
  stealth max_stealth;
  long elapsed_turns = 0;
};

#endif