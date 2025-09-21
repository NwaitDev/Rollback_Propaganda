#include "../include/game_concepts.hpp"
#include <cstddef>
#include <stdexcept>

void lair::store_ether(shenanigan card){
	if(card.get_shtype()==ETHER){
		ether_pool.push_back(card);
	} else {
		throw std::runtime_error("cannot store a non-ether card");
	}
}

std::list<shenanigan> lair::cast_spell(shenanigan card, size_t consumed_ether){
	if (card.get_shtype()!=SPELL) {
		throw std::runtime_error("cannot cast a non-spell shenanigan");
	}
	std::list<shenanigan> to_discard;
	if(consumed_ether > ether_pool.size()){
		// TODO : Algorithm in case ethers can have different values
		throw std::runtime_error("cannot draw that many ether from your pool");
	} else {
		for (size_t i = consumed_ether; i>0; --i) {
			to_discard.push_back(ether_pool.back());
			ether_pool.pop_back();
		}
	}
	to_discard.push_back(card);
	return to_discard;
}

void lair::prepare_shenanigan(shenanigan card){
	prepared_shenanigan = card;
}

shenanigan lair::reveal_shenanigan(){
	shenanigan sh = prepared_shenanigan;
	prepared_shenanigan = NULL;
	return sh;
}