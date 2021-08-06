BIN=syswatcher
SRCS=$(shell find . -maxdepth 1 | grep '\.c$$')
OBJS=$(SRCS:%.c=%.o)
CFLAGS=-Wl,-E -Wall -std=gnu99 -g
LIBS=-lpthread -ldl
INC=-I../include -I./

all:PRE_BUILD $(BIN)
PRE_BUILD:
	./genvers.sh
$(BIN):$(OBJS)
	cc $(CFLAGS) -o $@ $^ $(LIBS)
$(OBJS):%.o:%.c
	gcc $(CFLAGS) $(LIBS) -c $^ ${INC} -o $@
clean:
	rm -rf $(BIN) $(OBJS) vers.h
