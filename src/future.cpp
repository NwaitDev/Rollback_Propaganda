#include "../include/game_concepts.hpp"
#include <iostream>

void future::shuffle(){
	std::cout<<"shuffling the future!"<<std::endl;

	for (size_t i = remaining_cards()-1; i>0; --i) {
		size_t swap_id = rand()%i;
		event e1 = future[i];
		event e2 = future[swap_id];
		future[i] = e2;
		future[swap_id] = e1;
	}
}

void future::view(){
	size_t i = 0;
	std::cout<<"(future)\n";
	for (event card : future) {
		std::cout<<"card n°"<< i++ <<": "; card.view(); std::cout<<std::endl;
	}
}

const event future::draw(){
	event drawn = future.back();
	future.pop_back();
	return drawn;
}


void future::put_on_top(const event& event){
	future.push_back(event);
}

size_t future::remaining_cards(){
	return future.size();
}
