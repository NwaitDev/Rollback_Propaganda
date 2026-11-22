#include "../include/game_concepts.hpp"

// a card loses its modifications when it changes zone
void past::pass(const event& executed_event){
	past.push_back(executed_event);
	past.back().reset();
}
