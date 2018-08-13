OBJ=event_trace

all: clean
	gcc $(OBJ).c udp_server.c -g -I. -o $(OBJ) -DEVENT_LINUX_DECODE

clean:
	rm -f *.o $(OBJ)
