PLATFORM ?= linux

ifeq ($(PLATFORM), linux)
    CC = cc
    LIBS = -lraylib -lm
    # CFLAGS = -O3 -ggdb -Wall -Wextra -Wformat -Wformat=2 -Wimplicit-fallthrough -Werror=format-security -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS -fstrict-flex-arrays=3 -fstack-clash-protection -fstack-protector-strong
    CFLAGS = -O3 -ggdb
    CNOOB = -ffunction-sections -fdata-sections -flto
    # LDFLAGS = -Wl,--gc-sections -s -Wl,-z,nodlopen -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -Wl,--as-needed -Wl,--no-copy-dt-needed-entries
    # LDFLAGS = 
    LUA_CFLAGS = -DLUA_USE_POSIX
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

HEAD = external/lua-5.4.7/src/luaconf.h external/lua-5.4.7/src/lua.h external/lua-5.4.7/src/lualib.h external/lua-5.4.7/src/lauxlib.h
OBJ = build/boundedtext.o

all: build/main

build/main: build $(OBJ)
	mkdir -p build/lua
	make CFLAGS=$(LUA_CFLAGS) -C external/lua-5.4.7/src a
	install -p -m 644 $(HEAD) build/lua
	install -p external/lua-5.4.7/src/liblua.a build/lua
	install -p external/serpent.lua build/lua/serpent.lua
	$(CC) $(CFLAGS) $(CNOOB) src/main.c build/lua/liblua.a $(OBJ) -o build/main -Ibuild/lua $(LIBS) $(LDFLAGS)

build/boundedtext.o: build src/boundedtext.c src/boundedtext.h
	$(CC) -c $(CFLAGS) -o build/boundedtext.o src/boundedtext.c 

run:
	./build/main

build:
	mkdir -p build

clean:
	rm -f external/lua-5.4.7/src/*.o external/lua-5.4.7/src/*.a
	rm -rf build
