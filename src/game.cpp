#include "../include/game_concepts.hpp"
#include <iostream>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>


game::game(stealth max_stealth) : max_stealth(max_stealth){}

void game::add_event(const event& e){
	future_events.put_on_top(e);
}

void game::add_player(std::unique_ptr<player> p){
	players.push_back(std::move(p));
}

void game::play_turn(){
	while (	(present_events.get_stealth()<players[current_player]->get_greed())
			&& (present_events.get_stealth()<max_stealth)
			&& (future_events.remaining_cards()>0) ) {
		
		present_events.append_from_future(future_events);

		if(present_events.get_stealth()>max_stealth){
			std::cout<<"forced rollback!"<<std::endl;
			present_events.rollback(future_events);
			break;
		}

		switch (present_events.get_last_event().get_kind()) {

			// these first event kinds have no effect on reveal phase
			case OPERATOR:
				std::cout<<"multi_propaganda!"<<std::endl;
				break;
			case PROPAGANDA:
				std::cout<<"propaganda!"<<std::endl;
				break;
			case ELECTION:
				std::cout<<"election!"<<std::endl;
				break;
			case MISTAKE:
				std::cout<<"mistake!"<<std::endl;
				break;

			// events that trigger during reveal phase
			case EXPLOSION:
				present_events.discard_last();
				present_events.rollback(future_events);
				present_events.set_stealth(max_stealth);
				std::cout<<"explosion!"<<std::endl;
				break;
			case COLLAPSE:
				present_events.set_stealth(max_stealth);
				std::cout<<"forced execute!"<<std::endl;
				break;
        }
    }
	while (!present_events.is_empty()) {
		past_events.pass(present_events.execute(*this, *players[current_player]));
	}
	present_events.set_stealth(0);
	current_player = (current_player+1) % players.size();
	elapsed_turns++;
}


void game::play_game(){
	std::cout<<"The game is running..."<<std::endl;
	while (future_events.remaining_cards()>0) {
		std::cout<<"turn:"<<elapsed_turns<<".\tremaining cards: "<<future_events.remaining_cards()<<std::endl;
		play_turn();
	}
	std::cout<<"the game is over!"<<std::endl;
}

void game::shuffle(){
	std::cout<<"shuffling the future_events!"<<std::endl;
	future_events.shuffle();
}

void game::view(){
	std::cout<<"nb players: "<<players.size()<<"."<<std::endl;
	for (std::unique_ptr<player>& p : players) {
		p->view(); std::cout<<std::endl;
	}
	future_events.view();
	present_events.view();
	past_events.view();
}