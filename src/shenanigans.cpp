#include "../include/game_concepts.hpp"
#include <functional>
#include <map>
#include <stdexcept>

// Rules of every grimory card (see spells.md).
// To add a card: write its class below, then register it in make_card().


// --- common rules ------------------------------------------------------------

void ether::resolve(card_ptr self, game &g, player &owner) {
	owner.store_ether(std::move(self));
}

void spell::resolve(card_ptr self, game &g, player &caster) {
	unsigned ether = caster.choose_ether_to_spend(g, *this);
	if (ether > caster.available_ether()) {
		throw std::logic_error("a player chose to spend more ether than available");
	}
	if (effect(g, caster, ether)) {
		for (card_ptr &spent : caster.spend_ether(ether)) {
			g.cosmos_cards.absorb(std::move(spent));
		}
	}
	g.cosmos_cards.absorb(std::move(self));
}


// --- spells ------------------------------------------------------------------

namespace {

// Modifies the propaganda of the present chosen by the caster.
// The events of the present are controlled by the mage whose turn it is: if
// that mage is immune, the choice is cancelled (the spell is still spent).
bool modify_chosen_propaganda(game &g, player &caster, int modification) {
	event *target = caster.choose_propaganda(g, modification);
	if (target == nullptr) {
		return false;
	}
	if (!g.get_current_player().cancels_choice_by(caster)) {
		target->modify(modification);
	}
	return true;
}

// Euphorie Collective : augmente de 1 [+ éther] les valeurs de popularité de
// la carte propagande de votre choix.
class euphoria : public spell {
public:
	std::string name() const override { return "EUPHORIA"; }

protected:
	bool effect(game &g, player &caster, unsigned ether) override {
		return modify_chosen_propaganda(g, caster, 1 + static_cast<int>(ether));
	}
};

// Neurasthénie Collective : diminue de 1 [+ éther] les valeurs de popularité
// de la carte propagande de votre choix.
class neurasthenia : public spell {
public:
	std::string name() const override { return "NEURASTHENIA"; }

protected:
	bool effect(game &g, player &caster, unsigned ether) override {
		return modify_chosen_propaganda(g, caster, -1 - static_cast<int>(ether));
	}
};

// Précipitation des événements : force la personne dont c'est le tour à
// révéler 1 [+ éther] événements supplémentaires.
class precipitation : public spell {
public:
	std::string name() const override { return "PRECIPITATION"; }

protected:
	bool effect(game &g, player &caster, unsigned ether) override {
		g.reveal_x_more_events(1 + static_cast<int>(ether));
		return true;
	}
};

// Trou de mémoire contagieux : ne coûte aucun éther et se renvoie dans la main
// du mage de votre choix (un adversaire).
// It does not follow the common spell rule: no ether is chosen, and the card
// goes into a hand instead of the cosmos (unless the target is immune).
class memory_lapse : public spell {
public:
	std::string name() const override { return "MEMORY_LAPSE"; }

	void resolve(card_ptr self, game &g, player &caster) override {
		player *target = caster.choose_opponent(g);
		if (target == nullptr || target->cancels_choice_by(caster)) {
			g.cosmos_cards.absorb(std::move(self));
			return;
		}
		target->receive_card(std::move(self));
	}

protected:
	bool effect(game &g, player &caster, unsigned ether) override {
		return false; // never called: resolve() is overridden
	}
};

// Lecture : regarder secrètement les 1 [+ éther] événements du dessus du
// futur puis les y replacer dans le même ordre.
class reading : public spell {
public:
	std::string name() const override { return "READING"; }

protected:
	bool effect(game &g, player &caster, unsigned ether) override {
		if (g.future_events.remaining_cards() == 0) {
			return false;
		}
		caster.remember_future(g.future_events.peek(1 + ether));
		return true;
	}
};

// Renverser l'entropie : ajouter à sa main 1 [+ éther] cartes de votre choix
// depuis le cosmos (as many as the cosmos holds).
// The spell and the ether spent on it reach the cosmos only after the effect,
// so they cannot be taken back by it.
class entropy_reversal : public spell {
public:
	std::string name() const override { return "ENTROPY_REVERSAL"; }

protected:
	bool effect(game &g, player &caster, unsigned ether) override {
		if (g.cosmos_cards.size() == 0) {
			return false;
		}
		for (unsigned i = 0; i < 1 + ether && g.cosmos_cards.size() > 0; ++i) {
			caster.receive_card(g.cosmos_cards.take(caster.choose_cosmos_card(g)));
		}
		return true;
	}
};

// Fuite d'éther : défausser jusqu'à 1 [+ éther] cartes d'éther révélées de
// votre choix dans le cosmos (ethers of opponents, one choice at a time; the
// caster may stop before the limit). A choice aimed at an immune opponent is
// cancelled but still counts.
class ether_leak : public spell {
public:
	std::string name() const override { return "ETHER_LEAK"; }

protected:
	bool effect(game &g, player &caster, unsigned ether) override {
		if (no_opponent_ether(g, caster)) {
			return false;
		}
		for (unsigned i = 0; i < 1 + ether; ++i) {
			player *target = caster.choose_ether_to_leak(g);
			if (target == nullptr) {
				break;
			}
			if (target == &caster || target->available_ether() == 0) {
				throw std::logic_error("a player chose an invalid ether to leak");
			}
			if (!target->cancels_choice_by(caster)) {
				g.cosmos_cards.absorb(target->lose_ether());
			}
		}
		return true;
	}

private:
	static bool no_opponent_ether(game &g, player &caster) {
		for (player *p : caster.opponents(g)) {
			if (p->available_ether() > 0) {
				return false;
			}
		}
		return true;
	}
};

// Immunité parlementaire cosmique : annule les 1 [+ éther] prochaines fois où
// vous, un sort, un éther, un modificateur ou un événement que vous contrôlez
// seriez choisi.e ce tour-ci (see player::cancels_choice_by).
class immunity : public spell {
public:
	std::string name() const override { return "IMMUNITY"; }

protected:
	bool effect(game &g, player &caster, unsigned ether) override {
		caster.gain_immunity(1 + ether);
		return true;
	}
};

// Sauter un chapitre : placer les 1 [+ éther] prochaines cartes du futur dans
// le passé (as many as the future holds).
class chapter_skip : public spell {
public:
	std::string name() const override { return "CHAPTER_SKIP"; }

protected:
	bool effect(game &g, player &caster, unsigned ether) override {
		if (g.future_events.remaining_cards() == 0) {
			return false;
		}
		for (unsigned i = 0; i < 1 + ether && g.future_events.remaining_cards() > 0; ++i) {
			g.past_events.pass(g.future_events.draw());
		}
		return true;
	}
};

template <class card>
card_ptr make() {
	return std::make_unique<card>();
}

}


// --- names used in the deck files ----------------------------------------------

card_ptr make_card(const std::string &name) {
	static const std::map<std::string, std::function<card_ptr()>> factory = {
		{"ETHER", make<ether>},
		{"EUPHORIA", make<euphoria>},
		{"NEURASTHENIA", make<neurasthenia>},
		{"PRECIPITATION", make<precipitation>},
		{"MEMORY_LAPSE", make<memory_lapse>},
		{"READING", make<reading>},
		{"ENTROPY_REVERSAL", make<entropy_reversal>},
		{"ETHER_LEAK", make<ether_leak>},
		{"IMMUNITY", make<immunity>},
		{"CHAPTER_SKIP", make<chapter_skip>},
	};
	auto card = factory.find(name);
	if (card == factory.end()) {
		return nullptr;
	}
	return card->second();
}
