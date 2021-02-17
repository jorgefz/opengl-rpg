CC=g++

CFLAGS= -Wall -Wextra -lglfw -lGL -lGLEW

gltest: src/gltest.cpp
	$(CC) -o gltest src/gltest.cpp $(CFLAGS)

