CC = g++
CC_ARGS = -std=c++11 -Wall --pedantic

all: main system router

main: main.cpp
	$(CC) $(CC_ARGS) main.cpp -o main

router: router.cpp
	$(CC) $(CC_ARGS) router.cpp -o router

system: system.cpp
	$(CC) $(CC_ARGS) system.cpp -o system

clean: saaf
	rm -rf *.o *.out main router system

saaf:
	rm -f fifo_* file_*