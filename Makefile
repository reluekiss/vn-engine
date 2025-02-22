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
    CFLAGS = -DWINDOWS -O3 -g -Wall -Wextra
else
    $(error Unsupported platform: $(PLATFORM))
endif

OBJ = build/boundedtext.o

all: build/main

build/main: build src/main.c $(OBJ)
	mkdir -p build
	$(CC) src/main.c $(CFLAGS) -o build/main $(LIBS) $(OBJ)

build/boundedtext.o: build src/boundedtext.c src/boundedtext.h
	$(CC) -c $(CFLAGS) -o build/boundedtext.o src/boundedtext.c 

lua: build $(OBJ)
	$(MAKE) -C external/lua-5.4.7
	$(CC) $(CFLAGS) src/luamain.c build/lua/lib/liblua.a $(OBJ) -o build/luamain -Ibuild/lua/include $(LIBS)

build:
	mkdir -p build

clean:
	rm -rf build
