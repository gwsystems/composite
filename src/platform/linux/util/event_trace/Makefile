OBJ=event_trace

all: clean
	gcc $(OBJ).c udp_server.c -g -I. -o $(OBJ) -DLINUX_DECODE

clean:
	rm -f *.o $(OBJ)
