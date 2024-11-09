# Variables
CC = g++
CFLAGS = -Wall -Wextra -pedantic -std=c++11

# Target to build
ediLite: ediLite.cpp
	$(CC) $(CFLAGS) ediLite.cpp -o ediLite 