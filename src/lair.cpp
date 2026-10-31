#include "../include/game_concepts.hpp"
#include <stdexcept>

void lair::prepare_shenanigan(card_ptr card){
	prepared.push_back(std::move(card));
}

card_ptr lair::take_prepared_shenanigan(size_t index){
	if (index >= prepared.size()) {
		throw std::out_of_range("no schemed card at this index");
	}
	card_ptr card = std::move(prepared[index]);
	prepared.erase(prepared.begin() + index);
	return card;
}

std::vector<card_ptr> lair::take_all_prepared(){
	return std::move(prepared);
}

void lair::store_ether(card_ptr card){
	ether_pool.push_back(std::move(card));
}

std::vector<card_ptr> lair::spend_ether(unsigned amount){
	if (amount > ether_pool.size()) {
		throw std::logic_error("cannot spend that much ether from the pool");
	}
	std::vector<card_ptr> spent;
	for (unsigned i = 0; i < amount; ++i) {
		spent.push_back(std::move(ether_pool.back()));
		ether_pool.pop_back();
	}
	return spent;
}
