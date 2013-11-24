import socket
import sys

def client_hello(sock):
    sock.send("Text Client Hello")
def client_process(recv):
    print recv

sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
sock.connect((sys.argv[1],int(sys.argv[2])))
client_hello(sock)

while 1:
    data = sock.recv(256)
    client_process(data)

