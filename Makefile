# Variables
CC = g++
CFLAGS = -Wall -Wextra -pedantic -std=c++11

# Target to build
kilo: kilo.cpp
	$(CC) $(CFLAGS) kilo.cpp -o kilo 