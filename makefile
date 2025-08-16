# For Linux
REQUIRED_LIBS := -lstdc++
SRC_DIR := src
BUILD_DIR := build
COMPILER := g++
INC_DIR := include
INC_FLAGS := -I $(INC_DIR)
OTHER_FLAGS := -std=c++17 -Wextra -O2 -g

ADD_LIB:=

SOURCES := $(shell find $(SRC_DIR) -name '*.cpp' | sed 's|^.*/||')
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:%.cpp=%.o))

BINARY := simulation

all: $(BINARY)

build:
	mkdir build

$(BINARY) : $(OBJECTS)
	$(COMPILER) $^ $(OTHER_FLAGS) $(INC_FLAGS) -o $@

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.cpp $(INC_DIR)/game_concepts.hpp build
	$(COMPILER) $< -c $(INC_FLAGS) $(OTHER_FLAGS) -o $@

run :
	./$(BINARY)

check-run :
	valgrind -s --leak-check=full --track-origins=yes $(BINARY)

clean:
	rm build/* $(BINARY)
	rmdir build
