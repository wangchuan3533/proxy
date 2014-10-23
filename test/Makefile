CC=gcc
RM=rm -rf
INCLUDES=-I$(JUMBO_ROOT)/include -I..
LIBRARIES=-L$(JUMBO_ROOT)/lib
C_FLAGS=-g -Wall -DDUMP_DATA
LD_FLAGS=-L$(LIBRARIES) -levent

all: unit_test test client client_async evbuffer pipe test_atoi test_const_char valgrind_uninitialised_bytes

unit_test: unit_test.o ../http.o ../websocket.o ../base64_enc.o ../sha1.o
	$(CC) $(LIBRARIES) $(LD_FLAGS) -o $@ $^

test: test.o
	$(CC) $(LIBRARIES) $(LD_FLAGS) -o $@ $^

client: client.o
	$(CC) $(LIBRARIES) $(LD_FLAGS) -o $@ $^

client_async: client_async.o
	$(CC) $(LIBRARIES) $(LD_FLAGS) -o $@ $^

evbuffer: evbuffer.o
	$(CC) $(LIBRARIES) $(LD_FLAGS) -o $@ $^

pipe: pipe.o
	$(CC) $(LIBRARIES) $(LD_FLAGS) -o $@ $^

test_atoi: test_atoi.o
	$(CC) $(LIBRARIES) $(LD_FLAGS) -o $@ $^

test_const_char:test_const_char.o
	$(CC) $(LIBRARIES) $(LD_FLAGS) -o $@ $^

valgrind_uninitialised_bytes:valgrind_uninitialised_bytes.o
	$(CC) $(LIBRARIES) $(LD_FLAGS) -o $@ $^

%.o: %.c
	$(CC) $(C_FLAGS) $(INCLUDES) -c $< -o $@

clean:
	$(RM) unit_test test client client_async evbuffer pipe test_atoi test_const_char valgrind_uninitialised_bytes *.o ../*.o ../*.eo
