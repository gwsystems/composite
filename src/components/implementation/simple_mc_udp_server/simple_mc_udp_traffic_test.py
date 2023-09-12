import socket
import time

host='10.10.1.2'
#host='161.253.78.153'
#port=6
port=11211
udp_server_addr = (host, port)
#local_addr = ('', 10001)

timeout_seconds=10

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(timeout_seconds)

#sock.bind(local_addr)

expected_ret='\x00\x00\x00\x00\x00\x01\x00\x00VALUE GWU_SYS 0 5\r\nGREAT\r\nEND\r\n'

# Send a key with value to the server
data = '\0\0\0\0\0\1\0\0set GWU_SYS 0 0 5\r\nGREAT\r\n'
sock.sendto(data.encode(), udp_server_addr)
try:
        data = sock.recv(100)
        print(data)
except:
        print('data is not received')
        exit(0)

data = '\0\0\0\0\0\1\0\0get GWU_SYS\r\n'
sock.sendto(data.encode(), udp_server_addr)
try:
        data = sock.recv(100)
        data = repr(data.decode())
except:
        print('data is not received1')
        exit(0)
print(repr(expected_ret))
if data == repr(expected_ret):
        print('Simple query result: SUCCESS!')
print('end')
#exit(0)
#data = repr(data.decode())
#print(data)
#exit(0)
while True:
	data = '\0\0\0\0\0\1\0\0get GWU_SYS\r\n'
#	data = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
	sock.sendto(data.encode(), udp_server_addr)
	continue
