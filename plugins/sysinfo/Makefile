BIN=libsysinfo.so
SRCS=$(shell find . ../libs/ -maxdepth 1 | grep '\.c$$')
OBJS=$(SRCS:%.c=%.o)
CFLAGS=--std=gnu99 -g -fPIC -shared
LIBS=
INCLUDE=-I../plugin_include -I../include/ -I./
all:
	gcc -g -fPIC -shared $(SRCS) $(INCLUDE) -o $(BIN)
%.o:%.c
	gcc -c $^ $(INCLUDE) -o $@ 
clean:
	rm -rf $(BIN) $(OBJS)
