#include "../include/game_concepts.hpp"

void future::shuffle(std::mt19937& rng){
	shuffle_with(future, rng);
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
