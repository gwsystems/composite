MAKE=make
CC=gcc
CFLAGS=-Wall 
#-O3 yeah, that gets rid of the busy loop in udp_client
TRANS_DIR=/home/grad3/gabep1/transfer/

default: cnet_user udp_client

cnet_user: cnet_user.o
	$(CC) -o $@ $<

udp_client: udp_client.o
	$(CC) -o $@ $<

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

cp:
	cp cnet_user udp_client $(TRANS_DIR)

clean:
	rm -rf cnet_user udp_client *.o
