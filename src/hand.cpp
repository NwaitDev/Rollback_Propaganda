#include "../include/game_concepts.hpp"
#include <stdexcept>


hand::hand(size_t cards_limit) : cards_limit(cards_limit){};

void hand::add_card(card_ptr card){
	cards.push_back(std::move(card));
};

card_ptr hand::play_card(size_t index){
	if (index >= cards.size()) {
		throw std::out_of_range("no card at this index in hand");
	}
	card_ptr card = std::move(cards[index]);
	// fill the hole with the last card (the order of the hand is irrelevant)
	if (index != cards.size()-1) {
		cards[index] = std::move(cards.back());
	}
	cards.pop_back();
	return card;
};

const shenanigan& hand::card_at(size_t index) const {
	return *cards.at(index);
}

size_t hand::get_nb_cards() const {
	return cards.size();
}

void hand::draw_up_to_limit(grimory& deck){
	while ((cards.size()<cards_limit) && !deck.is_empty()) {
		add_card(deck.draw());
	}
}
