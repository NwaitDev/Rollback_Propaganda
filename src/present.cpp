#include "../include/game_concepts.hpp"
#include <cstddef>
#include <iostream>

stealth present::get_stealth() const {
	return current_stealth;
}

const event present::draw_last(){
	event drawn = present.back();
	present.pop_back();
	return drawn;
}

void present::set_stealth(const stealth& st){
	current_stealth = st;
}

const event present::get_last_event(){
	if(internally_reversed_for_reallocation_avoidance_purpose){
		reverse_for_reallocation_avoidance_purpose();
	}
	return present.back();
}

void present::discard_last(){
	if(internally_reversed_for_reallocation_avoidance_purpose){
		reverse_for_reallocation_avoidance_purpose();
	}
	present.pop_back();
}

void present::view(){
	if(internally_reversed_for_reallocation_avoidance_purpose){
		reverse_for_reallocation_avoidance_purpose();
	}
	size_t i = 0;
	std::cout<<"(present)\n";
	for (event card : present) {
		std::cout<<"card n°"<< i++ <<": "; card.view(); std::cout<<std::endl;
	}
}

stealth present::append_from_future(future& future){
	if(internally_reversed_for_reallocation_avoidance_purpose){
		reverse_for_reallocation_avoidance_purpose();
	}
	present.push_back(future.draw());
	current_stealth += present.back().get_stealth();
	return current_stealth;
}

void present::rollback(future& future){
	if(internally_reversed_for_reallocation_avoidance_purpose){
		reverse_for_reallocation_avoidance_purpose();
	}
	for (size_t i = 0 ; i < present.size(); ++i) {
		future.put_on_top(draw_last());
	}
}

void present::reverse_for_reallocation_avoidance_purpose(){
	size_t pos1;
	size_t pos2;
	for (size_t i = 0; i<present.size()/2; ++i) {
		pos1 = i;
		pos2 = present.size()-1-i;
		event temp = present[pos1];
		present[pos1] = present[pos2];
		present[pos2] = temp;
	}
	internally_reversed_for_reallocation_avoidance_purpose = !internally_reversed_for_reallocation_avoidance_purpose;
}

const event present::execute(game& g, player& controller) {
	if (!internally_reversed_for_reallocation_avoidance_purpose) {
		reverse_for_reallocation_avoidance_purpose();
	}
	controller.execute(present.back(), g);
	event to_pass = present.back();
	present.pop_back();
	return to_pass;
}

bool present::is_empty() const {
	return present.empty();
}

