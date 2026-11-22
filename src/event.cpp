#include "../include/game_concepts.hpp"
#include <string>
#include <stdexcept>


event::event(faction faction, int value, ::stealth stealth)
: value(value), st(stealth), kind(PROPAGANDA), type({.faction = faction}),
  left(nullptr), right(nullptr), modification(0) {}

event::event(event_kind ekind, ::stealth stealth, int value)
: value(value), st(stealth), kind(ekind), type({.faction = NONE}),
  left(nullptr), right(nullptr), modification(0) {
	if ((ekind==PROPAGANDA) || (ekind==OPERATOR)) {
		throw std::runtime_error("cannot instantiate an event of kind OPERATOR or PROPAGANDA without additional arguments");
	}
}


event::event(event_operator op, const event& left, const event& right)
: value(0), st(0), kind(OPERATOR), type({.op = op}),
  left(std::make_shared<event>(left)), right(std::make_shared<event>(right)),
  modification(0) {}


event::event(const event& other)
: value(other.value), st(other.st), kind(other.kind), type(other.type),
  left(other.left ? std::make_shared<event>(*other.left) : nullptr),
  right(other.right ? std::make_shared<event>(*other.right) : nullptr),
  modification(other.modification), id(other.id) {}

event& event::operator=(const event& other) {
	if (this != &other) {
		event copy(other);
		*this = std::move(copy);
	}
	return *this;
}


stealth event::get_stealth() const {
	if (kind==OPERATOR) {
		return left->get_stealth()+right->get_stealth();
	}
	return st;
}

// text of a card, as printed by the text views (see text_view.cpp)
std::string event::describe() const {
	switch (kind) {
		case OPERATOR:
			return left->describe()
				+ ((type.op == CONVERGENCE) ? " AND " : " OR ")
				+ right->describe();
		case PROPAGANDA: {
			std::string text = std::to_string(value);
			switch (type.faction) {
				case RED: text += " RED"; break;
				case YELLOW: text += " YELLOW"; break;
				case BLUE: text += " BLUE"; break;
				case ANY: text += " ANY"; break;
				case NONE: text += " NONE?"; break;
			}
			return text + " (" + std::to_string(st) + ")";
		}
		case ELECTION:
			return "ELECTION  (" + std::to_string(st) + ")";
		case EXPLOSION:
			return "EXPLOSION";
		case COLLAPSE:
			return "COLLAPSE";
		case MISTAKE:
			return "MISTAKE (x" + std::to_string(value) + ")";
	}
	return "?";
}

int event::get_value() const {
	if (get_kind()!=PROPAGANDA) {
		throw std::runtime_error("can't get the value of a non-propaganda event: " + describe());
	}
	return value+modification;
}

enum faction event::get_faction() const {
	if (get_kind()!=PROPAGANDA) {
		throw std::runtime_error("can't get the faction of a non-propaganda event: " + describe());
	}
	return type.faction;
}

enum event_kind event::get_kind() const {
	return kind;
}

bool event::is_propaganda_card() const {
	switch (kind) {
		case PROPAGANDA:
			return true;
		case OPERATOR:
			return left->is_propaganda_card() && right->is_propaganda_card();
		default:
			return false;
	}
}

enum event_operator event::get_operator() const {
	if (kind != OPERATOR) {
		throw std::runtime_error("can't get the operator of a non-operator event: " + describe());
	}
	return type.op;
}

event& event::get_left() {
	return *left;
}

event& event::get_right(){
	return *right;
}

const event& event::get_left() const {
	return *left;
}

const event& event::get_right() const {
	return *right;
}

void event::modify(const int modif) { 
	switch (get_kind()) {
        case OPERATOR:	// every value of a multiple propaganda is modified
		modification += modif;
		get_left().modify(modif);
		get_right().modify(modif);
		break;
        case PROPAGANDA:	// modifications add up
		modification += modif;
		break;
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
		get_right().reset();
		break;
        case PROPAGANDA:
		modification = 0;
		break;
        case ELECTION:
        case EXPLOSION:
        case COLLAPSE:
        case MISTAKE:
          break;
        }
 }
