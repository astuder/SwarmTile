CXX       := g++
CXX_FLAGS := -std=c++11 -ggdb -Wall
CXX_SRC   := ./*.cpp ../../src/*.cpp

CC        := gcc
C_FLAGS   := -std=c99 -Wall -c
C_SRC     := munit/*.c
C_OUTPUT  := munit.o

BIN     := bin
INCLUDE := -I. -I../../src
LIBRARIES   := -lpthread
EXECUTABLE  := main

all: $(BIN)/$(EXECUTABLE)

test: clean all
ifeq ($(OS),Windows_NT)
	cls
else
	clear
endif
	./$(BIN)/$(EXECUTABLE)

$(BIN)/$(EXECUTABLE): 
	$(CC) $(C_SRC) $(C_FLAGS) $(INCLUDE) -o $(BIN)/$(C_OUTPUT)
	$(CXX) $(CXX_SRC) $(CXX_FLAGS) $(INCLUDE) $^ -o $@ $(BIN)/$(C_OUTPUT) $(LIBRARIES)

clean:
ifeq ($(OS),Windows_NT)
	del /Q $(BIN)\*
else
	rm $(BIN)/*
endif
