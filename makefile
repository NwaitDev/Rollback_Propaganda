# For Linux
REQUIRED_LIBS := -lstdc++
SRC_DIR := src
COMPILER := g++
INC_DIR := include
INC_FLAGS := -I $(INC_DIR)
OTHER_FLAGS := -std=c++17 -Wextra -O2 -g

ADD_LIB:=


OBJS_FILES := build/main.o build/game_concepts.o

all: build/simulation

build:
	mkdir build

build/game_concepts.o : $(SRC_DIR)/game_concepts.cpp $(INC_DIR)/game_concepts.hpp build
	$(COMPILER) $< -c -o $@ $(INC_FLAGS) $(OTHER_FLAGS)

build/main.o : $(SRC_DIR)/main.cpp build
	$(COMPILER) $< -c -o $@ -g $(INC_FLAGS) $(OTHER_FLAGS)

build/simulation : $(OBJS_FILES) 
	$(COMPILER) $^ -o $@ $(OTHER_FLAGS) $(INC_FLAGS)

run :
	build/simulation

check-run :
	valgrind -s --leak-check=full --track-origins=yes build/simulation

clean:
	rm build/*
