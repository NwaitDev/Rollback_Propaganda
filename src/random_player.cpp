#include "../include/game_concepts.hpp"
#include <cstdlib>
#include <iostream>

random_player::random_player(faction f, stealth greed) : player(f, greed, RANDOM){}

void random_player::execute(event evt, game& g) const{
	switch (evt.get_kind()) {

        case OPERATOR:	// currently only handling mixes of propaganda events

			if ((evt.get_left().get_kind()==PROPAGANDA) && (evt.get_right().get_kind()==PROPAGANDA)) {
				if (evt.get_operator() == DIVERGENCE) {
					if 	(rand()%2==0) {
						execute(evt.get_left(), g);
					}else{
						execute(evt.get_right(), g);
					}
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
					if(p->get_faction()==RED){
						p->score_a_point();
					}
				}
			}

			if ((g.get_scores_ref()[YELLOW]>g.get_scores_ref()[RED])
				&& (g.get_scores_ref()[YELLOW]>g.get_scores_ref()[BLUE])) {
				for(std::unique_ptr<player>& p : g.get_players_ref()){
					if(p->get_faction()==YELLOW){
						p->score_a_point();
					}
				}
			}

			if ((g.get_scores_ref()[BLUE]>g.get_scores_ref()[YELLOW])
				&& (g.get_scores_ref()[BLUE]>g.get_scores_ref()[RED])) {
				for(std::unique_ptr<player>& p : g.get_players_ref()){
					if(p->get_faction()==BLUE){
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

void random_player::execute_any_any(game& g, const event any1, const event any2) const{
	enum faction unselected = (enum faction)(rand()%3);
	int order_switch = rand()%2;
	enum faction selected1 = (enum faction)(unselected+2-order_switch);
	enum faction selected2 = (enum faction)(unselected+1+order_switch);
	g.get_scores_ref()[selected1]+=any1.get_value();
	g.get_scores_ref()[selected2]+=any2.get_value();
}

void random_player::execute_any_else(game& g, const event any, const event elseev) const{
	enum faction rnd_faction = (enum faction) ((elseev.get_faction()+1+rand()%2)%3);
	g.get_scores_ref()[elseev.get_faction()]+=elseev.get_value();
	g.get_scores_ref()[rnd_faction]+=any.get_value();
}