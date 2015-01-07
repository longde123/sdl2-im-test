BIN=sdl2im
LIBS=sdl2 icu-uc

all: $(BIN)

$(BIN): sdl2im.c
	gcc sdl2im.c -g -o $@ -std=c99 `pkg-config --cflags --libs $(LIBS)` -lSDL2_ttf

clean:
	rm -f *.o
	rm $(BIN)
