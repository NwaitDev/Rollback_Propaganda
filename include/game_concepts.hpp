#ifndef __GAME_CONCEPTS__
#define __GAME_CONCEPTS__

#include <cstddef>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>

using stealth = int;

// Every random draw of a game uses the generator of that game, so that a game
// can be replayed from its seed. std::mt19937 gives the same numbers with
// every compiler; std::shuffle and the std distributions do not, hence these
// two helpers.
inline size_t random_below(std::mt19937 &rng, size_t n) { // in [0, n)
  return rng() % n;
}

template <class container>
void shuffle_with(container &cards, std::mt19937 &rng) {
  for (size_t i = cards.size(); i > 1; --i) {
    std::swap(cards[i - 1], cards[random_below(rng, i)]);
  }
}

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
  RED,    /// the fellowship
  YELLOW, /// the empire
  BLUE,   /// the architects
  ANY,    // any one of the previously mentionned factions
  NONE    // neutral event
};

enum event_operator {
  CONVERGENCE, // AND operator for events
  DIVERGENCE   // exclusive OR operator for events
};

enum strategy { RANDOM, SMART };

enum event_pile { PAST, PRESENT, FUTURE };

class event {
public:
  event(faction faction, int value, stealth stealth);
  event(event_kind ekind, stealth stealth, int value);
  event(event_operator etype, const event &left, const event &right);
  // copies are deep: a copy of a multiple propaganda has its own branches
  event(const event &other);
  event &operator=(const event &other);
  event(event &&other) = default;
  event &operator=(event &&other) = default;

  void view() const;
  stealth get_stealth() const;
  int get_value() const;
  event_kind get_kind() const;
  // a simple propaganda, or a multiple one (AND / OR) made of propagandas
  bool is_propaganda_card() const;
  enum faction get_faction() const;
  enum event_operator get_operator() const;
  event &get_left();
  event &get_right();
  const event &get_left() const;
  const event &get_right() const;

  // modifications add up; a card loses them when it changes zone (it is
  // reset when put in the future or the past)
  // identity of a card of the game (copies share it), -1 for a branch
  inline long get_id() const { return id; }
  inline void set_id(long new_id) { id = new_id; }

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
  std::shared_ptr<event> left;
  std::shared_ptr<event> right;
  int modification;
  long id = -1;
};

// --- shenanigans (grimory cards) ------------------------------------------
// Every grimory card derives from shenanigan. The game model only calls
// resolve() on a revealed card: each card class holds its own rule and asks
// its owner for the choices it needs (see the decisions of class player).
// The concrete cards are defined in shenanigans.cpp.

using card_ptr = std::unique_ptr<shenanigan>;

class shenanigan {
public:
  virtual ~shenanigan() = default;
  virtual std::string name() const = 0;

  // Applies the card once revealed by its owner. The card receives its own
  // ownership and moves itself where the rules send it (the owner's lair,
  // the cosmos, another mage's hand...).
  virtual void resolve(card_ptr self, game &g, player &owner) = 0;
};

// Ether: kept face up in its owner's lair, then spent to strengthen spells.
class ether : public shenanigan {
public:
  std::string name() const override { return "ETHER"; }
  void resolve(card_ptr self, game &g, player &owner) override;
};

// Spell: its owner chooses how much ether to spend, the effect is applied if
// possible, then the spell is discarded to the cosmos. The spent ether is
// discarded to the cosmos only if the effect applied.
class spell : public shenanigan {
public:
  void resolve(card_ptr self, game &g, player &caster) override;

protected:
  // returns false when the effect cannot apply (nothing to target...)
  virtual bool effect(game &g, player &caster, unsigned ether) = 0;
};

// creates a card from its name in the deck files, or nullptr if unknown
card_ptr make_card(const std::string &name);


class grimory {
public:
  card_ptr draw();
  void shuffle(std::mt19937 &rng);
  void put_at_bottom(card_ptr card);
  inline bool is_empty() const { return cards.empty(); }
  inline size_t size() const { return cards.size(); }

private:
  std::deque<card_ptr> cards; // back() is the top of the grimory
};

class hand {
public:
  hand(size_t cards_limit);
  void add_card(card_ptr card);
  card_ptr play_card(size_t index);
  const shenanigan &card_at(size_t index) const;
  size_t get_nb_cards() const;
  inline void set_limit(size_t limit) { cards_limit = limit; }
  void draw_up_to_limit(grimory &deck);

private:
  size_t cards_limit;
  std::vector<card_ptr> cards;
};

class lair {
public:
  void prepare_shenanigan(card_ptr card);
  inline size_t nb_prepared() const { return prepared.size(); }
  inline const shenanigan &prepared_at(size_t index) const { return *prepared.at(index); }
  card_ptr take_prepared_shenanigan(size_t index);
  std::vector<card_ptr> take_all_prepared();
  void store_ether(card_ptr card);
  std::vector<card_ptr> spend_ether(unsigned amount);
  inline unsigned available_ether() const { return ether_pool.size(); }

private:
  std::vector<card_ptr> ether_pool;
  std::vector<card_ptr> prepared; // schemed this turn, not handled yet
};


// a preselected set of grimory cards, read from a JSON file (see decks/)
struct starting_deck {
  std::string name;
  std::vector<card_ptr> cards;
};

starting_deck load_deck(const std::string &path);

// contract cards, read from a JSON file (see contracts/)
std::vector<faction> load_contracts(const std::string &path);

// event cards, read from a JSON file (see events/ and deck_loader.cpp)
std::vector<event> load_events(const std::string &path);


class cosmos {
public:
  void absorb(card_ptr card);
  card_ptr take(size_t index);
  inline const shenanigan &card_at(size_t index) const { return *cards.at(index); }
  inline size_t size() const { return cards.size(); }

private:
  std::vector<card_ptr> cards;
};

class player {
public:
  player(stealth greed, strategy strat);
  void sign_contract(faction f);
  void score_a_point();
  void view() const;
  int get_score();
  enum faction get_contract() const;
  int get_greed();
  inline size_t grimory_size() const { return deck.size(); }
  inline void refill_hand() { cards_in_hand.draw_up_to_limit(deck); }
  void give_starting_deck(starting_deck d);
  inline void set_hand_size(size_t size) { cards_in_hand.set_limit(size); }
  void setup(game &g);
  virtual ~player() = default;

  // shenanigan phases, called by the game
  void prepare(game &g);
  void do_shenanigans(game &g);
  void take_back_shenanigan();

  // used by the cards while they resolve
  void receive_card(card_ptr card);
  void store_ether(card_ptr card);
  unsigned available_ether() const;
  std::vector<card_ptr> spend_ether(unsigned amount);
  card_ptr lose_ether();
  void remember_future(const std::vector<event> &seen);
  void gain_immunity(unsigned charges);
  // true if this player is immune to a choice made by `chooser` (an
  // opponent): the choice is cancelled and one immunity charge is spent
  bool cancels_choice_by(const player &chooser);
  void end_turn();
  std::vector<player *> opponents(game &g);

  // decisions, taken according to the player's strategy
  // - on its turn, reveal one more event of the future (the future is not
  //   empty and the stealth limit is not reached)
  virtual bool wants_to_reveal_more(game &g) = 0;
  // - index (in hand) of the card to scheme this turn; the hand is not empty
  virtual size_t choose_card_to_scheme(game &g) = 0;
  // - index of the schemed card to handle next, among several
  virtual size_t choose_next_shenanigan(game &g) = 0;
  // - reveal the schemed card (true) or put it under the grimory (false)
  virtual bool wants_to_reveal(game &g, const shenanigan &card) = 0;
  // - ether to spend on a spell, at most available_ether()
  virtual unsigned choose_ether_to_spend(game &g, const spell &s) = 0;
  // - propaganda of the present that will be modified by `modification`,
  //   nullptr if there is none
  virtual event *choose_propaganda(game &g, int modification) = 0;
  // - another mage, nullptr if there is none
  virtual player *choose_opponent(game &g) = 0;
  // - index of a card to take back from the cosmos; the cosmos is not empty
  virtual size_t choose_cosmos_card(game &g) = 0;
  // - an opponent that will lose one ether, nullptr to stop leaking
  virtual player *choose_ether_to_leak(game &g) = 0;
  // - branch of a divergence (OR) that applies: 0 = left, 1 = right
  virtual size_t choose_divergence_branch(game &g, const event &divergence) = 0;
  // - faction receiving an ANY propaganda of this value, among `allowed`
  virtual faction choose_faction_for_any(game &g, int value,
                                         const std::vector<faction> &allowed) = 0;

protected:
  grimory deck;
  hand cards_in_hand = hand(4);
  lair cards_in_play;
  enum faction contract = NONE;

  event &spell_access(game &g, event_pile ep, size_t position);
  std::vector<event *> present_propaganda(game &g);
  std::vector<player *> opponents_with_ether(game &g);

  // events seen with the Reading spell, top of the future first
  std::vector<event> known_events;
  // the card of the game with this id, if this player has seen it
  const event *known_event(long id) const;

private:
  stealth greed;
  int points = 0;
  strategy strat;
  std::string deck_name = "none";
  unsigned immunity_charges = 0;
};

class random_player : public player {
public:
  random_player(stealth greed);

  bool wants_to_reveal_more(game &g) override;
  size_t choose_card_to_scheme(game &g) override;
  size_t choose_next_shenanigan(game &g) override;
  bool wants_to_reveal(game &g, const shenanigan &card) override;
  unsigned choose_ether_to_spend(game &g, const spell &s) override;
  event *choose_propaganda(game &g, int modification) override;
  player *choose_opponent(game &g) override;
  size_t choose_cosmos_card(game &g) override;
  player *choose_ether_to_leak(game &g) override;
  size_t choose_divergence_branch(game &g, const event &divergence) override;
  faction choose_faction_for_any(game &g, int value,
                                 const std::vector<faction> &allowed) override;
};

class smart_player : public player {
public:
  smart_player(stealth greed);

  bool wants_to_reveal_more(game &g) override;
  size_t choose_card_to_scheme(game &g) override;
  size_t choose_next_shenanigan(game &g) override;
  bool wants_to_reveal(game &g, const shenanigan &card) override;
  unsigned choose_ether_to_spend(game &g, const spell &s) override;
  event *choose_propaganda(game &g, int modification) override;
  player *choose_opponent(game &g) override;
  size_t choose_cosmos_card(game &g) override;
  player *choose_ether_to_leak(game &g) override;
  size_t choose_divergence_branch(game &g, const event &divergence) override;
  faction choose_faction_for_any(game &g, int value,
                                 const std::vector<faction> &allowed) override;
};

class present {
  friend class player;

public:
  stealth append_from_future(future &future);
  void rollback(future &future);
  event take_first();
  inline stealth get_stealth() const {return current_stealth;}
  inline void set_stealth(const stealth &st) {current_stealth = st;}
  const event get_last_event();
  const event draw_last();
  inline bool is_empty() const {return present.empty();};
  void view();
  inline const std::deque<event> &show_events() const {return present;};

private:
  std::deque<event> present = std::deque<event>();
  stealth current_stealth = 0;
};

class future {
  friend class player;

public:
  const event draw();
  void put_on_top(const event &event);
  size_t remaining_cards();
  // copies of the `count` events on top of the future, top first
  std::vector<event> peek(size_t count) const;
  // the next event to be revealed; the future is not empty
  inline const event &top() const { return future.back(); }
  void shuffle(std::mt19937 &rng);
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
  game(stealth max_stealth, unsigned seed, int starting_popularity = 0);
  void setup();
  bool current_player_wants_to_play_and_can();
  inline stealth get_max_stealth() const { return max_stealth; }
  void add_player(std::unique_ptr<player> p);
  void add_event(const event &e);
  void play_turn();
  // plays until the future is empty; returns false if the game is still
  // running after max_turns turns (0: no limit), as a game can loop forever
  bool play_game(long max_turns = 0);
  inline long get_elapsed_turns() const { return elapsed_turns; }
  void view();

  inline std::vector<int> &get_scores_ref() { return scores; };
  inline std::vector<std::unique_ptr<player>> &get_players_ref() {
    return players;
  };

  size_t current_player = 0;

  inline unsigned get_seed() const { return seed; }
  inline bool turn_was_rolled_back() const { return turn_rolled_back; }
  void set_contract_pile(std::vector<faction> contracts);
  // the mage with the most points; ties go to the biggest grimory, then to
  // a random draw (index in the players)
  size_t determine_winner();
  inline std::mt19937 &rng() { return generator; }
  inline size_t random_below(size_t n) { return ::random_below(generator, n); }
  // the mage whose turn it is controls the events of the present
  inline player &get_current_player() { return *players[current_player]; }

  void shuffle();

  past past_events;
  present present_events;
  future future_events;
  cosmos cosmos_cards;
  std::vector<event> oblivion; // revealed explosions

  void reveal_x_more_events(int);
private:
  void preparation_phase();
  void reveal_phase();
  bool reveal_one_event();
  void rollback();
  void resolve_event(const event &e, player &controller);
  void collect_propagandas(const event &e, player &controller,
                           std::vector<const event *> &propagandas);
  void apply_propagandas(const std::vector<const event *> &propagandas,
                         player &controller);
  void resolve_election();
  void shenanigan_phase();
  void execution_phase();
  void cleanup_phase();

  std::vector<int> scores; // popularity of each faction
  std::vector<std::unique_ptr<player>> players =
      std::vector<std::unique_ptr<player>>();
  stealth max_stealth;
  long elapsed_turns = 0;
  unsigned seed;
  std::mt19937 generator;
  std::vector<faction> contract_pile; // undealt contracts, face down
  bool turn_rolled_back = false;
  long next_event_id = 0;
};

#endif
