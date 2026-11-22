#include "../include/game_concepts.hpp"
#include <cstddef>

// the present is stored in reveal order: front() is the first revealed event,
// back() is the last revealed one.

const event present::draw_last(){
	event drawn = present.back();
	present.pop_back();
	return drawn;
}

const event present::get_last_event(){
	return present.back();
}

stealth present::append_from_future(future& future){
	present.push_back(future.draw());
	current_stealth += present.back().get_stealth();
	return current_stealth;
}

void present::rollback(future& future){
	// the last revealed event goes back on the future first, so that the
	// events will be drawn again in the same order
	while (!present.empty()) {
		future.put_on_top(draw_last());
	}
	current_stealth = 0;
}

// the first revealed event, to be resolved
event present::take_first() {
	event first = present.front();
	present.pop_front();
	return first;
}
