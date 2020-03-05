BIN=libcpu.so
SRCS=$(shell find . -maxdepth 1 | grep '\.c$$')
OBJS=$(SRCS:%.c=%.o)
CFLAGS=--std=gnu99 -g -fPIC -shared
LIBS=
INCLUDE=../plugin_include
all:
	gcc -g -fPIC -shared $(SRCS) -I$(INCLUDE) -o $(BIN)
%.o:%.c
	gcc -c $^ -I $(INCLUDE) -o $@ 
clean:
	rm -rf $(BIN) $(OBJS)
