CC=gcc
INCLUDE_PATH=$(JUMBO_ROOT)/include
LIBRARY_PATH=$(JUMBO_ROOT)/lib
C_FLAGS=-I$(INCLUDE_PATH) -g -Wall
LD_FLAGS=-L$(LIBRARY_PATH) -levent
RM=rm -rf



all:
	$(CC) $(C_FLAGS) client.c $(LD_FLAGS) -o client
	$(CC) $(C_FLAGS) client_async.c $(LD_FLAGS) -o client_async

clean:
	$(RM) client client_async
