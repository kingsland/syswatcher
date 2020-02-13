BIN=syswatcher
SRCS=$(shell find . -maxdepth 1 | grep '\.c$$')
OBJS=$(SRCS:%.c=%.o)
CFLAGS=-Wall

all:$(BIN)
$(BIN):$(OBJS)
	cc -o $@ $^
$(OBJS):%.o:%.c
	gcc $(CFLAGS) -c $^ -I../include -o $@
clean:
	rm -rf $(BIN) $(OBJS)
