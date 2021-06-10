CC = g++
CC_ARGS = -std=c++11 -Wall --pedantic

all: main system switch

main: main.cpp
	$(CC) $(CC_ARGS) main.cpp -o main

switch: switch.cpp
	$(CC) $(CC_ARGS) switch.cpp -o switch

system: system.cpp
	$(CC) $(CC_ARGS) system.cpp -o system

clean: saaf
	rm -rf *.o *.out main switch system

saaf:
	rm -f fifo_* file_*