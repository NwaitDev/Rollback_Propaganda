#include "../include/game_concepts.hpp"
#include <stdexcept>

card_ptr grimory::draw(){
	if (cards.empty()) {
		throw std::logic_error("cannot draw from an empty grimory");
	}
	card_ptr card = std::move(cards.back());
	cards.pop_back();
	return card;
}

void grimory::shuffle(std::mt19937& rng){
	shuffle_with(cards, rng);
}

void grimory::put_at_bottom(card_ptr card){
	cards.push_front(std::move(card));
}
