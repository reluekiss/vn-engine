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

HEAD = external/lua-5.4.7/src/luaconf.h external/lua-5.4.7/src/lua.h external/lua-5.4.7/src/lualib.h external/lua-5.4.7/src/lauxlib.h
OBJ = build/boundedtext.o

all: build/main

build/main: build $(OBJ)
	mkdir -p build/lua/include
	make -C external/lua-5.4.7/src a
	install -p -m 644 $(HEAD) build/lua/include
	install -p external/lua-5.4.7/src/liblua.a build/lua
	$(CC) $(CFLAGS) src/main.c build/lua/liblua.a $(OBJ) -o build/main -Ibuild/lua/include $(LIBS)

build/boundedtext.o: build src/boundedtext.c src/boundedtext.h
	$(CC) -c $(CFLAGS) -o build/boundedtext.o src/boundedtext.c 

run:
	./build/main

build:
	mkdir -p build

clean:
	rm -f external/lua-5.4.7/src/*.o external/lua-5.4.7/src/*.a
	rm -rf build
