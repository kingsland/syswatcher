BIN=syswatcher
CFLAGS=-Wl,-E -Wall -std=gnu99 -g
LIBS=-lpthread -ldl
INC=-I../include -I./
SRCS=$(shell find . -maxdepth 1 | grep '\.c$$')
SRCS+=vers.c
OBJS=$(SRCS:%.c=%.o)

all:PRE_BUILD $(BIN) AFTER_BUILD
PRE_BUILD:
	./genvers.sh
AFTER_BUILD:
	rm vers.c vers.h
$(BIN):$(OBJS)
	cc $(CFLAGS) -o $@ $^ $(LIBS)
$(OBJS):%.o:%.c
	gcc $(CFLAGS) $(LIBS) -c $^ ${INC} -o $@
clean:
	rm -rf $(BIN) $(OBJS)
