#include "../include/game_concepts.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>

smart_player::smart_player(faction f, stealth greed) : player(f, greed, SMART){}

void smart_player::execute(const event evt, game& g) const {

	switch (evt.get_kind()) {

        case OPERATOR:	// currently only handling mixes of propaganda events

			if ((evt.get_left().get_kind()==PROPAGANDA) && (evt.get_right().get_kind()==PROPAGANDA)) {
				if (evt.get_operator() == DIVERGENCE) {
					// TODO
				} else { // convergence type
					// cases where one of the two branches at least is any
					if (evt.get_left().get_faction()!=ANY) {
						if (evt.get_right().get_faction()!=ANY){
							execute(evt.get_left(), g);
							execute(evt.get_right(), g);
						} else {
							execute_any_else(g, evt.get_right(), evt.get_left());
						}
					}else {
						if (evt.get_right().get_faction()!=ANY){
							execute_any_else(g, evt.get_left(), evt.get_right());
						} else {
							execute_any_any(g, evt.get_left(), evt.get_right());
						}
					}
					
				}
			}
			
			break;

        case PROPAGANDA:
			switch (evt.get_faction()) {
				case RED:
					g.get_scores_ref()[RED]+=evt.get_value();
				break;
				case YELLOW:
					g.get_scores_ref()[YELLOW]+=evt.get_value();
					break;
				case BLUE:
					g.get_scores_ref()[BLUE]+=evt.get_value();
					break;
				case ANY:
					g.get_scores_ref()[(enum faction)(rand()%3)]+=evt.get_value();
					break;
				case NONE:
					break;
			}
			break;
        case ELECTION:
			if ((g.get_scores_ref()[RED]>g.get_scores_ref()[YELLOW])
				&& (g.get_scores_ref()[RED]>g.get_scores_ref()[BLUE])) {
				for(std::unique_ptr<player>& p : g.get_players_ref()){
					if(p->get_contract()==RED){
						p->score_a_point();
					}
				}
			}

			if ((g.get_scores_ref()[YELLOW]>g.get_scores_ref()[RED])
				&& (g.get_scores_ref()[YELLOW]>g.get_scores_ref()[BLUE])) {
				for(std::unique_ptr<player>& p : g.get_players_ref()){
					if(p->get_contract()==YELLOW){
						p->score_a_point();
					}
				}
			}

			if ((g.get_scores_ref()[BLUE]>g.get_scores_ref()[YELLOW])
				&& (g.get_scores_ref()[BLUE]>g.get_scores_ref()[RED])) {
				for(std::unique_ptr<player>& p : g.get_players_ref()){
					if(p->get_contract()==BLUE){
						p->score_a_point();
					}
				}
			}
			std::cout<<"ELECTION EXECUTEE"<<std::endl;
			break;

		// each one of these following cases has no effect during the execution
        case EXPLOSION:
        case COLLAPSE:
        case MISTAKE:
		break;
        }
}

void smart_player::execute_any_any(game& g, const event any1, const event any2) const{
	enum faction my_faction = get_contract();
	enum faction other_faction1 = (enum faction) ((my_faction+1)%3);
	enum faction other_faction2 = (enum faction) ((my_faction+2)%3);
	int best_value = std::max(any1.get_value(),any2.get_value());
	int worst_value = std::min(any1.get_value(),any2.get_value());
	enum faction latest_opponent;
	enum faction earliest_opponent;
	if (g.get_scores_ref()[other_faction1]==g.get_scores_ref()[other_faction2]) {
		if(rand()%2==0){
			latest_opponent = other_faction1;
			earliest_opponent = other_faction2;
		}else{
			earliest_opponent = other_faction1;
			latest_opponent = other_faction2;
		}
	}else{
		if (g.get_scores_ref()[other_faction1]>g.get_scores_ref()[other_faction2]) {
			earliest_opponent = other_faction1;
			latest_opponent = other_faction2;
		} else {
			latest_opponent = other_faction1;
			earliest_opponent = other_faction2;
		}
	}

	if (best_value >=0 && worst_value >=0) {
		g.get_scores_ref()[my_faction]+=best_value;
		g.get_scores_ref()[latest_opponent]+=worst_value;
	}else{
		if(best_value >= 0 && worst_value < 0){
			g.get_scores_ref()[my_faction]+=best_value;
			g.get_scores_ref()[earliest_opponent]+=worst_value;
		}else{
			g.get_scores_ref()[earliest_opponent]+=worst_value;
			g.get_scores_ref()[latest_opponent]+=best_value;	
		}
	}
}

void smart_player::execute_any_else(game& g, const event any, const event elseev) const{
	enum faction my_faction = get_contract();
	enum faction other_faction1 = (enum faction) ((my_faction+1)%3);
	enum faction other_faction2 = (enum faction) ((my_faction+2)%3);
	int any_value = any.get_value();
	int else_value = elseev.get_value();
	enum faction latest_opponent;
	enum faction earliest_opponent;
	if (g.get_scores_ref()[other_faction1]==g.get_scores_ref()[other_faction2]) {
		if(rand()%2==0){
			latest_opponent = other_faction1;
			earliest_opponent = other_faction2;
		}else{
			earliest_opponent = other_faction1;
			latest_opponent = other_faction2;
		}
	}else{
		if (g.get_scores_ref()[other_faction1]>g.get_scores_ref()[other_faction2]) {
			earliest_opponent = other_faction1;
			latest_opponent = other_faction2;
		} else {
			latest_opponent = other_faction1;
			earliest_opponent = other_faction2;
		}
	}

	g.get_scores_ref()[elseev.get_faction()]+=else_value;
	if (elseev.get_faction()==my_faction) {
		if (any_value>=0) {
			g.get_scores_ref()[latest_opponent]+=any_value;
		}else{
			g.get_scores_ref()[earliest_opponent]+=any_value;
		}
	} else {
		if (any_value>=0) {
			g.get_scores_ref()[my_faction]+=any_value;
		}else{
			if (elseev.get_faction()==other_faction1) {
				g.get_scores_ref()[other_faction2]+=any_value;
			}else{
				g.get_scores_ref()[other_faction1]+=any_value;
			}
		}
	}
	
}


void smart_player::prepare(){std::cout<<"shouldn't appear"<<std::endl;};
void smart_player::do_shenanigans(game& g){std::cout<<"shouldn't appear"<<std::endl;};