#include "../include/game_concepts.hpp"
#include <cstddef>
#include <stdexcept>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>


game::game(stealth max_stealth, unsigned seed, int starting_popularity)
: scores(factions_count, starting_popularity), max_stealth(max_stealth),
  seed(seed), generator(seed) {}

void game::set_contract_pile(std::vector<contract> contracts){
	contract_pile = std::move(contracts);
}

// shuffles the contracts and deals one to every mage (the rest stays face
// down), then every mage shuffles their grimory and draws their hand
void game::setup(){
	if (contract_pile.size() < players.size()) {
		throw std::runtime_error("not enough contracts for every mage");
	}
	shuffle_with(contract_pile, generator);
	for (std::unique_ptr<player>& p : players) {
		p->sign_contract(contract_pile.back());
		contract_pile.pop_back();
	}
	for (std::unique_ptr<player>& p : players) {
		p->setup(*this);
	}
}

size_t game::seat_of(const player& p) const{
	for (size_t i = 0; i < players.size(); ++i) {
		if (players[i].get() == &p) {
			return i;
		}
	}
	throw std::logic_error("this player is not in the game");
}

size_t game::determine_winner(tiebreak* how){
	std::vector<size_t> best;
	for (size_t i = 0; i < players.size(); ++i) {
		if (best.empty()) {
			best.push_back(i);
			continue;
		}
		player& candidate = *players[i];
		player& current = *players[best.front()];
		bool better = (candidate.get_score() != current.get_score())
			? (candidate.get_score() > current.get_score())
			: (candidate.grimory_size() > current.grimory_size());
		bool tied = (candidate.get_score() == current.get_score())
			&& (candidate.grimory_size() == current.grimory_size());
		if (better) {
			best.assign(1, i);
		} else if (tied) {
			best.push_back(i);
		}
	}
	if (how != nullptr) {
		*how = tiebreak::NONE;
		for (size_t i = 0; i < players.size(); ++i) {
			if (i != best.front() && players[i]->get_score() == players[best.front()]->get_score()) {
				*how = (best.size() > 1) ? tiebreak::DRAW : tiebreak::GRIMORY;
			}
		}
	}
	return best[random_below(best.size())];
}

// every card of the game gets its own id (see event::get_id)
void game::add_event(const event& e){
	event card = e;
	card.set_id(next_event_id++);
	future_events.put_on_top(card);
}

void game::add_player(std::unique_ptr<controller> c){
	c->seed_for(seed, players.size());
	players.push_back(std::make_unique<player>(std::move(c)));
}

std::vector<size_t> game::opponents_of(size_t seat) const{
	std::vector<size_t> others;
	for (size_t i = 0; i < players.size(); ++i) {
		if (i != seat) {
			others.push_back(i);
		}
	}
	return others;
}

std::vector<size_t> game::opponents_with_ether(size_t seat) const{
	std::vector<size_t> targets;
	for (size_t i : opponents_of(seat)) {
		if (players[i]->available_ether() > 0) {
			targets.push_back(i);
		}
	}
	return targets;
}

std::vector<size_t> game::propaganda_in_present() const{
	std::vector<size_t> positions;
	const std::deque<event>& present = present_events.show_events();
	for (size_t i = 0; i < present.size(); ++i) {
		if (present[i].is_propaganda_card()) {
			positions.push_back(i);
		}
	}
	return positions;
}

void game::shuffle(){
	future_events.shuffle(generator);
	observer().future_shuffled(*this);
}

bool game::play_game(long max_turns){
	observer().game_started(*this);
	while (future_events.remaining_cards()>0) {
		if (max_turns > 0 && elapsed_turns >= max_turns) {
			observer().game_blocked(*this);
			return false;
		}
		play_turn();
	}
	observer().game_finished(*this);
	return true;
}

void game::play_turn(){
	turn_rolled_back = false;
	outcome = turn_outcome::RESOLVED;
	observer().turn_started(*this);

	start_phase(game_phase::SCHEMES);
	preparation_phase();
	start_phase(game_phase::REVEAL);
	reveal_phase();

	// a rollback ends the turn without the following phases
	if (!turn_rolled_back) {
		start_phase(game_phase::SPELLS);
		shenanigan_phase();
	}
	if (!turn_rolled_back) {
		start_phase(game_phase::RESOLUTION);
		execution_phase();
	}

	start_phase(game_phase::CLEANUP);
	cleanup_phase();
	observer().turn_ended(*this, outcome);
}

void game::start_phase(game_phase p){
	phase = p;
	observer().phase_started(*this, p);
}

void game::preparation_phase(){
	for (std::unique_ptr<player>& p : players) {
		p->prepare(*this);
	}
	for (std::unique_ptr<player>& p : players) {
		const hand& kept = p->get_hand();
		for (size_t i = 0; i < kept.get_nb_cards(); ++i) {
			observer().card_kept_in_hand(*this, *p, kept.card_at(i));
		}
	}
}

void game::reveal_phase(){
	while (!turn_rolled_back && current_player_wants_to_play_and_can()) {
		if (!reveal_one_event()) {
			break;
		}
	}
}

// Reveals the top of the future and applies the reveal rules.
// Returns false when no more event must be revealed this turn (collapse or
// rollback).
bool game::reveal_one_event(){
	present_events.append_from_future(future_events);
	observer().event_revealed(*this, present_events.get_last_event());

	if(present_events.get_stealth()>max_stealth){
		outcome = turn_outcome::STEALTH_ROLLBACK;
		rollback();
		return false;
	}

	switch (present_events.get_last_event().get_kind()) {

		// these first event kinds have no effect on reveal phase
		case OPERATOR:
		case PROPAGANDA:
		case ELECTION:
		case MISTAKE:
			return true;

		// events that trigger during reveal phase
		case EXPLOSION:
			oblivion.push_back(present_events.draw_last());
			outcome = turn_outcome::EXPLOSION_ROLLBACK;
			rollback();
			return false;
		case COLLAPSE:
			present_events.set_stealth(max_stealth);
			observer().collapse_revealed(*this);
			return false;
	}
	return true;
}

// The present goes back on the future, every mage takes back the shenanigan
// still schemed this turn, and the turn ends.
void game::rollback(){
	// the last revealed event goes back first: the first revealed one ends
	// on top of the future
	std::vector<long> returned;
	for (const event& e : present_events.show_events()) {
		returned.push_back(e.get_id());
	}
	present_events.rollback(future_events);
	for (std::unique_ptr<player>& p : players) {
		size_t first = p->get_hand().get_nb_cards();
		p->take_back_shenanigan();
		for (size_t i = first; i < p->get_hand().get_nb_cards(); ++i) {
			observer().shenanigan_returned(*this, *p, p->get_hand().card_at(i));
		}
	}
	turn_rolled_back = true;
	observer().rolled_back(*this, outcome, returned);
}

void game::shenanigan_phase(){
	for (size_t i = 0; i < players.size() && !turn_rolled_back; ++i) {
		players[(current_player+i)%players.size()]->do_shenanigans(*this);
	}
}
void game::execution_phase(){
	while (!present_events.is_empty()) {
		event resolved = present_events.take_first();
		resolve_event(resolved, *players[current_player]);
		past_events.pass(resolved);
		observer().event_resolved(*this, resolved);
	}
}

void game::cleanup_phase(){
	for (std::unique_ptr<player>& p : players) {
		size_t before = p->get_hand().get_nb_cards();
		p->refill_hand();
		size_t drawn = p->get_hand().get_nb_cards() - before;
		if (drawn > 0) {
			observer().cards_drawn(*this, *p, drawn);
		}
		bool immune = p->get_immunity() > 0;
		p->end_turn();
		if (immune) {
			observer().immunity_expired(*this, *p);
		}
	}
	present_events.set_stealth(0);
	current_player = (current_player+1) % players.size();
	elapsed_turns++;
}



bool game::current_player_wants_to_play_and_can(){
	return (present_events.get_stealth()<max_stealth)
			&& (future_events.remaining_cards()>0)
			&& players[current_player]->wants_to_reveal_more(*this);
}

// Precipitation: reveals up to x more events with the reveal rules
void game::reveal_x_more_events(int x){
	for (; x > 0 && future_events.remaining_cards() > 0; --x) {
		if (!reveal_one_event()) {
			break;
		}
	}
}
