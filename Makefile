CC = cc
LIBS = -lraylib -lm
CFLAGS = -O2
OBJ = build/scene.o build/llist.o build/hashtable.o

all: build/main

build/main: src/main.c $(OBJ)
	mkdir -p build
	$(CC) src/boundedtext.c src/main.c $(CFLAGS) -o build/main $(LIBS) $(OBJ)

build/scene.o: src/scene.c src/scene.h
	mkdir -p build
	$(CC) -c $(CFLAGS) -o build/scene.o src/scene.c 

build/llist.o: src/llist.c src/llist.h
	mkdir -p build
	$(CC) -c $(CFLAGS) -o build/llist.o src/llist.c 

build/hashtable.o: src/hashtable.c src/hashtable.h
	mkdir -p build
	$(CC) -c $(CFLAGS) -o build/hashtable.o src/hashtable.c 

clean:
	rm -rf build
