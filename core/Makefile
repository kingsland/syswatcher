BIN=syswatcher
SRCS=$(shell find . -maxdepth 1 | grep '\.c$$')
OBJS=$(SRCS:%.c=%.o)
CFLAGS=-Wl,-E -std=gnu99 -g
LIBS=-lpthread -ldl

all:$(BIN)
$(BIN):$(OBJS)
	cc $(CFLAGS) -o $@ $^ $(LIBS)
$(OBJS):%.o:%.c
	gcc $(CFLAGS) $(LIBS) -c $^ -I../include -o $@
clean:
	rm -rf $(BIN) $(OBJS)
