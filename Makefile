CC=g++

CFLAGS= -Wall -Wextra -lglfw -lGL -lGLEW

gltest: src/main.cpp src/Engine.cpp
	$(CC) -o gltest src/main.cpp src/Engine.cpp $(CFLAGS)

editor: src/tilemap-editor.cpp src/Engine.cpp
	$(CC) -o editor src/tilemap-editor.cpp src/Engine.cpp $(CFLAGS)
