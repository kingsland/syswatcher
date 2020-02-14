BIN=ipmiserver
SRV_PATH=./
SRV_C_SRCS=$(shell find $(SRV_PATH) -maxdepth 1 | grep '\.c$$')
SRV_C_OBJS=$(SRV_C_SRCS:%.c=%.o)
LIBS=-lpthread
INCLUDE=-I./ -I./lib
CFLAGS=-fgnu89-inline -D_GNU_SOURCE
CFLAGS += -rdynamic -g -DPRINT_DEBUG #for debug

all:$(BIN) 
$(BIN):$(SRV_C_OBJS)
	gcc -o $(BIN) $(SRV_C_OBJS) $(LIBS) $(CFLAGS)
$(SRV_C_OBJS):%.o:%.c
	gcc -c $^ $(INCLUDE) -o $@ $(CFLAGS)

clean:
	rm -rf $(SRV_C_OBJS) $(BIN) 
