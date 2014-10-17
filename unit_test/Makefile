CC=gcc
RM=rm -rf
INCLUDE_PATH=$(JUMBO_ROOT)/include
LIBRARY_PATH=$(JUMBO_ROOT)/lib
C_FLAGS=-I$(INCLUDE_PATH) -I.. -g -Wall# -DDUMP
LD_FLAGS=-L$(LIBRARY_PATH) -levent
HEADERS=../define.h ../http.h ../websocket.h
SOURCES=test.c ../http.c ../websocket.c ../sha1.c ../base64_enc.c
OBJS=$(patsubst %c, %o, $(SOURCES))
OBJ=test

all: $(OBJ)

$(OBJ): $(OBJS)
	$(CC) $(LD_FLAGS) $(OBJS) -o $@

%.o: %.c $(HEADERS)
	$(CC) $(C_FLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(OBJ)
