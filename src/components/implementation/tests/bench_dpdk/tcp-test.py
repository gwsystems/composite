import socket
import time

host='10.10.1.2'
port=80

timeout_seconds=10

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(timeout_seconds)
result = sock.connect((host,int(port)))
sock.send('hello composite dpdk lwip and tcp server from client\n'.encode())

# if result == 0:
#     print("Host: {}, Port: {} - True".format(host, port))
# else:
#     print("Host: {}, Port: {} - False".format(host, port))
time.sleep(5)

data = sock.recv(1000)
print(data.decode())


sock.send('second hello from client\n'.encode())

data = sock.recv(1000)
print(data.decode())

sock.close()
