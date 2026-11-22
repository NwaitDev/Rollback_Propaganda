#ifndef __GAME_CONCEPTS__
#define __GAME_CONCEPTS__

#include <cstddef>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>

using stealth = int;

// Every random draw of a game uses the generator of that game (shuffles,
// contracts, ties), or the generator of a controller (its decisions), which
// is seeded from the game's seed and the seat: a game can be replayed from its
// seed, and a controller playing through the server draws the same numbers
// as in the simulation. std::mt19937 gives the same numbers with every
// compiler; std::shuffle and the std distributions do not, hence these two
// helpers.
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

  // text of the card, e.g. "2 RED (4)" (see text_view.hpp)
  std::string describe() const;
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
  // value printed on the card (the multiplier of a MISTAKE), and the
  // modification it received in its current zone (see get_value)
  inline int get_base_value() const { return value; }
  inline int get_modification() const { return modification; }
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
  friend class state_codec;
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

  // identity of this card in the game, given by game::setup() (-1 before)
  inline long get_id() const { return id; }
  inline void set_id(long new_id) { id = new_id; }

  // Applies the card once revealed by its owner. The card receives its own
  // ownership and moves itself where the rules send it (the owner's lair,
  // the cosmos, another mage's hand...).
  virtual void resolve(card_ptr self, game &g, player &owner) = 0;

private:
  long id = -1;
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
// the names of every card make_card() knows, in alphabetical order
std::vector<std::string> card_names();


class grimory {
public:
  card_ptr draw();
  void shuffle(std::mt19937 &rng);
  void put_at_bottom(card_ptr card);
  inline bool is_empty() const { return cards.empty(); }
  inline size_t size() const { return cards.size(); }
  // gives the ids next_id, next_id + 1... to the cards, bottom first
  void number_cards(long &next_id);

private:
  friend class state_codec;
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
  friend class state_codec;
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
  inline const shenanigan &ether_at(size_t index) const { return *ether_pool.at(index); }

private:
  friend class state_codec;
  std::vector<card_ptr> ether_pool;
  std::vector<card_ptr> prepared; // schemed this turn, not handled yet
};


// a preselected set of grimory cards, read from a JSON file (see decks/)
struct starting_deck {
  std::string name;
  std::vector<card_ptr> cards;
};

starting_deck load_deck(const std::string &path);

// --- contracts ----------------------------------------------------------------
// A contract pays `points` at each election if one of its factions holds the
// popularity rank `rank` (1: the most popular) alone. A faction tied with
// another one holds no rank: with RED 5, YELLOW 5, BLUE 2, nobody is first
// and BLUE is third. The contracts are defined in contract.cpp.

struct contract {
  std::string name;
  std::vector<faction> factions;
  int rank = 1;
  int points = 1;

  // points earned if an election happened with these popularities
  int points_earned(const std::vector<int> &popularity) const;

  // How good these popularities are for this contract, to compare the
  // outcomes of choices. Higher is better: a satisfied contract always rates
  // better than an unsatisfied one, then the margin decides (how far the
  // best of its factions is from losing the rank, or from reaching it).
  long rate(const std::vector<int> &popularity) const;
};

// rank held alone by a faction (1: the most popular), 0 if it is tied
int rank_of(faction f, const std::vector<int> &popularity);

// contract cards, read from a JSON file (see contracts/)
std::vector<contract> load_contracts(const std::string &path);

// event cards, read from a JSON file (see events/ and deck_loader.cpp)
std::vector<event> load_events(const std::string &path);


class cosmos {
public:
  void absorb(card_ptr card);
  card_ptr take(size_t index);
  inline const shenanigan &card_at(size_t index) const { return *cards.at(index); }
  inline size_t size() const { return cards.size(); }

private:
  friend class state_codec;
  std::vector<card_ptr> cards;
};

// --- decisions -------------------------------------------------------------------
// The game never decides for a mage: it asks the controller of that mage
// (a strategy of the simulation, or a remote player of the server). A
// controller sees the game read-only and answers with values or indices,
// never with pointers into the game. Every answer is checked by the game.

// what a controller knows when it is asked to decide
struct decision {
  const game &g;
  const player &self;
  size_t seat;        // seat of `self`
  std::mt19937 &rng;  // random draws of the controller
  inline size_t random_below(size_t n) const { return ::random_below(rng, n); }
};

class controller {
public:
  virtual ~controller() = default;
  // the generator of the decisions, seeded for this game and this seat
  // (called by game::add_player)
  void seed_for(unsigned game_seed, size_t seat);
  inline std::mt19937 &rng() { return generator; }

  // shown by the text views ("RANDOM", "SMART"...)
  virtual std::string strategy_name() const = 0;
  // stealth a strategy is willing to reach while revealing (0: none)
  virtual stealth get_greed() const { return 0; }

  // - on its turn, reveal one more event of the future (the future is not
  //   empty and the stealth limit is not reached)
  virtual bool reveal_more(const decision &d) = 0;
  // - index (in hand) of the card to scheme this turn; the hand is not empty
  virtual size_t scheme_card(const decision &d) = 0;
  // - index (among the schemed cards) of the one to handle next; there are
  //   several
  virtual size_t next_shenanigan(const decision &d) = 0;
  // - reveal the schemed card (true) or put it under the grimory (false)
  virtual bool reveal_shenanigan(const decision &d, const shenanigan &card) = 0;
  // - ether to spend on a spell, at most the ether of the mage
  virtual unsigned ether_to_spend(const decision &d, const spell &s) = 0;
  // - index in `present` (positions of propagandas in the present, not
  //   empty) of the one that will be modified by `modification`
  virtual size_t propaganda_target(const decision &d, int modification,
                                   const std::vector<size_t> &present) = 0;
  // - index in `seats` (other mages, not empty) of the target of a spell
  virtual size_t opponent_target(const decision &d, const std::vector<size_t> &seats) = 0;
  // - index of a card to take back from the cosmos; the cosmos is not empty
  //   and `remaining` cards can still be taken (this one included)
  virtual size_t cosmos_card(const decision &d, unsigned remaining) = 0;
  // - index in `seats` (other mages with ether, maybe empty) of the one that
  //   will lose an ether, seats.size() to stop leaking; `remaining` ethers
  //   can still be leaked (this one included)
  virtual size_t ether_leak_target(const decision &d, const std::vector<size_t> &seats,
                                   unsigned remaining) = 0;
  // - branch of a divergence (OR) that applies: 0 = left, 1 = right
  virtual size_t divergence_branch(const decision &d, const event &divergence) = 0;
  // - index in `allowed` of the faction receiving an ANY propaganda of this
  //   value
  virtual size_t any_faction(const decision &d, int value,
                             const std::vector<faction> &allowed) = 0;

private:
  std::mt19937 generator;
};


// A mage at the table: their cards, contract and points. The decisions are
// taken by their controller; the methods choose_* ask it, check the answer
// and return what the rules need.
class player {
public:
  explicit player(std::unique_ptr<controller> c);
  void sign_contract(const contract &c);
  void score_points(int amount);
  int get_score() const;
  const contract &get_contract() const;
  inline controller &get_controller() { return *ctrl; }
  inline const controller &get_controller() const { return *ctrl; }
  inline size_t grimory_size() const { return deck.size(); }
  inline void refill_hand() { cards_in_hand.draw_up_to_limit(deck); }
  inline const hand &get_hand() const { return cards_in_hand; }
  inline const lair &get_lair() const { return cards_in_play; }
  inline const std::string &get_deck_name() const { return deck_name; }
  inline unsigned get_immunity() const { return immunity_charges; }
  inline void add_to_grimory(card_ptr card) { deck.put_at_bottom(std::move(card)); }
  void give_starting_deck(starting_deck d);
  // gives an id to every card of the grimory (see grimory::number_cards)
  inline void number_grimory(long &next_id) { deck.number_cards(next_id); }
  inline void set_hand_size(size_t size) { cards_in_hand.set_limit(size); }
  void setup(game &g);

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
  bool cancels_choice_by(game &g, const player &chooser);
  void end_turn();

  // events seen with the Reading spell, top of the future first
  inline const std::vector<event> &get_known_events() const { return known_events; }
  // the card of the game with this id, if this player has seen it
  const event *known_event(long id) const;

  // decisions (see class controller), checked
  bool wants_to_reveal_more(game &g);
  size_t choose_card_to_scheme(game &g);
  size_t choose_next_shenanigan(game &g);
  bool wants_to_reveal(game &g, const shenanigan &card);
  unsigned choose_ether_to_spend(game &g, const spell &s);
  // nullptr if there is no propaganda in the present
  event *choose_propaganda(game &g, int modification);
  // nullptr if there is no other mage
  player *choose_opponent(game &g);
  size_t choose_cosmos_card(game &g, unsigned remaining);
  // nullptr to stop leaking
  player *choose_ether_to_leak(game &g, unsigned remaining);
  size_t choose_divergence_branch(game &g, const event &divergence);
  faction choose_faction_for_any(game &g, int value, const std::vector<faction> &allowed);

private:
  friend class state_codec;
  decision decide(game &g) const;

  std::unique_ptr<controller> ctrl;
  grimory deck;
  hand cards_in_hand = hand(4);
  lair cards_in_play;
  contract signed_contract;
  std::vector<event> known_events;
  int points = 0;
  std::string deck_name = "none";
  unsigned immunity_charges = 0;
};

// every choice is random, except revealing: up to the greed
class random_player : public controller {
public:
  explicit random_player(stealth greed) : greed(greed) {}
  std::string strategy_name() const override { return "RANDOM"; }
  stealth get_greed() const override { return greed; }

  bool reveal_more(const decision &d) override;
  size_t scheme_card(const decision &d) override;
  size_t next_shenanigan(const decision &d) override;
  bool reveal_shenanigan(const decision &d, const shenanigan &card) override;
  unsigned ether_to_spend(const decision &d, const spell &s) override;
  size_t propaganda_target(const decision &d, int modification,
                           const std::vector<size_t> &present) override;
  size_t opponent_target(const decision &d, const std::vector<size_t> &seats) override;
  size_t cosmos_card(const decision &d, unsigned remaining) override;
  size_t ether_leak_target(const decision &d, const std::vector<size_t> &seats,
                           unsigned remaining) override;
  size_t divergence_branch(const decision &d, const event &divergence) override;
  size_t any_faction(const decision &d, int value,
                     const std::vector<faction> &allowed) override;

private:
  stealth greed;
};

// simple heuristics in favour of its contract (see smart_player.cpp)
class smart_player : public controller {
public:
  explicit smart_player(stealth greed) : greed(greed) {}
  std::string strategy_name() const override { return "SMART"; }
  stealth get_greed() const override { return greed; }

  bool reveal_more(const decision &d) override;
  size_t scheme_card(const decision &d) override;
  size_t next_shenanigan(const decision &d) override;
  bool reveal_shenanigan(const decision &d, const shenanigan &card) override;
  unsigned ether_to_spend(const decision &d, const spell &s) override;
  size_t propaganda_target(const decision &d, int modification,
                           const std::vector<size_t> &present) override;
  size_t opponent_target(const decision &d, const std::vector<size_t> &seats) override;
  size_t cosmos_card(const decision &d, unsigned remaining) override;
  size_t ether_leak_target(const decision &d, const std::vector<size_t> &seats,
                           unsigned remaining) override;
  size_t divergence_branch(const decision &d, const event &divergence) override;
  size_t any_faction(const decision &d, int value,
                     const std::vector<faction> &allowed) override;

private:
  stealth greed;
};

class present {

public:
  stealth append_from_future(future &future);
  void rollback(future &future);
  event take_first();
  inline stealth get_stealth() const {return current_stealth;}
  inline void set_stealth(const stealth &st) {current_stealth = st;}
  const event get_last_event();
  const event draw_last();
  inline bool is_empty() const {return present.empty();};
  inline const std::deque<event> &show_events() const {return present;};
  inline event &event_at(size_t position) { return present.at(position); }

private:
  friend class state_codec;
  std::deque<event> present = std::deque<event>();
  stealth current_stealth = 0;
};

class future {

public:
  const event draw();
  void put_on_top(const event &event);
  size_t remaining_cards();
  // copies of the `count` events on top of the future, top first
  std::vector<event> peek(size_t count) const;
  // the next event to be revealed; the future is not empty
  inline const event &top() const { return future.back(); }
  void shuffle(std::mt19937 &rng);
  // bottom first: the top of the future is the last card
  inline const std::vector<event> &show_events() const { return future; }

private:
  friend class state_codec;
  std::vector<event> future = std::vector<event>();
};

class past {

public:
  void pass(const event &executed_event);
  inline const std::vector<event> &show_events() const { return past; }

private:
  friend class state_codec;
  std::vector<event> past = std::vector<event>();
};

// --- observation -------------------------------------------------------------
// The game reports what happens to an observer, which can measure it (see
// campaign.hpp). The default observer ignores everything: observing never
// changes the course of a game.

enum class turn_outcome { RESOLVED, STEALTH_ROLLBACK, EXPLOSION_ROLLBACK };

enum class tiebreak { NONE, GRIMORY, DRAW };

// the phases of a turn, in order; a rollback skips SPELLS and RESOLUTION
enum class game_phase { SCHEMES, REVEAL, SPELLS, RESOLUTION, CLEANUP };

// Every notification comes after the change it reports: the game is already
// in its new state.
class game_observer {
public:
  virtual ~game_observer() = default;

  // --- the game --------------------------------------------------------------
  // the future has just been shuffled (before the setup)
  virtual void future_shuffled(game &g) {}
  // play_game() starts, and stops: the future is empty (finished) or the
  // turn limit is reached (blocked)
  virtual void game_started(game &g) {}
  virtual void game_finished(game &g) {}
  virtual void game_blocked(game &g) {}
  // a turn starts (before the Phase de Manigances), then each phase
  virtual void turn_started(game &g) {}
  virtual void phase_started(game &g, game_phase phase) {}
  // end of a turn (after the hands are refilled)
  virtual void turn_ended(game &g, turn_outcome outcome) {}

  // --- events ------------------------------------------------------------------
  // an event has just been added to the present (Phase de Révélation or
  // Précipitation); the reveal rules (rollback, collapse) apply after
  virtual void event_revealed(game &g, const event &e) {}
  // a collapse stops the Phase de Révélation (the stealth is at its limit)
  virtual void collapse_revealed(game &g) {}
  // The present went back on the future (ids of these events, top of the
  // future first) and the turn ends. For an explosion, the explosion is
  // already in the oblivion; shenanigan_returned comes before.
  virtual void rolled_back(game &g, turn_outcome cause, const std::vector<long> &returned) {}
  // a card still schemed goes back to its mage's hand (rollback)
  virtual void shenanigan_returned(game &g, const player &p, const shenanigan &card) {}
  // one propaganda of the event being resolved adds `value` to `f`
  // (after the choices of OR branch and ANY faction)
  virtual void propaganda_applied(game &g, faction f, int value) {}
  // an event of the present has been resolved and is now in the past (`e`
  // still carries its modifications)
  virtual void event_resolved(game &g, const event &e) {}
  // every mage has scored their contract (`points` earned, by seat)
  virtual void election_resolved(game &g, const std::vector<int> &points) {}

  // --- shenanigans -------------------------------------------------------------
  // a mage schemes a card (Phase de Manigances): it left the hand
  virtual void card_schemed(game &g, const player &p, const shenanigan &card) {}
  // a card stays in a mage's hand during the Phase de Manigances
  virtual void card_kept_in_hand(game &g, const player &p, const shenanigan &card) {}
  // a mage puts a schemed card under their grimory instead of revealing it
  virtual void shenanigan_buried(game &g, const player &p, const shenanigan &card) {}
  // a mage reveals a schemed card (it resolves next)
  virtual void card_revealed(game &g, const player &p, const shenanigan &card) {}
  // an ether is stored in its mage's lair
  virtual void ether_stored(game &g, const player &p) {}
  // a spell applied: `amount` ether of its caster went to the cosmos
  virtual void ether_spent(game &g, const player &p, unsigned amount) {}
  // a revealed spell has no effect, for lack of a target (no ether spent)
  virtual void spell_failed(game &g, const player &p, const shenanigan &card) {}
  // a resolved spell is now in the cosmos
  virtual void spell_discarded(game &g, const player &p, const shenanigan &card) {}
  // the propaganda at this position of the present was modified
  virtual void propaganda_modified(game &g, size_t position, int modification) {}
  // Memory lapse: the card went from the caster to the target's hand
  virtual void memory_lapse_received(game &g, const player &caster, const player &target) {}
  // Entropy reversal: the card went from the cosmos to the mage's hand
  virtual void cosmos_card_taken(game &g, const player &p, const shenanigan &card) {}
  // Ether leak: an ether of `target` went to the cosmos
  virtual void ether_leaked(game &g, const player &caster, const player &target) {}
  // Immunity: the mage gains charges for this turn
  virtual void immunity_gained(game &g, const player &p, unsigned charges) {}
  // a choice of `chooser` is cancelled by the immunity of `immune`
  virtual void immunity_triggered(game &g, const player &immune, const player &chooser) {}
  // the immunity charges left at the end of the turn are lost
  virtual void immunity_expired(game &g, const player &p) {}
  // Reading: the mage saw these events, top of the future first
  virtual void future_read(game &g, const player &p, const std::vector<event> &events) {}
  // Chapter skip: an event went from the top of the future to the past
  virtual void event_skipped(game &g, const event &e) {}
  // Precipitation: up to `count` events are revealed now
  virtual void forced_reveal(game &g, int count) {}
  // a mage draws `count` cards at the end of a turn (the last ones of the hand)
  virtual void cards_drawn(game &g, const player &p, size_t count) {}
};

// Forwards every notification to several observers, in the order they were
// added. The observers are not owned.
class observer_list : public game_observer {
public:
  inline void add(game_observer *o) { observers.push_back(o); }
  inline void clear() { observers.clear(); }
  inline bool empty() const { return observers.empty(); }

  void future_shuffled(game &g) override;
  void game_started(game &g) override;
  void game_finished(game &g) override;
  void game_blocked(game &g) override;
  void turn_started(game &g) override;
  void phase_started(game &g, game_phase phase) override;
  void turn_ended(game &g, turn_outcome outcome) override;
  void event_revealed(game &g, const event &e) override;
  void collapse_revealed(game &g) override;
  void rolled_back(game &g, turn_outcome cause, const std::vector<long> &returned) override;
  void shenanigan_returned(game &g, const player &p, const shenanigan &card) override;
  void propaganda_applied(game &g, faction f, int value) override;
  void event_resolved(game &g, const event &e) override;
  void election_resolved(game &g, const std::vector<int> &points) override;
  void card_schemed(game &g, const player &p, const shenanigan &card) override;
  void card_kept_in_hand(game &g, const player &p, const shenanigan &card) override;
  void shenanigan_buried(game &g, const player &p, const shenanigan &card) override;
  void card_revealed(game &g, const player &p, const shenanigan &card) override;
  void ether_stored(game &g, const player &p) override;
  void ether_spent(game &g, const player &p, unsigned amount) override;
  void spell_failed(game &g, const player &p, const shenanigan &card) override;
  void spell_discarded(game &g, const player &p, const shenanigan &card) override;
  void propaganda_modified(game &g, size_t position, int modification) override;
  void memory_lapse_received(game &g, const player &caster, const player &target) override;
  void cosmos_card_taken(game &g, const player &p, const shenanigan &card) override;
  void ether_leaked(game &g, const player &caster, const player &target) override;
  void immunity_gained(game &g, const player &p, unsigned charges) override;
  void immunity_triggered(game &g, const player &immune, const player &chooser) override;
  void immunity_expired(game &g, const player &p) override;
  void future_read(game &g, const player &p, const std::vector<event> &events) override;
  void event_skipped(game &g, const event &e) override;
  void forced_reveal(game &g, int count) override;
  void cards_drawn(game &g, const player &p, size_t count) override;

private:
  std::vector<game_observer *> observers;
};


class game {
public:
  game(stealth max_stealth, unsigned seed, int starting_popularity = 0);
  void setup();
  bool current_player_wants_to_play_and_can();
  inline stealth get_max_stealth() const { return max_stealth; }
  // a new mage, whose decisions are taken by `c`
  void add_player(std::unique_ptr<controller> c);
  void add_event(const event &e);
  void play_turn();
  // plays until the future is empty; returns false if the game is still
  // running after max_turns turns (0: no limit), as a game can loop forever
  bool play_game(long max_turns = 0);
  inline long get_elapsed_turns() const { return elapsed_turns; }

  inline std::vector<int> &get_scores_ref() { return scores; };
  inline std::vector<std::unique_ptr<player>> &get_players_ref() {
    return players;
  };

  // read-only queries, for the controllers and the observers
  inline const std::vector<int> &get_popularity() const { return scores; }
  inline size_t nb_players() const { return players.size(); }
  inline const player &player_at(size_t seat) const { return *players.at(seat); }
  // seats of the other mages, in seat order
  std::vector<size_t> opponents_of(size_t seat) const;
  // seats of the other mages that have revealed ether, in seat order
  std::vector<size_t> opponents_with_ether(size_t seat) const;
  // positions in the present of the propaganda cards
  std::vector<size_t> propaganda_in_present() const;

  size_t current_player = 0;

  inline unsigned get_seed() const { return seed; }
  // the phase being played (SCHEMES before the first turn)
  inline game_phase get_phase() const { return phase; }
  inline size_t contracts_left() const { return contract_pile.size(); }
  inline bool turn_was_rolled_back() const { return turn_rolled_back; }
  void set_contract_pile(std::vector<contract> contracts);
  // the mage with the most points; ties go to the biggest grimory, then to
  // a random draw (index in the players). `how` tells how a tie was broken.
  size_t determine_winner(tiebreak *how = nullptr);

  // The observers are not owned by the game and are notified in the order
  // they were added. set_observer() replaces them all by one (nullptr: none).
  inline void add_observer(game_observer *o) { if (o) observers.add(o); }
  inline void set_observer(game_observer *o) { observers.clear(); add_observer(o); }
  inline game_observer &observer() { return observers; }
  size_t seat_of(const player &p) const;
  inline std::mt19937 &rng() { return generator; }
  inline size_t random_below(size_t n) { return ::random_below(generator, n); }
  // the mage whose turn it is controls the events of the present
  inline player &get_current_player() { return *players[current_player]; }
  // the event at this position of the present, to be modified by a spell
  inline event &present_event_at(size_t position) { return present_events.event_at(position); }

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
  std::vector<contract> contract_pile; // undealt contracts, face down
  bool turn_rolled_back = false;
  turn_outcome outcome = turn_outcome::RESOLVED;
  observer_list observers;
  long next_event_id = 0;
  game_phase phase = game_phase::SCHEMES;
  void start_phase(game_phase p);

  // reads and rebuilds a whole game (see protocol.cpp)
  friend class state_codec;
};

#endif
