#include "../include/game_concepts.hpp"
#include <iostream>

void past::view() const{
	size_t i = 0;
	std::cout<<"(past)\n";
	for (event card : past) {
		std::cout<<"card n°"<< i++ <<": "; card.view(); std::cout<<std::endl;
	}
}

void past::pass(const event& executed_event){
	past.push_back(executed_event);
}
