#include "../include/game_concepts.hpp"
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>


game::game(stealth max_stealth, unsigned seed, int starting_popularity)
: scores(factions_count, starting_popularity), max_stealth(max_stealth),
  seed(seed), generator(seed) {}

void game::set_contract_pile(std::vector<faction> contracts){
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

size_t game::determine_winner(){
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
	return best[random_below(best.size())];
}

// every card of the game gets its own id (see event::get_id)
void game::add_event(const event& e){
	event card = e;
	card.set_id(next_event_id++);
	future_events.put_on_top(card);
}

void game::add_player(std::unique_ptr<player> p){
	players.push_back(std::move(p));
}

void game::shuffle(){
	std::cout<<"shuffling the future_events!"<<std::endl;
	future_events.shuffle(generator);
}

void game::view(){
	std::cout<<"nb players: "<<players.size()<<"."<<std::endl;
	for (std::unique_ptr<player>& p : players) {
		p->view(); std::cout<<std::endl;
	}
	future_events.view();
	present_events.view();
	past_events.view();
	std::cout<<"(oblivion)\n";
	for (size_t i = 0; i < oblivion.size(); ++i) {
		std::cout<<"card n°"<<i<<": "; oblivion[i].view(); std::cout<<std::endl;
	}
}

bool game::play_game(long max_turns){
	std::cout<<"The game is running..."<<std::endl;
	while (future_events.remaining_cards()>0) {
		if (max_turns > 0 && elapsed_turns >= max_turns) {
			std::cout<<"the game is blocked after "<<elapsed_turns<<" turns!"<<std::endl;
			return false;
		}
		std::cout<<"turn:"<<elapsed_turns<<".\tremaining cards: "<<future_events.remaining_cards()<<std::endl;
		play_turn();
	}
	std::cout<<"the game is over!"<<std::endl;
	return true;
}

void game::play_turn(){
	turn_rolled_back = false;

	preparation_phase();

	reveal_phase();

	// a rollback ends the turn without the following phases
	if (!turn_rolled_back) {
		shenanigan_phase();
	}
	if (!turn_rolled_back) {
		execution_phase();
	}

	cleanup_phase();
}

void game::preparation_phase(){
	for (std::unique_ptr<player>& p : players) {
		p->prepare(*this);
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

	if(present_events.get_stealth()>max_stealth){
		std::cout<<"forced rollback!"<<std::endl;
		rollback();
		return false;
	}

	switch (present_events.get_last_event().get_kind()) {

		// these first event kinds have no effect on reveal phase
		case OPERATOR:
			std::cout<<"multi_propaganda!"<<std::endl;
			return true;
		case PROPAGANDA:
			std::cout<<"propaganda!"<<std::endl;
			return true;
		case ELECTION:
			std::cout<<"election!"<<std::endl;
			return true;
		case MISTAKE:
			std::cout<<"mistake!"<<std::endl;
			return true;

		// events that trigger during reveal phase
		case EXPLOSION:
			std::cout<<"explosion!"<<std::endl;
			oblivion.push_back(present_events.draw_last());
			rollback();
			return false;
		case COLLAPSE:
			std::cout<<"forced execute!"<<std::endl;
			present_events.set_stealth(max_stealth);
			return false;
	}
	return true;
}

// The present goes back on the future, every mage takes back the shenanigan
// still schemed this turn, and the turn ends.
void game::rollback(){
	present_events.rollback(future_events);
	for (std::unique_ptr<player>& p : players) {
		p->take_back_shenanigan();
	}
	turn_rolled_back = true;
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
	}
}

void game::cleanup_phase(){
	for (std::unique_ptr<player>& p : players) {
		p->refill_hand();
		p->end_turn();
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
