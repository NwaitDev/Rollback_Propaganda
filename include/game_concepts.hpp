#ifndef __GAME_CONCEPTS__
#define __GAME_CONCEPTS__

#include <cstddef>
#include <vector>


using stealth = int;
class game;

enum event_kind {
	OPERATOR, PROPAGANDA, ELECTION, EXPLOSION, COLLAPSE, MISTAKE
};

const int factions_count = 3;
enum influence {
	RED, // the fellowship
	GREEN, // the architects
	BLUE, // the empire
	ANY, // any one of the previously mentionned factions
	NONE // neutral event
};

enum event_operator {
	 CONVERGENCE, // AND operator for events
	 DIVERGENCE // exclusive OR operator for events
};

enum strategy {
	RANDOM,
	SMART
};

class event{
	public:
	event(influence faction, int value, stealth stealth);
	event(event_kind ekind, stealth stealth, int value);
	event(event_operator etype, const event& left, const event& right);

	/*
	returns the number of point a faction could earn if
	that faction controls the event
	(specifying NONE returns 0)
	*/ 
	int interest_for(influence) const;
	/*
	returns the number of point a faction could loose if
	an ennemy ot that faction controls the event
	(specifying NONE returns 0)
	*/ 
	int interest_against(influence) const;

	stealth get_stealth() const;

	void exec(game& g) const;
	event_kind kind;
	union type {
		influence faction;
		event_operator op;
	} type ;
	const event* left; 
	const event* right;
	int value;
	stealth st;
	void view()const ;
};

class player{
	public:
	player(influence faction, stealth greed, strategy strat);
	void score_a_point();
	void view();
	influence faction;
	stealth greed;
	int points = 0;
	strategy strat;
};



class present_events{
	public:
	stealth see_next_event(std::vector<event>& future);
	void rollback(std::vector<event>& future);
	void collapse(std::vector<event>& past, game& g);
	const event last_event();
	void discard();
	void mult(int);
	std::vector<event> current_events = std::vector<event>();
	stealth st = 0;
	stealth current_mult = 1;

};

class game{
	public:
	game(stealth max_stealth);
	void add_player(player p);
	void add_event(event e);
	void play_turn();
	void play_game();
	void view();

	std::vector<int> scores = std::vector<int>({0,0,0});
	std::vector<player> players = std::vector<player>();
	size_t current_player=0;

	void shuffle();

	private:

	std::vector<event> past_events = std::vector<event>();
	present_events current_events;
	std::vector<event> future_events = std::vector<event>();
	
	stealth max_stealth;
	long elapsed_turns = 0;
};

#endif