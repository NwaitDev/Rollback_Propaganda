#include "../include/game_concepts.hpp"
#include <stdexcept>

void cosmos::absorb(card_ptr card){
	cards.push_back(std::move(card));
}

card_ptr cosmos::take(size_t index){
	if (index >= cards.size()) {
		throw std::out_of_range("no card at this index in the cosmos");
	}
	card_ptr card = std::move(cards[index]);
	cards.erase(cards.begin() + index);
	return card;
}
