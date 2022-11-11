# You can run this test script using this command:
# python3 src/components/implementation/simple_udp_echo_server/simple_udp_test.py

import socket
import time

host='10.10.1.2'
port=80
udp_server_addr = (host, port)
local_addr = ('', 10000)

timeout_seconds=10

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(timeout_seconds)

sock.bind(local_addr)

i = 0
while i < 100:
	data = 'hello from client: %s' % i
	sock.sendto(data.encode(), udp_server_addr)
	try:
		data = sock.recv(100)
	except:
		print('TEST FAILED: data %s not received' %i)
		i = i + 1
		exit(0)
	print(data.decode())
	i = i + 1

print('TEST SUCCESS: Simple UDP test done')
