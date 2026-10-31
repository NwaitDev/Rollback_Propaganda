#include "../include/game_concepts.hpp"
#include <iostream>

void future::shuffle(std::mt19937& rng){
	std::cout<<"shuffling the future!"<<std::endl;
	shuffle_with(future, rng);
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


// a card loses its modifications when it changes zone
void future::put_on_top(const event& event){
	future.push_back(event);
	future.back().reset();
}

std::vector<event> future::peek(size_t count) const{
	std::vector<event> top;
	for (size_t i = 0; i < count && i < future.size(); ++i) {
		top.push_back(future[future.size()-1-i]); // back() is the top
	}
	return top;
}

size_t future::remaining_cards(){
	return future.size();
}
