#include "../include/game_concepts.hpp"
#include <algorithm>
#include <random>

grimory::grimory(){}

shenanigan grimory::draw(){
	shenanigan card = cards.back();
	cards.pop_back();
	return card;
}

void grimory::shuffle(){ // thx stack overflow
	// c++11 takes advantage of efficiently copying by value.
	std::vector<shenanigan> temp(std::make_move_iterator(cards.begin()),
							std::make_move_iterator(cards.end()));

	// clear out the zombie objects that will have been left by the move
	cards.clear();

	std::random_device rd;
    std::mt19937 g(rd());
	
	// those clever fellows in the standards committee thought of everything...
	// even shuffling a deck of cards!
	std::shuffle(temp.begin(), temp.end(),g);

	// move the cards back into the linked list in the correct order
	std::move(temp.begin(), temp.end(), back_inserter(cards));
}

void grimory::put_at_bottom(shenanigan card){
	cards.push_front(card);
}