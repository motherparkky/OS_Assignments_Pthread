.SUFFIXES : .c .o

CC = /usr/bin/gcc
CPP = /usr/bin/g++
CFLAGS = -pthread -g -c -Wall
LDFLAGS= -Wall -lm
LIBFILE= -pthread

SRC= datasplit.c

OBJ= $(addsuffix .o, $(basename $(SRC)))

TARGET= datasplit

all: $(TARGET)

datasplit: datasplit.o
	$(CC) $(LDFLAGS) $(LIBDIR) $(LIBFILE) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) $(INCDIR) $<

clean:
	rm -rf $(OBJ) $(TARGET) core
