CC=gcc
RM=rm -rf
#INCLUDE_PATH=$(JUMBO_ROOT)/include
#LIBRARY_PATH=$(JUMBO_ROOT)/lib
#C_FLAGS=-I$(INCLUDE_PATH) -g -Wall# -DTRACE -DDUMP_DATA
#LD_FLAGS=-L$(LIBRARY_PATH) -levent
C_FLAGS=-g -Wall -DTRACE -DDUMP_DATA
LD_FLAGS=-levent -lpthread
HEADERS=$(wildcard *.h)
SOURCES=$(wildcard *.c)

#tcp echo server
O1_FLAGS=-DECHO_SERVER
O1_OBJS=$(patsubst %c, %o1, $(SOURCES))
O1_OBJ=echo_server

#websocket echo server
O2_FLAGS=-DWEBSOCKET_ECHO
O2_OBJS=$(patsubst %c, %o2, $(SOURCES))
O2_OBJ=websocket_echo_server


#websocket broadcast server
O3_FLAGS=-DWEBSOCKET_BROADCAST
O3_OBJS=$(patsubst %c, %o3, $(SOURCES))
O3_OBJ=websocket_broadcast_server

#websocket proxy server
O4_FLAGS=-DWEBSOCKET_PROXY
O4_OBJS=$(patsubst %c, %o4, $(SOURCES))
O4_OBJ=websocket_proxy_server

all: $(O1_OBJ) $(O2_OBJ) $(O3_OBJ) $(O4_OBJ)

$(O1_OBJ): $(O1_OBJS)
	$(CC) $^ -o $@ $(LD_FLAGS)

$(O2_OBJ): $(O2_OBJS)
	$(CC) $^ -o $@ $(LD_FLAGS)

$(O3_OBJ): $(O3_OBJS)
	$(CC) $^ -o $@ $(LD_FLAGS)

$(O4_OBJ): $(O4_OBJS)
	$(CC) $^ -o $@ $(LD_FLAGS)


%.o1: %.c $(HEADERS)
	$(CC) $(O1_FLAGS) $(C_FLAGS) -c $< -o $@

%.o2: %.c $(HEADERS)
	$(CC) $(O2_FLAGS) $(C_FLAGS) -c $< -o $@

%.o3: %.c $(HEADERS)
	$(CC) $(O3_FLAGS) $(C_FLAGS) -c $< -o $@

%.o4: %.c $(HEADERS)
	$(CC) $(O4_FLAGS) $(C_FLAGS) -c $< -o $@

clean:
	$(RM) $(O1_OBJ) $(O2_OBJ) $(O3_OBJ) $(O4_OBJ) $(O1_OBJS) $(O2_OBJS) $(O3_OBJS) $(O4_OBJS)
