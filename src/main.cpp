#include "../include/game_concepts.hpp"

#include <cstddef>

int main(int argc, char* argv[]){

	game g = game(21);

	g.add_player(player(RED,10,RANDOM));
	g.add_player(player(GREEN,10,RANDOM));
	g.add_player(player(BLUE,10,RANDOM));

	//adding elections
	for (size_t i=0; i<10; ++i) {
		g.add_event(event(ELECTION,3,0));
	}

	//adding positive mono propaganda
	for (size_t i=0; i<10; ++i) {
		g.add_event(event(RED,2,4));

		g.add_event(event(GREEN,2,4));

		g.add_event(event(BLUE,2,4));

		g.add_event(event(ANY,2,5));
	}

	//adding CONV propaganda
	for (size_t i=0; i<7; ++i) {
		g.add_event(
			event(CONVERGENCE,
				event(ANY,1,1),
				event(GREEN, 2, 2)
			)
		);
		g.add_event(
			event(CONVERGENCE,
				event(ANY,1,1),
				event(RED, 2, 2)
			)
		);
		g.add_event(
			event(CONVERGENCE,
				event(ANY,1,1),
				event(BLUE, 2, 2)
			)
		);
		g.add_event(
			event(CONVERGENCE,
				event(ANY,1,1),
				event(ANY, 2, 2)
			)
		);
	}

	//adding DIV propaganda
	for (size_t i=0; i<7; ++i) {
		g.add_event(
			event(DIVERGENCE,
				event(ANY,1,1),
				event(GREEN, 2, 2)
			)
		);
		g.add_event(
			event(DIVERGENCE,
				event(ANY,1,1),
				event(RED, 2, 2)
			)
		);
		g.add_event(
			event(DIVERGENCE,
				event(ANY,1,1),
				event(BLUE, 2, 2)
			)
		);
		g.add_event(
			event(DIVERGENCE,
				event(ANY,1,1),
				event(ANY, 2, 2)
			)
		);
	}

	for (size_t i=0; i<5; ++i) {
		g.add_event(event(COLLAPSE,0,0));
	}

	for (size_t i=0; i<5; ++i) {
		g.add_event(event(EXPLOSION,0,0));
	}

	for (size_t i=0; i<5; ++i) {
		g.add_event(event(MISTAKE,0,2));
	}

	g.view();

	g.shuffle();

	g.view();

	g.play_game();

	g.view();

	return 0;
}