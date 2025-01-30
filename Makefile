CC = cc
LIBS = -lraylib -lm
CFLAGS = -O2 -ggdb -Wall -Wextra 
OBJ = build/scene.o build/boundedtext.o

all: build/main

build/main: src/main.c $(OBJ)
	mkdir -p build
	$(CC) src/main.c $(CFLAGS) -o build/main $(LIBS) $(OBJ)

build/scene.o: src/scene.c src/scene.h
	mkdir -p build
	$(CC) -c $(CFLAGS) -o build/scene.o src/scene.c 

build/boundedtext.o: src/boundedtext.c src/boundedtext.h
	mkdir -p build
	$(CC) -c $(CFLAGS) -o build/boundedtext.o src/boundedtext.c 

clean:
	rm -rf build
