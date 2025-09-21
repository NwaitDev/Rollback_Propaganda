#include "../include/game_concepts.hpp"
#include <stdexcept>


shenanigan::shenanigan(int ether_val) : shtype(ETHER), value({ether_val}){};

shenanigan::shenanigan(spell_type spell) : shtype(SPELL), value({spell}){}

shenanigan::shenanigan() : shtype(NOTHING) {}

shenanigan_type shenanigan::get_shtype(){
	return shtype;
}

int shenanigan::get_ether_value(){
	switch (shtype) {
		case ETHER:
			return value.ether_value;
		case SPELL:
			throw std::runtime_error("cannot retrieve ether value of a spell");
		case NOTHING:
			throw std::runtime_error("cannot retrieve ether value of a nothing shenanigan");
	}
}

spell_type shenanigan::get_spell_type(){
	switch (shtype) {
		case ETHER:
			throw std::runtime_error("cannot retrieve spell type of an ether");
		case SPELL:
			return value.spell;
		case NOTHING:
			throw std::runtime_error("cannot retrieve spell type of a nothing shenanigan");
	}
}