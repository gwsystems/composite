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
while True:
	data = 'hello from client: %s' % i
	sock.sendto(data.encode(), udp_server_addr)
	# try:
	# 	data = sock.recv(100)
	# except:
	# 	print('data %s not received' %i)
	# 	i = i + 1
	# 	# continue
	# 	exit(0)
	# print(data.decode())
	# time.sleep(1)
	i = i + 1

print('udp sent done')