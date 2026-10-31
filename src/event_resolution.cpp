#include "../include/game_concepts.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

// Resolution of the events of the present (Phase de Résolution).
// The rules are applied here once for every player; the mage whose turn it
// is (the controller) only makes the choices the cards leave open.

void game::resolve_event(const event& e, player& controller){
	switch (e.get_kind()) {
		case OPERATOR:
		case PROPAGANDA: {
			std::vector<const event*> propagandas;
			collect_propagandas(e, controller, propagandas);
			apply_propagandas(propagandas, controller);
			break;
		}
		case ELECTION:
			resolve_election();
			break;

		// each one of these following kinds has no effect during the resolution
		case EXPLOSION:
		case COLLAPSE:
		case MISTAKE:
			break;
	}
}

// Gathers the simple propagandas that apply: every branch of a convergence
// (AND), the branch chosen by the controller for a divergence (OR).
void game::collect_propagandas(const event& e, player& controller,
                               std::vector<const event*>& propagandas){
	if (e.get_kind() == PROPAGANDA) {
		propagandas.push_back(&e);
		return;
	}
	if (e.get_kind() != OPERATOR) {
		return; // only propagandas can be combined for now
	}
	if (e.get_operator() == CONVERGENCE) {
		collect_propagandas(e.get_left(), controller, propagandas);
		collect_propagandas(e.get_right(), controller, propagandas);
		return;
	}
	size_t branch = controller.choose_divergence_branch(*this, e);
	if (branch > 1) {
		throw std::logic_error("a player chose an invalid branch of a divergence");
	}
	collect_propagandas((branch == 0) ? e.get_left() : e.get_right(), controller, propagandas);
}

// Applies the propagandas of one event. The factions printed on the card
// apply directly. Then the controller gives each ANY a faction that this
// event does not target yet (largest absolute value first); when every
// faction is already targeted, any faction can be chosen.
void game::apply_propagandas(const std::vector<const event*>& propagandas,
                             player& controller){
	std::vector<faction> targeted;
	std::vector<const event*> anys;
	for (const event* p : propagandas) {
		faction f = p->get_faction();
		if (f == ANY) {
			anys.push_back(p);
		} else if (f != NONE) {
			scores[f] += p->get_value();
			targeted.push_back(f);
		}
	}

	std::stable_sort(anys.begin(), anys.end(), [](const event* a, const event* b) {
		return std::abs(a->get_value()) > std::abs(b->get_value());
	});
	for (const event* any : anys) {
		std::vector<faction> allowed;
		for (faction f : {RED, YELLOW, BLUE}) {
			if (std::find(targeted.begin(), targeted.end(), f) == targeted.end()) {
				allowed.push_back(f);
			}
		}
		if (allowed.empty()) {
			allowed = {RED, YELLOW, BLUE};
		}
		faction chosen = controller.choose_faction_for_any(*this, any->get_value(), allowed);
		if (std::find(allowed.begin(), allowed.end(), chosen) == allowed.end()) {
			throw std::logic_error("a player chose a forbidden faction for an ANY propaganda");
		}
		scores[chosen] += any->get_value();
		targeted.push_back(chosen);
	}
}

// Every mage whose contract is the strictly most popular faction scores a
// point (no point on a tie).
void game::resolve_election(){
	for (faction f : {RED, YELLOW, BLUE}) {
		bool most_popular = true;
		for (faction other : {RED, YELLOW, BLUE}) {
			if (other != f && scores[other] >= scores[f]) {
				most_popular = false;
			}
		}
		if (most_popular) {
			for (std::unique_ptr<player>& p : players) {
				if (p->get_contract() == f) {
					p->score_a_point();
				}
			}
		}
	}
	std::cout<<"ELECTION EXECUTEE"<<std::endl;
}
