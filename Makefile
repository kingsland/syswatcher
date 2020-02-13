CORE_DIR=core
IPMISERVER_DIR=ipmiserver
PLUGINS_DIR=metrics

#AGENT_LIB=libagent.so
#AGENT_LIB_C=lib/libagent.c
#AGENT_LIB_O=$(AGENT_LIB_C:%.c=%.o)
#SRV_PATH=./
#SRV_C_SRCS=$(shell find $(SRV_PATH) -maxdepth 1 | grep '\.c$$')
#SRV_C_OBJS=$(SRV_C_SRCS:%.c=%.o)
#LIBS=-lpthread
#INCLUDE=-I./ -I./lib
#CFLAGS=-fgnu89-inline -D_GNU_SOURCE
#CFLAGS += -rdynamic -g -DPRINT_DEBUG #for debug

all:firmware #ipmisrv plugins
firmware:
	make -C ./$(CORE_DIR)
clean:
	make -C ./$(CORE_DIR) clean
#lib:$(AGENT_LIB)
#$(AGENT_LIB):$(AGENT_LIB_O)
#	cc -o $(AGENT_LIB) -shared $(AGENT_LIB_O)
#$(AGENT_LIB_O):$(AGENT_LIB_C)
#	gcc -c $^ -fPIC -o $@
#$(SRV_C_OBJS):%.o:%.c
#	gcc -c $^ $(INCLUDE) -o $@ $(CFLAGS)
#tests:
#	make -C test
#
#clean:
#	rm -rf $(SRV_C_OBJS) $(AGENT_LIB_O) $(BIN) $(AGENT_LIB)
#	make -C ipmi_driver clean
#	make -C test clean
