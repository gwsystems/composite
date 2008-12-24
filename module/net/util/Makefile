MAKE=make
CC=gcc
CFLAGS=-Wall 
#-O3 yeah, that gets rid of the busy loop in udp_client
TRANS_DIR=/home/grad3/gabep1/transfer/
SRC=cnet_user.c udp_client.c udp_server.c tcp_server.c tcp_client.c
OBJ=$(SRC:.c=.o)

all: $(OBJ)
	$(CC) -o cnet_user cnet_user.o
	$(CC) -o udp_client udp_client.o
	$(CC) -o udp_server udp_server.o
	$(CC) -o tcp_server tcp_server.o
	$(CC) -o tcp_client tcp_client.o

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

cp:
	cp $(OBJ:.o=) $(TRANS_DIR)

clean:
	rm -rf $(OBJ:.o=) *.o
