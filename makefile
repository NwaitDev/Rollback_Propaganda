# Rollback Propaganda : simulation
#
#   make             compile les deux programmes, ./simulation et ./campaign
#   make run         lance la simulation (mise en place et affichage)
#   make play        joue une partie complète
#   make experiment  joue la campagne EXP (par défaut experiments/base.json)
#                    et écrit ses résultats dans results/<nom>/
#   make check       compile avec les sanitizers, joue CHECK_RUNS parties
#                    avec chaque deck de decks/ et la campagne de test
#                    experiments/smoke.json
#   make valgrind    joue une partie complète sous valgrind
#   make dashboard   ouvre le tableau de bord (Python : voir dashboard/requirements.txt)
#   make dashboard-test  teste le tableau de bord sur de petites campagnes de test
#   make test        tests unitaires (C++, avec les sanitizers) puis tests des
#                    programmes (Python) ; TEST_ARGS filtre les tests C++, par
#                    exemple TEST_ARGS='-tce="*[slow]*"' (voir tests/README.md)
#   make test-all    make test, make check et make dashboard-test
#   make golden      réécrit les sorties de référence des tests (après un
#                    changement voulu du déroulement des parties)
#   make clean       supprime tout ce qui a été généré
#
# ARGS transmet des options à ./simulation, par exemple :
#   make play ARGS="--deck decks/modifiers.json"
#   make play ARGS="--deck decks/a.json --deck decks/b.json --deck decks/c.json"
#
# Deux variantes sont compilées chacune dans son dossier, avec les mêmes
# règles : build/release (optimisée) et build/check (instrumentée).
# Chaque programme a son propre point d'entrée (MAINS) ; tout le reste du
# code est commun.

CXX       := g++
CPPFLAGS  := -Iinclude -isystem third_party -MMD -MP
CXXFLAGS  := -std=c++17 -Wall -Wextra -Wno-unused-parameter

SRC_DIR   := src
BUILD_DIR := build
SOURCES   := $(wildcard $(SRC_DIR)/*.cpp)
MAINS     := $(SRC_DIR)/main.cpp $(SRC_DIR)/campaign_main.cpp
COMMON    := $(filter-out $(MAINS),$(SOURCES))
EXP       ?= experiments/base.json
PYTHON    ?= ./bin/python3
DECKS     := $(wildcard decks/*.json)
ARGS      ?=

# objets communs d'une variante : src/x.cpp -> <dossier>/x.o
objects_in = $(COMMON:$(SRC_DIR)/%.cpp=$(1)/%.o)


# --- variante release ------------------------------------------------------

RELEASE_DIR   := $(BUILD_DIR)/release
RELEASE_FLAGS := -O2 -g
RELEASE_OBJS  := $(call objects_in,$(RELEASE_DIR))
BINARY        := simulation
CAMPAIGN      := campaign


# --- variante check --------------------------------------------------------
# AddressSanitizer : débordements, use-after-free, fuites mémoire
# UBSan            : division par zéro, fin de fonction sans return, ...
# -Werror          : le moindre warning fait échouer la vérification

CHECK_DIR     := $(BUILD_DIR)/check
CHECK_FLAGS   := -O1 -g -Werror -fno-omit-frame-pointer \
                 -fsanitize=address,undefined -fno-sanitize-recover=undefined
CHECK_OBJS    := $(call objects_in,$(CHECK_DIR))
CHECK_BINARY  := $(CHECK_DIR)/simulation
CHECK_CAMPAIGN := $(CHECK_DIR)/campaign
CHECK_RUNS    ?= 5


# --- tests -------------------------------------------------------------------
# les tests C++ (tests/*.cpp, doctest) sont liés aux objets communs de la
# variante check : ils profitent des mêmes sanitizers

TEST_DIR      := $(CHECK_DIR)/tests
TEST_SOURCES  := $(wildcard tests/*.cpp)
TEST_OBJS     := $(TEST_SOURCES:tests/%.cpp=$(TEST_DIR)/%.o)
TEST_BINARY   := $(CHECK_DIR)/unit_tests
TEST_ARGS     ?=


# --- cibles ----------------------------------------------------------------

.PHONY: all run play experiment check valgrind dashboard dashboard-test test test-all golden clean
.DELETE_ON_ERROR:

all: $(BINARY) $(CAMPAIGN)

run: $(BINARY)
	./$(BINARY) $(ARGS)

play: $(BINARY)
	./$(BINARY) --play $(ARGS)

experiment: $(CAMPAIGN)
	./$(CAMPAIGN) $(EXP)

# chaque deck joue CHECK_RUNS parties, mélangées différemment ;
# le journal de chaque partie reste dans build/check/<deck>_<N>.log
check: $(CHECK_BINARY) $(CHECK_CAMPAIGN)
	@for deck in $(DECKS); do \
		echo "=== deck $$deck"; \
		for i in $$(seq 1 $(CHECK_RUNS)); do \
			log=$(CHECK_DIR)/$$(basename $$deck .json)_$$i.log; \
			if ./$(CHECK_BINARY) --play --deck $$deck > $$log 2>&1; then \
				echo "--- partie $$i : OK"; \
			else \
				echo "--- partie $$i : ERREUR (journal complet : $$log)"; \
				grep -E "^seed:|runtime error|ERROR|SUMMARY|terminate|error:" $$log | head -20; \
				exit 1; \
			fi; \
		done; \
	done
	@echo "=== campagne de test experiments/smoke.json"
	@./$(CHECK_CAMPAIGN) experiments/smoke.json > $(CHECK_DIR)/smoke.log 2>&1 \
		&& ./$(CHECK_CAMPAIGN) experiments/smoke.json --replay with_smart 3 12 >> $(CHECK_DIR)/smoke.log 2>&1 \
		|| { echo "--- ERREUR (journal complet : $(CHECK_DIR)/smoke.log)"; \
		     grep -E "runtime error|ERROR|SUMMARY|terminate|error:" $(CHECK_DIR)/smoke.log | head -20; exit 1; }
	@echo "--- OK"
	@echo "=== $(words $(DECKS)) decks x $(CHECK_RUNS) parties et une campagne sans warning, erreur mémoire ni comportement indéfini"

valgrind: $(BINARY)
	valgrind -s --leak-check=full --track-origins=yes ./$(BINARY) --play $(ARGS)

dashboard: $(CAMPAIGN)
	$(PYTHON) -m streamlit run dashboard/app.py

# the test campaigns are written in build/dashboard_test, never in results/
dashboard-test: $(CAMPAIGN)
	dashboard/tests/make_test_results.sh build/dashboard_test
	ROLLBACK_TEST_RESULTS=build/dashboard_test/results $(PYTHON) -m pytest -q dashboard/tests

# the C++ tests open the files of the project: they run from its root
test: $(TEST_BINARY) $(BINARY) $(CAMPAIGN)
	./$(TEST_BINARY) $(TEST_ARGS)
	$(PYTHON) -m pytest -q tests/integration

test-all: test check dashboard-test

golden: $(BINARY)
	tests/integration/make_golden.sh

clean:
	rm -rf $(BUILD_DIR) $(BINARY) $(CAMPAIGN)


# --- édition de liens ------------------------------------------------------

$(BINARY): $(RELEASE_OBJS) $(RELEASE_DIR)/main.o
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $^ -o $@

$(CAMPAIGN): $(RELEASE_OBJS) $(RELEASE_DIR)/campaign_main.o
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $^ -o $@

$(CHECK_BINARY): $(CHECK_OBJS) $(CHECK_DIR)/main.o
	$(CXX) $(CXXFLAGS) $(CHECK_FLAGS) $^ -o $@

$(CHECK_CAMPAIGN): $(CHECK_OBJS) $(CHECK_DIR)/campaign_main.o
	$(CXX) $(CXXFLAGS) $(CHECK_FLAGS) $^ -o $@

$(TEST_BINARY): $(CHECK_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(CHECK_FLAGS) $^ -o $@


# --- compilation -----------------------------------------------------------
# "| dossier" : le dossier doit exister, mais sa date est ignorée

$(RELEASE_DIR)/%.o: $(SRC_DIR)/%.cpp | $(RELEASE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(RELEASE_FLAGS) -c $< -o $@

$(CHECK_DIR)/%.o: $(SRC_DIR)/%.cpp | $(CHECK_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CHECK_FLAGS) -c $< -o $@

$(TEST_DIR)/%.o: tests/%.cpp | $(TEST_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CHECK_FLAGS) -c $< -o $@

$(RELEASE_DIR) $(CHECK_DIR) $(TEST_DIR):
	mkdir -p $@

# dépendances vers les headers, générées automatiquement par -MMD -MP
-include $(wildcard $(RELEASE_DIR)/*.d $(CHECK_DIR)/*.d $(TEST_DIR)/*.d)
