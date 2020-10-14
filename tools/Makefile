PLUGIN=ldsyswatcher_plg
PLUGIN_C_SRCS=plugin_client.c
PLUGIN_C_OBJS=plugin_client.o

INCLUDE=../include

all: 
	gcc -std=c99 -g $(PLUGIN_C_SRCS) -I $(INCLUDE) -o $(PLUGIN) 

clean:
	rm -rf $(PLUGIN_C_OBJS) $(PLUGIN) 
