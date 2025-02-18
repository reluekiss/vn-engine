PLATFORM ?= linux

ifeq ($(PLATFORM), linux)
    CC = cc
    LIBS = -lraylib -lm
    CFLAGS = -O3 -ggdb -Wall -Wextra
else ifeq ($(PLATFORM), darwin)
    CC = cc
    LIBS = -lraylib -lm
    CFLAGS = -O3 -ggdb -Wall -Wextra
else ifeq ($(PLATFORM), windows)
    CC = gcc
    LIBS = -lraylib -lm -lgdi32 -lwinmm
    CFLAGS = -O3 -g -Wall -Wextra
else
    $(error Unsupported platform: $(PLATFORM))
endif

OBJ = build/boundedtext.o

all: build/main

build/main: src/main.c $(OBJ)
	mkdir -p build
	$(CC) src/main.c $(CFLAGS) -o build/main $(LIBS) $(OBJ)

build/boundedtext.o: src/boundedtext.c src/boundedtext.h
	mkdir -p build
	$(CC) -c $(CFLAGS) -o build/boundedtext.o src/boundedtext.c 

clean:
	rm -rf build
