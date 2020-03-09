PLUGIN_AA=libpluginAA.so
PLUGIN_AA_C_SRCS=libpluginAA.c
PLUGIN_AA_C_OBJS=libpluginAA.o

PLUGIN_BB=libpluginBB.so
PLUGIN_BB_C_SRCS=libpluginBB.c
PLUGIN_BB_C_OBJS=libpluginBB.o

PLUGIN_CC=libpluginCC.so
PLUGIN_CC_C_SRCS=libpluginCC.c
PLUGIN_CC_C_OBJS=libpluginCC.o


INCLUDE=../../include

all: 
	gcc -std=c89 -g -fPIC -shared $(PLUGIN_AA_C_SRCS) -I $(INCLUDE) -o $(PLUGIN_AA)
	gcc -std=c99 -g -fPIC -shared $(PLUGIN_BB_C_SRCS) -I $(INCLUDE) -o $(PLUGIN_BB)
	gcc -std=c99 -g -fPIC -shared $(PLUGIN_CC_C_SRCS) -I $(INCLUDE) -o $(PLUGIN_CC)
        
%.o:%.c
	gcc -std=c99 -c $^ -I $(INCLUDE) -o $@ 

clean:
	rm -rf $(PLUGIN_C_OBJS) $(PLUGIN) $(PLUGIN_AA) $(PLUGIN_AA_C_OBJS) $(PLUGIN_BB) $(PLUGIN_BB_C_OBJS) $(PLUGIN_CC) $(PLUGIN_CC_C_OBJS) 
install:
	cp -rf $(PLUGIN_AA) /usr/lib
	cp -rf $(PLUGIN_BB) /usr/lib
	cp -rf $(PLUGIN_CC) /usr/lib
