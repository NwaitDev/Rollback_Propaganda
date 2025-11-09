#include "../include/game_concepts.hpp"
#include <iostream>
#include <stdexcept>


event::event(faction faction, int value, ::stealth stealth)
: kind(PROPAGANDA), type({.faction = faction}), value(value), st(stealth), modification(0){}

event::event(event_kind ekind, ::stealth stealth, int value){
	if ((ekind!=PROPAGANDA) && (ekind!=OPERATOR)) {
		this->kind = ekind;
		this->type.faction = NONE;
		this->value = value;
		this->st = stealth;
	}else{
		throw std::runtime_error("cannot instantiate an event of kind OPERATOR or PROPAGANDA without additional arguments");
	}
}


event::event(event_operator op, event& left, event& right)
: kind(OPERATOR), type({.op = op}), value(0), left(&left), right(&right) {}


stealth event::get_stealth() const {
	if (kind==OPERATOR) {
		return left->get_stealth()+right->get_stealth();
	}
	return st;
}

void event::view() const {
	switch (kind) {

        case OPERATOR:
		switch (type.op) {
            case CONVERGENCE:
			left->view();
			std::cout<<" AND ";
			right->view();
			break;
            case DIVERGENCE:
			left->view();
			std::cout<<" OR ";
			right->view();
            break;
        }
		break;
        case PROPAGANDA:
			std::cout<<value;
			switch (type.faction) {
				case RED:
				std::cout<<" RED";
				break;
				case YELLOW:
				std::cout<<" YELLOW";
				break;
				case BLUE:
				std::cout<<" BLUE";
				break;
				case ANY:
				std::cout<<" ANY";
				break;
				case NONE:
				std::cout<<" NONE?";
				break;
			}
			std::cout<<" ("<<st<<")";
			break;
        case ELECTION:
			std::cout<<"ELECTION "<<" ("<<st<<")";
			break;
        case EXPLOSION:
			std::cout<<"EXPLOSION";
			break;
        case COLLAPSE:
			std::cout<<"COLLAPSE";
			break;
        case MISTAKE:
			std::cout<<"MISTAKE (x"<<value<<")";
          break;
        }
}

int event::get_value() const {
	if (get_kind()!=PROPAGANDA) {
		std::cout<<"error thrown by "; view();
		throw std::runtime_error("can't get the value of a non-propaganda event");
	}
	return value+modification;
}

enum faction event::get_faction() const {
	if (get_kind()!=PROPAGANDA) {
		std::cout<<"error thrown by "; view();
		throw std::runtime_error("can't get the faction of a non-propaganda event");
	}
	return type.faction;
}

enum event_kind event::get_kind() const {
	return kind;
}

enum event_operator event::get_operator() const {
	if (kind != OPERATOR) {
		std::cout<<"error thrown by "; view();
		throw std::runtime_error("can't get the operator of a non-operator event");
	}
	return type.op;
}

event& event::get_left() {
	return *left;
}

event& event::get_right(){
	return *right;
}

void event::modify(const int modif) { 
	switch (get_kind()) {
        case OPERATOR:
		modification = modif;
		get_left().modify(modif);
		break;
        case PROPAGANDA:
		modification = modif;
        case ELECTION:
        case EXPLOSION:
        case COLLAPSE:
        case MISTAKE:
          break;
        }
}

void event::reset() { 
	switch (get_kind()) {
        case OPERATOR:
		modification = 0;
		get_left().reset();
		break;
        case PROPAGANDA:
		modification = 0;
        case ELECTION:
        case EXPLOSION:
        case COLLAPSE:
        case MISTAKE:
          break;
        }
 }