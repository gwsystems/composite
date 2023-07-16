# You can run this test script using this command:
# python3 src/components/implementation/simple_pingpong_udp_server/simple_pingpong_udp_test.py

import socket
import time

host='10.10.1.2'
port=6
udp_server_addr = (host, port)
local_addr = ('', 10000)

timeout_seconds=10

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(timeout_seconds)

sock.bind(local_addr)

expected_ret='\x00\x00\x00\x00\x00\x01\x00\x00VALUE GWU_SYS 0 5\r\nGREAT\r\nEND\r\n'
while True:
	data = '\0\0\0\0\0\1\0\0set GWU_SYS 0 0 5\r\nGREAT\r\n'
	sock.sendto(data.encode(), udp_server_addr)
	try:
		data = sock.recv(100)
	except:
		print('data is not received, test failed!')
		exit(0)
	data = repr(data.decode())
	print(data)
	
	print('Simple query result: SUCCESS!')
	exit(0)
