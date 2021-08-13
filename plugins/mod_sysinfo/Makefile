MODPATH=$(shell basename `pwd`)
MODNAME=$(subst mod_,lib,${MODPATH})
LIB=${MODNAME}.so
SRCS=$(shell find . -maxdepth 1 | grep '\.c$$')
OBJS=$(SRCS:%.c=%.o)
CFLAGS=--std=gnu99 -g -fPIC -shared
INCLUDE=-I./plugin_include  -I./
all:${LIB}
${LIB}:${OBJS}
	gcc -g -fPIC -shared $(SRCS) $(INCLUDE) -o $(LIB)
%.o:%.c
	gcc -c $^ $(INCLUDE) -o $@ 
clean:
	rm -rf $(LIB) $(OBJS)
