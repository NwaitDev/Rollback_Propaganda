#include "../include/game_concepts.hpp"


hand::hand(size_t cards_limit) : cards_limit(cards_limit){};

void hand::add_card(shenanigan card){
	cards.push_back(card);
};

shenanigan hand::play_card(size_t index){
	shenanigan card = cards[index];
	cards[index]=cards[cards.size()-1];
	cards.pop_back();
	return card;
};

size_t hand::get_nb_cards(){
	return cards.size();
}

void hand::draw_up_to_limit(grimory deck){
	while ((cards.size()<cards_limit) && !deck.is_empty()) {
		add_card(deck.draw());
	}
}