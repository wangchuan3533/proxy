CC=gcc
RM=rm -rf
INCLUDE_PATH=$(JUMBO_ROOT)/include
LIBRARY_PATH=$(JUMBO_ROOT)/lib
C_FLAGS=-I$(INCLUDE_PATH) -g -Wall -DTRACE -DSEE_DATA
LD_FLAGS=-L$(LIBRARY_PATH) -levent
HEADERS=$(wildcard *.h)
SOURCES=$(wildcard *.c)
OBJS=$(patsubst %c, %o, $(SOURCES))
OBJ=server

#echo server
ECHO_FLAGS=-DECHO_SERVER
ECHO_OBJS=$(patsubst %c, %eo, $(SOURCES))
ECHO_OBJ=echo_server


all: $(OBJ) $(ECHO_OBJ)

$(OBJ): $(OBJS)
	$(CC) $(LD_FLAGS) $(OBJS) -o $@

%.o: %.c $(HEADERS)
	$(CC) $(C_FLAGS) -c $<

$(ECHO_OBJ): $(ECHO_OBJS)
	$(CC) $(LD_FLAGS) $(ECHO_OBJS) -o $@

%.eo: %.c $(HEADERS)
	$(CC) $(ECHO_FLAGS) $(C_FLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(OBJ) $(ECHO_OBJS) $(ECHO_OBJ)
