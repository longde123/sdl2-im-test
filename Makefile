BIN=sdl2im
LIBS=sdl2 icu-uc

ifeq ($(shell uname -o),Cygwin)
  PKGCONFIG:=$(shell pkg-config --cflags --libs $(LIBS)|sed -e 's/-XCClinker//')
else
  PKGCONFIG:=`pkg-config --cflags --libs $(LIBS)`
endif

all: $(BIN)

$(BIN): sdl2im.c
	gcc sdl2im.c -g -o $@ -std=c99 $(PKGCONFIG) -lSDL2_ttf

clean:
	rm -f *.o
	rm $(BIN)
