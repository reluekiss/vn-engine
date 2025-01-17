CC = cc
LIBS = -lraylib -lm
CFLAGS = -O2

main: src/main.c
	mkdir -p build
	$(CC) src/main.c $(CFLAGS) -o build/main $(LIBS)

clean:
	rm -rf build
