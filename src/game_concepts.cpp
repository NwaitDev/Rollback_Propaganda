#include "../include/game_concepts.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <vector>

event::event(influence faction, int value, ::stealth stealth)
: kind(PROPAGANDA), type({.faction = faction}), value(value), st(stealth){}

event::event(event_kind ekind, ::stealth stealth, int value){
	if ((ekind!=PROPAGANDA) && (ekind!=OPERATOR)) {
		this->kind = ekind;
		this->type.faction = NONE;
		this->value = value;
		this->st = stealth;
	}else{
		throw std::runtime_error("cannot instantiate an event of kind OPERATOR or PROPAGANDA without additional arguments");
	}
}


event::event(event_operator op, const event& left, const event& right)
: kind(OPERATOR), type({.op = op}), value(0), left(&left), right(&right) {
	
}

int event::interest_for(influence faction) const {
	
	if (this->kind==PROPAGANDA) {
		if (faction == NONE) {
			return 0;
		}
		if ((faction == this->type.faction) || (this->type.faction==ANY)) {
			return std::max(this->value, 0);
		}
		return 0;
    }
	if (this->kind==OPERATOR) {
		return std::max(this->left->interest_for(faction), this->right->interest_for(faction));
	}
	// in case of neutral event
	return 0;
}

int event::interest_against(influence faction) const{
	
	if (this->kind==PROPAGANDA) {
		if (faction == NONE) {
			return 0;
		}
		if ((faction == this->type.faction) || (this->type.faction==ANY)) {
			return std::min(this->value, 0);
		}
		return 0;
    }
	if (this->kind==OPERATOR) {
		return std::min(this->left->interest_against(faction), this->right->interest_against(faction));
	}
	// in case of neutral event
	return 0;
}

stealth event::get_stealth() const {
	if (kind==OPERATOR) {
		return left->get_stealth()+right->get_stealth();
	}
	return st;
}


void solve_exec_any_any(game g, const event* best_any, const event* worst_any ){
	// best/worst any must be events of propaganda type with both any faction

	influence current_player_faction = g.players[g.current_player].faction;
	strategy strat = g.players[g.current_player].strat;
	int worst_interest_for_red = worst_any->interest_for(RED);
	int worst_interest_for_green = worst_any->interest_for(GREEN) ; 
	int worst_interest_for_blue = worst_any->interest_for(BLUE);
	
	if ((strat == SMART) || (0 == rand()%2)) {
		best_any->exec(g);
		bool rnd_decision = 0==rand()%2 ; 
		switch (current_player_faction) {

			case RED:
				if ((strat==SMART && (worst_interest_for_green>worst_interest_for_blue)) || rnd_decision){
					g.scores[BLUE]+=worst_any->value;
				}else{
					g.scores[GREEN]+=worst_any->value;
				}
				break;

			case GREEN:
				if ((strat==SMART && (worst_interest_for_blue>worst_interest_for_red)) || rnd_decision){
					g.scores[RED]+=worst_any->value;
				}else{
					g.scores[BLUE]+=worst_any->value;
				}
				break;

			case BLUE:
				if ((strat==SMART && (worst_interest_for_red>worst_interest_for_green)) || rnd_decision){
					g.scores[GREEN]+=worst_any->value;
				}else{
					g.scores[RED]+=worst_any->value;
				}
			case ANY:
			case NONE:
				break;
		}
					
	} else {
		worst_any->exec(g);
		bool rnd_decision = 0==rand()%2 ; 
		switch (current_player_faction) {

			case RED:
				if (rnd_decision){
					g.scores[BLUE]+=best_any->value;
				}else{
					g.scores[GREEN]+=best_any->value;
				}
				break;

			case GREEN:
				if (rnd_decision){
					g.scores[RED]+=best_any->value;
				}else{
					g.scores[BLUE]+=best_any->value;
				}
				break;

			case BLUE:
				if (rnd_decision){
					g.scores[RED]+=best_any->value;
				}else{
					g.scores[GREEN]+=best_any->value;
				}	
				break;
			case ANY:
			case NONE:
				break;
		}
	}
	
}

void solve_exec_any_elseev(game g, const event* any, const event* elseev){

	influence current_player_faction = g.players[g.current_player].faction;
	strategy strat = g.players[g.current_player].strat;

	if (elseev->type.faction==ANY) {
		if (any->interest_for(current_player_faction)
			> elseev->interest_for(current_player_faction)) {
			
			solve_exec_any_any(g, any, elseev);
		}else{
			solve_exec_any_any(g, elseev, any);
		}
		return;
	}
	bool rnd_decision = (0==rand()%2);
	if (elseev->type.faction==RED) {
		if(current_player_faction==RED){
			elseev->exec(g);
			if (((strat == SMART) && (any->interest_for(GREEN)<any->interest_for(BLUE))) || rnd_decision) {
				g.scores[GREEN]+=any->value;
			}else{
				g.scores[BLUE]+=any->value;
			}
		} else {
			any->exec(g);
			elseev->exec(g);
		}
		return;
	}
	if (elseev->type.faction==GREEN) {
		if(current_player_faction==GREEN){
			elseev->exec(g);
			if (((strat == SMART) && (any->interest_for(BLUE)<any->interest_for(RED))) || rnd_decision) {
				g.scores[BLUE]+=any->value;
			}else{
				g.scores[RED]+=any->value;
			}
		} else {
			any->exec(g);
			elseev->exec(g);
		}
		return;
	}
	if (elseev->type.faction==BLUE) {
		if(current_player_faction==BLUE){
			elseev->exec(g);
			if (((strat == SMART) && (any->interest_for(RED)<any->interest_for(GREEN))) || rnd_decision) {
				g.scores[RED]+=any->value;
			}else{
				g.scores[GREEN]+=any->value;
			}
		} else {
			any->exec(g);
			elseev->exec(g);
		}
		return;
	}
}

void event::exec(game& g) const{

	influence current_player_faction = g.players[g.current_player].faction;

	switch (this->kind) {

        case OPERATOR:	// currently only handling mixes of propaganda events

			if ((left->kind==PROPAGANDA) && (right->kind==PROPAGANDA)) {
				if (type.op == DIVERGENCE) {
					if 	((		(g.players[g.current_player].strat==SMART) 
								&& (left->interest_for(current_player_faction) > right->interest_for(current_player_faction))) 
							|| 	(rand()%2==0)
						) {
						
						left->exec(g);
					}else{
						right->exec(g);
					}
				} else { // convergence type
					// cases where one of the two branches at least is any
					if (left->type.faction==ANY) {
						solve_exec_any_elseev(g, left, right);
					}else {
						if (right->type.faction==ANY){
							solve_exec_any_elseev(g, right, left);
						} else {
							left->exec(g);
							right->exec(g);
						}
					}
					
				}
			}
			
			break;

        case PROPAGANDA:
			switch (this->type.faction) {
				case RED:
					g.scores[RED]+=value;
				break;
				case GREEN:
					g.scores[GREEN]+=value;
					break;
				case BLUE:
					g.scores[BLUE]+=value;
					break;
				case ANY:
					g.scores[current_player_faction]+=value;
					break;
				case NONE:
					break;
			}
			break;
        case ELECTION:
			std::cout<<"ELECTION EXECUT2E"<<std::endl;
			if ((g.scores[RED]>g.scores[GREEN])
				&& (g.scores[RED]>g.scores[BLUE])) {
				for(player& p : g.players){
					if(p.faction==RED){
						p.score_a_point();
					}
				}
			}

			if ((g.scores[GREEN]>g.scores[RED])
				&& (g.scores[GREEN]>g.scores[BLUE])) {
				for(player& p : g.players){
					if(p.faction==GREEN){
						p.score_a_point();
					}
				}
			}

			if ((g.scores[BLUE]>g.scores[GREEN])
				&& (g.scores[BLUE]>g.scores[RED])) {
				for(player& p : g.players){
					if(p.faction==BLUE){
						p.score_a_point();
					}
				}
			}
			break;

		// each one of these following cases has no effect during the execution
        case EXPLOSION:
        case COLLAPSE:
        case MISTAKE:
		break;
        }
}

void event::view() const {
	switch (kind) {

        case OPERATOR:
		switch (type.op) {
            case CONVERGENCE:
			left->view();
			std::cout<<" AND ";
			right->view();
			break;
            case DIVERGENCE:
			left->view();
			std::cout<<" OR ";
			right->view();
            break;
        }
		break;
        case PROPAGANDA:
			std::cout<<value;
			switch (type.faction) {
				case RED:
				std::cout<<" RED";
				break;
				case GREEN:
				std::cout<<" GREEN";
				break;
				case BLUE:
				std::cout<<" BLUE";
				break;
				case ANY:
				std::cout<<" ANY";
				break;
				case NONE:
				std::cout<<" NONE?";
				break;
			}
			std::cout<<" ("<<st<<")";
			break;
        case ELECTION:
			std::cout<<"ELECTION "<<" ("<<st<<")";
			break;
        case EXPLOSION:
			std::cout<<"EXPLOSION";
			break;
        case COLLAPSE:
			std::cout<<"COLLAPSE";
			break;
        case MISTAKE:
			std::cout<<"MISTAKE (x"<<value<<")";
          break;
        }
}


player::player(influence faction, stealth greed, strategy strat) : faction(faction), greed(greed), strat(strat){}

void player::score_a_point(){
	this->points++;
}

void player::view(){
	std::cout<<"faction: ";
	switch (faction) {

        case RED:
		std::cout<<"RED";
		break;
        case GREEN:
		std::cout<<"GREEN";
		break;
        case BLUE:
		std::cout<<"BLUE";
		break;
        case ANY:
		std::cout<<"ANY?";
		break;
        case NONE:
		std::cout<<"NONE???";
        break;
    }
    std::cout<<" / strategy: ";
	switch(strat){
    	case RANDOM:
		std::cout<<"RANDOM";
		break;
    	case SMART:
		std::cout<<"SMART";
      	break;
    }
    std::cout<<" / greed: " << greed << " (score :" << points << ")";
}

stealth random_stealth(stealth lower_bound, stealth upper_bound){
	return lower_bound + rand()%(upper_bound-lower_bound);
}

game::game(stealth max_stealth) : max_stealth(max_stealth){}

const event present_events::last_event(){
	return current_events.back();
}

void present_events::discard(){
	current_events.pop_back();
}

void present_events::mult(int mult){
	current_mult*=mult;
}

stealth present_events::see_next_event(std::vector<event>& future){
	current_events.push_back(future.back());
	st+= current_mult * future.back().get_stealth();
	future.pop_back();
	return st;
}

void present_events::rollback(std::vector<event>& future){
	for (size_t i = 0 ; i < current_events.size(); ++i) {
		future.push_back(current_events[current_events.size() - 1 - i]);
	}
	current_events.clear();
	current_mult = 1;
}

void present_events::collapse(std::vector<event>& past, game& g){
	while (current_events.size()>0) {
		past.push_back(current_events.back());
		current_events.back().exec(g);
		current_events.pop_back();
	}
}

void game::play_turn(){
	player p = players[current_player];
	while ((current_events.st<p.greed)) {
		if (future_events.size()<=0) {
			std::cout<<"endgame!"<<std::endl;
			break;
		}
		current_events.see_next_event(future_events);
		if(current_events.st>max_stealth){
			std::cout<<"forced rollback!"<<std::endl;
			current_events.rollback(future_events);
			break;
		}
		switch (current_events.last_event().kind) {

			// three first event kinds have no effect on reveal phase
			case OPERATOR:
				std::cout<<"multi_propaganda!"<<std::endl;
				break;
			case PROPAGANDA:
				std::cout<<"propaganda!"<<std::endl;
				break;
			case ELECTION:
				std::cout<<"election!"<<std::endl;
				break;
			
			// events that trigger during reveal phase
			case EXPLOSION:
				current_events.discard();
				current_events.rollback(future_events);
				current_events.st = max_stealth;
				std::cout<<"explosion!"<<std::endl;
				break;
			case COLLAPSE:
				current_events.collapse(past_events, *this);
				current_events.st = max_stealth;
				std::cout<<"forced collapse!"<<std::endl;
				break;
			case MISTAKE:
				current_events.mult(current_events.last_event().value);
				std::cout<<"mistake!"<<std::endl;
				break;
        }
    }
	current_events.collapse(past_events, *this);
	current_events.st = 0;
	current_events.current_mult = 1;
	current_player = (current_player+1) % players.size();
	elapsed_turns++;
}

void game::add_event(event e){
	future_events.push_back(e);
}

void game::add_player(player p){
	players.push_back(p);
}

void game::play_game(){
	std::cout<<"The game is running..."<<std::endl;
	while (future_events.size()>0) {
		play_turn();
		std::cout<<"elapsed turns:"<<elapsed_turns<<".\nremaining cards: "<<future_events.size()<<std::endl;
	}
	std::cout<<"the game is over!"<<std::endl;
}

void game::shuffle(){
	std::cout<<"shuffling the future!"<<std::endl;

	for (size_t i = future_events.size()-1; i>0; --i) {
		size_t swap_id = rand()%i;
		event e1 = future_events[i];
		event e2 = future_events[swap_id];
		future_events[i] = e2;
		future_events[swap_id] = e1;
	}
}

void game::view(){
	std::cout<<"nb players: "<<players.size()<<"."<<std::endl;
	for (player p : players) {
		p.view(); std::cout<<std::endl;
	}
	size_t i = 0;
	for (event card : future_events) {
		std::cout<<"card n°"<< i++ <<": "; card.view(); std::cout<<std::endl;
	}
}