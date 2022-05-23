CC=gcc
CFLAGS=-Wall -Wno-unused

ifeq ($(BUILD),debug)
	CFLAGS+= -O0 -g -DLOGS
else
	CFLAGS += -O2 -s -DNDEBUG -DLOGS
endif

INC=-I../common/include \
-I/usr/include/libbson-1.0 \
-I/usr/include/libmongoc-1.0
LDFLAGS=-lmongoc-1.0 \
-lbson-1.0

.PHONY: clean

all: main

debug:
	make "BUILD=debug"

main: main.o
	$(CC) -o ./build/main ./build/main.o $(LDFLAGS)

main.o: ./source/main.c
	$(CC) $(CFLAGS) -c ./source/main.c -o ./build/main.o $(INC)

clean:
	rm -r ./build/*

