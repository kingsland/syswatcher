BIN=syswatcher
SRCS=$(shell find . -maxdepth 1 | grep '\.c$$')
OBJS=$(SRCS:%.c=%.o)
CFLAGS=-Wall
LIBS=-lpthread

all:$(BIN)
$(BIN):$(OBJS)
	cc -o $@ $^ $(LIBS)
$(OBJS):%.o:%.c
	gcc $(CFLAGS) $(LIBS) -c $^ -I../include -o $@
clean:
	rm -rf $(BIN) $(OBJS)
